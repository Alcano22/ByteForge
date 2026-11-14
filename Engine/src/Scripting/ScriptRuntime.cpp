#include "Scripting/ScriptRuntime.h"

#include "Engine/Core/Log.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/Entity.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scripting/ScriptEngine.h"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <utility>

namespace ByteForge
{
    namespace
    {
        bool SameId(const UUID a, const UUID b)
        {
            return static_cast<u64>(a) == static_cast<u64>(b);
        }

        void ApplyFields(const ScriptBackend& backend, const std::string& className,
                         ScriptInstance& script, const ScriptEntry& entry)
        {
            for (const ScriptFieldInfo& field : backend.GetFields(className))
            {
                const auto it = entry.Fields.find(field.Name);
                if (it == entry.Fields.end() || !IsCompatible(it->second, field)) continue;

                if (!script.SetField(field.Name, it->second))
                    CORE_WARN("Could not set field '{}' of script '{}'", field.Name, className);
            }
        }
    }

    ScriptRuntime::ScriptRuntime(Scene& scene)
        : m_Scene(scene)
    {
        m_Scene.m_Registry.on_destroy<ScriptComponent>().connect<&ScriptRuntime::OnComponentDestroyed>(*this);
    }

    ScriptRuntime::~ScriptRuntime()
    {
        m_Scene.m_Registry.on_destroy<ScriptComponent>().disconnect<&ScriptRuntime::OnComponentDestroyed>(*this);
    }

    void ScriptRuntime::Start()
    {
        if (m_Running) return;

        m_Running = true;
        ScriptEngine::Get().OnRuntimeStart(m_Scene);
        Sync();
    }

    void ScriptRuntime::Stop()
    {
        if (!m_Running) return;

        std::vector<entt::entity> handles;
        handles.reserve(m_Instances.size());
        for (const auto& [handle, instance] : m_Instances)
            handles.push_back(handle);

        for (const entt::entity handle : handles)
            ReleaseAll(handle);

        m_Released.clear();
        ScriptEngine::Get().OnRuntimeStop(m_Scene);
        m_Running = false;
    }

    void ScriptRuntime::Update(const Timestep ts)
    {
        Sync();

        for (const auto& [handle, entry] : CollectKeys())
            Invoke(handle, entry, "OnUpdate", [ts](ScriptInstance& script) { script.OnUpdate(ts); });

        m_Released.clear();
    }

    void ScriptRuntime::DispatchContact(const entt::entity self, const ContactEvent event, const Entity other)
    {
        const entt::registry& registry = m_Scene.m_Registry;
        if (!registry.valid(self)) return;

        const auto* component = registry.try_get<ScriptComponent>(self);
        if (component == nullptr) return;

        std::vector<UUID> entries;
        entries.reserve(component->Scripts.size());
        for (const ScriptEntry& entry : component->Scripts)
            entries.push_back(entry.Id);

        for (const UUID entry : entries)
            Invoke(self, entry, "OnContact", [event, other](ScriptInstance& script) { script.OnContact(event, other); });
    }

    ScriptInstance* ScriptRuntime::FindInstance(const entt::entity handle, const UUID entry) const
    {
        const Instance* instance = const_cast<ScriptRuntime*>(this)->FindMutable(handle, entry);
        if (instance == nullptr || instance->Failed)
            return nullptr;

        return instance->Script.get();
    }

    ScriptRuntime::Instance* ScriptRuntime::FindMutable(const entt::entity handle, const UUID entry)
    {
        const auto it = m_Instances.find(handle);
        if (it == m_Instances.end())
            return nullptr;

        const auto found = std::ranges::find_if(it->second, [entry](const Instance& instance)
        {
            return SameId(instance.EntryId, entry);
        });
        return found != it->second.end() ? &*found : nullptr;
    }

    std::vector<ScriptRuntime::ScriptKey> ScriptRuntime::CollectKeys() const
    {
        std::vector<ScriptKey> keys;

        const entt::registry& registry = m_Scene.m_Registry;
        for (const auto handle : registry.view<ScriptComponent>())
        {
            for (const ScriptEntry& entry : registry.get<ScriptComponent>(handle).Scripts)
                keys.push_back({ handle, entry.Id });
        }

        return keys;
    }

    void ScriptRuntime::Sync()
    {
        entt::registry& registry = m_Scene.m_Registry;

        std::vector<ScriptKey> stale;
        for (const auto& [handle, instances] : m_Instances)
        {
            const auto* component = registry.valid(handle) ? registry.try_get<ScriptComponent>(handle) : nullptr;

            for (const Instance& instance : instances)
            {
                const ScriptEntry* entry = component != nullptr ? component->Find(instance.EntryId) : nullptr;
                if (entry == nullptr || entry->ClassName != instance.ClassName)
                    stale.push_back({ handle, instance.EntryId });
            }
        }

        for (const auto& [handle, entry] : stale)
            Release(handle, entry);

        std::vector<ScriptKey> created;

        for (const auto& [handle, entryId] : CollectKeys())
        {
            if (!registry.valid(handle) || FindMutable(handle, entryId) != nullptr) continue;

            const auto* component = registry.try_get<ScriptComponent>(handle);
            const ScriptEntry* entry = component != nullptr ? component->Find(entryId) : nullptr;

            if (entry == nullptr || entry->ClassName.empty()) continue;

            Create(handle, *entry);
            created.push_back({ handle, entryId });
        }

        for (const auto& [handle, entryId] : created)
            Invoke(handle, entryId, "OnCreate", [](ScriptInstance& script) { script.OnCreate(); });
    }

    void ScriptRuntime::Create(const entt::entity handle, const ScriptEntry& entry)
    {
        const Entity entity(handle, &m_Scene);

        const std::string className = entry.ClassName;
        const UUID entryId = entry.Id;

        Instance instance{ .EntryId = entryId, .EntityId = entity.GetUUID(), .ClassName = className };

        ScriptBackend* backend = ScriptEngine::Get().FindBackend(className);
        if (backend == nullptr)
        {
            instance.Failed = true;
            CORE_WARN("Script class '{}' on '{}' does not exist", className, entity.GetTag());
        } else
        {
            try
            {
                instance.Script = backend->CreateInstance(className, entity);
                ApplyFields(*backend, className, *instance.Script, entry);
            } catch (const std::exception& e)
            {
                instance.Failed = true;
                CORE_ERROR("Could not create script '{}' on '{}': {}", className, entity.GetTag(), e.what());
            }
        }

        m_Instances[handle].push_back(std::move(instance));
    }

    void ScriptRuntime::Release(const entt::entity handle, const UUID entry)
    {
        if (FindMutable(handle, entry) == nullptr) return;

        Invoke(handle, entry, "OnDestroy", [](ScriptInstance& script) { script.OnDestroy(); });

        const auto it = m_Instances.find(handle);
        if (it == m_Instances.end()) return;

        std::vector<Instance>& instances = it->second;
        const auto found = std::ranges::find_if(instances, [entry](const Instance& instance)
        {
            return SameId(instance.EntryId, entry);
        });
        if (found == instances.end()) return;

        m_Released.push_back(std::move(*found));
        instances.erase(found);

        if (instances.empty())
            m_Instances.erase(it);
    }

    void ScriptRuntime::ReleaseAll(const entt::entity handle)
    {
        const auto it = m_Instances.find(handle);
        if (it == m_Instances.end()) return;

        std::vector<UUID> entries;
        entries.reserve(it->second.size());
        for (const Instance& instance : it->second)
            entries.push_back(instance.EntryId);

        for (auto entry = entries.rbegin(); entry != entries.rend(); ++entry)
            Release(handle, *entry);
    }

    void ScriptRuntime::OnComponentDestroyed(entt::registry&, const entt::entity handle)
    {
        if (m_Running)
            ReleaseAll(handle);
    }

    template<typename Fn>
    void ScriptRuntime::Invoke(const entt::entity handle, const UUID entry, const char* callback, Fn&& call)
    {
        const Instance* instance = FindMutable(handle, entry);
        if (instance == nullptr || instance->Failed || !instance->Script) return;

        ScriptInstance& script = *instance->Script;

        try
        {
            call(script);
        } catch (const std::exception& e)
        {
            Instance* failed = FindMutable(handle, entry);
            if (failed == nullptr)
            {
                CORE_ERROR("A released script failed in {}: {}", callback, e.what());
                return;
            }

            failed->Failed = true;
            CORE_ERROR("Script '{}' on entity {} failed in {}: {} (disabled until the next play)",
                       failed->ClassName, static_cast<u64>(failed->EntityId), callback, e.what());
        }
    }
}

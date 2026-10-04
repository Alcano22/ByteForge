#include "Scripting/ScriptRuntime.h"

#include "Engine/Core/Log.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/Entity.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scripting/ScriptEngine.h"

#include <cstdint>
#include <exception>
#include <utility>

namespace ByteForge
{
    namespace
    {
        void ApplyFields(const ScriptBackend& backend, const std::string& className,
                         ScriptInstance& script, const ScriptComponent& component)
        {
            for (const ScriptFieldInfo& field : backend.GetFields(className))
            {
                const auto it = component.Fields.find(field.Name);
                if (it == component.Fields.end() || !IsCompatible(it->second, field)) continue;

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
            Release(handle);

        m_Released.clear();
        ScriptEngine::Get().OnRuntimeStop(m_Scene);
        m_Running = false;
    }

    void ScriptRuntime::Update(const Timestep ts)
    {
        Sync();

        std::vector<entt::entity> handles;
        handles.reserve(m_Instances.size());
        for (const auto& [handle, instance] : m_Instances)
            handles.push_back(handle);

        for (const entt::entity handle : handles)
            Invoke(handle, "OnUpdate", [ts](ScriptInstance& script) { script.OnUpdate(ts); });

        m_Released.clear();
    }

    void ScriptRuntime::DispatchContact(const entt::entity self, const ContactEvent event, const Entity other)
    {
        Invoke(self, "OnContact", [event, other](ScriptInstance& script) { script.OnContact(event, other); });
    }

    ScriptInstance* ScriptRuntime::FindInstance(const entt::entity handle) const
    {
        const auto it = m_Instances.find(handle);
        if (it == m_Instances.end() || it->second.Failed)
            return nullptr;
        return it->second.Script.get();
    }

    void ScriptRuntime::Sync()
    {
        entt::registry& registry = m_Scene.m_Registry;

        std::vector<std::pair<entt::entity, std::string>> pending;
        for (const auto handle : registry.view<ScriptComponent>())
        {
            const std::string& className = registry.get<ScriptComponent>(handle).ClassName;

            const auto it = m_Instances.find(handle);
            if (it == m_Instances.end() || it->second.ClassName != className)
                pending.emplace_back(handle, className);
        }

        for (const auto& [handle, className] : pending)
        {
            if (!registry.valid(handle)) continue;

            Release(handle);
            Create(handle, className);
        }
    }

    void ScriptRuntime::Create(const entt::entity handle, const std::string& className)
    {
        const Entity entity(handle, &m_Scene);
        Instance instance{ .ClassName = className, .EntityId = entity.GetUUID() };

        ScriptBackend* backend = className.empty() ? nullptr : ScriptEngine::Get().FindBackend(className);
        if (backend == nullptr)
        {
            instance.Failed = true;
            if (!className.empty())
                CORE_WARN("Script class '{}' on '{}' does not exist", className, entity.GetTag());
        } else
        {
            try
            {
                instance.Script = backend->CreateInstance(className, entity);
                ApplyFields(*backend, className, *instance.Script, entity.GetComponent<ScriptComponent>());
            } catch (const std::exception& e)
            {
                instance.Failed = true;
                CORE_ERROR("Could not create script '{}' on '{}': {}", className, entity.GetTag(), e.what());
            }
        }

        m_Instances.insert_or_assign(handle, std::move(instance));
        Invoke(handle, "OnCreate", [](ScriptInstance& script) { script.OnCreate(); });
    }

    void ScriptRuntime::Release(const entt::entity handle)
    {
        if (!m_Instances.contains(handle)) return;

        Invoke(handle, "OnDestroy", [](ScriptInstance& script) { script.OnDestroy(); });

        if (const auto it = m_Instances.find(handle); it != m_Instances.end())
        {
            m_Released.push_back(std::move(it->second));
            m_Instances.erase(it);
        }
    }

    void ScriptRuntime::OnComponentDestroyed(entt::registry&, const entt::entity handle)
    {
        if (m_Running)
            Release(handle);
    }

    template<typename Fn>
    void ScriptRuntime::Invoke(const entt::entity handle, const char* callback, Fn&& call)
    {
        const auto it = m_Instances.find(handle);
        if (it == m_Instances.end() || it->second.Failed || !it->second.Script) return;

        ScriptInstance& script = *it->second.Script;

        try
        {
            call(script);
        } catch (const std::exception& e)
        {
            const auto failed = m_Instances.find(handle);
            if (failed == m_Instances.end())
            {
                CORE_ERROR("A released script failed in {}: {}", callback, e.what());
                return;
            }

            failed->second.Failed = true;
            CORE_ERROR("Script '{}' on entity {} failed in {}: {} (disabled until the next play)",
                       failed->second.ClassName, static_cast<uint64_t>(failed->second.EntityId), callback, e.what());
        }
    }
}

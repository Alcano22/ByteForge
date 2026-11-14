#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Core/Timestep.h"
#include "Engine/Scene/UUID.h"
#include "Engine/Scripting/ScriptBackend.h"

#include <entt/entt.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace ByteForge
{
    class Scene;

    class ScriptRuntime
    {
    public:
        explicit ScriptRuntime(Scene& scene);
        ~ScriptRuntime();

        ScriptRuntime(const ScriptRuntime&) = delete;
        ScriptRuntime& operator=(const ScriptRuntime&) = delete;

        void Start();
        void Stop();

        void Update(Timestep ts);

        void DispatchContact(entt::entity self, ContactEvent event, Entity other);

        [[nodiscard]] ScriptInstance* FindInstance(entt::entity handle, UUID entry) const;

    private:
        struct Instance
        {
            UUID EntryId;
            UUID EntityId;
            std::string ClassName;
            Scope<ScriptInstance> Script;
            bool Failed = false;
        };

        struct ScriptKey
        {
            entt::entity Handle;
            UUID Entry;
        };

        void Sync();

        void Create(entt::entity handle, const ScriptEntry& entry);
        void Release(entt::entity handle, UUID entry);
        void ReleaseAll(entt::entity handle);

        [[nodiscard]] Instance* FindMutable(entt::entity handle, UUID entry);
        [[nodiscard]] std::vector<ScriptKey> CollectKeys() const;

        void OnComponentDestroyed(entt::registry& registry, entt::entity handle);

        template<typename Fn>
        void Invoke(entt::entity handle, UUID entry, const char* callback, Fn&& call);

    private:
        Scene& m_Scene;
        std::unordered_map<entt::entity, std::vector<Instance>> m_Instances;

        std::vector<Instance> m_Released;

        bool m_Running = false;
    };
}

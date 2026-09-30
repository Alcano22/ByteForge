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

    private:
        void Sync();

        void Create(entt::entity handle, const std::string& className);
        void Release(entt::entity handle);

        void OnComponentDestroyed(entt::registry& registry, entt::entity handle);

        template<typename Fn>
        void Invoke(entt::entity handle, const char* callback, Fn&& call);

    private:
        struct Instance
        {
            std::string ClassName;
            UUID EntityId;
            Scope<ScriptInstance> Script;
            bool Failed = false;
        };

        Scene& m_Scene;
        std::unordered_map<entt::entity, Instance> m_Instances;

        std::vector<Instance> m_Released;

        bool m_Running = false;
    };
}

#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Core/NonCopyable.h"
#include "Engine/Core/Timestep.h"
#include "Engine/Renderer/Camera.h"
#include "Engine/Renderer/Renderer2D.h"
#include "Engine/Scene/UUID.h"

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <tuple>

namespace ByteForge
{
    class Entity;
    class Physics2DWorld;
    struct RaycastHit2D;
    class ScriptRuntime;
    class ScriptInstance;

    class BYTEFORGE_API Scene : NonCopyable
    {
    public:
        Scene();
        ~Scene();

        Entity CreateEntity(const std::string& name = std::string());
        void DestroyEntity(Entity entity);
        Entity DuplicateEntity(Entity source);
        [[nodiscard]] Entity FindEntityByUUID(UUID uuid);
        [[nodiscard]] Entity FindEntityByPickingId(uint32_t pickingId);

        void OnUpdateRuntime(Timestep ts);

        void Render(Renderer2D& renderer, const Camera& camera);

        void OnRuntimeStart();
        void OnRuntimeStop();

        [[nodiscard]] ScriptInstance* FindScriptInstance(Entity entity) const;

        [[nodiscard]] bool IsRunning() const { return m_ScriptRuntime != nullptr; }

        [[nodiscard]] RaycastHit2D Raycast2D(const glm::vec2& origin, const glm::vec2& direction,
                                             float maxDistance) const;

        template<typename... Components, typename Func>
        void Each(Func func)
        {
            for (const auto handle : m_Registry.view<Components...>())
            {
                std::apply(func, std::tuple<Entity, Components&...>(
                    Entity(handle, this), m_Registry.get<Components>(handle)...));
            }
        }

        void Clear();

    private:
        void ResetPhysicsWorld();

        [[nodiscard]] static uint32_t ToPickingId(entt::entity handle);

        static void DispatchSensorEvent(Entity self, Entity other, bool entered);
        static void DispatchCollisionEvent(Entity self, Entity other, bool entered);

    private:
        friend class Entity;
        friend class Physics2DWorld;
        friend class ScriptRuntime;

        entt::registry m_Registry;
        Scope<Physics2DWorld> m_PhysicsWorld;
        Scope<ScriptRuntime> m_ScriptRuntime;
    };
}

#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Core/NonCopyable.h"
#include "Engine/Core/Timestep.h"
#include "Engine/Renderer/Camera.h"
#include "Engine/Renderer/Renderer2D.h"
#include "Engine/Scene/UUID.h"
#include "Engine/Scene/PhysicsLayers.h"

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace ByteForge
{
    class Entity;
    class Physics2DWorld;
    struct RaycastHit2D;
    struct TransformComponent;
    class ScriptRuntime;
    class ScriptInstance;

    class BYTEFORGE_API Scene : NonCopyable
    {
    public:
        Scene();
        ~Scene();

        Entity CreateEntity(const std::string& name = std::string());
        Entity CreateEntityWithUUID(UUID uuid, const std::string& name = std::string());

        void DestroyEntity(Entity entity);
        void QueueDestroyEntity(UUID uuid);

        Entity DuplicateEntity(Entity source);
        Entity CopySubtree(Entity source);

        [[nodiscard]] Entity FindEntityByUUID(UUID uuid) const;
        [[nodiscard]] Entity FindEntityByPickingId(u32 pickingId);
        [[nodiscard]] Entity GetPrimaryCamera();

        void SetParent(Entity child, Entity parent, bool keepWorldTransform = true, i32 index = -1);
        [[nodiscard]] Entity GetParent(Entity entity) const;
        [[nodiscard]] std::vector<Entity> GetChildren(Entity entity) const;
        [[nodiscard]] std::vector<Entity> GetRootEntities();
        [[nodiscard]] bool IsDescendantOf(Entity entity, Entity ancestor) const;

        [[nodiscard]] i32 GetSiblingIndex(Entity entity) const;

        [[nodiscard]] TransformComponent GetWorldTransform(Entity entity) const;
        void SetWorldTransform(Entity entity, const TransformComponent& world);

        void OnUpdateRuntime(Timestep ts);
        void OnStepRuntime();

        void Render(Renderer2D& renderer, const Camera& camera);

        void OnRuntimeStart();
        void OnRuntimeStop();

        void RequestSceneLoad(UUID sceneAsset);

        void EnsureRuntimeBody(Entity entity);
        void SyncRuntimeBodies(Entity root);
        void RebuildRuntimeBody(Entity entity);

        [[nodiscard]] ScriptInstance* FindScriptInstance(Entity entity) const;

        [[nodiscard]] bool IsRunning() const { return m_ScriptRuntime != nullptr; }

        [[nodiscard]] RaycastHit2D Raycast2D(const glm::vec2& origin, const glm::vec2& direction,
                                             f32 maxDistance, u32 layerMask = PhysicsLayers::AllLayers) const;

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

        void FlushPendingDestroy();
        void ApplyPendingSceneLoad();

        [[nodiscard]] Entity CopyEntityTree(Entity source, std::unordered_map<UUID, UUID>& remap);

        [[nodiscard]] static u32 ToPickingId(entt::entity handle);

        static void DispatchSensorEvent(Entity self, Entity other, bool entered);
        static void DispatchCollisionEvent(Entity self, Entity other, bool entered);

    private:
        friend class Entity;
        friend class Physics2DWorld;
        friend class ScriptRuntime;

        entt::registry m_Registry;
        std::unordered_map<UUID, entt::entity> m_EntityByUUID;

        std::vector<UUID> m_RootOrder;

        std::vector<UUID> m_PendingDestroy;
        std::optional<UUID> m_PendingSceneLoad;
        Scope<Physics2DWorld> m_PhysicsWorld;
        Scope<ScriptRuntime> m_ScriptRuntime;
    };
}

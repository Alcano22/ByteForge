#include "Editor/Commands/EntityCommands.h"

#include <Engine/Scene/Entity.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Scene/SceneSerializer.h>
#include <Engine/Scene/Components.h>

#include <cstdint>
#include <format>
#include <stdexcept>
#include <utility>

namespace ByteForge
{
    namespace
    {
        UUID UuidOf(const nlohmann::json& entityJson)
        {
            return UUID(entityJson.at("uuid").get<u64>());
        }

        UUID RootUuidOf(const nlohmann::json& subtree)
        {
            return UuidOf(subtree.at(0));
        }

        Entity FindOrThrow(const Scene& scene, const UUID id)
        {
            const Entity entity = scene.FindEntityByUUID(id);
            if (!entity.IsValid())
                throw std::runtime_error(std::format("Entity {} no longer exists", static_cast<u64>(id)));

            return entity;
        }

        void RestoreSubtree(Scene& scene, const nlohmann::json& subtree)
        {
            if (scene.FindEntityByUUID(RootUuidOf(subtree)).IsValid())
            {
                throw std::runtime_error(std::format("Entity {} already exists",
                                                     static_cast<u64>(RootUuidOf(subtree))));
            }

            SceneSerializer::DeserializeSubtree(scene, subtree);
        }
    }

    ModifyEntityCommand::ModifyEntityCommand(Scene& scene, std::string name,
                                             nlohmann::json before, nlohmann::json after)
        : m_Scene(scene), m_Name(std::move(name)), m_Before(std::move(before)), m_After(std::move(after)) {}

    void ModifyEntityCommand::Execute() { SceneSerializer::ApplyEntity(FindOrThrow(m_Scene, UuidOf(m_After)), m_After); }
    void ModifyEntityCommand::Undo() { SceneSerializer::ApplyEntity(FindOrThrow(m_Scene, UuidOf(m_Before)), m_Before); }

    CreateEntityCommand::CreateEntityCommand(Scene& scene, std::string name, nlohmann::json subtree)
        : m_Scene(scene), m_Name(std::move(name)), m_Subtree(std::move(subtree)) {}

    void CreateEntityCommand::Execute() { RestoreSubtree(m_Scene, m_Subtree); }
    void CreateEntityCommand::Undo() { m_Scene.DestroyEntity(FindOrThrow(m_Scene, RootUuidOf(m_Subtree))); }

    DestroyEntityCommand::DestroyEntityCommand(Scene& scene, std::string name, nlohmann::json subtree)
        : m_Scene(scene), m_Name(std::move(name)), m_Subtree(std::move(subtree)) {}

    void DestroyEntityCommand::Execute() { m_Scene.DestroyEntity(FindOrThrow(m_Scene, RootUuidOf(m_Subtree))); }
    void DestroyEntityCommand::Undo() { RestoreSubtree(m_Scene, m_Subtree); }

    ModifyTransformCommand::ModifyTransformCommand(Scene& scene, const UUID entityId, std::string name,
                                                   const TransformComponent& before, const TransformComponent& after)
        : m_Scene(scene), m_EntityId(entityId), m_Name(std::move(name)), m_Before(before), m_After(after) {}

    void ModifyTransformCommand::Apply(const TransformComponent& transform) const
    {
        const Entity entity = FindOrThrow(m_Scene, m_EntityId);
        entity.GetComponent<TransformComponent>() = transform;
    }

    SetParentCommand::SetParentCommand(Scene& scene, const UUID entityId, std::string name,
                                       const UUID oldParent, const i32 oldIndex, const TransformComponent& before,
                                       const UUID newParent, const i32 newIndex, const TransformComponent& after)
        : m_Scene(scene), m_EntityId(entityId), m_Name(std::move(name)),
          m_OldParent(oldParent), m_OldIndex(oldIndex), m_Before(before),
          m_NewParent(newParent), m_NewIndex(newIndex), m_After(after) {}

    void SetParentCommand::Apply(const UUID parentId, const i32 index, const TransformComponent& local) const
    {
        const Entity entity = FindOrThrow(m_Scene, m_EntityId);
        const Entity parent = static_cast<u64>(parentId) != 0 ? FindOrThrow(m_Scene, parentId) : Entity{};

        entity.SetParent(parent, false, index);
        entity.GetComponent<TransformComponent>() = local;
    }
}

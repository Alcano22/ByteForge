#include "Editor/Commands/EntityCommands.h"

#include <Engine/Scene/Entity.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Scene/SceneSerializer.h>

#include <cstdint>
#include <format>
#include <stdexcept>
#include <utility>

namespace ByteForge
{
    namespace
    {
        UUID UuidOf(const nlohmann::json& snapshot)
        {
            return UUID(snapshot.at("uuid").get<uint64_t>());
        }

        Entity FindOrThrow(Scene& scene, const nlohmann::json& snapshot)
        {
            const Entity entity = scene.FindEntityByUUID(UuidOf(snapshot));
            if (!entity.IsValid())
            {
                throw std::runtime_error(std::format("Entity {} no longer exists",
                                                     static_cast<uint64_t>(UuidOf(snapshot))));
            }
            return entity;
        }

        void Restore(Scene& scene, const nlohmann::json& snapshot)
        {
            if (scene.FindEntityByUUID(UuidOf(snapshot)).IsValid())
            {
                throw std::runtime_error(std::format("Entity {} already exists",
                                                     static_cast<uint64_t>(UuidOf(snapshot))));
            }

            SceneSerializer::DeserializeEntity(scene, snapshot);
        }
    }

    ModifyEntityCommand::ModifyEntityCommand(Scene& scene, std::string name,
                                             nlohmann::json before, nlohmann::json after)
        : m_Scene(scene), m_Name(std::move(name)), m_Before(std::move(before)), m_After(std::move(after)) {}

    void ModifyEntityCommand::Execute() { SceneSerializer::ApplyEntity(FindOrThrow(m_Scene, m_After), m_After); }
    void ModifyEntityCommand::Undo() { SceneSerializer::ApplyEntity(FindOrThrow(m_Scene, m_Before), m_Before); }

    CreateEntityCommand::CreateEntityCommand(Scene& scene, std::string name, nlohmann::json snapshot)
        : m_Scene(scene), m_Name(std::move(name)), m_Snapshot(std::move(snapshot)) {}

    void CreateEntityCommand::Execute() { Restore(m_Scene, m_Snapshot); }
    void CreateEntityCommand::Undo() { m_Scene.DestroyEntity(FindOrThrow(m_Scene, m_Snapshot)); }

    DestroyEntityCommand::DestroyEntityCommand(Scene& scene, std::string name, nlohmann::json snapshot)
        : m_Scene(scene), m_Name(std::move(name)), m_Snapshot(std::move(snapshot)) {}

    void DestroyEntityCommand::Execute() { m_Scene.DestroyEntity(FindOrThrow(m_Scene, m_Snapshot)); }
    void DestroyEntityCommand::Undo() { Restore(m_Scene, m_Snapshot); }
}

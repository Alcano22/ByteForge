#include "Editor/Commands/EntityEditTracker.h"
#include "Editor/Commands/CommandHistory.h"
#include "Editor/Commands/EntityCommands.h"

#include <Engine/Scene/Scene.h>
#include <Engine/Scene/SceneSerializer.h>

#include <imgui.h>

#include <format>
#include <string>
#include <utility>

namespace ByteForge
{
    void EntityEditTracker::Begin(const Entity entity, CommandHistory& history)
    {
        if (m_Pending && static_cast<uint64_t>(m_Pending->EntityId) != static_cast<uint64_t>(entity.GetUUID()))
            Flush(history);

        m_FrameStart = SceneSerializer::SerializeEntity(entity);
    }

    void EntityEditTracker::End(const Entity entity, CommandHistory& history)
    {
        if (!entity.IsValid())
        {
            m_Pending.reset();
            return;
        }

        nlohmann::json current = SceneSerializer::SerializeEntity(entity);

        if (!m_Pending && current != m_FrameStart)
        {
            m_Pending = PendingEdit{
                .OwnerScene = &entity.GetScene(),
                .EntityId   = entity.GetUUID(),
                .Before     = std::move(m_FrameStart),
                .Revision   = history.GetRevision()
            };
        }

        if (m_Pending && !ImGui::IsAnyItemActive())
            Commit(history, std::move(current));
    }

    void EntityEditTracker::Flush(CommandHistory& history)
    {
        if (!m_Pending) return;

        const Entity entity = m_Pending->OwnerScene->FindEntityByUUID(m_Pending->EntityId);
        if (!entity.IsValid())
        {
            m_Pending.reset();
            return;
        }

        Commit(history, SceneSerializer::SerializeEntity(entity));
    }

    void EntityEditTracker::Commit(CommandHistory& history, nlohmann::json after)
    {
        PendingEdit pending = std::move(*m_Pending);
        m_Pending.reset();

        if (pending.Revision != history.GetRevision() || pending.Before == after) return;

        const std::string name = std::format("Edit '{}'", after.value("tag", std::string("Entity")));
        history.Record(MakeScope<ModifyEntityCommand>(*pending.OwnerScene, name,
                                                      std::move(pending.Before), std::move(after)));
    }
}

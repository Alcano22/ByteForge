#pragma once

#include <Engine/Scene/Entity.h>
#include <Engine/Scene/UUID.h>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <optional>

namespace ByteForge
{
    class CommandHistory;
    class Scene;

    class EntityEditTracker
    {
    public:
        void Begin(Entity entity, CommandHistory& history);
        void End(Entity entity, CommandHistory& history);

        void Flush(CommandHistory& history);

        void Cancel() { m_Pending.reset(); }

    private:
        void Commit(CommandHistory& history, nlohmann::json after);

    private:
        struct PendingEdit
        {
            Scene* OwnerScene = nullptr;
            UUID EntityId;
            nlohmann::json Before;
            uint64_t Revision = 0;
        };

        nlohmann::json m_FrameStart;
        std::optional<PendingEdit> m_Pending;
    };
}

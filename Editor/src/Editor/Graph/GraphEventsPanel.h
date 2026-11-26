#pragma once

#include "Editor/Graph/GraphDocument.h"
#include "Editor/Graph/GraphParameterEditor.h"

#include <Engine/Scene/UUID.h>
#include <Engine/Scripting/Visual/ScriptGraph.h>

#include <imgui.h>

#include <cstring>
#include <optional>
#include <string>

namespace ByteForge
{
    inline constexpr const char* GraphEventPayloadType = "ByteForge.GraphCustomEvent";

    [[nodiscard]] inline std::optional<UUID> ReadGraphEventPayload(const ImGuiPayload* payload)
    {
        if (payload == nullptr || !payload->IsDataType(GraphEventPayloadType) ||
            payload->DataSize != static_cast<i32>(sizeof(u64)))
            return std::nullopt;

        u64 id = 0;
        std::memcpy(&id, payload->Data, sizeof(id));
        return UUID(id);
    }

    class GraphEventsPanel
    {
    public:
        void Draw(GraphDocument& document);
        void Reset();

    private:
        void DrawList(GraphDocument& document);
        void DrawDetails(GraphDocument& document, const GraphCustomEvent& event);

        void Apply(GraphDocument& document, GraphCustomEvent changed, const char* action);
        void AddEvent(GraphDocument& document);
        void DeleteEvent(GraphDocument& document, UUID event);

    private:
        UUID m_Selected{ 0 };
        GraphParameterEditor m_Parameters;
        std::string m_Error;
    };
}

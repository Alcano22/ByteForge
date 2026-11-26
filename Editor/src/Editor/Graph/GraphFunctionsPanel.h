#pragma once

#include "Editor/Graph/GraphDocument.h"
#include "Editor/Graph/GraphParameterEditor.h"

#include <Engine/Scene/UUID.h>
#include <Engine/Scripting/Visual/ScriptGraph.h>

#include <imgui.h>

#include <cstring>
#include <optional>
#include <string>
#include <utility>

namespace ByteForge
{
    inline constexpr const char* GraphFunctionPayloadType = "ByteForge.GraphFunction";

    [[nodiscard]] inline std::optional<UUID> ReadGraphFunctionPayload(const ImGuiPayload* payload)
    {
        if (payload == nullptr || !payload->IsDataType(GraphFunctionPayloadType) ||
            payload->DataSize != static_cast<i32>(sizeof(u64)))
            return std::nullopt;

        u64 id = 0;
        std::memcpy(&id, payload->Data, sizeof(id));
        return UUID(id);
    }

    class GraphFunctionsPanel
    {
    public:
        void Draw(GraphDocument& document);
        void Reset();

        [[nodiscard]] std::optional<UUID> TakeOpenRequest() { return std::exchange(m_OpenRequest, std::nullopt); }

    private:
        void DrawList(GraphDocument& document);
        void DrawDetails(GraphDocument& document, const GraphFunction& function);

        void Apply(GraphDocument& document, GraphFunction changed, const char* action);
        void AddFunction(GraphDocument& document);
        void DeleteFunction(GraphDocument& document, UUID function);

    private:
        UUID m_Selected{ 0 };
        std::optional<UUID> m_OpenRequest;
        GraphParameterEditor m_Parameters;
        std::string m_Error;
    };
}

#pragma once

#include "Editor/Graph/GraphDocument.h"
#include "Editor/Selection.h"

#include <Engine/Scene/UUID.h>
#include <Engine/Scripting/ScriptField.h>
#include <Engine/Scripting/Visual/ScriptGraph.h>

#include <imgui.h>

#include <cstring>
#include <optional>
#include <string>

namespace ByteForge
{
    inline constexpr const char* GraphVariablePayloadType = "ByteForge.GraphVariable";

    [[nodiscard]] inline std::optional<UUID> ReadGraphVariablePayload(const ImGuiPayload* payload)
    {
        if (payload == nullptr || !payload->IsDataType(GraphVariablePayloadType) ||
            payload->DataSize != static_cast<i32>(sizeof(u64)))
            return std::nullopt;

        u64 id = 0;
        std::memcpy(&id, payload->Data, sizeof(id));
        return UUID(id);
    }

    class GraphVariablesPanel
    {
    public:
        void Draw(GraphDocument& document, Selection& selection);
        void Reset();

    private:
        void DrawList(GraphDocument& document);
        void DrawDetails(GraphDocument& document, const GraphVariable& variable, Selection& selection);
        void DrawName(GraphDocument& document, const GraphVariable& variable);
        static void DrawType(GraphDocument& document, const GraphVariable& variable);
        void DrawDefault(GraphDocument& document, const GraphVariable& variable, Selection& selection);

        void AddVariable(GraphDocument& document);
        void DeleteVariable(GraphDocument& document, UUID variable);

    private:
        struct DefaultEdit
        {
            UUID Variable{ 0 };
            ScriptValue Value;
        };

        UUID m_Selected{ 0 };

        std::string m_Name;
        UUID m_NameVariable{ 0 };
        u64 m_NameRevision = 0;
        std::string m_NameError;
        bool m_EditingName = false;
        bool m_FocusName = false;

        std::optional<DefaultEdit> m_DefaultEdit;
    };
}

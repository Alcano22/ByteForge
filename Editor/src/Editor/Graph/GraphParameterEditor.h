#pragma once

#include <Engine/Scripting/Visual/ScriptGraph.h>

#include <optional>
#include <string>
#include <vector>

namespace ByteForge
{
    class GraphParameterEditor
    {
    public:
        struct Change
        {
            std::vector<GraphParameter> Parameters;
            const char* Action = "";
        };

        [[nodiscard]] std::optional<Change> Draw(const char* title, const char* addTooltip,
                                                 const std::vector<GraphParameter>& parameters);

        [[nodiscard]] std::optional<std::string> NameField(const char* label, u64 key, const std::string& current);

        void Reset();

    private:
        [[nodiscard]] std::optional<Change> DrawRow(const std::vector<GraphParameter>& parameters, usize index);

    private:
        u64 m_EditKey = 0;
        std::string m_EditText;
    };
}

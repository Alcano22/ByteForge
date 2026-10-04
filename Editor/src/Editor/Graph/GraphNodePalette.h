#pragma once

#include <Engine/Scripting/Visual/ScriptGraph.h>

#include <optional>
#include <string>
#include <vector>

namespace ByteForge
{
    struct PaletteContext
    {
        PinType Type;
        PinDirection Direction = PinDirection::Output;
    };

    class GraphNodePalette
    {
    public:
        void Open(const ScriptGraph& graph, std::optional<PaletteContext> context);

        [[nodiscard]] std::optional<NodeData> Draw();

    private:
        struct Entry
        {
            std::string Category;
            std::string Title;
            std::string SearchText;
            NodeData Data;
        };

        std::vector<Entry> m_Entries;
        std::string m_Search;
        std::string m_Hint;
        bool m_FocusSearch = false;
    };
}

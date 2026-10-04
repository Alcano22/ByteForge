#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Scripting/Visual/ScriptGraph.h"

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ByteForge
{
    namespace GraphPin
    {
        inline constexpr std::string_view Exec      = "Exec";
        inline constexpr std::string_view Then      = "Then";
        inline constexpr std::string_view True      = "True";
        inline constexpr std::string_view False     = "False";
        inline constexpr std::string_view Condition = "Condition";
        inline constexpr std::string_view Duration  = "Duration";
        inline constexpr std::string_view Value     = "Value";
    }

    struct NodeSignature
    {
        std::string Title;
        std::string Category;
        std::vector<PinInfo> Pins;
        bool Pure = false;
        bool Latent = false;
        std::optional<std::string> Error;
    };

    [[nodiscard]] BYTEFORGE_API NodeSignature DescribeNode(const ScriptGraph& graph, const GraphNode& node);
    [[nodiscard]] BYTEFORGE_API const PinInfo* FindPin(const NodeSignature& signature, std::string_view name,
                                                       PinDirection direction);

    [[nodiscard]] BYTEFORGE_API std::string DescribePinType(const PinType& type);

    [[nodiscard]] BYTEFORGE_API bool IsAssignable(const PinType& from, const PinType& to);

    [[nodiscard]] BYTEFORGE_API std::expected<PinType, std::string> CanConnect(const ScriptGraph& graph,
                                                                               const PinRef& from, const PinRef& to);

    enum class GraphSeverity : uint8_t { Warning, Error };

    struct GraphDiagnostic
    {
        UUID Node{ 0 };
        GraphSeverity Severity = GraphSeverity::Error;
        std::string Message;
    };

    [[nodiscard]] BYTEFORGE_API std::vector<GraphDiagnostic> ValidateGraph(const ScriptGraph& graph);
}

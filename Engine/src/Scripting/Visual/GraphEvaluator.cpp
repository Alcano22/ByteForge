#include "Engine/Scripting/Visual/GraphEvaluator.h"

#include "Engine/Scripting/API/ScriptAPI.h"
#include "Engine/Scripting/Visual/GraphSchema.h"

#include <algorithm>
#include <array>
#include <exception>
#include <string_view>
#include <variant>
#include <vector>

namespace ByteForge
{
    namespace
    {
        constexpr usize MaxDepth = 64;

        constexpr std::array<std::string_view, 3> ConstantCategories{ "Math", "Logic", "String" };

        const GraphLink* FindLinkTo(const ScriptGraph& graph, const UUID node, const std::string_view pin)
        {
            for (const GraphLink& link : graph.GetLinks())
            {
                if (link.To.Node == node && link.To.Pin == pin)
                    return &link;
            }
            return nullptr;
        }

        const GraphLink* FindLinkFrom(const ScriptGraph& graph, const UUID node, const std::string_view pin)
        {
            for (const GraphLink& link : graph.GetLinks())
            {
                if (link.From.Node == node && link.From.Pin == pin)
                    return &link;
            }
            return nullptr;
        }

        class Evaluator
        {
        public:
            explicit Evaluator(const ScriptGraph& graph)
                : m_Graph(graph) {}

            std::optional<ScriptValue> Output(const PinRef& pin, const usize frame)
            {
                const GraphNode* node = m_Graph.FindNode(pin.Node);
                if (node == nullptr || m_Depth >= MaxDepth)
                    return std::nullopt;

                ++m_Depth;
                std::optional<ScriptValue> value = std::visit([&](const auto& data)
                {
                    return Evaluate(*node, data, pin.Pin, frame);
                }, node->Data);
                --m_Depth;

                return value;
            }

        private:
            struct Frame
            {
                const GraphNode* Call = nullptr;
                usize Parent = 0;
            };

            template<typename T>
            std::optional<ScriptValue> Evaluate(const GraphNode&, const T&, std::string_view, usize)
            {
                return std::nullopt;
            }

            std::optional<ScriptValue> Evaluate(const GraphNode&, const LiteralNode& literal, std::string_view, usize)
            {
                return literal.Value;
            }

            std::optional<ScriptValue> Evaluate(const GraphNode& node, const RerouteNode&,
                                                std::string_view, const usize frame)
            {
                return Input(node, GraphPin::In, frame);
            }

            std::optional<ScriptValue> Evaluate(const GraphNode& node, const SelectNode&,
                                                std::string_view, const usize frame)
            {
                const std::optional<ScriptValue> condition = Input(node, GraphPin::Condition, frame);
                const auto* pick = condition ? std::get_if<bool>(&*condition) : nullptr;
                if (pick == nullptr)
                    return std::nullopt;

                return Input(node, *pick ? GraphPin::IfTrue : GraphPin::IfFalse, frame);
            }

            std::optional<ScriptValue> Evaluate(const GraphNode& node, const CallNode& call,
                                                std::string_view, const usize frame)
            {
                const ScriptFunction* function = ScriptAPI::Get().Find(call.Function);
                if (function == nullptr || !function->Pure || !function->Result ||
                    std::ranges::find(ConstantCategories, function->Category) == ConstantCategories.end())
                    return std::nullopt;

                std::vector<ScriptValue> arguments;
                for (const PinInfo& pin : DescribeNode(m_Graph, node).Pins)
                {
                    if (pin.Direction != PinDirection::Input || pin.Type.IsExec) continue;

                    std::optional<ScriptValue> argument = Input(node, pin, frame);
                    if (!argument)
                        return std::nullopt;
                    arguments.push_back(std::move(*argument));
                }

                try
                {
                    return function->Invoke(ScriptCallContext{}, arguments);
                } catch (const std::exception&)
                {
                    return std::nullopt;
                }
            }

            std::optional<ScriptValue> Evaluate(const GraphNode& node, const CallFunctionNode& call,
                                                const std::string_view pin, const usize frame)
            {
                const GraphFunction* function = m_Graph.FindFunction(call.Function);
                if (function == nullptr || !function->Pure)
                    return std::nullopt;

                const GraphNode* result = FindDirectReturn(function->Id);
                if (result == nullptr)
                    return std::nullopt;

                m_Frames.push_back({ &node, frame });
                return Input(*result, pin, m_Frames.size() - 1);
            }

            std::optional<ScriptValue> Evaluate(const GraphNode&, const FunctionEntryNode& entry,
                                                const std::string_view pin, const usize frame)
            {
                if (frame == 0)
                    return std::nullopt;

                const Frame& current = m_Frames[frame];
                const auto* call = std::get_if<CallFunctionNode>(&current.Call->Data);
                if (call == nullptr || call->Function != entry.Function)
                    return std::nullopt;

                return Input(*current.Call, pin, current.Parent);
            }

            [[nodiscard]] const GraphNode* FindDirectReturn(const UUID function) const
            {
                for (const GraphNode& node : m_Graph.GetNodes())
                {
                    const auto* entry = std::get_if<FunctionEntryNode>(&node.Data);
                    if (entry == nullptr || entry->Function != function) continue;

                    const GraphLink* next = FindLinkFrom(m_Graph, node.Id, GraphPin::Then);
                    const GraphNode* target = next != nullptr ? m_Graph.FindNode(next->To.Node) : nullptr;
                    return target != nullptr && std::holds_alternative<FunctionReturnNode>(target->Data) ? target : nullptr;
                }
                return nullptr;
            }

            std::optional<ScriptValue> Input(const GraphNode& node, const std::string_view name, const usize frame)
            {
                const NodeSignature signature = DescribeNode(m_Graph, node);
                const PinInfo* pin = FindPin(signature, name, PinDirection::Input);
                if (pin == nullptr || pin->Type.IsExec || pin->Type.IsWildcard())
                    return std::nullopt;

                return Input(node, *pin, frame);
            }

            std::optional<ScriptValue> Input(const GraphNode& node, const PinInfo& pin, const usize frame)
            {
                std::optional<ScriptValue> value;
                if (const GraphLink* link = FindLinkTo(m_Graph, node.Id, pin.Name))
                    value = Output(link->From, frame);
                else if (pin.Type.Value == ScriptFieldType::Entity)
                    return std::nullopt;
                else if (const auto stored = node.Defaults.find(pin.Name);
                         stored != node.Defaults.end() && IsAssignable(PinType::Of(stored->second), pin.Type))
                    value = stored->second;
                else
                    value = DefaultValueFor(pin.Type);

                if (value && pin.Type.Value == ScriptFieldType::Float && std::holds_alternative<i32>(*value))
                    value = static_cast<f32>(std::get<i32>(*value));

                return value;
            }

        private:
            const ScriptGraph& m_Graph;
            std::vector<Frame> m_Frames{ Frame{} };
            usize m_Depth = 0;
        };
    }

    std::optional<ScriptValue> EvaluateConstant(const ScriptGraph& graph, const PinRef& output)
    {
        return Evaluator(graph).Output(output, 0);
    }

    PinRef FindValueSource(const ScriptGraph& graph, PinRef output)
    {
        for (usize step = 0; step < MaxDepth; ++step)
        {
            const GraphNode* node = graph.FindNode(output.Node);
            if (node == nullptr || !std::holds_alternative<RerouteNode>(node->Data))
                return output;

            const GraphLink* link = FindLinkTo(graph, output.Node, GraphPin::In);
            if (link == nullptr)
                return output;

            output = link->From;
        }
        return output;
    }
}

#include "Scripting/Visual/GraphCompiler.h"

#include "Engine/Scripting/API/ScriptAPI.h"
#include "Engine/Scripting/Visual/GraphSchema.h"

#include <format>
#include <map>
#include <set>
#include <string_view>
#include <utility>

namespace ByteForge
{
    namespace
    {
        bool NeedsIntToFloat(const PinType& from, const PinType& to)
        {
            return from.Value == ScriptFieldType::Int && to.Value == ScriptFieldType::Float;
        }

        class Compiler
        {
        public:
            explicit Compiler(const ScriptGraph& graph)
                : m_Graph(graph) {}

            std::expected<GraphProgram, std::string> Run()
            {
                if (std::optional<std::string> error = FirstError())
                    return std::unexpected(std::move(*error));

                AddSlot(EntityRef{}); // SelfSlot, set per instance

                for (const GraphVariable& variable : m_Graph.GetVariables())
                {
                    const u32 slot = AddSlot(variable.Default);
                    m_VariableSlots.emplace(variable.Id, slot);
                    m_Program.VariableSlots.emplace(variable.Name, slot);

                    if (variable.Exposed)
                        m_Program.Fields.push_back({ .Name = variable.Name, .Type = GetFieldType(variable.Default),
                                                     .Default = variable.Default });
                }

                for (const GraphNode& node : m_Graph.GetNodes())
                {
                    const auto* event = std::get_if<EventNode>(&node.Data);
                    if (event == nullptr) continue;

                    u32 payload = NoSlot;
                    for (const PinInfo& pin : Signature(node.Id).Pins)
                    {
                        if (pin.Direction == PinDirection::Output && !pin.Type.IsExec)
                            payload = OutputSlot(node.Id, pin);
                    }

                    const u32 entry = Here();
                    m_Label = Label(node.Id);
                    EmitContinuation(node.Id, GraphPin::Then);

                    m_Program.Events[static_cast<usize>(event->Event)] = GraphEntry{ entry, payload };
                }

                return std::move(m_Program);
            }

        private:
            std::optional<std::string> FirstError() const
            {
                for (const GraphDiagnostic& diagnostic : ValidateGraph(m_Graph))
                {
                    if (diagnostic.Severity != GraphSeverity::Error) continue;

                    const GraphNode* node = m_Graph.FindNode(diagnostic.Node);
                    return node != nullptr
                         ? std::format("{}: {}", DescribeNode(m_Graph, *node).Title, diagnostic.Message)
                         : diagnostic.Message;
                }
                return std::nullopt;
            }

            void EmitContinuation(const UUID node, const std::string_view pin)
            {
                const GraphLink* link = FindLinkFrom(node, pin);
                if (link == nullptr)
                {
                    Emit(GraphOp::Return);
                    return;
                }

                if (const auto it = m_NodeCode.find(link->To.Node); it != m_NodeCode.end())
                {
                    Emit(GraphOp::Jump, it->second);
                    return;
                }

                EmitNode(link->To.Node);
            }

            void EmitNode(const UUID id)
            {
                m_NodeCode.emplace(id, Here());
                const GraphNode& node = *m_Graph.FindNode(id);
                std::visit([&](const auto& data) { EmitStatement(node, data); }, node.Data);
            }

            void EmitStatement(const GraphNode& node, const CallNode& call)
            {
                std::set<u64> evaluated;
                EmitCall(node, call, evaluated);
                EmitContinuation(node.Id, GraphPin::Then);
            }

            void EmitStatement(const GraphNode& node, const SetVariableNode& set)
            {
                std::set<u64> evaluated;
                const u32 value = ResolveInput(node, RequirePin(node.Id, GraphPin::Value, PinDirection::Input),
                                                    evaluated);

                m_Label = Label(node.Id);
                Emit(GraphOp::Copy, value, m_VariableSlots.at(set.Variable));
                EmitContinuation(node.Id, GraphPin::Then);
            }

            void EmitStatement(const GraphNode& node, const BranchNode&)
            {
                std::set<u64> evaluated;
                const u32 condition = ResolveInput(node, RequirePin(node.Id, GraphPin::Condition, PinDirection::Input), evaluated);

                m_Label = Label(node.Id);
                const u32 jump = Emit(GraphOp::JumpIfFalse, condition);
                EmitContinuation(node.Id, GraphPin::True);

                m_Program.Code[jump].B = Here();
                EmitContinuation(node.Id, GraphPin::False);
            }

            void EmitStatement(const GraphNode& node, const DelayNode&)
            {
                std::set<u64> evaluated;
                const u32 duration = ResolveInput(node, RequirePin(node.Id, GraphPin::Duration, PinDirection::Input), evaluated);

                m_Label = Label(node.Id);
                Emit(GraphOp::Delay, m_Program.DelayCount++, duration);
                EmitContinuation(node.Id, GraphPin::Then);
            }

            void EmitStatement(const GraphNode&, const EventNode&) { Emit(GraphOp::Return); }
            void EmitStatement(const GraphNode&, const GetVariableNode&) { Emit(GraphOp::Return); }
            void EmitStatement(const GraphNode&, const LiteralNode&) { Emit(GraphOp::Return); }

            void EmitCall(const GraphNode& node, const CallNode& call, std::set<u64>& evaluated)
            {
                std::vector<u32> arguments;
                u32 result = NoSlot;

                for (const PinInfo& pin : Signature(node.Id).Pins)
                {
                    if (pin.Type.IsExec) continue;

                    if (pin.Direction == PinDirection::Input)
                        arguments.push_back(ResolveInput(node, pin, evaluated));
                    else
                        result = OutputSlot(node.Id, pin);
                }

                const auto first = static_cast<u32>(m_Program.Arguments.size());
                m_Program.Arguments.insert(m_Program.Arguments.end(), arguments.begin(), arguments.end());

                m_Label = Label(node.Id);
                Emit(GraphOp::Call, ScriptAPI::Get().FindIndex(call.Function), first,
                     static_cast<u32>(arguments.size()), result);
            }

            void EmitPure(const GraphNode& node, std::set<u64>& evaluated)
            {
                if (const auto* call = std::get_if<CallNode>(&node.Data))
                    EmitCall(node, *call, evaluated);
            }

            u32 ResolveInput(const GraphNode& node, const PinInfo& input, std::set<u64>& evaluated)
            {
                if (const GraphLink* link = FindLinkTo(node.Id, input.Name))
                {
                    const NodeSignature& source = Signature(link->From.Node);
                    const PinInfo& output = *FindPin(source, link->From.Pin, PinDirection::Output);

                    if (source.Pure && evaluated.insert(link->From.Node).second)
                        EmitPure(*m_Graph.FindNode(link->From.Node), evaluated);

                    const u32 slot = OutputSlot(link->From.Node, output);
                    if (!NeedsIntToFloat(output.Type, input.Type))
                        return slot;

                    const u32 converted = AddSlot(0.0f);
                    Emit(GraphOp::IntToFloat, slot, converted);
                    return converted;
                }

                if (const auto it = node.Defaults.find(input.Name);
                    it != node.Defaults.end() && IsAssignable(PinType::Of(it->second), input.Type))
                {
                    if (NeedsIntToFloat(PinType::Of(it->second), input.Type))
                        return AddSlot(static_cast<f32>(std::get<i32>(it->second)));
                    return AddSlot(it->second);
                }

                if (input.Type.Value == ScriptFieldType::Entity)
                    return GraphProgram::SelfSlot;

                return AddSlot(DefaultValueFor(input.Type));
            }

            u32 OutputSlot(const UUID node, const PinInfo& pin)
            {
                const GraphNode& graphNode = *m_Graph.FindNode(node);
                if (const auto* get = std::get_if<GetVariableNode>(&graphNode.Data))
                    return m_VariableSlots.at(get->Variable);
                if (const auto* set = std::get_if<SetVariableNode>(&graphNode.Data))
                    return m_VariableSlots.at(set->Variable);

                std::pair<u64, std::string> key{ node, pin.Name };
                if (const auto it = m_OutputSlots.find(key); it != m_OutputSlots.end())
                    return it->second;

                const auto* literal = std::get_if<LiteralNode>(&graphNode.Data);
                const u32 slot = AddSlot(literal != nullptr ? literal->Value : DefaultValueFor(pin.Type));
                m_OutputSlots.emplace(std::move(key), slot);
                return slot;
            }

            const NodeSignature& Signature(const UUID node)
            {
                const auto key = static_cast<u64>(node);
                if (const auto it = m_Signatures.find(key); it != m_Signatures.end())
                    return it->second;

                return m_Signatures.emplace(key, DescribeNode(m_Graph, *m_Graph.FindNode(node))).first->second;
            }

            const PinInfo& RequirePin(const UUID node, const std::string_view name, const PinDirection direction)
            {
                return *FindPin(Signature(node), name, direction);
            }

            const GraphLink* FindLinkFrom(const UUID node, const std::string_view pin) const
            {
                for (const GraphLink& link : m_Graph.GetLinks())
                {
                    if (static_cast<u64>(link.From.Node) == static_cast<u64>(node) && link.From.Pin == pin)
                        return &link;
                }
                return nullptr;
            }

            const GraphLink* FindLinkTo(const UUID node, const std::string_view pin) const
            {
                for (const GraphLink& link : m_Graph.GetLinks())
                {
                    if (static_cast<u64>(link.To.Node) == static_cast<u64>(node) && link.To.Pin == pin)
                        return &link;
                }
                return nullptr;
            }

            u32 Label(const UUID node)
            {
                const auto key = static_cast<u64>(node);
                if (const auto it = m_Labels.find(key); it != m_Labels.end())
                    return it->second;

                const auto index = static_cast<u32>(m_Program.Labels.size());
                m_Program.Labels.push_back(Signature(node).Title);
                m_Labels.emplace(key, index);
                return index;
            }

            u32 Here() const { return static_cast<u32>(m_Program.Code.size()); }

            u32 Emit(const GraphOp op, const u32 a = 0, const u32 b = 0,
                          const u32 c = 0, const u32 d = 0)
            {
                m_Program.Code.push_back({ op, a, b, c, d, m_Label });
                return Here() - 1;
            }

            u32 AddSlot(ScriptValue initial)
            {
                m_Program.InitialSlots.push_back(std::move(initial));
                return static_cast<u32>(m_Program.InitialSlots.size() - 1);
            }

        private:
            const ScriptGraph& m_Graph;
            GraphProgram m_Program;

            std::map<u64, NodeSignature> m_Signatures;
            std::map<u64, u32> m_VariableSlots;
            std::map<std::pair<u64, std::string>, u32> m_OutputSlots;
            std::map<u64, u32> m_NodeCode;
            std::map<u64, u32> m_Labels;
            u32 m_Label = 0;
        };
    }

    std::expected<GraphProgram, std::string> CompileScriptGraph(const ScriptGraph& graph)
    {
        return Compiler(graph).Run();
    }
}

#include "Scripting/Visual/GraphCompiler.h"

#include "Engine/Scripting/API/ScriptAPI.h"
#include "Engine/Scripting/Visual/GraphSchema.h"

#include <algorithm>
#include <format>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

            Result<GraphProgram> Run()
            {
                if (std::optional<std::string> error = FirstError())
                    return Unexpected(std::move(*error));

                AddSlot(EntityRef{});

                for (const GraphVariable& variable : m_Graph.GetVariables())
                {
                    const u32 slot = AddSlot(variable.Default);
                    m_VariableSlots.emplace(variable.Id, slot);

                    if (variable.Scope != UUID(0))
                    {
                        m_LocalResets[static_cast<u64>(variable.Scope)].emplace_back(AddSlot(variable.Default), slot);
                        continue;
                    }

                    m_Program.VariableSlots.emplace(variable.Name, slot);
                    if (variable.Exposed)
                    {
                        m_Program.Fields.push_back({ .Name = variable.Name, .Type = GetFieldType(variable.Default),
                                                     .Default = variable.Default, .Id = variable.Id });
                    }
                }

                for (const GraphCustomEvent& event : m_Graph.GetEvents())
                {
                    for (const GraphParameter& parameter : event.Parameters)
                        m_ParameterSlots.emplace(ParameterKey(event.Id, parameter.Name), AddSlot(parameter.Default));
                }

                for (const GraphFunction& function : m_Graph.GetFunctions())
                {
                    for (const GraphParameter& input : function.Inputs)
                        m_FunctionInputs.emplace(ParameterKey(function.Id, input.Name), AddSlot(input.Default));
                    for (const GraphParameter& output : function.Outputs)
                        m_FunctionOutputs.emplace(ParameterKey(function.Id, output.Name), AddSlot(output.Default));
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

                for (const GraphNode& node : m_Graph.GetNodes())
                {
                    const auto* entry = std::get_if<CustomEventNode>(&node.Data);
                    if (entry == nullptr) continue;

                    m_EventEntries.emplace(entry->Event, Here());
                    m_Label = Label(node.Id);
                    EmitContinuation(node.Id, GraphPin::Then);
                }

                for (const GraphNode& node : m_Graph.GetNodes())
                {
                    const auto* entry = std::get_if<FunctionEntryNode>(&node.Data);
                    if (entry == nullptr) continue;

                    m_FunctionEntries.emplace(entry->Function, Here());
                    m_Label = Label(node.Id);

                    if (const auto resets = m_LocalResets.find(entry->Function); resets != m_LocalResets.end())
                    {
                        for (const auto& [initial, slot] : resets->second)
                            Emit(GraphOp::Copy, initial, slot);
                    }

                    EmitContinuation(node.Id, GraphPin::Then);
                }

                for (const auto& [call, event] : m_EventCalls)
                    m_Program.Code[call].A = m_EventEntries.at(event);
                for (const auto& [call, function] : m_FunctionCalls)
                    m_Program.Code[call].A = m_FunctionEntries.at(function);

                return std::move(m_Program);
            }

        private:
            [[nodiscard]] std::optional<std::string> FirstError() const
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

                Emit(GraphOp::Trace, static_cast<u32>(m_Program.TracedLinks.size()));
                m_Program.TracedLinks.push_back(*link);

                if (const auto it = m_NodeCode.find({ link->To.Node, link->To.Pin }); it != m_NodeCode.end())
                {
                    Emit(GraphOp::Jump, it->second);
                    return;
                }

                EmitNode(link->To.Node, link->To.Pin);
            }

            void EmitNode(const UUID id, const std::string_view entry)
            {
                m_NodeCode.emplace(std::pair<u64, std::string>{ id, std::string(entry) }, Here());
                const GraphNode& node = *m_Graph.FindNode(id);
                std::visit([&](const auto& data) { EmitStatement(node, data, entry); }, node.Data);
            }

            void EmitSubroutine(const u32 gosub, const UUID node, const std::string_view pin)
            {
                m_Program.Code[gosub].A = Here();
                EmitContinuation(node, pin);
            }

            u32 StateSlot(const UUID node, ScriptValue initial)
            {
                if (const auto it = m_StateSlots.find(node); it != m_StateSlots.end())
                    return it->second;

                const u32 slot = AddSlot(std::move(initial));
                m_StateSlots.emplace(node, slot);
                return slot;
            }

            void EmitStatement(const GraphNode& node, const CallNode& call, std::string_view)
            {
                std::set<u64> evaluated;
                EmitCall(node, call, evaluated);
                EmitContinuation(node.Id, GraphPin::Then);
            }

            void EmitStatement(const GraphNode& node, const SetVariableNode& set, std::string_view)
            {
                std::set<u64> evaluated;
                const u32 value = ResolveInput(node, RequirePin(node.Id, GraphPin::Value, PinDirection::Input),
                                               evaluated);

                m_Label = Label(node.Id);
                Emit(GraphOp::Copy, value, m_VariableSlots.at(set.Variable));
                EmitContinuation(node.Id, GraphPin::Then);
            }

            void EmitStatement(const GraphNode& node, const BranchNode&, std::string_view)
            {
                std::set<u64> evaluated;
                const u32 condition = ResolveInput(node, RequirePin(node.Id, GraphPin::Condition, PinDirection::Input),
                                                   evaluated);

                m_Label = Label(node.Id);
                const u32 jump = Emit(GraphOp::JumpIfFalse, condition);
                EmitContinuation(node.Id, GraphPin::True);

                m_Program.Code[jump].B = Here();
                EmitContinuation(node.Id, GraphPin::False);
            }

            void EmitStatement(const GraphNode& node, const DelayNode&, std::string_view)
            {
                std::set<u64> evaluated;
                const u32 duration = ResolveInput(node, RequirePin(node.Id, GraphPin::Duration, PinDirection::Input), evaluated);

                m_Label = Label(node.Id);
                Emit(GraphOp::Delay, m_Program.DelayCount++, duration);
                EmitContinuation(node.Id, GraphPin::Then);
            }

            void EmitStatement(const GraphNode& node, const SequenceNode& sequence, std::string_view)
            {
                m_Label = Label(node.Id);

                std::vector<u32> calls;
                for (u32 i = 0; i + 1 < sequence.Outputs; ++i)
                    calls.push_back(Emit(GraphOp::Gosub));

                EmitContinuation(node.Id, SequencePinName(sequence.Outputs - 1));

                for (u32 i = 0; i < calls.size(); ++i)
                    EmitSubroutine(calls[i], node.Id, SequencePinName(i));
            }

            void EmitStatement(const GraphNode& node, const ForLoopNode&, std::string_view)
            {
                std::set<u64> evaluated;
                const u32 first = ResolveInput(node, RequirePin(node.Id, GraphPin::First, PinDirection::Input), evaluated);
                const u32 last = ResolveInput(node, RequirePin(node.Id, GraphPin::Last, PinDirection::Input), evaluated);

                m_Label = Label(node.Id);
                const u32 counter = OutputSlot(node.Id, RequirePin(node.Id, GraphPin::Index, PinDirection::Output));
                const u32 end = AddSlot(0);

                Emit(GraphOp::Copy, first, counter);
                Emit(GraphOp::Copy, last, end);
                const u32 empty = Emit(GraphOp::JumpIfGreaterInt, counter, end);

                const u32 body = Emit(GraphOp::Gosub);
                const u32 more = Emit(GraphOp::JumpIfGreaterInt, end, counter);
                const u32 done = Emit(GraphOp::Jump);

                m_Program.Code[more].C = Here();
                Emit(GraphOp::IncrementInt, counter);
                Emit(GraphOp::Jump, body);

                m_Program.Code[empty].C = Here();
                m_Program.Code[done].A = Here();
                EmitContinuation(node.Id, GraphPin::Completed);

                EmitSubroutine(body, node.Id, GraphPin::LoopBody);
            }

            void EmitStatement(const GraphNode& node, const DoOnceNode&, const std::string_view entry)
            {
                m_Label = Label(node.Id);
                const u32 done = StateSlot(node.Id, false);

                if (entry == GraphPin::Reset)
                {
                    Emit(GraphOp::Copy, AddSlot(false), done);
                    Emit(GraphOp::Return);
                    return;
                }

                const u32 pass = Emit(GraphOp::JumpIfFalse, done);
                Emit(GraphOp::Return);

                m_Program.Code[pass].B = Here();
                Emit(GraphOp::Copy, AddSlot(true), done);
                EmitContinuation(node.Id, GraphPin::Completed);
            }

            void EmitStatement(const GraphNode& node, const FlipFlopNode&, std::string_view)
            {
                m_Label = Label(node.Id);
                const u32 isA = OutputSlot(node.Id, RequirePin(node.Id, GraphPin::IsA, PinDirection::Output));

                const u32 toA = Emit(GraphOp::JumpIfFalse, isA);
                Emit(GraphOp::Copy, AddSlot(false), isA);
                EmitContinuation(node.Id, GraphPin::B);

                m_Program.Code[toA].B = Here();
                Emit(GraphOp::Copy, AddSlot(true), isA);
                EmitContinuation(node.Id, GraphPin::A);
            }

            void EmitStatement(const GraphNode& node, const GateNode&, const std::string_view entry)
            {
                m_Label = Label(node.Id);
                const u32 isOpen = StateSlot(node.Id, true);

                if (entry == GraphPin::Open || entry == GraphPin::Close)
                {
                    Emit(GraphOp::Copy, AddSlot(entry == GraphPin::Open), isOpen);
                    Emit(GraphOp::Return);
                    return;
                }

                if (entry == GraphPin::Toggle)
                {
                    const u32 opening = Emit(GraphOp::JumpIfFalse, isOpen);
                    Emit(GraphOp::Copy, AddSlot(false), isOpen);
                    Emit(GraphOp::Return);

                    m_Program.Code[opening].B = Here();
                    Emit(GraphOp::Copy, AddSlot(true), isOpen);
                    Emit(GraphOp::Return);
                    return;
                }

                const u32 closed = Emit(GraphOp::JumpIfFalse, isOpen);
                EmitContinuation(node.Id, GraphPin::Exit);

                m_Program.Code[closed].B = Here();
                Emit(GraphOp::Return);
            }

            void EmitStatement(const GraphNode&, const EventNode&, std::string_view) { Emit(GraphOp::Return); }
            void EmitStatement(const GraphNode&, const GetVariableNode&, std::string_view) { Emit(GraphOp::Return); }
            void EmitStatement(const GraphNode&, const LiteralNode&, std::string_view) { Emit(GraphOp::Return); }
            void EmitStatement(const GraphNode&, const CommentNode&, std::string_view) { Emit(GraphOp::Return); }

            void EmitStatement(const GraphNode& node, const RerouteNode&, std::string_view)
            {
                EmitContinuation(node.Id, GraphPin::Out);
            }

            void EmitStatement(const GraphNode&, const SelectNode&, std::string_view) { Emit(GraphOp::Return); }

            void EmitStatement(const GraphNode& node, const CallEventNode& call, std::string_view)
            {
                std::set<u64> evaluated;
                std::vector<std::pair<u32, u32>> arguments;
                for (const GraphParameter& parameter : m_Graph.FindEvent(call.Event)->Parameters)
                {
                    const u32 value = ResolveInput(node, RequirePin(node.Id, parameter.Name, PinDirection::Input),
                                                   evaluated);
                    arguments.emplace_back(value, m_ParameterSlots.at(ParameterKey(call.Event, parameter.Name)));
                }

                m_Label = Label(node.Id);
                for (const auto& [value, slot] : arguments)
                    Emit(GraphOp::Copy, value, slot);

                if (HasEventEntry(m_Graph, call.Event))
                    m_EventCalls.emplace_back(Emit(GraphOp::Gosub), call.Event);

                EmitContinuation(node.Id, GraphPin::Then);
            }

            void EmitStatement(const GraphNode&, const CustomEventNode&, std::string_view) { Emit(GraphOp::Return); }

            void EmitStatement(const GraphNode& node, const CallFunctionNode& call, std::string_view)
            {
                std::set<u64> evaluated;
                EmitFunctionCall(node, call, evaluated);
                EmitContinuation(node.Id, GraphPin::Then);
            }

            void EmitStatement(const GraphNode& node, const FunctionReturnNode& result, std::string_view)
            {
                std::set<u64> evaluated;
                std::vector<std::pair<u32, u32>> values;
                for (const GraphParameter& output : m_Graph.FindFunction(result.Function)->Outputs)
                {
                    const u32 value = ResolveInput(node, RequirePin(node.Id, output.Name, PinDirection::Input),
                                                   evaluated);
                    values.emplace_back(value, m_FunctionOutputs.at(ParameterKey(result.Function, output.Name)));
                }

                m_Label = Label(node.Id);
                for (const auto& [value, slot] : values)
                    Emit(GraphOp::Copy, value, slot);
                Emit(GraphOp::Return);
            }

            void EmitStatement(const GraphNode&, const FunctionEntryNode&, std::string_view) { Emit(GraphOp::Return); }

            void EmitFunctionCall(const GraphNode& node, const CallFunctionNode& call, std::set<u64>& evaluated)
            {
                const GraphFunction& function = *m_Graph.FindFunction(call.Function);

                std::vector<std::pair<u32, u32>> arguments;
                for (const GraphParameter& input : function.Inputs)
                {
                    const u32 value = ResolveInput(node, RequirePin(node.Id, input.Name, PinDirection::Input),
                                                   evaluated);
                    arguments.emplace_back(value, m_FunctionInputs.at(ParameterKey(function.Id, input.Name)));
                }

                m_Label = Label(node.Id);
                for (const auto& [value, slot] : arguments)
                    Emit(GraphOp::Copy, value, slot);

                if (HasFunctionEntry(m_Graph, function.Id))
                    m_FunctionCalls.emplace_back(Emit(GraphOp::Gosub), function.Id);

                for (const GraphParameter& output : function.Outputs)
                {
                    Emit(GraphOp::Copy, m_FunctionOutputs.at(ParameterKey(function.Id, output.Name)),
                         OutputSlot(node.Id, RequirePin(node.Id, output.Name, PinDirection::Output)));
                }
            }

            static std::pair<u64, std::string> ParameterKey(const UUID event, std::string name)
            {
                return { static_cast<u64>(event), std::move(name) };
            }

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
                else if (const auto* function = std::get_if<CallFunctionNode>(&node.Data))
                    EmitFunctionCall(node, *function, evaluated);
                else if (std::holds_alternative<SelectNode>(node.Data))
                    EmitSelect(node, evaluated);
            }

            void EmitSelect(const GraphNode& node, std::set<u64>& evaluated)
            {
                const u32 condition = ResolveInput(node, RequirePin(node.Id, GraphPin::Condition,
                                                   PinDirection::Input), evaluated);
                const u32 ifTrue = ResolveInput(node, RequirePin(node.Id, GraphPin::IfTrue,
                                                PinDirection::Input), evaluated);
                const u32 ifFalse = ResolveInput(node, RequirePin(node.Id, GraphPin::IfFalse,
                                                 PinDirection::Input), evaluated);
                const u32 result = OutputSlot(node.Id, RequirePin(node.Id, GraphPin::Value, PinDirection::Output));

                m_Label = Label(node.Id);
                const u32 pickFalse = Emit(GraphOp::JumpIfFalse, condition);
                Emit(GraphOp::Copy, ifTrue, result);
                const u32 done = Emit(GraphOp::Jump);

                m_Program.Code[pickFalse].B = Here();
                Emit(GraphOp::Copy, ifFalse, result);
                m_Program.Code[done].A = Here();
            }

            u32 ResolveInput(const GraphNode& node, const PinInfo& input, std::set<u64>& evaluated)
            {
                if (const GraphLink* link = FindLinkTo(node.Id, input.Name))
                {
                    const GraphNode& sourceNode = *m_Graph.FindNode(link->From.Node);
                    const NodeSignature& source = Signature(link->From.Node);
                    const PinInfo& output = *FindPin(source, link->From.Pin, PinDirection::Output);

                    u32 slot = 0;
                    if (std::holds_alternative<RerouteNode>(sourceNode.Data))
                    {
                        slot = ResolveInput(sourceNode, RequirePin(sourceNode.Id, GraphPin::In, PinDirection::Input),
                                            evaluated);
                    } else
                    {
                        if (source.Pure && evaluated.insert(link->From.Node).second)
                            EmitPure(sourceNode, evaluated);
                        slot = OutputSlot(link->From.Node, output);
                    }

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
                if (const auto* entry = std::get_if<CustomEventNode>(&graphNode.Data))
                    return m_ParameterSlots.at(ParameterKey(entry->Event, pin.Name));
                if (const auto* entry = std::get_if<FunctionEntryNode>(&graphNode.Data))
                    return m_FunctionInputs.at(ParameterKey(entry->Function, pin.Name));

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
                if (const auto it = m_Signatures.find(node); it != m_Signatures.end())
                    return it->second;

                return m_Signatures.emplace(node, DescribeNode(m_Graph, *m_Graph.FindNode(node))).first->second;
            }

            const PinInfo& RequirePin(const UUID node, const std::string_view name, const PinDirection direction)
            {
                return *FindPin(Signature(node), name, direction);
            }

            [[nodiscard]] const GraphLink* FindLinkFrom(const UUID node, const std::string_view pin) const
            {
                for (const GraphLink& link : m_Graph.GetLinks())
                {
                    if (link.From.Node == node && link.From.Pin == pin)
                        return &link;
                }
                return nullptr;
            }

            [[nodiscard]] const GraphLink* FindLinkTo(const UUID node, const std::string_view pin) const
            {
                for (const GraphLink& link : m_Graph.GetLinks())
                {
                    if (link.To.Node == node && link.To.Pin == pin)
                        return &link;
                }
                return nullptr;
            }

            u32 Label(const UUID node)
            {
                if (const auto it = m_Labels.find(node); it != m_Labels.end())
                    return it->second;

                const auto index = static_cast<u32>(m_Program.Labels.size());
                m_Program.Labels.push_back({ Signature(node).Title, node });
                m_Labels.emplace(node, index);
                return index;
            }

            [[nodiscard]] u32 Here() const { return static_cast<u32>(m_Program.Code.size()); }

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

            std::map<UUID, NodeSignature> m_Signatures;
            std::map<u64, u32> m_VariableSlots;
            std::map<std::pair<u64, std::string>, u32> m_OutputSlots;
            std::map<std::pair<u64, std::string>, u32, std::less<>> m_NodeCode;
            std::map<u64, u32> m_StateSlots;
            std::map<UUID, u32> m_Labels;
            std::map<std::pair<u64, std::string>, u32> m_ParameterSlots;
            std::map<u64, u32> m_EventEntries;
            std::vector<std::pair<u32, u64>> m_EventCalls;
            std::map<u64, std::vector<std::pair<u32, u32>>> m_LocalResets;
            std::map<std::pair<u64, std::string>, u32> m_FunctionInputs;
            std::map<std::pair<u64, std::string>, u32> m_FunctionOutputs;
            std::map<u64, u32> m_FunctionEntries;
            std::vector<std::pair<u32, u64>> m_FunctionCalls;
            u32 m_Label = 0;
        };
    }

    Result<GraphProgram> CompileScriptGraph(const ScriptGraph& graph)
    {
        return Compiler(graph).Run();
    }
}

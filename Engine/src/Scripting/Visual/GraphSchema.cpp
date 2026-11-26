#include "Engine/Scripting/Visual/GraphSchema.h"
#include "Engine/Scripting/API/ScriptAPI.h"

#include <format>
#include <map>
#include <set>
#include <span>
#include <utility>

namespace ByteForge
{
    namespace
    {
        PinInfo In(const std::string_view name, const PinType type)
        {
            return { std::string(name), PinDirection::Input, type };
        }

        PinInfo Out(const std::string_view name, const PinType type)
        {
            return { std::string(name), PinDirection::Output, type };
        }

        void AddExecPins(NodeSignature& signature)
        {
            signature.Pins.push_back(In(GraphPin::Exec, PinType::Exec()));
            signature.Pins.push_back(Out(GraphPin::Then, PinType::Exec()));
        }

        PinType ParameterType(const ScriptParameter& parameter)
        {
            if (parameter.Enum != nullptr)
                return PinType::OfEnum(parameter.Enum->Name);

            return PinType::Of(parameter.Type, parameter.Asset);
        }

        bool IsValidEnumName(const PinType& type, const ScriptValue& value)
        {
            if (type.Enum.empty())
                return true;

            const ScriptEnum* scriptEnum = ScriptAPI::Get().FindEnum(type.Enum);
            const auto* number = std::get_if<i32>(&value);
            return scriptEnum == nullptr || (number != nullptr && scriptEnum->Find(*number) != nullptr);
        }

        std::optional<std::string> MissingEnum(const std::string_view name)
        {
            if (name.empty() || ScriptAPI::Get().FindEnum(name) != nullptr)
                return std::nullopt;
            return std::format("The enum '{}' no longer exists", name);
        }

        std::optional<std::string> MissingEnum(const std::span<const GraphParameter> parameters)
        {
            for (const GraphParameter& parameter : parameters)
            {
                if (std::optional<std::string> missing = MissingEnum(parameter.Enum))
                    return missing;
            }
            return std::nullopt;
        }

        const char* EventTitle(const GraphEvent event)
        {
            switch (event)
            {
                case GraphEvent::OnCreate:         return "On Create";
                case GraphEvent::OnUpdate:         return "On Update";
                case GraphEvent::OnDestroy:        return "On Destroy";
                case GraphEvent::OnSensorEnter:    return "On Sensor Enter";
                case GraphEvent::OnSensorExit:     return "On Sensor Exit";
                case GraphEvent::OnCollisionEnter: return "On Collision Enter";
                case GraphEvent::OnCollisionExit:  return "On Collision Exit";
                case GraphEvent::OnClick:          return "On Click";
            }
            return "Unknown Event";
        }

        NodeSignature Describe(const ScriptGraph&, const EventNode& node)
        {
            NodeSignature signature{ .Title = EventTitle(node.Event), .Category = "Events" };
            signature.Pins.push_back(Out(GraphPin::Then, PinType::Exec()));

            switch (node.Event)
            {
                case GraphEvent::OnUpdate:
                    signature.Pins.push_back(Out("DeltaTime", PinType::Of(ScriptFieldType::Float)));
                    break;
                case GraphEvent::OnSensorEnter:
                case GraphEvent::OnSensorExit:
                case GraphEvent::OnCollisionEnter:
                case GraphEvent::OnCollisionExit:
                    signature.Pins.push_back(Out("Other", PinType::Of(ScriptFieldType::Entity)));
                    break;
                case GraphEvent::OnCreate:
                case GraphEvent::OnDestroy:
                case GraphEvent::OnClick:
                    break;
            }
            return signature;
        }

        NodeSignature Describe(const ScriptGraph&, const CallNode& node)
        {
            const ScriptFunction* function = ScriptAPI::Get().Find(node.Function);
            if (function == nullptr)
            {
                return { .Title = node.Function, .Category = "Unknown",
                         .Error = std::format("Function '{}' does not exist", node.Function) };
            }

            NodeSignature signature{ .Title = function->DisplayName, .Category = function->Category, .Pure = function->Pure };
            if (!function->Pure)
                AddExecPins(signature);

            for (const ScriptParameter& parameter : function->Parameters)
                signature.Pins.push_back(In(parameter.Name, ParameterType(parameter)));

            if (function->Result)
                signature.Pins.push_back(Out(function->Result->Name, ParameterType(*function->Result)));

            return signature;
        }

        NodeSignature Describe(const ScriptGraph&, const BranchNode&)
        {
            NodeSignature signature{ .Title = "Branch", .Category = "Flow" };
            signature.Pins.push_back(In(GraphPin::Exec, PinType::Exec()));
            signature.Pins.push_back(In(GraphPin::Condition, PinType::Of(ScriptFieldType::Bool)));
            signature.Pins.push_back(Out(GraphPin::True, PinType::Exec()));
            signature.Pins.push_back(Out(GraphPin::False, PinType::Exec()));
            return signature;
        }

        NodeSignature Describe(const ScriptGraph&, const DelayNode&)
        {
            NodeSignature signature{ .Title = "Delay", .Category = "Flow", .Latent = true };
            AddExecPins(signature);
            signature.Pins.push_back(In(GraphPin::Duration, PinType::Of(ScriptFieldType::Float)));
            return signature;
        }

        NodeSignature Describe(const ScriptGraph&, const SequenceNode& node)
        {
            NodeSignature signature{ .Title = "Sequence", .Category = "Flow" };
            signature.Pins.push_back(In(GraphPin::Exec, PinType::Exec()));
            for (u32 i = 0; i < node.Outputs; ++i)
                signature.Pins.push_back(Out(SequencePinName(i), PinType::Exec()));
            return signature;
        }

        NodeSignature Describe(const ScriptGraph&, const ForLoopNode&)
        {
            NodeSignature signature{ .Title = "For Loop", .Category = "Flow" };
            signature.Pins.push_back(In(GraphPin::Exec, PinType::Exec()));
            signature.Pins.push_back(In(GraphPin::First, PinType::Of(ScriptFieldType::Int)));
            signature.Pins.push_back(In(GraphPin::Last, PinType::Of(ScriptFieldType::Int)));
            signature.Pins.push_back(Out(GraphPin::LoopBody, PinType::Exec()));
            signature.Pins.push_back(Out(GraphPin::Index, PinType::Of(ScriptFieldType::Int)));
            signature.Pins.push_back(Out(GraphPin::Completed, PinType::Exec()));
            return signature;
        }

        NodeSignature Describe(const ScriptGraph&, const DoOnceNode&)
        {
            NodeSignature signature{ .Title = "Do Once", .Category = "Flow" };
            signature.Pins.push_back(In(GraphPin::Exec, PinType::Exec()));
            signature.Pins.push_back(In(GraphPin::Reset, PinType::Exec()));
            signature.Pins.push_back(Out(GraphPin::Completed, PinType::Exec()));
            return signature;
        }

        NodeSignature Describe(const ScriptGraph&, const FlipFlopNode&)
        {
            NodeSignature signature{ .Title = "Flip Flop", .Category = "Flow" };
            signature.Pins.push_back(In(GraphPin::Exec, PinType::Exec()));
            signature.Pins.push_back(Out(GraphPin::A, PinType::Exec()));
            signature.Pins.push_back(Out(GraphPin::B, PinType::Exec()));
            signature.Pins.push_back(Out(GraphPin::IsA, PinType::Of(ScriptFieldType::Bool)));
            return signature;
        }

        NodeSignature Describe(const ScriptGraph&, const GateNode&)
        {
            NodeSignature signature{ .Title = "Gate", .Category = "Flow" };
            signature.Pins.push_back(In(GraphPin::Enter, PinType::Exec()));
            signature.Pins.push_back(In(GraphPin::Open, PinType::Exec()));
            signature.Pins.push_back(In(GraphPin::Close, PinType::Exec()));
            signature.Pins.push_back(In(GraphPin::Toggle, PinType::Exec()));
            signature.Pins.push_back(Out(GraphPin::Exit, PinType::Exec()));
            return signature;
        }

        NodeSignature Describe(const ScriptGraph&, const CommentNode& node)
        {
            return { .Title = node.Text, .Category = "Comments" };
        }

        NodeSignature Describe(const ScriptGraph&, const RerouteNode& node)
        {
            const PinType type = node.Type.value_or(PinType::Any());

            NodeSignature signature{ .Title = "Reroute", .Category = "Flow", .Pure = !type.IsExec };
            signature.Pins.push_back(In(GraphPin::In, type));
            signature.Pins.push_back(Out(GraphPin::Out, type));
            return signature;
        }

        NodeSignature Describe(const ScriptGraph&, const SelectNode& node)
        {
            const PinType type = node.Type.value_or(PinType::AnyData());

            NodeSignature signature{ .Title = "Select", .Category = "Flow", .Pure = true };
            signature.Pins.push_back(In(GraphPin::Condition, PinType::Of(ScriptFieldType::Bool)));
            signature.Pins.push_back(In(GraphPin::IfTrue, type));
            signature.Pins.push_back(In(GraphPin::IfFalse, type));
            signature.Pins.push_back(Out(GraphPin::Value, type));
            return signature;
        }

        NodeSignature Describe(const ScriptGraph& graph, const CustomEventNode& node)
        {
            const GraphCustomEvent* event = graph.FindEvent(node.Event);
            if (event == nullptr)
                return { .Title = "Event ?", .Category = "Events", .Error = "The event no longer exists" };

            NodeSignature signature{ .Title = event->Name, .Category = "Events",
                                     .Error = MissingEnum(event->Parameters) };
            signature.Pins.push_back(Out(GraphPin::Then, PinType::Exec()));
            for (const GraphParameter& parameter : event->Parameters)
                signature.Pins.push_back(Out(parameter.Name, TypeOf(parameter)));
            return signature;
        }

        NodeSignature Describe(const ScriptGraph& graph, const CallEventNode& node)
        {
            const GraphCustomEvent* event = graph.FindEvent(node.Event);
            if (event == nullptr)
                return { .Title = "Call ?", .Category = "Custom Events", .Error = "The event no longer exists" };

            NodeSignature signature{ .Title = std::format("Call {}", event->Name), .Category = "Custom Events",
                                     .Error = MissingEnum(event->Parameters) };
            signature.Pins.push_back(In(GraphPin::Exec, PinType::Exec()));
            for (const GraphParameter& parameter : event->Parameters)
                signature.Pins.push_back(In(parameter.Name, TypeOf(parameter)));
            signature.Pins.push_back(Out(GraphPin::Then, PinType::Exec()));
            return signature;
        }

        NodeSignature Describe(const ScriptGraph& graph, const GetVariableNode& node)
        {
            const GraphVariable* variable = graph.FindVariable(node.Variable);
            if (variable == nullptr)
                return { .Title = "Get ?", .Category = "Variables", .Error = "The variable no longer exists" };

            NodeSignature signature{ .Title = variable->Name, .Category = "Variables", .Pure = true,
                                     .Error = MissingEnum(variable->Enum) };
            signature.Pins.push_back(Out(GraphPin::Value, TypeOf(*variable)));
            return signature;
        }

        NodeSignature Describe(const ScriptGraph& graph, const SetVariableNode& node)
        {
            const GraphVariable* variable = graph.FindVariable(node.Variable);
            if (variable == nullptr)
                return { .Title = "Set ?", .Category = "Variables", .Error = "The variable no longer exists" };

            const PinType type = TypeOf(*variable);

            NodeSignature signature{ .Title = std::format("Set {}", variable->Name), .Category = "Variables",
                                     .Error = MissingEnum(variable->Enum) };
            AddExecPins(signature);
            signature.Pins.push_back(In(GraphPin::Value, type));
            signature.Pins.push_back(Out(GraphPin::Value, type));
            return signature;
        }

        NodeSignature Describe(const ScriptGraph&, const LiteralNode& node)
        {
            const PinType type = TypeOf(node.Value, node.Enum);
            std::string title = !type.Enum.empty()                   ? std::string(type.Enum)
                              : type.Value == ScriptFieldType::Asset ? AssetTypeToString(type.Asset)
                                                                     : std::string(ScriptFieldTypeName(type.Value));

            NodeSignature signature{ .Title = std::move(title), .Category = "Literals", .Pure = true,
                                     .Error = MissingEnum(node.Enum) };
            signature.Pins.push_back(Out(GraphPin::Value, type));
            return signature;
        }

        NodeSignature Describe(const ScriptGraph& graph, const FunctionEntryNode& node)
        {
            const GraphFunction* function = graph.FindFunction(node.Function);
            if (function == nullptr)
                return { .Title = "Entry ?", .Category = "Functions", .Error = "The function no longer exists" };

            NodeSignature signature{ .Title = function->Name, .Category = "Functions",
                                     .Error = MissingEnum(function->Inputs) };
            signature.Pins.push_back(Out(GraphPin::Then, PinType::Exec()));
            for (const GraphParameter& input : function->Inputs)
                signature.Pins.push_back(Out(input.Name, TypeOf(input)));
            return signature;
        }

        NodeSignature Describe(const ScriptGraph& graph, const FunctionReturnNode& node)
        {
            const GraphFunction* function = graph.FindFunction(node.Function);
            if (function == nullptr)
                return { .Title = "Return", .Category = "Functions", .Error = "The function no longer exists" };

            NodeSignature signature{ .Title = "Return", .Category = "Functions",
                                     .Error = MissingEnum(function->Outputs) };
            signature.Pins.push_back(In(GraphPin::Exec, PinType::Exec()));
            for (const GraphParameter& output : function->Outputs)
                signature.Pins.push_back(In(output.Name, TypeOf(output)));
            return signature;
        }

        NodeSignature Describe(const ScriptGraph& graph, const CallFunctionNode& node)
        {
            const GraphFunction* function = graph.FindFunction(node.Function);
            if (function == nullptr)
                return { .Title = "Call ?", .Category = "Functions", .Error = "The function no longer exists" };

            std::optional<std::string> missing = MissingEnum(function->Inputs);
            if (!missing)
                missing = MissingEnum(function->Outputs);

            NodeSignature signature{ .Title = function->Name, .Category = "Functions", .Pure = function->Pure,
                                     .Error = std::move(missing) };
            if (!function->Pure)
                AddExecPins(signature);
            for (const GraphParameter& input : function->Inputs)
                signature.Pins.push_back(In(input.Name, TypeOf(input)));
            for (const GraphParameter& output : function->Outputs)
                signature.Pins.push_back(Out(output.Name, TypeOf(output)));
            return signature;
        }

        bool CallsBack(const ScriptGraph& graph, const UUID caller, const UUID callee)
        {
            std::vector<u64> pending{ callee };
            std::set<u64> visited;

            while (!pending.empty())
            {
                const u64 function = pending.back();
                pending.pop_back();

                if (function == static_cast<u64>(caller))
                    return true;
                if (!visited.insert(function).second) continue;

                for (const GraphNode& node : graph.GetNodes())
                {
                    const auto* call = std::get_if<CallFunctionNode>(&node.Data);
                    if (call != nullptr && node.Scope == UUID(function))
                        pending.push_back(call->Function);
                }
            }
            return false;
        }

        std::optional<std::string> CheckPlacement(const ScriptGraph& graph, const GraphNode& node)
        {
            const bool inFunction = node.Scope != UUID(0);

            if (inFunction && (std::holds_alternative<EventNode>(node.Data) ||
                               std::holds_alternative<CustomEventNode>(node.Data)))
                return "Events cannot be placed inside a function";

            if (inFunction && std::holds_alternative<DelayNode>(node.Data))
                return "A function cannot wait; move the Delay into a custom event";

            if (const auto* entry = std::get_if<FunctionEntryNode>(&node.Data);
                entry != nullptr && entry->Function != node.Scope)
                return "This entry belongs to another function";
            if (const auto* result = std::get_if<FunctionReturnNode>(&node.Data);
                result != nullptr && result->Function != node.Scope)
                return "This return belongs to another function";

            UUID variable{ 0 };
            if (const auto* get = std::get_if<GetVariableNode>(&node.Data))
                variable = get->Variable;
            else if (const auto* set = std::get_if<SetVariableNode>(&node.Data))
                variable = set->Variable;

            if (const GraphVariable* found = graph.FindVariable(variable);
                found != nullptr && !IsVisibleIn(*found, node.Scope))
                return "The variable is local to another function";

            if (const auto* call = std::get_if<CallFunctionNode>(&node.Data);
                call != nullptr && inFunction && CallsBack(graph, node.Scope, call->Function))
                return "Functions cannot call themselves, directly or through other functions";

            return std::nullopt;
        }

        class SignatureCache
        {
        public:
            explicit SignatureCache(const ScriptGraph& graph)
                : m_Graph(graph) {}

            const NodeSignature* Find(const UUID id)
            {
                if (const auto it = m_Signatures.find(id); it != m_Signatures.end())
                    return &it->second;

                const GraphNode* node = m_Graph.FindNode(id);
                if (node == nullptr)
                    return nullptr;

                return &m_Signatures.emplace(id, DescribeNode(m_Graph, *node)).first->second;
            }

        private:
            const ScriptGraph& m_Graph;
            std::unordered_map<UUID, NodeSignature> m_Signatures;
        };

        using DataEdges = std::map<u64, std::vector<u64>>;

        DataEdges CollectDataEdges(const ScriptGraph& graph, SignatureCache& cache)
        {
            DataEdges edges;
            for (const GraphLink& link : graph.GetLinks())
            {
                const NodeSignature* source = cache.Find(link.From.Node);
                const PinInfo* output = source != nullptr ? FindPin(*source, link.From.Pin, PinDirection::Output) : nullptr;

                if (output != nullptr && !output->Type.IsExec)
                    edges[link.From.Node].push_back(link.To.Node);
            }
            return edges;
        }

        bool Reaches(const DataEdges& edges, const u64 start, const u64 target)
        {
            std::vector<u64> pending{ start };
            std::set<u64> visited;

            while (!pending.empty())
            {
                const u64 node = pending.back();
                pending.pop_back();

                if (node == target)
                    return true;
                if (!visited.insert(node).second)
                    continue;

                if (const auto it = edges.find(node); it != edges.end())
                    pending.insert(pending.end(), it->second.begin(), it->second.end());
            }
            return false;
        }

        std::optional<UUID> FindDataLoop(const DataEdges& edges)
        {
            enum class Mark : u8 { Visiting, Done };
            std::map<u64, Mark> marks;

            const auto visit = [&](const auto& self, const u64 node) -> std::optional<UUID>
            {
                if (const auto it = marks.find(node); it != marks.end())
                    return it->second == Mark::Visiting ? std::optional(UUID(node)) : std::nullopt;

                marks.emplace(node, Mark::Visiting);
                if (const auto it = edges.find(node); it != edges.end())
                {
                    for (const u64 next : it->second)
                    {
                        if (std::optional<UUID> loop = self(self, next))
                            return loop;
                    }
                }

                marks[node] = Mark::Done;
                return std::nullopt;
            };

            for (const auto& [node, consumers] : edges)
            {
                if (std::optional<UUID> loop = visit(visit, node))
                    return loop;
            }
            return std::nullopt;
        }
    }

    NodeSignature DescribeNode(const ScriptGraph& graph, const GraphNode& node)
    {
        return std::visit([&](const auto& data) { return Describe(graph, data); }, node.Data);
    }

    const PinInfo* FindPin(const NodeSignature& signature, const std::string_view name, const PinDirection direction)
    {
        for (const PinInfo& pin : signature.Pins)
        {
            if (pin.Direction == direction && pin.Name == name)
                return &pin;
        }
        return nullptr;
    }

    std::string DescribePinType(const PinType& type)
    {
        if (type.IsWildcard())
            return "Wildcard";

        if (type.IsExec)
            return "Exec";

        if (!type.Enum.empty())
            return std::string(type.Enum);

        std::string name(ScriptFieldTypeName(type.Value));
        if (type.Value == ScriptFieldType::Asset)
            name += ':' + std::string(AssetTypeToString(type.Asset));
        return name;
    }

    bool IsAssignable(const PinType& from, const PinType& to)
    {
        if (from.IsWildcard() || to.IsWildcard())
            return false;

        if (from.IsExec || to.IsExec)
            return from.IsExec && to.IsExec;

        if (from == to)
            return true;

        if (from.Value == ScriptFieldType::Int && to.Value == ScriptFieldType::Int)
            return from.Enum.empty() || to.Enum.empty();

        return from.Value == ScriptFieldType::Int && from.Enum.empty() && to.Value == ScriptFieldType::Float;
    }

    bool CanLink(const PinType& from, const PinType& to)
    {
        const auto takes = [](const PinType& wildcard, const PinType& type)
        {
            return !type.IsWildcard() && (!type.IsExec || wildcard.Wildcard == WildcardKind::Any);
        };

        if (to.IsWildcard())
            return takes(to, from);
        if (from.IsWildcard())
            return takes(from, to);
        return IsAssignable(from, to);
    }

    Result<PinType> CanConnect(const ScriptGraph& graph, const PinRef& from, const PinRef& to)
    {
        if (from.Node == to.Node)
            return Unexpected("A node cannot connect to itself");

        const GraphNode* sourceNode = graph.FindNode(from.Node);
        const GraphNode* targetNode = graph.FindNode(to.Node);
        if (sourceNode == nullptr || targetNode == nullptr)
            return Unexpected("The node does not exist");
        if (sourceNode->Scope != targetNode->Scope)
            return Unexpected("The nodes are in different functions");

        SignatureCache cache(graph);
        const NodeSignature* source = cache.Find(from.Node);
        const NodeSignature* target = cache.Find(to.Node);

        const PinInfo* output = FindPin(*source, from.Pin, PinDirection::Output);
        const PinInfo* input = FindPin(*target, to.Pin, PinDirection::Input);
        if (output == nullptr || input == nullptr)
            return Unexpected("The pin does not exist");

        if (!CanLink(output->Type, input->Type))
        {
            if (output->Type.IsWildcard() && input->Type.IsWildcard())
                return Unexpected("Connect one of these pins to a typed pin first");
            return Unexpected("Cannot connect {} to {}", DescribePinType(output->Type), DescribePinType(input->Type));
        }

        const PinType type = output->Type.IsWildcard() ? input->Type : output->Type;
        if (!type.IsExec && Reaches(CollectDataEdges(graph, cache), to.Node, from.Node))
            return Unexpected("This connection would create a loop");

        return type;
    }

    std::vector<GraphDiagnostic> ValidateGraph(const ScriptGraph& graph)
    {
        std::vector<GraphDiagnostic> diagnostics;
        const auto report = [&](const UUID node, const GraphSeverity severity, std::string message)
        {
            diagnostics.push_back({ node, severity, std::move(message) });
        };

        std::set<std::string, std::less<>> variableNames;
        for (const GraphVariable& variable : graph.GetVariables())
        {
            if (variable.Name.empty())
                report(UUID(0), GraphSeverity::Error, "A variable has no name");
            else if (variable.Scope == UUID(0) && !variableNames.insert(variable.Name).second)
                report(UUID(0), GraphSeverity::Error, std::format("Variable '{}' is declared twice", variable.Name));
        }

        std::set<std::string, std::less<>> eventNames;
        for (const GraphCustomEvent& event : graph.GetEvents())
        {
            if (event.Name.empty())
                report(UUID(0), GraphSeverity::Error, "An event has no name");
            else if (!eventNames.insert(event.Name).second)
                report(UUID(0), GraphSeverity::Error, std::format("Event '{}' is declared twice", event.Name));
        }

        std::set<std::string, std::less<>> functionNames;
        for (const GraphFunction& function : graph.GetFunctions())
        {
            if (function.Name.empty())
                report(UUID(0), GraphSeverity::Error, "A function has no name");
            else if (!functionNames.insert(function.Name).second)
                report(UUID(0), GraphSeverity::Error, std::format("Function '{}' is declared twice", function.Name));
        }

        SignatureCache cache(graph);
        std::set<u64> nodeIds;
        std::set<GraphEvent> events;
        std::set<u64> customEntries;
        std::set<u64> functionEntries;

        for (const GraphNode& node : graph.GetNodes())
        {
            if (!nodeIds.insert(node.Id).second)
            {
                report(node.Id, GraphSeverity::Error, "Two nodes share the same id");
                continue;
            }

            if (node.Scope != UUID(0) && graph.FindFunction(node.Scope) == nullptr)
            {
                report(node.Id, GraphSeverity::Error, "The node belongs to a function that no longer exists");
                continue;
            }

            const NodeSignature& signature = *cache.Find(node.Id);
            if (signature.Error)
            {
                report(node.Id, GraphSeverity::Error, *signature.Error);
                continue;
            }

            if (std::optional<std::string> misplaced = CheckPlacement(graph, node))
                report(node.Id, GraphSeverity::Error, std::move(*misplaced));

            if (const auto* event = std::get_if<EventNode>(&node.Data);
                event != nullptr && !events.insert(event->Event).second)
                report(node.Id, GraphSeverity::Error, std::format("'{}' exists more than once", signature.Title));

            if (const auto* entry = std::get_if<CustomEventNode>(&node.Data);
                entry != nullptr && !customEntries.insert(entry->Event).second)
                report(node.Id, GraphSeverity::Error, std::format("'{}' exists more than once", signature.Title));

            if (const auto* entry = std::get_if<FunctionEntryNode>(&node.Data);
                entry != nullptr && !functionEntries.insert(entry->Function).second)
                report(node.Id, GraphSeverity::Error, std::format("'{}' has more than one entry node", signature.Title));

            if (const auto* call = std::get_if<CallEventNode>(&node.Data);
                call != nullptr && !HasEventEntry(graph, call->Event))
            {
                report(node.Id, GraphSeverity::Warning,
                       std::format("'{}' has no event node, so the call does nothing",
                                   graph.FindEvent(call->Event)->Name));
            }

            for (const auto& [pin, value] : node.Defaults)
            {
                const PinInfo* input = FindPin(signature, pin, PinDirection::Input);
                if (input == nullptr || input->Type.IsExec)
                {
                    report(node.Id, GraphSeverity::Warning,
                           std::format("Stored value for unknown input '{}' is ignored", pin));
                } else if (!IsAssignable(PinType::Of(value), input->Type))
                {
                    report(node.Id, GraphSeverity::Warning,
                           std::format("Stored value for '{}' has the wrong type and is ignored", pin));
                } else if (!IsValidEnumName(input->Type, value))
                {
                    report(node.Id, GraphSeverity::Warning,
                           std::format("Stored value for '{}' is not a valid {}", pin, input->Type.Enum));
                }
            }
        }

        for (const GraphFunction& function : graph.GetFunctions())
        {
            if (!functionEntries.contains(function.Id))
                report(UUID(0), GraphSeverity::Warning,
                       std::format("Function '{}' has no entry node, so calling it does nothing", function.Name));
        }

        using PinKey = std::pair<u64, std::string>;
        std::map<PinKey, i32> dataInputLinks;
        std::map<PinKey, i32> execOutputLinks;

        for (const GraphLink& link : graph.GetLinks())
        {
            const NodeSignature* source = cache.Find(link.From.Node);
            const NodeSignature* target = cache.Find(link.To.Node);

            if (source == nullptr || target == nullptr)
            {
                report(target != nullptr ? link.To.Node : link.From.Node, GraphSeverity::Error,
                       "Link to a missing node");
                continue;
            }

            if (graph.FindNode(link.From.Node)->Scope != graph.FindNode(link.To.Node)->Scope)
            {
                report(link.To.Node, GraphSeverity::Error, "Link between nodes of different functions");
                continue;
            }

            if (source->Error || target->Error) continue;

            const PinInfo* output = FindPin(*source, link.From.Pin, PinDirection::Output);
            const PinInfo* input = FindPin(*target, link.To.Pin, PinDirection::Input);
            if (output == nullptr || input == nullptr)
            {
                report(link.To.Node, GraphSeverity::Error,
                       std::format("Broken link from '{}.{}' to '{}.{}'",
                                   source->Title, link.From.Pin, target->Title, link.To.Pin));
                continue;
            }

            if (!IsAssignable(output->Type, input->Type))
            {
                report(link.To.Node, GraphSeverity::Error,
                       std::format("'{}' cannot receive {}", input->Name, DescribePinType(output->Type)));
            }

            if (output->Type.IsExec)
            {
                if (++execOutputLinks[{ link.From.Node, link.From.Pin }] == 2)
                {
                    report(link.From.Node, GraphSeverity::Error,
                           std::format("'{}' has more than one connection", output->Name));
                }
            } else if (++dataInputLinks[{ link.To.Node, link.To.Pin }] == 2)
            {
                report(link.To.Node, GraphSeverity::Error,
                       std::format("'{}' has more than one connection", input->Name));
            }
        }

        if (const std::optional<UUID> loop = FindDataLoop(CollectDataEdges(graph, cache)))
            report(*loop, GraphSeverity::Error, "This node is part of a loop of data connections");

        return diagnostics;
    }

    ScriptValue DefaultValueFor(const PinType& type)
    {
        if (!type.Enum.empty())
        {
            const ScriptEnum* scriptEnum = ScriptAPI::Get().FindEnum(type.Enum);
            if (scriptEnum != nullptr && !scriptEnum->Values.empty())
                return scriptEnum->Values.front().Value;
        }

        switch (type.Value)
        {
            case ScriptFieldType::Bool:    return false;
            case ScriptFieldType::Int:     return 0;
            case ScriptFieldType::Float:   return 0.0f;
            case ScriptFieldType::Double:  return 0.0;
            case ScriptFieldType::Vector2: return glm::vec2(0.0f);
            case ScriptFieldType::Vector3: return glm::vec3(0.0f);
            case ScriptFieldType::Vector4: return glm::vec4(0.0f);
            case ScriptFieldType::Entity:  return EntityRef{};
            case ScriptFieldType::Asset:   return AssetRef{ type.Asset, UUID(0) };
            case ScriptFieldType::String:  return std::string();
        }
        return 0.0f;
    }

    PinType TypeOf(const ScriptValue& value, const std::string_view enumName)
    {
        if (!enumName.empty() && std::holds_alternative<i32>(value))
        {
            if (const ScriptEnum* scriptEnum = ScriptAPI::Get().FindEnum(enumName))
                return PinType::OfEnum(scriptEnum->Name);
        }
        return PinType::Of(value);
    }

    std::vector<PinType> GetLiteralTypes()
    {
        std::vector<PinType> types;
        for (const ScriptFieldType type : { ScriptFieldType::Bool, ScriptFieldType::Int, ScriptFieldType::Float,
                                            ScriptFieldType::Vector2, ScriptFieldType::Vector3,
                                            ScriptFieldType::Vector4, ScriptFieldType::String })
            types.push_back(PinType::Of(type));

        std::set<AssetType> assets;
        for (const ScriptFunction& function : ScriptAPI::Get().GetFunctions())
        {
            for (const ScriptParameter& parameter : function.Parameters)
            {
                if (parameter.Type == ScriptFieldType::Asset)
                    assets.insert(parameter.Asset);
            }
        }

        for (const AssetType asset : assets)
            types.push_back(PinType::Of(ScriptFieldType::Asset, asset));

        for (const ScriptEnum* scriptEnum : ScriptAPI::Get().GetEnums())
            types.push_back(PinType::OfEnum(scriptEnum->Name));

        return types;
    }

    std::vector<PinType> GetVariableTypes()
    {
        std::vector<PinType> types = GetLiteralTypes();
        types.push_back(PinType::Of(ScriptFieldType::Entity));
        return types;
    }

    std::string GetValueTypeName(const PinType& type)
    {
        if (type.IsWildcard())
            return "Wildcard";

        if (type.IsExec)
            return "Exec";

        if (!type.Enum.empty())
            return std::string(type.Enum);

        return type.Value == ScriptFieldType::Asset ? std::string(AssetTypeToString(type.Asset))
                                                    : std::string(ScriptFieldTypeName(type.Value));
    }

    std::string SequencePinName(const u32 index)
    {
        return std::format("Then {}", index);
    }
}

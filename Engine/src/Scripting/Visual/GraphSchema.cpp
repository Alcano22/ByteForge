#include "Engine/Scripting/Visual/GraphSchema.h"
#include "Engine/Scripting/API/ScriptAPI.h"

#include <format>
#include <map>
#include <set>
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
            return PinType::Of(parameter.Type, parameter.Asset);
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

        NodeSignature Describe(const ScriptGraph& graph, const GetVariableNode& node)
        {
            const GraphVariable* variable = graph.FindVariable(node.Variable);
            if (variable == nullptr)
                return { .Title = "Get ?", .Category = "Variables", .Error = "The variable no longer exists" };

            NodeSignature signature{ .Title = variable->Name, .Category = "Variables", .Pure = true };
            signature.Pins.push_back(Out(GraphPin::Value, PinType::Of(variable->Default)));
            return signature;
        }

        NodeSignature Describe(const ScriptGraph& graph, const SetVariableNode& node)
        {
            const GraphVariable* variable = graph.FindVariable(node.Variable);
            if (variable == nullptr)
                return { .Title = "Set ?", .Category = "Variables", .Error = "The variable no longer exists" };

            const PinType type = PinType::Of(variable->Default);

            NodeSignature signature{ .Title = std::format("Set {}", variable->Name), .Category = "Variables" };
            AddExecPins(signature);
            signature.Pins.push_back(In(GraphPin::Value, type));
            signature.Pins.push_back(Out(GraphPin::Value, type));
            return signature;
        }

        NodeSignature Describe(const ScriptGraph&, const LiteralNode& node)
        {
            const PinType type = PinType::Of(node.Value);
            std::string title = type.Value == ScriptFieldType::Asset ? AssetTypeToString(type.Asset)
                                                                     : std::string(ScriptFieldTypeName(type.Value));

            NodeSignature signature{ .Title = std::move(title), .Category = "Literals", .Pure = true };
            signature.Pins.push_back(Out(GraphPin::Value, type));
            return signature;
        }

        class SignatureCache
        {
        public:
            explicit SignatureCache(const ScriptGraph& graph)
                : m_Graph(graph) {}

            const NodeSignature* Find(const UUID id)
            {
                const auto key = static_cast<uint64_t>(id);
                if (const auto it = m_Signatures.find(key); it != m_Signatures.end())
                    return &it->second;

                const GraphNode* node = m_Graph.FindNode(id);
                if (node == nullptr)
                    return nullptr;

                return &m_Signatures.emplace(key, DescribeNode(m_Graph, *node)).first->second;
            }

        private:
            const ScriptGraph& m_Graph;
            std::map<uint64_t, NodeSignature> m_Signatures;
        };

        using DataEdges = std::map<uint64_t, std::vector<uint64_t>>; // producer -> consumers

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

        bool Reaches(const DataEdges& edges, const uint64_t start, const uint64_t target)
        {
            std::vector<uint64_t> pending{ start };
            std::set<uint64_t> visited;

            while (!pending.empty())
            {
                const uint64_t node = pending.back();
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
            enum class Mark : uint8_t { Visiting, Done };
            std::map<uint64_t, Mark> marks;

            const auto visit = [&](const auto& self, const uint64_t node) -> std::optional<UUID>
            {
                if (const auto it = marks.find(node); it != marks.end())
                    return it->second == Mark::Visiting ? std::optional(UUID(node)) : std::nullopt;

                marks.emplace(node, Mark::Visiting);
                if (const auto it = edges.find(node); it != edges.end())
                {
                    for (const uint64_t next : it->second)
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
        if (type.IsExec)
            return "Exec";

        std::string name(ScriptFieldTypeName(type.Value));
        if (type.Value == ScriptFieldType::Asset)
            name += ':' + std::string(AssetTypeToString(type.Asset));
        return name;
    }

    bool IsAssignable(const PinType& from, const PinType& to)
    {
        if (from.IsExec || to.IsExec)
            return from.IsExec && to.IsExec;

        if (from == to)
            return true;

        return from.Value == ScriptFieldType::Int && to.Value == ScriptFieldType::Float;
    }

    std::expected<PinType, std::string> CanConnect(const ScriptGraph& graph, const PinRef& from, const PinRef& to)
    {
        if (static_cast<uint64_t>(from.Node) == static_cast<uint64_t>(to.Node))
            return std::unexpected("A node cannot connect to itself");

        SignatureCache cache(graph);
        const NodeSignature* source = cache.Find(from.Node);
        const NodeSignature* target = cache.Find(to.Node);
        if (source == nullptr || target == nullptr)
            return std::unexpected("The node does not exist");

        const PinInfo* output = FindPin(*source, from.Pin, PinDirection::Output);
        const PinInfo* input = FindPin(*target, to.Pin, PinDirection::Input);
        if (output == nullptr || input == nullptr)
            return std::unexpected("The pin does not exist");

        if (!IsAssignable(output->Type, input->Type))
            return std::unexpected(std::format("Cannot connect {} to {}", DescribePinType(output->Type),
                                               DescribePinType(input->Type)));

        if (!output->Type.IsExec && Reaches(CollectDataEdges(graph, cache), to.Node, from.Node))
            return std::unexpected("This connection would create a loop");

        return output->Type;
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
            else if (!variableNames.insert(variable.Name).second)
                report(UUID(0), GraphSeverity::Error, std::format("Variable '{}' is declared twice", variable.Name));
        }

        SignatureCache cache(graph);
        std::set<uint64_t> nodeIds;
        std::set<GraphEvent> events;

        for (const GraphNode& node : graph.GetNodes())
        {
            if (!nodeIds.insert(node.Id).second)
            {
                report(node.Id, GraphSeverity::Error, "Two nodes share the same id");
                continue;
            }

            const NodeSignature& signature = *cache.Find(node.Id);
            if (signature.Error)
            {
                report(node.Id, GraphSeverity::Error, *signature.Error);
                continue;
            }

            if (const auto* event = std::get_if<EventNode>(&node.Data);
                event != nullptr && !events.insert(event->Event).second)
                report(node.Id, GraphSeverity::Error, std::format("'{}' exists more than once", signature.Title));

            for (const auto& [pin, value] : node.Defaults)
            {
                const PinInfo* input = FindPin(signature, pin, PinDirection::Input);
                if (input == nullptr || input->Type.IsExec)
                {
                    report(node.Id, GraphSeverity::Warning,
                           std::format("Stored value for unknown input '{}' is ignored", pin));
                }
                else if (!IsAssignable(PinType::Of(value), input->Type))
                {
                    report(node.Id, GraphSeverity::Warning,
                           std::format("Stored value for '{}' has the wrong type and is ignored", pin));
                }
            }
        }

        using PinKey = std::pair<uint64_t, std::string>;
        std::map<PinKey, int> dataInputLinks;
        std::map<PinKey, int> execOutputLinks;

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
                report(link.To.Node, GraphSeverity::Error,
                       std::format("'{}' cannot receive {}", input->Name, DescribePinType(output->Type)));

            if (output->Type.IsExec)
            {
                if (++execOutputLinks[{ link.From.Node, link.From.Pin }] == 2)
                    report(link.From.Node, GraphSeverity::Error,
                           std::format("'{}' has more than one connection", output->Name));
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
}

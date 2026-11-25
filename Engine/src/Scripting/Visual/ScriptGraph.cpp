#include "Engine/Scripting/Visual/ScriptGraph.h"
#include "Engine/Scripting/Visual/GraphSchema.h"

#include <algorithm>
#include <format>
#include <functional>
#include <map>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ByteForge
{
    namespace
    {
        std::optional<PinType>* WildcardType(NodeData& data)
        {
            if (auto* reroute = std::get_if<RerouteNode>(&data))
                return &reroute->Type;
            if (auto* select = std::get_if<SelectNode>(&data))
                return &select->Type;
            return nullptr;
        }

        bool IsWildcardPin(const GraphNode& node, const std::string_view pin)
        {
            if (std::holds_alternative<RerouteNode>(node.Data))
                return true;
            return std::holds_alternative<SelectNode>(node.Data) && pin != GraphPin::Condition;
        }
    }

    PinType PinType::Of(const ScriptValue& value)
    {
        const auto* asset = std::get_if<AssetRef>(&value);
        return Of(GetFieldType(value), asset != nullptr ? asset->Type : AssetType::None);
    }

    ScriptGraph ScriptGraph::FromParts(std::vector<GraphNode> nodes, std::vector<GraphLink> links,
                                       std::vector<GraphVariable> variables, std::vector<GraphCustomEvent> events)
    {
        ScriptGraph graph;
        graph.m_Nodes = std::move(nodes);
        graph.m_Links = std::move(links);
        graph.m_Variables = std::move(variables);
        graph.m_Events = std::move(events);
        return graph;
    }

    const GraphNode* ScriptGraph::FindNode(const UUID id) const
    {
        const auto it = std::ranges::find(m_Nodes, id, &GraphNode::Id);
        return it != m_Nodes.end() ? &*it : nullptr;
    }

    GraphNode* ScriptGraph::FindNode(const UUID id)
    {
        return const_cast<GraphNode*>(std::as_const(*this).FindNode(id));
    }

    const GraphVariable* ScriptGraph::FindVariable(const UUID id) const
    {
        const auto it = std::ranges::find(m_Variables, id, &GraphVariable::Id);
        return it != m_Variables.end() ? &*it : nullptr;
    }

    GraphVariable* ScriptGraph::FindVariableForEdit(const UUID id)
    {
        return const_cast<GraphVariable*>(std::as_const(*this).FindVariable(id));
    }

    const GraphVariable* ScriptGraph::FindVariable(const std::string_view name) const
    {
        const auto it = std::ranges::find(m_Variables, name, &GraphVariable::Name);
        return it != m_Variables.end() ? &*it : nullptr;
    }

    UUID ScriptGraph::AddNode(NodeData data, const glm::vec2 position)
    {
        return m_Nodes.emplace_back(GraphNode{ .Id = UUID(), .Data = std::move(data), .Position = position }).Id;
    }

    void ScriptGraph::RemoveNode(const UUID id)
    {
        const std::vector<UUID> touched = EraseLinks([id](const GraphLink& link)
        {
            return link.From.Node == id || link.To.Node == id;
        });
        std::erase_if(m_Nodes, [id](const GraphNode& node) { return node.Id == id; });
        ResetUnlinkedWildcards(touched);
    }

    Result<> ScriptGraph::Connect(const PinRef& from, const PinRef& to)
    {
        const Result<PinType> type = CanConnect(*this, from, to);
        if (!type)
            return Unexpected(type.error());

        AssignWildcardType(from, *type);
        AssignWildcardType(to, *type);

        const std::vector<UUID> touched = type->IsExec
            ? EraseLinks([&](const GraphLink& link) { return link.From == from; })
            : EraseLinks([&](const GraphLink& link) { return link.To == to; });

        m_Links.push_back({ from, to });
        ResetUnlinkedWildcards(touched);
        return {};
    }

    void ScriptGraph::Disconnect(const PinRef& pin, const PinDirection direction)
    {
        const std::vector<UUID> touched = EraseLinks([&](const GraphLink& link)
        {
            return (direction == PinDirection::Output ? link.From : link.To) == pin;
        });
        ResetUnlinkedWildcards(touched);
    }

    void ScriptGraph::RemoveLink(const GraphLink& link)
    {
        const std::vector<UUID> touched = EraseLinks([&](const GraphLink& existing)
        {
            return existing.From == link.From && existing.To == link.To;
        });
        ResetUnlinkedWildcards(touched);
    }

    Result<UUID> ScriptGraph::AddVariable(std::string name, ScriptValue defaultValue)
    {
        if (auto valid = CheckVariableName(name, UUID(0)); !valid)
            return Unexpected(valid.error());

        return m_Variables.emplace_back(GraphVariable{
            .Id      = UUID(),
            .Name    = std::move(name),
            .Default = std::move(defaultValue)
        }).Id;
    }

    Result<> ScriptGraph::RenameVariable(const UUID id, std::string name)
    {
        const auto it = std::ranges::find(m_Variables, static_cast<u64>(id),
                                          [](const GraphVariable& variable)
                                          {
                                              return static_cast<u64>(variable.Id);
                                          });
        if (it == m_Variables.end())
            return Unexpected("The variable does not exist");

        if (auto valid = CheckVariableName(name, id); !valid)
            return valid;

        it->Name = std::move(name);
        return {};
    }

    void ScriptGraph::RemoveVariable(const UUID id)
    {
        std::vector<UUID> users;
        for (const GraphNode& node : m_Nodes)
        {
            if (UsesVariable(node, id))
                users.push_back(node.Id);
        }

        for (const UUID node : users)
            RemoveNode(node);

        std::erase_if(m_Variables, [id](const GraphVariable& variable) { return variable.Id == id; });
    }

    Result<> ScriptGraph::SetVariableDefault(const UUID id, ScriptValue value)
    {
        GraphVariable* variable = FindVariableForEdit(id);
        if (variable == nullptr)
            return Unexpected("The variable does not exist");

        const bool retyped = PinType::Of(variable->Default) != PinType::Of(value);
        variable->Default = std::move(value);

        if (retyped)
            DropMismatchedVariableUses(id);
        return {};
    }

    Result<> ScriptGraph::SetVariableExposed(const UUID id, bool exposed)
    {
        GraphVariable* variable = FindVariableForEdit(id);
        if (variable == nullptr)
            return Unexpected("The variable does not exist");

        variable->Exposed = exposed;
        return {};
    }

    void ScriptGraph::DropMismatchedVariableUses(const UUID variable)
    {
        const GraphVariable* declared = FindVariable(variable);
        if (declared == nullptr) return;

        const PinType type = PinType::Of(declared->Default);

        const std::vector<UUID> touched = EraseLinks([&](const GraphLink& link)
        {
            const GraphNode* from = FindNode(link.From.Node);
            const GraphNode* to = FindNode(link.To.Node);
            if (from == nullptr || to == nullptr)
                return false;
            if (!UsesVariable(*from, variable) && !UsesVariable(*to, variable))
                return false;

            const NodeSignature source = DescribeNode(*this, *from);
            const NodeSignature target = DescribeNode(*this, *to);

            const PinInfo* output = FindPin(source, link.From.Pin, PinDirection::Output);
            const PinInfo* input = FindPin(target, link.To.Pin, PinDirection::Input);
            return output != nullptr && input != nullptr && !IsAssignable(output->Type, input->Type);
        });
        ResetUnlinkedWildcards(touched);

        for (GraphNode& node : m_Nodes)
        {
            if (!std::holds_alternative<SetVariableNode>(node.Data) || !UsesVariable(node, variable)) continue;

            const auto stored = node.Defaults.find(GraphPin::Value);
            if (stored != node.Defaults.end() && !IsAssignable(PinType::Of(stored->second), type))
                node.Defaults.erase(stored);
        }
    }

    std::vector<UUID> ScriptGraph::EraseLinks(const std::function<bool(const GraphLink&)>& matches)
    {
        std::vector<UUID> touched;
        std::erase_if(m_Links, [&](const GraphLink& link)
        {
            if (!matches(link)) return false;

            touched.push_back(link.From.Node);
            touched.push_back(link.To.Node);
            return true;
        });
        return touched;
    }

    void ScriptGraph::AssignWildcardType(const PinRef& pin, const PinType& type)
    {
        GraphNode* node = FindNode(pin.Node);
        if (node == nullptr || !IsWildcardPin(*node, pin.Pin)) return;

        std::optional<PinType>* stored = WildcardType(node->Data);
        if (stored != nullptr && !stored->has_value())
            *stored = type;
    }

    void ScriptGraph::ResetUnlinkedWildcards(const std::span<const UUID> nodes)
    {
        for (const UUID id : nodes)
        {
            GraphNode* node = FindNode(id);
            std::optional<PinType>* type = node != nullptr ? WildcardType(node->Data) : nullptr;
            if (type == nullptr || !type->has_value()) continue;

            const bool linked = std::ranges::any_of(m_Links, [&](const GraphLink& link)
            {
                return (link.From.Node == id && IsWildcardPin(*node, link.From.Pin)) ||
                       (link.To.Node == id && IsWildcardPin(*node, link.To.Pin));
            });
            if (linked) continue;

            type->reset();
            std::erase_if(node->Defaults, [&](const auto& entry) { return IsWildcardPin(*node, entry.first); });
        }
    }

    Result<> ScriptGraph::CheckVariableName(const std::string_view name, const UUID ignore) const
    {
        if (name.empty())
            return Unexpected("A variable needs a name");

        const GraphVariable* existing = FindVariable(name);
        if (existing != nullptr && existing->Id != ignore)
            return Unexpected("A variable named '{}' already exists", name);

        return {};
    }

    const GraphCustomEvent* ScriptGraph::FindEvent(const UUID id) const
    {
        const auto it = std::ranges::find(m_Events, id, &GraphCustomEvent::Id);
        return it != m_Events.end() ? &*it : nullptr;
    }

    const GraphCustomEvent* ScriptGraph::FindEvent(const std::string_view name) const
    {
        const auto it = std::ranges::find(m_Events, name, &GraphCustomEvent::Name);
        return it != m_Events.end() ? &*it : nullptr;
    }

    Result<UUID> ScriptGraph::AddEvent(std::string name)
    {
        if (auto valid = CheckEventName(name, UUID(0)); !valid)
            return Unexpected(valid.error());

        return m_Events.emplace_back(GraphCustomEvent{ .Id = UUID(), .Name = std::move(name) }).Id;
    }

    Result<> ScriptGraph::UpdateEvent(GraphCustomEvent updated)
    {
        const auto it = std::ranges::find(m_Events, updated.Id, &GraphCustomEvent::Id);
        if (it == m_Events.end())
            return Unexpected("The event does not exist");

        if (auto valid = CheckEventName(updated.Name, updated.Id); !valid)
            return valid;

        std::set<std::string, std::less<>> names;
        for (const GraphParameter& parameter : updated.Parameters)
        {
            if (parameter.Name.empty())
                return Unexpected("A parameter needs a name");
            if (parameter.Name == GraphPin::Exec || parameter.Name == GraphPin::Then)
                return Unexpected("'{}' is reserved for the exec pins", parameter.Name);
            if (!names.insert(parameter.Name).second)
                return Unexpected("Two parameters are named '{}'", parameter.Name);
        }

        std::set<std::string, std::less<>> dropped;
        std::map<std::string, std::string, std::less<>> renamed;
        for (const GraphParameter& before : it->Parameters)
        {
            const auto after = std::ranges::find(updated.Parameters, before.Id, &GraphParameter::Id);
            if (after == updated.Parameters.end() || PinType::Of(after->Default) != PinType::Of(before.Default))
                dropped.insert(before.Name);
            else if (after->Name != before.Name)
                renamed.emplace(before.Name, after->Name);
        }

        const UUID id = updated.Id;
        *it = std::move(updated);

        std::set<u64> entries;
        std::set<u64> calls;
        for (const GraphNode& node : m_Nodes)
        {
            if (const auto* entry = std::get_if<CustomEventNode>(&node.Data); entry != nullptr && entry->Event == id)
                entries.insert(node.Id);
            else if (const auto* call = std::get_if<CallEventNode>(&node.Data); call != nullptr && call->Event == id)
                calls.insert(node.Id);
        }

        const std::vector<UUID> touched = EraseLinks([&](const GraphLink& link)
        {
            return (entries.contains(link.From.Node) && dropped.contains(link.From.Pin)) ||
                   (calls.contains(link.To.Node) && dropped.contains(link.To.Pin));
        });
        ResetUnlinkedWildcards(touched);

        const auto rename = [&](std::string& pin)
        {
            if (const auto found = renamed.find(pin); found != renamed.end())
                pin = found->second;
        };

        for (GraphLink& link : m_Links)
        {
            if (entries.contains(link.From.Node))
                rename(link.From.Pin);
            if (calls.contains(link.To.Node))
                rename(link.To.Pin);
        }

        for (GraphNode& node : m_Nodes)
        {
            if (!calls.contains(node.Id)) continue;

            std::map<std::string, ScriptValue, std::less<>> defaults;
            for (auto& [pin, value] : node.Defaults)
            {
                if (dropped.contains(pin)) continue;

                std::string name = pin;
                rename(name);
                defaults.insert_or_assign(std::move(name), std::move(value));
            }
            node.Defaults = std::move(defaults);
        }

        return {};
    }

    void ScriptGraph::RemoveEvent(const UUID id)
    {
        std::vector<UUID> users;
        for (const GraphNode& node : m_Nodes)
        {
            if (UsesEvent(node, id))
                users.push_back(node.Id);
        }

        for (const UUID node : users)
            RemoveNode(node);

        std::erase_if(m_Events, [id](const GraphCustomEvent& event) { return event.Id == id; });
    }

    Result<> ScriptGraph::CheckEventName(const std::string_view name, const UUID ignore) const
    {
        if (name.empty())
            return Unexpected("An event needs a name");

        const GraphCustomEvent* existing = FindEvent(name);
        if (existing != nullptr && existing->Id != ignore)
            return Unexpected("An event named '{}' already exists", name);

        return {};
    }

    std::string ScriptGraphClassName(const UUID graphAsset)
    {
        return std::format("Graph:{}", static_cast<u64>(graphAsset));
    }

    bool UsesVariable(const GraphNode& node, const UUID variable)
    {
        if (const auto* get = std::get_if<GetVariableNode>(&node.Data))
            return get->Variable == variable;
        if (const auto* set = std::get_if<SetVariableNode>(&node.Data))
            return set->Variable == variable;
        return false;
    }

    bool UsesEvent(const GraphNode& node, const UUID event)
    {
        if (const auto* entry = std::get_if<CustomEventNode>(&node.Data))
            return entry->Event == event;
        if (const auto* call = std::get_if<CallEventNode>(&node.Data))
            return call->Event == event;
        return false;
    }

    bool HasEventEntry(const ScriptGraph& graph, const UUID event)
    {
        return std::ranges::any_of(graph.GetNodes(), [event](const GraphNode& node)
        {
            const auto* entry = std::get_if<CustomEventNode>(&node.Data);
            return entry != nullptr && entry->Event == event;
        });
    }
}

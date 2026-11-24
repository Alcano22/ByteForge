#include "Engine/Scripting/Visual/ScriptGraph.h"
#include "Engine/Scripting/Visual/GraphSchema.h"

#include <algorithm>
#include <format>
#include <utility>

namespace ByteForge
{
    PinType PinType::Of(const ScriptValue& value)
    {
        const auto* asset = std::get_if<AssetRef>(&value);
        return Of(GetFieldType(value), asset != nullptr ? asset->Type : AssetType::None);
    }

    ScriptGraph ScriptGraph::FromParts(std::vector<GraphNode> nodes, std::vector<GraphLink> links,
                                       std::vector<GraphVariable> variables)
    {
        ScriptGraph graph;
        graph.m_Nodes = std::move(nodes);
        graph.m_Links = std::move(links);
        graph.m_Variables = std::move(variables);
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
        std::erase_if(m_Links, [id](const GraphLink& link) { return link.From.Node == id || link.To.Node == id; });
        std::erase_if(m_Nodes, [id](const GraphNode& node) { return node.Id == id; });
    }

    Result<> ScriptGraph::Connect(const PinRef& from, const PinRef& to)
    {
        const Result<PinType> type = CanConnect(*this, from, to);
        if (!type)
            return Unexpected(type.error());

        if (type->IsExec)
            std::erase_if(m_Links, [&](const GraphLink& link) { return link.From == from; });
        else
            std::erase_if(m_Links, [&](const GraphLink& link) { return link.To == to; });

        m_Links.push_back({ from, to });
        return {};
    }

    void ScriptGraph::Disconnect(const PinRef& pin, const PinDirection direction)
    {
        std::erase_if(m_Links, [&](const GraphLink& link)
        {
            return (direction == PinDirection::Output ? link.From : link.To) == pin;
        });
    }

    void ScriptGraph::RemoveLink(const GraphLink& link)
    {
        std::erase_if(m_Links, [&](const GraphLink& existing)
        {
            return existing.From == link.From && existing.To == link.To;
        });
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

        std::erase_if(m_Links, [&](const GraphLink& link)
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

        for (GraphNode& node : m_Nodes)
        {
            if (!std::holds_alternative<SetVariableNode>(node.Data) || !UsesVariable(node, variable)) continue;

            const auto stored = node.Defaults.find(GraphPin::Value);
            if (stored != node.Defaults.end() && !IsAssignable(PinType::Of(stored->second), type))
                node.Defaults.erase(stored);
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

    bool UsesVariable(const GraphNode& node, const UUID variable)
    {
        if (const auto* get = std::get_if<GetVariableNode>(&node.Data))
            return get->Variable == variable;
        if (const auto* set = std::get_if<SetVariableNode>(&node.Data))
            return set->Variable == variable;
        return false;
    }

    std::string ScriptGraphClassName(const UUID graphAsset)
    {
        return std::format("Graph:{}", static_cast<u64>(graphAsset));
    }
}

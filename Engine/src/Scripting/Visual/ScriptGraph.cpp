#include "Engine/Scripting/Visual/ScriptGraph.h"
#include "Engine/Scripting/Visual/GraphSchema.h"

#include <algorithm>
#include <format>
#include <utility>

namespace ByteForge
{
    namespace
    {
        bool UsesVariable(const GraphNode& node, const UUID variable)
        {
            const auto key = static_cast<u64>(variable);
            if (const auto* get = std::get_if<GetVariableNode>(&node.Data))
                return static_cast<u64>(get->Variable) == key;
            if (const auto* set = std::get_if<SetVariableNode>(&node.Data))
                return static_cast<u64>(set->Variable) == key;
            return false;
        }
    }

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
        const auto it = std::ranges::find(m_Nodes, static_cast<u64>(id),
                                          [](const GraphNode& node) { return static_cast<u64>(node.Id); });
        return it != m_Nodes.end() ? &*it : nullptr;
    }

    GraphNode* ScriptGraph::FindNode(const UUID id)
    {
        return const_cast<GraphNode*>(std::as_const(*this).FindNode(id));
    }

    const GraphVariable* ScriptGraph::FindVariable(const UUID id) const
    {
        const auto it = std::ranges::find(m_Variables, static_cast<u64>(id),
                                          [](const GraphVariable& variable) { return static_cast<u64>(variable.Id); });
        return it != m_Variables.end() ? &*it : nullptr;
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
        const auto key = static_cast<u64>(id);
        std::erase_if(m_Links, [key](const GraphLink& link)
        {
            return static_cast<u64>(link.From.Node) == key || static_cast<u64>(link.To.Node) == key;
        });
        std::erase_if(m_Nodes, [key](const GraphNode& node) { return static_cast<u64>(node.Id) == key; });
    }

    Result<> ScriptGraph::Connect(const PinRef& from, const PinRef& to)
    {
        const Result<PinType> type = CanConnect(*this, from, to);
        if (!type)
            return std::unexpected(type.error());

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
            return std::unexpected(valid.error());

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
            return std::unexpected("The variable does not exist");

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

        std::erase_if(m_Variables, [key = static_cast<u64>(id)](const GraphVariable& variable)
        {
            return static_cast<u64>(variable.Id) == key;
        });
    }

    Result<> ScriptGraph::CheckVariableName(const std::string_view name,
                                                                    const UUID ignore) const
    {
        if (name.empty())
            return std::unexpected("A variable needs a name");

        const GraphVariable* existing = FindVariable(name);
        if (existing != nullptr && static_cast<u64>(existing->Id) != static_cast<u64>(ignore))
            return std::unexpected(std::format("A variable named '{}' already exists", name));

        return {};
    }

    std::string ScriptGraphClassName(const UUID graphAsset)
    {
        return std::format("Graph:{}", static_cast<u64>(graphAsset));
    }
}

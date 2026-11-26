#include "Engine/Scripting/Visual/GraphClipboard.h"
#include "Engine/Scripting/Visual/ScriptGraphSerializer.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <format>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <utility>

namespace ByteForge
{
    namespace
    {
        constexpr const char* ClipboardKey = "byteforgeGraphNodes";
        constexpr i32 ClipboardVersion = 1;

        bool HasEvent(const ScriptGraph& graph, const GraphEvent event)
        {
            return std::ranges::any_of(graph.GetNodes(), [event](const GraphNode& node)
            {
                const auto* data = std::get_if<EventNode>(&node.Data);
                return data != nullptr && data->Event == event;
            });
        }

        std::optional<UUID> UsedVariable(const NodeData& data)
        {
            if (const auto* get = std::get_if<GetVariableNode>(&data))
                return get->Variable;
            if (const auto* set = std::get_if<SetVariableNode>(&data))
                return set->Variable;
            return std::nullopt;
        }

        void ReplaceVariable(NodeData& data, const UUID variable)
        {
            if (auto* get = std::get_if<GetVariableNode>(&data))
                get->Variable = variable;
            else if (auto* set = std::get_if<SetVariableNode>(&data))
                set->Variable = variable;
        }

        std::string FreeVariableName(const ScriptGraph& graph, const std::string& name)
        {
            if (graph.FindVariable(std::string_view(name)) == nullptr)
                return name;

            for (u32 suffix = 2;; ++suffix)
            {
                std::string candidate = std::format("{} {}", name, suffix);
                if (graph.FindVariable(std::string_view(candidate)) == nullptr)
                    return candidate;
            }
        }

        bool SameType(const GraphVariable& a, const GraphVariable& b)
        {
            return PinType::Of(a.Default) == PinType::Of(b.Default);
        }

        Result<UUID> MatchVariable(ScriptGraph& target, const GraphVariable& source, const UUID scope)
        {
            if (const GraphVariable* same = target.FindVariable(source.Id);
                same != nullptr && IsVisibleIn(*same, scope) && SameType(*same, source))
                return same->Id;

            for (const GraphVariable& named : target.GetVariables())
            {
                if (named.Name == source.Name && IsVisibleIn(named, scope) && SameType(named, source))
                    return named.Id;
            }

            const UUID owner = source.Scope == UUID(0) ? UUID(0) : scope;
            const Result<UUID> added = target.AddVariable(FreeVariableName(target, source.Name), source.Default, owner);
            if (!added)
                return added;

            if (owner == UUID(0))
            {
                if (const Result<> exposed = target.SetVariableExposed(*added, source.Exposed); !exposed)
                    return Unexpected(exposed.error());
            }
            return added;
        }

        std::optional<UUID> UsedEvent(const NodeData& data)
        {
            if (const auto* entry = std::get_if<CustomEventNode>(&data))
                return entry->Event;
            if (const auto* call = std::get_if<CallEventNode>(&data))
                return call->Event;
            return std::nullopt;
        }

        void ReplaceEvent(NodeData& data, const UUID event)
        {
            if (auto* entry = std::get_if<CustomEventNode>(&data))
                entry->Event = event;
            else if (auto* call = std::get_if<CallEventNode>(&data))
                call->Event = event;
        }

        std::string FreeEventName(const ScriptGraph& graph, const std::string& name)
        {
            if (graph.FindEvent(std::string_view(name)) == nullptr)
                return name;

            for (u32 suffix = 2;; ++suffix)
            {
                std::string candidate = std::format("{} {}", name, suffix);
                if (graph.FindEvent(std::string_view(candidate)) == nullptr)
                    return candidate;
            }
        }

        Result<UUID> MatchEvent(ScriptGraph& target, const GraphCustomEvent& source)
        {
            if (const GraphCustomEvent* same = target.FindEvent(source.Id))
                return same->Id;
            if (const GraphCustomEvent* named = target.FindEvent(std::string_view(source.Name)))
                return named->Id;

            const Result<UUID> added = target.AddEvent(FreeEventName(target, source.Name));
            if (!added)
                return added;

            GraphCustomEvent copy = source;
            copy.Id = *added;
            copy.Name = target.FindEvent(*added)->Name;
            if (const Result<> updated = target.UpdateEvent(std::move(copy)); !updated)
                return Unexpected(updated.error());
            return added;
        }

        std::optional<UUID> UsedFunction(const NodeData& data)
        {
            if (const auto* entry = std::get_if<FunctionEntryNode>(&data))
                return entry->Function;
            if (const auto* result = std::get_if<FunctionReturnNode>(&data))
                return result->Function;
            if (const auto* call = std::get_if<CallFunctionNode>(&data))
                return call->Function;
            return std::nullopt;
        }

        void ReplaceFunction(NodeData& data, const UUID function)
        {
            if (auto* entry = std::get_if<FunctionEntryNode>(&data))
                entry->Function = function;
            else if (auto* result = std::get_if<FunctionReturnNode>(&data))
                result->Function = function;
            else if (auto* call = std::get_if<CallFunctionNode>(&data))
                call->Function = function;
        }

        std::string FreeFunctionName(const ScriptGraph& graph, const std::string& name)
        {
            if (graph.FindFunction(std::string_view(name)) == nullptr)
                return name;

            for (u32 suffix = 2;; ++suffix)
            {
                std::string candidate = std::format("{} {}", name, suffix);
                if (graph.FindFunction(std::string_view(candidate)) == nullptr)
                    return candidate;
            }
        }

        Result<UUID> MatchFunction(ScriptGraph& target, const GraphFunction& source)
        {
            if (const GraphFunction* same = target.FindFunction(source.Id))
                return same->Id;
            if (const GraphFunction* named = target.FindFunction(std::string_view(source.Name)))
                return named->Id;

            const Result<UUID> added = target.AddFunction(FreeFunctionName(target, source.Name));
            if (!added)
                return added;

            GraphFunction copy = source;
            copy.Id = *added;
            copy.Name = target.FindFunction(*added)->Name;
            if (const Result<> updated = target.UpdateFunction(std::move(copy)); !updated)
                return Unexpected(updated.error());
            return added;
        }

        Result<ScriptGraph> ReadClipboard(const std::string_view text)
        {
            const nlohmann::json json = nlohmann::json::parse(text, nullptr, false);
            if (json.is_discarded() || !json.is_object())
                return Unexpected("The clipboard holds no script graph nodes");

            const auto version = json.find(ClipboardKey);
            if (version == json.end() || !version->is_number_integer() || !json.contains("graph"))
                return Unexpected("The clipboard holds no script graph nodes");
            if (version->get<i32>() > ClipboardVersion)
                return Unexpected("The nodes were copied from a newer version of ByteForge");

            Result<ScriptGraph> copied = DeserializeScriptGraph(json.at("graph"));
            if (!copied)
                return Unexpected("The copied nodes cannot be read: {}", copied.error());
            return copied;
        }
    }

    std::string CopyGraphNodes(const ScriptGraph& graph, const std::span<const UUID> nodes)
    {
        std::set<u64> selected;
        for (const UUID node : nodes)
        {
            if (graph.FindNode(node) != nullptr)
                selected.insert(node);
        }

        std::vector<GraphNode> copiedNodes;
        std::set<u64> usedVariables;
        std::set<u64> usedEvents;
        std::set<u64> usedFunctions;
        for (const GraphNode& node : graph.GetNodes())
        {
            if (!selected.contains(node.Id)) continue;

            copiedNodes.push_back(node);
            if (const std::optional<UUID> variable = UsedVariable(node.Data))
                usedVariables.insert(*variable);
            if (const std::optional<UUID> event = UsedEvent(node.Data))
                usedEvents.insert(*event);
            if (const std::optional<UUID> function = UsedFunction(node.Data))
                usedFunctions.insert(*function);
        }

        std::vector<GraphLink> copiedLinks;
        for (const GraphLink& link : graph.GetLinks())
        {
            if (selected.contains(link.From.Node) && selected.contains(link.To.Node))
                copiedLinks.push_back(link);
        }

        std::vector<GraphVariable> copiedVariables;
        for (const GraphVariable& variable : graph.GetVariables())
        {
            if (usedVariables.contains(variable.Id))
                copiedVariables.push_back(variable);
        }

        std::vector<GraphCustomEvent> copiedEvents;
        for (const GraphCustomEvent& event : graph.GetEvents())
        {
            if (usedEvents.contains(event.Id))
                copiedEvents.push_back(event);
        }

        std::vector<GraphFunction> copiedFunctions;
        for (const GraphFunction& function : graph.GetFunctions())
        {
            if (usedFunctions.contains(function.Id))
                copiedFunctions.push_back(function);
        }

        const ScriptGraph copied = ScriptGraph::FromParts(std::move(copiedNodes), std::move(copiedLinks),
                                                          std::move(copiedVariables), std::move(copiedEvents),
                                                          std::move(copiedFunctions));

        const nlohmann::json json{
            { ClipboardKey, ClipboardVersion            },
            { "graph",      SerializeScriptGraph(copied) }
        };
        return json.dump();
    }

    Result<PastedGraphNodes> PasteGraphNodes(ScriptGraph& graph, const std::string_view text,
                                             const glm::vec2 position, const UUID scope)
    {
        const Result<ScriptGraph> copied = ReadClipboard(text);
        if (!copied)
            return Unexpected(copied.error());

        glm::vec2 origin(std::numeric_limits<f32>::max());
        for (const GraphNode& node : copied->GetNodes())
            origin = glm::min(origin, node.Position);

        const bool inFunction = scope != UUID(0);

        PastedGraphNodes pasted;
        std::map<u64, UUID> nodeIds;
        std::map<u64, UUID> variableIds;
        std::map<u64, UUID> eventIds;
        std::map<u64, UUID> functionIds;

        for (const GraphNode& node : copied->GetNodes())
        {
            if (const auto* event = std::get_if<EventNode>(&node.Data);
                event != nullptr && (inFunction || HasEvent(graph, event->Event)))
            {
                ++pasted.SkippedEvents;
                continue;
            }
            if (inFunction && std::holds_alternative<CustomEventNode>(node.Data))
            {
                ++pasted.SkippedEvents;
                continue;
            }

            NodeData data = node.Data;
            if (const std::optional<UUID> variable = UsedVariable(data))
            {
                auto match = variableIds.find(*variable);
                if (match == variableIds.end())
                {
                    const GraphVariable* source = copied->FindVariable(*variable);
                    const Result<UUID> target = source != nullptr ? MatchVariable(graph, *source, scope)
                                                                  : Result<UUID>(*variable);
                    if (!target) continue;

                    match = variableIds.emplace(*variable, *target).first;
                }
                ReplaceVariable(data, match->second);
            }

            if (const std::optional<UUID> event = UsedEvent(data))
            {
                auto match = eventIds.find(*event);
                if (match == eventIds.end())
                {
                    const GraphCustomEvent* source = copied->FindEvent(*event);
                    const Result<UUID> target = source != nullptr ? MatchEvent(graph, *source)
                                                                  : Result<UUID>(*event);
                    if (!target) continue;

                    match = eventIds.emplace(*event, *target).first;
                }
                ReplaceEvent(data, match->second);

                if (const auto* entry = std::get_if<CustomEventNode>(&data);
                    entry != nullptr && HasEventEntry(graph, entry->Event))
                {
                    ++pasted.SkippedEvents;
                    continue;
                }
            }

            if (const std::optional<UUID> function = UsedFunction(data))
            {
                auto match = functionIds.find(*function);
                if (match == functionIds.end())
                {
                    const GraphFunction* source = copied->FindFunction(*function);
                    const Result<UUID> target = source != nullptr ? MatchFunction(graph, *source)
                                                                  : Result<UUID>(*function);
                    if (!target) continue;

                    match = functionIds.emplace(*function, *target).first;
                }
                ReplaceFunction(data, match->second);

                const bool entry = std::holds_alternative<FunctionEntryNode>(data);
                const bool result = std::holds_alternative<FunctionReturnNode>(data);
                if (((entry || result) && match->second != scope) || (entry && HasFunctionEntry(graph, match->second)))
                {
                    ++pasted.SkippedEvents;
                    continue;
                }
            }

            const UUID id = graph.AddNode(std::move(data), position + (node.Position - origin), scope);
            graph.FindNode(id)->Defaults = node.Defaults;

            nodeIds.emplace(node.Id, id);
            pasted.Nodes.push_back(id);
        }

        for (const GraphLink& link : copied->GetLinks())
        {
            const auto from = nodeIds.find(link.From.Node);
            const auto to = nodeIds.find(link.To.Node);
            if (from == nodeIds.end() || to == nodeIds.end()) continue;

            static_cast<void>(graph.Connect({ from->second, link.From.Pin }, { to->second, link.To.Pin }));
        }

        return pasted;
    }
}

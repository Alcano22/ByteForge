#include "Engine/Scripting/Visual/GraphDebugger.h"

#include <algorithm>
#include <map>
#include <utility>

namespace ByteForge
{
    namespace
    {
        struct DebuggerState
        {
            UUID Watched{ 0 };
            std::vector<GraphLink> Links;
            std::map<u64, std::vector<GraphRuntimeError>> Errors;
        };

        DebuggerState& State()
        {
            static DebuggerState state;
            return state;
        }

        bool SameLink(const GraphLink& a, const GraphLink& b)
        {
            return a.From == b.From && a.To == b.To;
        }
    }

    void GraphDebugger::Watch(const UUID graph)
    {
        DebuggerState& state = State();
        if (state.Watched == graph) return;

        state.Watched = graph;
        state.Links.clear();
    }

    bool GraphDebugger::IsWatched(const UUID graph)
    {
        return graph != UUID(0) && State().Watched == graph;
    }

    void GraphDebugger::OnLinkTaken(const UUID graph, const GraphLink& link)
    {
        if (!IsWatched(graph)) return;

        std::vector<GraphLink>& links = State().Links;
        if (std::ranges::none_of(links, [&](const GraphLink& taken) { return SameLink(taken, link); }))
            links.push_back(link);
    }

    void GraphDebugger::OnError(const UUID graph, GraphRuntimeError error)
    {
        std::vector<GraphRuntimeError>& errors = State().Errors[graph];

        const auto it = std::ranges::find(errors, error.Node, &GraphRuntimeError::Node);
        if (it != errors.end())
            *it = std::move(error);
        else
            errors.push_back(std::move(error));
    }

    std::vector<GraphLink> GraphDebugger::TakeLinks()
    {
        return std::exchange(State().Links, {});
    }

    std::vector<GraphRuntimeError> GraphDebugger::GetErrors(const UUID graph)
    {
        const auto& errors = State().Errors;
        const auto it = errors.find(graph);
        return it != errors.end() ? it->second : std::vector<GraphRuntimeError>{};
    }

    void GraphDebugger::ClearErrors() { State().Errors.clear(); }
}

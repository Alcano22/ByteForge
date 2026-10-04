#include "Editor/Graph/GraphNodePalette.h"

#include <Engine/Scripting/API/ScriptAPI.h>
#include <Engine/Scripting/Visual/GraphSchema.h>

#include <imgui.h>
#include <magic_enum/magic_enum.hpp>

#include <algorithm>
#include <format>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace ByteForge::GraphNodePalette
{
    namespace
    {
        bool HasEvent(const ScriptGraph& graph, const GraphEvent event)
        {
            return std::ranges::any_of(graph.GetNodes(), [event](const GraphNode& node)
            {
                const auto* data = std::get_if<EventNode>(&node.Data);
                return data != nullptr && data->Event == event;
            });
        }
    }

    std::optional<NodeData> DrawMenu(const ScriptGraph& graph)
    {
        std::optional<NodeData> chosen;

        if (ImGui::BeginMenu("Events"))
        {
            for (const GraphEvent event : magic_enum::enum_values<GraphEvent>())
            {
                const std::string title = DescribeNode(graph, GraphNode{ .Data = EventNode{ event } }).Title;
                if (ImGui::MenuItem(title.c_str(), nullptr, false, !HasEvent(graph, event)))
                    chosen = EventNode{ event };
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Flow"))
        {
            if (ImGui::MenuItem("Branch"))
                chosen = BranchNode{};
            if (ImGui::MenuItem("Delay"))
                chosen = DelayNode{};
            ImGui::EndMenu();
        }

        if (!graph.GetVariables().empty() && ImGui::BeginMenu("Variables"))
        {
            for (const GraphVariable& variable : graph.GetVariables())
            {
                ImGui::PushID(static_cast<int>(static_cast<uint64_t>(variable.Id) & 0x7fffffff));
                if (ImGui::MenuItem(std::format("Get {}", variable.Name).c_str()))
                    chosen = GetVariableNode{ variable.Id };
                if (ImGui::MenuItem(std::format("Set {}", variable.Name).c_str()))
                    chosen = SetVariableNode{ variable.Id };
                ImGui::PopID();
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();

        std::map<std::string_view, std::vector<const ScriptFunction*>> categories;
        for (const ScriptFunction& function : ScriptAPI::Get().GetFunctions())
            categories[function.Category].push_back(&function);

        for (const auto& [category, functions] : categories)
        {
            if (!ImGui::BeginMenu(std::string(category).c_str())) continue;

            for (const ScriptFunction* function : functions)
            {
                ImGui::PushID(function->Id.c_str());
                if (ImGui::MenuItem(function->DisplayName.c_str()))
                    chosen = CallNode{ function->Id };
                ImGui::PopID();
            }
            ImGui::EndMenu();
        }

        return chosen;
    }
}

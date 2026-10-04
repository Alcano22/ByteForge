#include "Editor/Graph/GraphNodePalette.h"

#include <Engine/Scripting/API/ScriptAPI.h>
#include <Engine/Scripting/Visual/GraphSchema.h>

#include <imgui.h>
#include <imgui_stdlib.h>
#include <magic_enum/magic_enum.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <format>
#include <sstream>
#include <string_view>
#include <utility>
#include <tuple>

namespace ByteForge
{
    namespace
    {
        constexpr float PaletteWidth = 280.0f;
        constexpr float ListHeight   = 320.0f;

        std::string ToLower(const std::string_view text)
        {
            std::string result(text);
            std::ranges::transform(result, result.begin(), [](const unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });
            return result;
        }

        bool HasEvent(const ScriptGraph& graph, const GraphEvent event)
        {
            return std::ranges::any_of(graph.GetNodes(), [event](const GraphNode& node)
            {
                const auto* data = std::get_if<EventNode>(&node.Data);
                return data != nullptr && data->Event == event;
            });
        }

        bool CanConnectTo(const NodeSignature& signature, const PaletteContext& context)
        {
            return std::ranges::any_of(signature.Pins, [&](const PinInfo& pin)
            {
                if (pin.Direction == context.Direction)
                    return false;

                return context.Direction == PinDirection::Output ? IsAssignable(context.Type, pin.Type)
                                                                 : IsAssignable(pin.Type, context.Type);
            });
        }

        bool Matches(const std::string_view searchText, const std::string& query)
        {
            std::istringstream words(ToLower(query));
            for (std::string word; words >> word;)
            {
                if (searchText.find(word) == std::string_view::npos)
                    return false;
            }
            return true;
        }
    }

    void GraphNodePalette::Open(const ScriptGraph& graph, const std::optional<PaletteContext> context)
    {
        m_Entries.clear();
        m_Search.clear();
        m_FocusSearch = true;
        m_Hint = context ? std::format("Nodes for a {} {}", DescribePinType(context->Type),
                                       context->Direction == PinDirection::Output ? "output" : "input")
                         : std::string();

        const auto add = [&](NodeData data)
        {
            const NodeSignature signature = DescribeNode(graph, GraphNode{ .Data = data });
            if (signature.Error || (context && !CanConnectTo(signature, *context)))
                return;

            std::string title = std::holds_alternative<GetVariableNode>(data)
                                    ? std::format("Get {}", signature.Title)
                                    : signature.Title;
            std::string searchText = ToLower(std::format("{} {}", signature.Category, title));

            m_Entries.push_back({ signature.Category, std::move(title), std::move(searchText), std::move(data) });
        };

        for (const GraphEvent event : magic_enum::enum_values<GraphEvent>())
        {
            if (!HasEvent(graph, event))
                add(EventNode{ event });
        }

        add(BranchNode{});
        add(DelayNode{});

        constexpr std::array valueTypes{ ScriptFieldType::Bool, ScriptFieldType::Int, ScriptFieldType::Float,
                                         ScriptFieldType::Vector2, ScriptFieldType::Vector3, ScriptFieldType::Vector4,
                                         ScriptFieldType::String };
        for (const ScriptFieldType type : valueTypes)
            add(LiteralNode{ DefaultValueFor(PinType::Of(type)) });

        for (const AssetType asset : { AssetType::AudioClip, AssetType::Texture2D, AssetType::PhysicsMaterial2D })
            add(LiteralNode{ AssetRef{ asset, UUID(0) } });

        for (const GraphVariable& variable : graph.GetVariables())
        {
            add(GetVariableNode{ variable.Id });
            add(SetVariableNode{ variable.Id });
        }

        std::vector<const ScriptFunction*> functions;
        for (const ScriptFunction& function : ScriptAPI::Get().GetFunctions())
            functions.push_back(&function);

        std::ranges::sort(functions, [](const ScriptFunction* a, const ScriptFunction* b)
        {
            return std::tie(a->Category, a->DisplayName) < std::tie(b->Category, b->DisplayName);
        });

        for (const ScriptFunction* function : functions)
            add(CallNode{ function->Id });
    }

    std::optional<NodeData> GraphNodePalette::Draw()
    {
        if (!m_Hint.empty())
            ImGui::TextDisabled("%s", m_Hint.c_str());

        if (m_FocusSearch)
        {
            ImGui::SetKeyboardFocusHere();
            m_FocusSearch = false;
        }

        ImGui::SetNextItemWidth(PaletteWidth);
        const bool submitted = ImGui::InputTextWithHint("##search", "Search nodes...", &m_Search,
                                                        ImGuiInputTextFlags_EnterReturnsTrue);

        std::optional<NodeData> chosen;

        if (m_Entries.empty())
        {
            ImGui::TextDisabled("No matching nodes");
            return chosen;
        }

        if (m_Search.empty())
        {
            for (size_t first = 0; first < m_Entries.size();)
            {
                const std::string& category = m_Entries[first].Category;
                size_t end = first;
                while (end < m_Entries.size() && m_Entries[end].Category == category)
                    ++end;

                if (ImGui::BeginMenu(category.c_str()))
                {
                    for (size_t i = first; i < end; ++i)
                    {
                        ImGui::PushID(&m_Entries[i]);
                        if (ImGui::MenuItem(m_Entries[i].Title.c_str()))
                            chosen = m_Entries[i].Data;
                        ImGui::PopID();
                    }
                    ImGui::EndMenu();
                }

                first = end;
            }

            return chosen;
        }

        const Entry* firstMatch = nullptr;
        ImGui::BeginChild("##entries", ImVec2(PaletteWidth, ListHeight));

        std::string_view category;
        for (const Entry& entry : m_Entries)
        {
            if (!Matches(entry.SearchText, m_Search))
                continue;

            if (firstMatch == nullptr)
                firstMatch = &entry;

            if (entry.Category != category)
            {
                category = entry.Category;
                ImGui::SeparatorText(entry.Category.c_str());
            }

            ImGui::PushID(&entry);
            if (ImGui::Selectable(entry.Title.c_str()))
                chosen = entry.Data;
            ImGui::PopID();
        }

        if (firstMatch == nullptr)
            ImGui::TextDisabled("No matching nodes");

        ImGui::EndChild();

        if (submitted && firstMatch != nullptr && !chosen)
            chosen = firstMatch->Data;

        return chosen;
    }
}

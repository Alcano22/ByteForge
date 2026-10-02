#include "Editor/Panels/InspectorPanel.h"
#include "Editor/EditorContext.h"
#include "Editor/Overloaded.h"
#include "Editor/StringUtils.h"
#include "Editor/Inspectors/AssetInspector.h"
#include "Editor/Inspectors/ComponentInspectors.h"
#include "Editor/Inspectors/LogEntryInspector.h"
#include "Editor/Inspectors/ScriptInspector.h"

#include <Engine/Scene/Components.h>

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <string_view>
#include <variant>

namespace ByteForge
{
    namespace
    {
        constexpr const char* AddComponentPopupId = "##AddComponent";
    }

    void InspectorPanel::OnImGuiRender()
    {
        if (!m_Open) return;
        if (!ImGui::Begin(GetName().c_str(), &m_Open))
        {
            ImGui::End();
            return;
        }

        CommandHistory& history = GetContext().History;

        std::visit(Overloaded{
            [&](std::monostate)
            {
                m_EditTracker.Flush(history);
                ImGui::TextDisabled("Nothing selected");
            },
            [&](const Entity entity)
            {
                if (!entity.IsValid())
                {
                    m_EditTracker.Flush(history);
                    ImGui::TextDisabled("Nothing selected");
                    return;
                }

                if (!GetContext().IsEditing())
                {
                    m_EditTracker.Cancel();
                    DrawEntity(entity);
                    return;
                }

                m_EditTracker.Begin(entity, history);
                DrawEntity(entity);
                m_EditTracker.End(entity, history);
            },
            [&](const AssetSelection& asset)
            {
                m_EditTracker.Flush(history);
                EditorUI::DrawAssetInspector(asset.Handle, GetContext());
            },
            [&](const LogSelection& log)
            {
                m_EditTracker.Flush(history);
                EditorUI::DrawLogEntryInspector(log.Entry);
            }
        }, GetContext().SelectionContext.Get());

        ImGui::End();
    }

    void InspectorPanel::DrawEntity(const Entity entity) const
    {
        DrawHeader(entity);
        ImGui::Separator();

        for (const ComponentInspector& inspector : GetComponentInspectors())
        {
            if (inspector.Has(entity))
                DrawComponentSection(inspector, entity);
        }

        EditorUI::DrawScriptDropZone(entity);
    }

    void InspectorPanel::DrawComponentSection(const ComponentInspector& inspector, const Entity entity) const
    {
        constexpr ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen
                                           | ImGuiTreeNodeFlags_Framed
                                           | ImGuiTreeNodeFlags_SpanAvailWidth
                                           | ImGuiTreeNodeFlags_FramePadding
                                           | ImGuiTreeNodeFlags_AllowOverlap;

        ImGui::PushID(inspector.Name);

        const float buttonSize = ImGui::GetFrameHeight();
        const float headerEnd = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;

        const bool open = ImGui::TreeNodeEx("##header", flags, "%s", inspector.Name);
        ImGui::OpenPopupOnItemClick("##options", ImGuiPopupFlags_MouseButtonRight);

        ImGui::SameLine(headerEnd - buttonSize);
        if (ImGui::Button("...", ImVec2(buttonSize, buttonSize)))
            ImGui::OpenPopup("##options");

        bool remove = false;
        if (ImGui::BeginPopup("##options"))
        {
            if (ImGui::MenuItem("Reset"))
                inspector.Reset(entity);

            if (ImGui::MenuItem("Remove Component", nullptr, false, inspector.Removable))
                remove = true;

            ImGui::EndPopup();
        }

        if (open)
        {
            if (!remove)
                inspector.Draw(entity, GetContext());
            ImGui::TreePop();
        }

        ImGui::PopID();

        if (remove)
            inspector.Remove(entity);
    }

    void InspectorPanel::DrawHeader(const Entity entity)
    {
        auto& tag = entity.GetComponent<TagComponent>();

        std::array<char, 256> buffer{};
        const size_t length = std::min(tag.Tag.size(), buffer.size() - 1);
        std::memcpy(buffer.data(), tag.Tag.data(), length);
        buffer[length] = '\0';

        const ImGuiStyle& style = ImGui::GetStyle();
        const float buttonWidth = ImGui::CalcTextSize("Add Component").x + style.FramePadding.x * 2.0f;

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - buttonWidth - style.ItemSpacing.x);
        if (ImGui::InputText("##Tag", buffer.data(), buffer.size()))
            tag.Tag = buffer.data();

        ImGui::SameLine();
        if (ImGui::Button("Add Component"))
            ImGui::OpenPopup(AddComponentPopupId);

        DrawAddComponentPopup(entity);
    }

    void InspectorPanel::DrawAddComponentPopup(const Entity entity)
    {
        if (!ImGui::BeginPopup(AddComponentPopupId)) return;

        static std::array<char, 64> s_Filter{};
        if (ImGui::IsWindowAppearing())
        {
            s_Filter.fill('\0');
            ImGui::SetKeyboardFocusHere();
        }

        ImGui::SetNextItemWidth(220.0f);
        const bool submitted = ImGui::InputTextWithHint("##search", "Search components...", s_Filter.data(),
                                                        s_Filter.size(), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::Separator();

        const std::string_view query(s_Filter.data());
        bool anyMatch = false;

        for (const ComponentInspector& inspector : GetComponentInspectors())
        {
            if (!inspector.Removable || inspector.Has(entity) || !ContainsIgnoreCase(inspector.Name, query)) continue;

            const bool pickedByEnter = submitted && !anyMatch;
            anyMatch = true;

            if (ImGui::Selectable(inspector.Name) || pickedByEnter)
            {
                inspector.Add(entity);
                ImGui::CloseCurrentPopup();
            }
        }

        if (!anyMatch)
            ImGui::TextDisabled(query.empty() ? "All components added" : "No components match");

        ImGui::EndPopup();
    }
}

#include "Editor/Panels/InspectorPanel.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorWidgets.h"
#include "Editor/Overloaded.h"
#include "Editor/StringUtils.h"
#include "Editor/Inspectors/AssetInspector.h"
#include "Editor/Inspectors/ComponentInspectors.h"
#include "Editor/Inspectors/ComponentOrdering.h"
#include "Editor/Inspectors/LogEntryInspector.h"
#include "Editor/Inspectors/ScriptInspector.h"

#include <Engine/Scene/Components.h>
#include <Engine/Assets/AssetRegistry.h>
#include <Engine/Assets/Prefab.h>
#include <Engine/Scripting/ScriptEngine.h>

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <string_view>
#include <variant>
#include <format>
#include <string>
#include <vector>

namespace ByteForge
{
    namespace
    {
        constexpr const char* AddComponentPopupId = "##AddComponent";

        constexpr const char* ComponentPayloadType = "ByteForge.Component";

        ScriptEntry* FindScript(const Entity entity, const u64 scriptId)
        {
            auto* component = entity.TryGetComponent<ScriptComponent>();
            return component != nullptr ? component->Find(UUID(scriptId)) : nullptr;
        }
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
        DrawPrefabBar(entity);
        ImGui::Separator();

        for (const InspectorSection& section : GetOrderedSections(entity))
            DrawSection(section, entity);

        EditorUI::DrawScriptDropZone(entity);
    }

    void InspectorPanel::DrawSection(const InspectorSection& section, const Entity entity) const
    {
        EditorContext& context = GetContext();
        const ComponentInspector* inspector = section.Inspector;

        ScriptEntry* script = nullptr;
        std::string title;
        std::string id;
        EditorIcon icon = EditorIcon::FileScript;
        bool removable = true;
        bool missing = false;

        if (inspector != nullptr)
        {
            title = inspector->Name;
            id = inspector->Name;
            icon = inspector->Icon;
            removable = inspector->Removable;
        } else
        {
            script = FindScript(entity, section.Key.ScriptId);
            if (script == nullptr) return;

            title = EditorUI::GetScriptTitle(*script);
            id = std::format("script{}", section.Key.ScriptId);
            missing = EditorUI::IsScriptMissing(*script);
        }

        constexpr ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen
                                           | ImGuiTreeNodeFlags_Framed
                                           | ImGuiTreeNodeFlags_SpanAvailWidth
                                           | ImGuiTreeNodeFlags_FramePadding
                                           | ImGuiTreeNodeFlags_AllowOverlap;

        ImGui::PushID(id.c_str());

        const f32 buttonSize = ImGui::GetFrameHeight();
        const f32 headerEnd = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;

        bool open = false;
        {
            const EditorUI::ScopedStyleColor missingColor(ImGuiCol_Text, EditorUI::ErrorColor, missing);
            open = ImGui::TreeNodeEx("##header", flags, "%s  %s", EditorIcons::Glyph(icon), title.c_str());
        }

        const ImVec2 headerMin = ImGui::GetItemRectMin();
        const ImVec2 headerMax = ImGui::GetItemRectMax();
        const f32 halfGap = ImGui::GetStyle().ItemSpacing.y * 0.5f;

        if (removable && ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload(ComponentPayloadType, &section.Key, sizeof(ComponentOrderKey));
            ImGui::TextUnformatted(title.c_str());
            ImGui::EndDragDropSource();
        }

        bool showLineAfterSection = false;

        if (ImGui::BeginDragDropTarget())
        {
            constexpr ImGuiDragDropFlags dropFlags = ImGuiDragDropFlags_AcceptBeforeDelivery
                                                   | ImGuiDragDropFlags_AcceptNoDrawDefaultRect;

            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(ComponentPayloadType, dropFlags);
                payload != nullptr && payload->DataSize == sizeof(ComponentOrderKey))
            {
                ComponentOrderKey moved;
                std::memcpy(&moved, payload->Data, sizeof(ComponentOrderKey));

                const bool after = ImGui::GetMousePos().y > (headerMin.y + headerMax.y) * 0.5f;

                if (CanMoveSection(entity, moved, section.Key, after))
                {
                    if (payload->IsDelivery())
                        MoveSection(entity, moved, section.Key, after);
                    else if (after)
                        showLineAfterSection = true;
                    else
                        EditorUI::DrawInsertionLine(headerMin.y - halfGap, headerMin.x, headerMax.x);
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::OpenPopupOnItemClick("##options", ImGuiPopupFlags_MouseButtonRight);

        ImGui::SameLine(headerEnd - buttonSize);
        if (ImGui::Button("...", ImVec2(buttonSize, buttonSize)))
            ImGui::OpenPopup("##options");

        bool remove = false;
        if (ImGui::BeginPopup("##options"))
        {
            if (ImGui::MenuItem("Reset"))
            {
                if (inspector != nullptr)
                    inspector->Reset(entity);
                else
                    script->Fields.clear();
            }

            if (ImGui::MenuItem("Remove Component", nullptr, false, removable))
                remove = true;

            ImGui::EndPopup();
        }

        if (open)
        {
            if (!remove)
            {
                if (inspector != nullptr)
                    inspector->Draw(entity, context);
                else
                    EditorUI::DrawScriptEntryInspector(entity, *script, context.SelectionContext);
            }
            ImGui::TreePop();
        }

        if (showLineAfterSection)
            EditorUI::DrawInsertionLine(ImGui::GetCursorScreenPos().y - halfGap, headerMin.x, headerMax.x);

        ImGui::PopID();

        if (remove)
        {
            if (inspector != nullptr)
                inspector->Remove(entity);
            else
                EditorUI::RemoveScript(entity, UUID(section.Key.ScriptId));
        }
    }

    void InspectorPanel::DrawPrefabBar(const Entity entity) const
    {
        const auto* link = entity.TryGetComponent<PrefabInstanceComponent>();
        if (link == nullptr) return;

        EditorContext& context = GetContext();

        const UUID prefab = link->Prefab;
        const Entity root = Prefab::GetInstanceRoot(entity);

        AssetMetadata metadata;
        const bool exists = Prefab::Exists(prefab) && AssetRegistry::TryGetMetadata(prefab, metadata);

        constexpr const char* SelectRootLabel = "Select Root";
        constexpr const char* SelectAssetLabel = "Select Asset";
        constexpr const char* UnpackLabel = "Unpack";

        const bool showSelectRoot = root.IsValid() && root != entity;
        const f32 spacing = ImGui::GetStyle().ItemSpacing.x;

        f32 buttonsWidth = EditorUI::ButtonWidth(UnpackLabel);
        if (exists)
            buttonsWidth += EditorUI::ButtonWidth(SelectAssetLabel) + spacing;
        if (showSelectRoot)
            buttonsWidth += EditorUI::ButtonWidth(SelectRootLabel) + spacing;

        const f32 rowEnd = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;

        ImGui::AlignTextToFramePadding();
        if (exists)
        {
            const EditorUI::ScopedStyleColor color(ImGuiCol_Text, EditorColors::Get(EditorColor::PrefabInstance));
            ImGui::Text("%s  %s", EditorIcons::Glyph(EditorIcons::ForFile(metadata.Path)),
                        metadata.Path.stem().string().c_str());
        } else
            EditorUI::ErrorText("Missing prefab");

        if (!root.IsValid() && exists)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("(detached from its instance)");
        }

        ImGui::SameLine(std::max(rowEnd - buttonsWidth, ImGui::GetCursorPosX()));

        if (showSelectRoot)
        {
            if (ImGui::Button(SelectRootLabel))
                context.SelectionContext.Select(root);
            ImGui::SameLine();
        }

        if (exists)
        {
            if (ImGui::Button(SelectAssetLabel))
                context.SelectionContext.SelectAsset(prefab);
            ImGui::SameLine();
        }

        if (ImGui::Button(UnpackLabel))
            context.UnpackPrefab(entity);
        ImGui::SetItemTooltip("Turn the instance back into ordinary entities");
    }

    void InspectorPanel::DrawHeader(const Entity entity)
    {
        auto& tag = entity.GetComponent<TagComponent>();

        const f32 buttonWidth = EditorUI::ButtonWidth("Add Component");

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - buttonWidth - ImGui::GetStyle().ItemSpacing.x);
        ImGui::InputText("##Tag", &tag.Tag);

        ImGui::SameLine();
        if (ImGui::Button("Add Component"))
            ImGui::OpenPopup(AddComponentPopupId);

        DrawAddComponentPopup(entity);
    }

    void InspectorPanel::DrawAddComponentPopup(const Entity entity)
    {
        if (!ImGui::BeginPopup(AddComponentPopupId)) return;

        static std::string s_Filter;
        const bool submitted = EditorUI::PopupSearchField("Search components...", s_Filter, 220.0f);
        ImGui::Separator();

        const std::string_view query = s_Filter;
        bool anyMatch = false;

        for (const ComponentInspector& inspector : GetComponentInspectors())
        {
            if (!inspector.Removable || inspector.Has(entity) || !ContainsIgnoreCase(inspector.Name, query)) continue;

            const bool pickedByEnter = submitted && !anyMatch;
            anyMatch = true;

            const std::string label = std::format("{}  {}", EditorIcons::Glyph(inspector.Icon), inspector.Name);
            if (ImGui::Selectable(label.c_str()) || pickedByEnter)
            {
                inspector.Add(entity);
                ImGui::CloseCurrentPopup();
            }
        }

        bool scriptsHeaderShown = false;
        for (const ScriptClassInfo& info : ScriptEngine::Get().GetClasses())
        {
            if (!ContainsIgnoreCase(info.DisplayName.c_str(), query)) continue;

            if (!scriptsHeaderShown)
            {
                ImGui::SeparatorText("Scripts");
                scriptsHeaderShown = true;
            }

            const bool pickedByEnter = submitted && !anyMatch;
            anyMatch = true;

            ImGui::PushID(info.Name.c_str());

            const std::string label = std::format("{}  {}", EditorIcons::Glyph(EditorIcon::FileScript),
                                                  info.DisplayName);
            if (ImGui::Selectable(label.c_str()) || pickedByEnter)
            {
                EditorUI::AddScript(entity, info.Name);
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            ImGui::TextDisabled("%s", info.Backend.c_str());

            ImGui::PopID();
        }

        if (!anyMatch)
            ImGui::TextDisabled(query.empty() ? "Nothing to add" : "No components match");

        ImGui::EndPopup();
    }
}

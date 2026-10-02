#pragma once

#include "Editor/EditorPanel.h"
#include "Editor/LogFormat.h"
#include "Editor/Commands/EntityEditTracker.h"

#include <Engine/Scene/Entity.h>
#include <Engine/Scene/UUID.h>

#include <imgui.h>

#include <type_traits>

namespace ByteForge
{
    struct AssetMetadata;

    class InspectorPanel : public EditorPanel
    {
    public:
        explicit InspectorPanel(EditorContext& context)
            : EditorPanel(context, "Inspector") {}

        void OnImGuiRender() override;

    private:
        void DrawComponents(Entity entity) const;
        static void DrawHeader(Entity entity);
        static void DrawAddComponentPopup(Entity entity);

        template<typename T, typename UIFunction>
        static void DrawComponent(const char* name, const Entity entity, UIFunction uiFunction,
                                  const bool removable = true)
        {
            if (!entity.HasComponent<T>()) return;

            constexpr ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen
                                               | ImGuiTreeNodeFlags_Framed
                                               | ImGuiTreeNodeFlags_SpanAvailWidth
                                               | ImGuiTreeNodeFlags_FramePadding
                                               | ImGuiTreeNodeFlags_AllowOverlap;

            ImGui::PushID(name);

            const float buttonSize = ImGui::GetFrameHeight();
            const float headerEnd = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;

            const bool open = ImGui::TreeNodeEx("##header", flags, "%s", name);
            ImGui::OpenPopupOnItemClick("##options", ImGuiPopupFlags_MouseButtonRight);

            ImGui::SameLine(headerEnd - buttonSize);
            if (ImGui::Button("...", ImVec2(buttonSize, buttonSize)))
                ImGui::OpenPopup("##options");

            bool remove = false;
            if (ImGui::BeginPopup("##options"))
            {
                if (ImGui::MenuItem("Reset"))
                {
                    if constexpr (std::is_same_v<T, ScriptComponent>)
                        entity.GetComponent<T>().Fields.clear();
                    else
                        entity.GetComponent<T>() = T{};
                }

                if (ImGui::MenuItem("Remove Component", nullptr, false, removable))
                    remove = true;

                ImGui::EndPopup();
            }

            if (open)
            {
                if (!remove)
                    uiFunction(entity.GetComponent<T>());
                ImGui::TreePop();
            }

            ImGui::PopID();

            if (remove)
                entity.RemoveComponent<T>();
        }

        void DrawAsset(UUID handle) const;
        void DrawTextureSettings(const AssetMetadata& metadata) const;

        static void DrawLogEntry(const LogEntry& entry);

    private:
        EntityEditTracker m_EditTracker;
    };
}

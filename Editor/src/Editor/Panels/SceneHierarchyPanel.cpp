#include "Editor/Panels/SceneHierarchyPanel.h"
#include "Editor/EditorContext.h"

#include <Engine/Scene/Components.h>
#include <Engine/Scene/Entity.h>
#include <Engine/Scene/Scene.h>

#include <imgui.h>

namespace ByteForge
{
    void SceneHierarchyPanel::OnImGuiRender()
    {
        if (!m_Open) return;
        if (!ImGui::Begin(GetName().c_str(), &m_Open))
        {
            ImGui::End();
            return;
        }

        EditorContext& context = GetContext();

        if (context.ActiveScene != nullptr)
        {
            context.ActiveScene->Each<TagComponent>([&context](const Entity entity, TagComponent& tag)
            {
                const bool selected = entity == context.SelectionContext;

                const auto uuid = static_cast<uint64_t>(entity.GetUUID());
                ImGui::PushID(reinterpret_cast<void*>(uuid));
                if (ImGui::Selectable(tag.Tag.c_str(), selected))
                    context.SelectionContext = entity;
                ImGui::PopID();
            });
        }

        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered())
            context.SelectionContext = {};

        ImGui::End();
    }
}

#include "Editor/Panels/SceneHierarchyPanel.h"
#include "Editor/EditorContext.h"

#include <Engine/Scene/Components.h>
#include <Engine/Scene/Scene.h>

#include <imgui.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <format>
#include <string>

namespace ByteForge
{
    namespace
    {
        constexpr const char* CreatePopupId = "##CreateEntity";

        void AddBox(const Entity entity, const Rigidbody2DComponent::BodyType type)
        {
            entity.AddComponent<SpriteRendererComponent>();
            entity.AddComponent<Rigidbody2DComponent>().Type = type;
            entity.AddComponent<BoxCollider2DComponent>();
        }
    }

    void SceneHierarchyPanel::OnImGuiRender()
    {
        if (!m_Open) return;
        if (!ImGui::Begin(GetName().c_str(), &m_Open))
        {
            ImGui::End();
            return;
        }

        EditorContext& context = GetContext();
        if (context.ActiveScene == nullptr)
        {
            ImGui::TextDisabled("No active scene");
            ImGui::End();
            return;
        }

        if (ImGui::Button("+ Create"))
            ImGui::OpenPopup(CreatePopupId);
        if (ImGui::BeginPopup(CreatePopupId))
        {
            DrawCreateMenu();
            ImGui::EndPopup();
        }

        ImGui::Separator();

        context.ActiveScene->Each<TagComponent>([this](const Entity entity, TagComponent& tag)
        {
            DrawEntity(entity, tag);
        });

        constexpr ImGuiPopupFlags contextFlags = ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems;
        if (ImGui::BeginPopupContextWindow("##HierarchyContext", contextFlags))
        {
            DrawCreateMenu();
            ImGui::EndPopup();
        }

        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered())
            context.SelectionContext.Clear();

        HandleShortcuts();

        ImGui::End();
    }

    void SceneHierarchyPanel::DrawEntity(const Entity entity, TagComponent& tag)
    {
        EditorContext& context = GetContext();

        const auto uuid = static_cast<uint64_t>(entity.GetUUID());
        ImGui::PushID(reinterpret_cast<void*>(static_cast<uintptr_t>(uuid)));

        if (entity == m_RenameTarget)
        {
            DrawRenameField(tag);
            ImGui::PopID();
            return;
        }

        const std::string label = std::format("{}###entity", tag.Tag);
        if (ImGui::Selectable(label.c_str(), context.SelectionContext.IsEntity(entity)))
            context.SelectionContext.Select(entity);

        if (ImGui::BeginPopupContextItem("##EntityContext"))
        {
            if (ImGui::IsWindowAppearing())
                context.SelectionContext.Select(entity);

            if (ImGui::MenuItem("Rename", "F2"))
                BeginRename(entity);
            if (ImGui::MenuItem("Duplicate", "Ctrl+D"))
                context.DuplicateEntity(entity);

            ImGui::Separator();

            if (ImGui::MenuItem("Delete", "Del"))
                context.DestroyEntity(entity);

            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    void SceneHierarchyPanel::DrawCreateMenu() const
    {
        EditorContext& context = GetContext();

        if (ImGui::MenuItem("Empty Entity"))
            context.CreateEntity("Entity");

        if (ImGui::MenuItem("Sprite"))
            context.CreateEntity("Sprite", [](const Entity entity) { entity.AddComponent<SpriteRendererComponent>(); });

        if (ImGui::BeginMenu("Physics"))
        {
            if (ImGui::MenuItem("Static Box"))
            {
                context.CreateEntity("Static Box", [](const Entity entity)
                {
                    AddBox(entity, Rigidbody2DComponent::BodyType::Static);
                });
            }

            if (ImGui::MenuItem("Dynamic Box"))
            {
                context.CreateEntity("Dynamic Box", [](const Entity entity)
                {
                    AddBox(entity, Rigidbody2DComponent::BodyType::Dynamic);
                });
            }

            ImGui::EndMenu();
        }
    }

    void SceneHierarchyPanel::HandleShortcuts()
    {
        EditorContext& context = GetContext();
        const Entity selected = context.SelectionContext.GetEntity();
        if (!selected.IsValid() || m_RenameTarget.IsValid()) return;

        if (ImGui::Shortcut(ImGuiKey_Delete))
            context.DestroyEntity(selected);
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_D))
            context.DuplicateEntity(selected);
        if (ImGui::Shortcut(ImGuiKey_F2))
            BeginRename(selected);
    }

    void SceneHierarchyPanel::BeginRename(const Entity entity)
    {
        const std::string& tag = entity.GetTag();
        const size_t length = std::min(tag.size(), m_RenameBuffer.size() - 1);
        std::memcpy(m_RenameBuffer.data(), tag.data(), length);
        m_RenameBuffer[length] = '\0';

        m_RenameTarget = entity;
        m_FocusRenameField = true;
    }

    void SceneHierarchyPanel::DrawRenameField(TagComponent& tag)
    {
        if (m_FocusRenameField)
        {
            ImGui::SetKeyboardFocusHere();
            m_FocusRenameField = false;
        }

        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText("##rename", m_RenameBuffer.data(), m_RenameBuffer.size(), ImGuiInputTextFlags_AutoSelectAll);

        if (ImGui::IsItemDeactivated())
        {
            if (!ImGui::IsKeyPressed(ImGuiKey_Escape) && m_RenameBuffer[0] != '\0')
                tag.Tag = m_RenameBuffer.data();

            m_RenameTarget = {};
        }
    }
}

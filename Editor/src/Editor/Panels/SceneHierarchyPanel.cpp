#include "Editor/Panels/SceneHierarchyPanel.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorWidgets.h"

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
        constexpr f32 MinDropZoneHeight = 24.0f;

        void AddBox(const Entity entity, const Rigidbody2DComponent::BodyType type)
        {
            entity.AddComponent<SpriteRendererComponent>();
            entity.AddComponent<Rigidbody2DComponent>().Type = type;
            entity.AddComponent<BoxCollider2DComponent>();
        }

        Entity AcceptEntityDrop(Scene& scene)
        {
            Entity dropped;

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(EditorUI::EntityPayloadType);
                    payload != nullptr && payload->DataSize == sizeof(u64))
                {
                    dropped = scene.FindEntityByUUID(UUID(*static_cast<const u64*>(payload->Data)));
                }

                ImGui::EndDragDropTarget();
            }

            return dropped;
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

        for (const Entity root : context.ActiveScene->GetRootEntities())
            DrawEntityNode(root);

        DrawRootDropZone();

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

    void SceneHierarchyPanel::DrawEntityNode(const Entity entity)
    {
        EditorContext& context = GetContext();
        Scene& scene = *context.ActiveScene;

        const auto uuid = static_cast<u64>(entity.GetUUID());
        ImGui::PushID(reinterpret_cast<void*>(static_cast<uintptr_t>(uuid)));

        if (entity == m_RenameTarget)
        {
            DrawRenameField();
            ImGui::PopID();
            return;
        }

        const std::vector<Entity> children = entity.GetChildren();

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (children.empty())
            flags |= ImGuiTreeNodeFlags_Leaf;
        if (context.SelectionContext.IsEntity(entity))
            flags |= ImGuiTreeNodeFlags_Selected;

        const std::string label = std::format("{}###entity", entity.GetTag());
        const bool open = ImGui::TreeNodeEx(label.c_str(), flags);

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
            context.SelectionContext.Select(entity);

        if (ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload(EditorUI::EntityPayloadType, &uuid, sizeof(uuid));
            ImGui::TextUnformatted(entity.GetTag().c_str());
            ImGui::EndDragDropSource();
        }

        if (const Entity dropped = AcceptEntityDrop(scene); dropped.IsValid())
            context.ReparentEntity(dropped, entity);

        if (ImGui::BeginPopupContextItem("##EntityContext"))
        {
            if (ImGui::IsWindowAppearing())
                context.SelectionContext.Select(entity);

            if (ImGui::MenuItem("Rename", "F2"))
                BeginRename(entity);
            if (ImGui::MenuItem("Duplicate", "Ctrl+D"))
                context.DuplicateEntity(entity);

            ImGui::Separator();

            if (ImGui::MenuItem("Create Empty Child"))
            {
                context.CreateEntity("Entity", [entity](const Entity child) { child.SetParent(entity, false); });
            }

            if (ImGui::MenuItem("Unparent", nullptr, false, entity.HasParent()))
                context.ReparentEntity(entity, {});

            ImGui::Separator();

            if (ImGui::MenuItem("Delete", "Del"))
                context.DestroyEntity(entity);

            ImGui::EndPopup();
        }

        if (open)
        {
            for (const Entity child : children)
                DrawEntityNode(child);

            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    void SceneHierarchyPanel::DrawRootDropZone() const
    {
        EditorContext& context = GetContext();

        ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x,
                            std::max(ImGui::GetContentRegionAvail().y, MinDropZoneHeight)));

        if (const Entity dropped = AcceptEntityDrop(*context.ActiveScene); dropped.IsValid())
            context.ReparentEntity(dropped, {});
    }

    void SceneHierarchyPanel::DrawCreateMenu() const
    {
        EditorContext& context = GetContext();

        if (ImGui::MenuItem("Empty Entity"))
            context.CreateEntity("Entity");

        if (ImGui::MenuItem("Sprite"))
            context.CreateEntity("Sprite", [](const Entity entity) { entity.AddComponent<SpriteRendererComponent>(); });

        if (ImGui::MenuItem("Camera"))
            context.CreateEntity("Camera", [](const Entity entity) { entity.AddComponent<CameraComponent>(); });

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
        const usize length = std::min(tag.size(), m_RenameBuffer.size() - 1);
        std::memcpy(m_RenameBuffer.data(), tag.data(), length);
        m_RenameBuffer[length] = '\0';

        m_RenameTarget = entity;
        m_FocusRenameField = true;
    }

    void SceneHierarchyPanel::DrawRenameField()
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
            if (!ImGui::IsKeyPressed(ImGuiKey_Escape))
                GetContext().RenameEntity(m_RenameTarget, m_RenameBuffer.data());

            m_RenameTarget = {};
        }
    }
}

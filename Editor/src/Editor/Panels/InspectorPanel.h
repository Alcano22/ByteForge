#pragma once

#include "Editor/EditorPanel.h"

#include <Engine/Scene/Entity.h>
#include <Engine/Scene/UUID.h>

#include <imgui.h>

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
        static void DrawComponents(Entity entity);

        template<typename T, typename UIFunction>
        static void DrawComponent(const char* name, Entity entity, UIFunction uiFunction)
        {
            if (!entity.HasComponent<T>()) return;

            constexpr ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen
                                               | ImGuiTreeNodeFlags_Framed
                                               | ImGuiTreeNodeFlags_SpanAvailWidth;

            ImGui::PushID(name);
            const bool open = ImGui::TreeNodeEx(name, flags);
            ImGui::PopID();

            if (open)
            {
                uiFunction(entity.GetComponent<T>());
                ImGui::TreePop();
            }
        }

        void DrawAsset(UUID handle) const;
        void DrawTextureSettings(const AssetMetadata& metadata) const;
    };
}

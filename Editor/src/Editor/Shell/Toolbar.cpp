#include "Editor/Shell/Toolbar.h"
#include "Editor/EditorActions.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorWidgets.h"

#include <imgui.h>

#include <format>
#include <string>

namespace ByteForge
{
    namespace
    {
        constexpr f32 BuildStatusWidth = 200.0f;

        void ActionButton(EditorContext& context, const char* id, const EditorAction& action,
                          const EditorIcon icon, const bool active)
        {
            ImGui::BeginDisabled(!action.IsAvailable(context));
            const bool pressed = EditorUI::IconButton(id, context.Icons.Get(icon), active);
            ImGui::EndDisabled();

            ImGui::SetItemTooltip("%s", action.GetTooltip(context).c_str());

            if (pressed)
                action.TryExecute(context);
        }

        void DrawScriptBuildStatus(const EditorContext& context)
        {
            const ScriptProject& scripts = context.Scripts;
            if (!scripts.IsBuilding()) return;

            const ImGuiStyle& style = ImGui::GetStyle();
            ImGui::SameLine(ImGui::GetWindowWidth() - BuildStatusWidth - style.ItemSpacing.x * 2.0f);

            const std::string overlay = std::format("Compiling scripts {:.1f}s", scripts.GetBuildSeconds());

            ImGui::ProgressBar(-1.0f * static_cast<f32>(ImGui::GetTime()),
                               ImVec2(BuildStatusWidth, ImGui::GetFrameHeight()), overlay.c_str());
        }

        void DrawPrefabBreadcrumb(EditorContext& context)
        {
            if (!context.Document.IsEditingPrefab()) return;

            const std::string back = std::format("< {}", context.Document.GetSceneName());
            if (ImGui::Button(back.c_str()))
                context.Dialogs.RequestClosePrefab();
            ImGui::SetItemTooltip("Back to the scene (its instances of this prefab are updated)");

            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            {
                const EditorUI::ScopedStyleColor color(ImGuiCol_Text, EditorColors::Get(EditorColor::PrefabInstance));
                ImGui::Text("Prefab: %s%s", context.Document.GetName().c_str(), context.Document.IsDirty() ? "*" : "");
            }

            ImGui::SameLine();
        }
    }

    void DrawToolbar(EditorContext& context)
    {
        ImGui::Separator();
        DrawPrefabBreadcrumb(context);

        const ImGuiStyle& style = ImGui::GetStyle();
        const f32 buttonWidth = ImGui::GetTextLineHeight() + style.FramePadding.x * 2.0f;
        constexpr f32 buttonCount = 3.0f;
        const f32 groupWidth = buttonWidth * buttonCount + style.ItemSpacing.x * (buttonCount - 1.0f);
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - groupWidth) * 0.5f);

        const bool editing = context.IsEditing();

        ActionButton(context, "##playStop", EditorActions::TogglePlay,
                     editing ? EditorIcon::PlayerPlay : EditorIcon::PlayerStop, !editing);
        ImGui::SameLine();
        ActionButton(context, "##pause", EditorActions::TogglePause, EditorIcon::PlayerPause, context.IsPaused());
        ImGui::SameLine();
        ActionButton(context, "##step", EditorActions::Step, EditorIcon::PlayerStep, false);

        DrawScriptBuildStatus(context);
    }
}

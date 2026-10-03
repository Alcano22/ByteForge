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
        constexpr float BuildStatusWidth = 200.0f;

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

            ImGui::ProgressBar(-1.0f * static_cast<float>(ImGui::GetTime()),
                               ImVec2(BuildStatusWidth, ImGui::GetFrameHeight()), overlay.c_str());
        }
    }

    void DrawToolbar(EditorContext& context)
    {
        ImGui::Separator();

        const ImGuiStyle& style = ImGui::GetStyle();
        const float buttonWidth = ImGui::GetTextLineHeight() + style.FramePadding.x * 2.0f;
        constexpr float buttonCount = 3.0f;
        const float groupWidth = buttonWidth * buttonCount + style.ItemSpacing.x * (buttonCount - 1.0f);
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

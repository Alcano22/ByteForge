#include "Editor/Shell/EditorShell.h"
#include "Editor/Shell/MainMenuBar.h"
#include "Editor/Shell/Toolbar.h"
#include "Editor/EditorActions.h"
#include "Editor/EditorContext.h"

#include <imgui.h>

namespace ByteForge
{
    void DrawEditorShell(EditorContext& context)
    {
        constexpr ImGuiWindowFlags hostFlags = ImGuiWindowFlags_MenuBar
                                             | ImGuiWindowFlags_NoDocking
                                             | ImGuiWindowFlags_NoTitleBar
                                             | ImGuiWindowFlags_NoCollapse
                                             | ImGuiWindowFlags_NoResize
                                             | ImGuiWindowFlags_NoMove
                                             | ImGuiWindowFlags_NoBringToFrontOnFocus
                                             | ImGuiWindowFlags_NoNavFocus;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGui::Begin("DockSpaceHost", nullptr, hostFlags);
        ImGui::PopStyleVar(3);

        EditorActions::HandleShortcuts(context);

        DrawMainMenuBar(context);
        DrawToolbar(context);

        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable)
            ImGui::DockSpace(ImGui::GetID("EditorDockSpace"));

        context.Dialogs.Draw();

        ImGui::End();
    }
}

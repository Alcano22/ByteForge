#include "Editor/EditorLayouts.h"
#include "Editor/EditorContext.h"
#include "Editor/Panels/AssetsPanel.h"
#include "Editor/Panels/ConsolePanel.h"
#include "Editor/Panels/GamePanel.h"
#include "Editor/Panels/InspectorPanel.h"
#include "Editor/Panels/SceneHierarchyPanel.h"
#include "Editor/Panels/ScenePanel.h"
#include "Editor/Panels/GraphEditorPanel.h"

#include <Engine/Core/Log.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string_view>
#include <system_error>

namespace ByteForge
{
    namespace
    {
        constexpr const char* SceneWindow     = ScenePanel::Name;
        constexpr const char* GameWindow      = GamePanel::Name;
        constexpr const char* HierarchyWindow = SceneHierarchyPanel::Name;
        constexpr const char* InspectorWindow = InspectorPanel::Name;
        constexpr const char* AssetsWindow    = AssetsPanel::Name;
        constexpr const char* ConsoleWindow   = ConsolePanel::Name;
        constexpr const char* GraphWindow     = GraphEditorPanel::WindowId;

        constexpr const char* LayoutExtension = ".ini";

        fs::path GetStateDirectory() { return fs::current_path() / ".byteforge"; }
        fs::path GetLayoutDirectory() { return GetStateDirectory() / "layouts"; }
        fs::path GetLayoutPath(const std::string& name) { return GetLayoutDirectory() / (name + LayoutExtension); }

        std::string MakeLayoutName(const std::string_view text)
        {
            std::string name(text);
            for (char& c : name)
            {
                if (std::string_view("/\\:*?\"<>|").find(c) != std::string_view::npos ||
                    static_cast<unsigned char>(c) < 32)
                    c = '_';
            }

            const usize first = name.find_first_not_of(" .");
            if (first == std::string::npos)
                return {};

            const usize last = name.find_last_not_of(" .");
            return name.substr(first, last - first + 1);
        }

        const char* GetPresetName(const LayoutPreset preset)
        {
            for (const LayoutPresetInfo& info : LayoutPresets)
            {
                if (info.Preset == preset)
                    return info.Name;
            }
            return "Unknown";
        }

        struct Areas
        {
            ImGuiID Left   = 0;
            ImGuiID Right  = 0;
            ImGuiID Bottom = 0;
            ImGuiID Center = 0;
        };

        Areas SplitEditor(const ImGuiID root, const f32 left, const f32 right, const f32 bottom)
        {
            Areas areas;
            areas.Center = root;

            ImGui::DockBuilderSplitNode(areas.Center, ImGuiDir_Left,  left,   &areas.Left,   &areas.Center);
            ImGui::DockBuilderSplitNode(areas.Center, ImGuiDir_Right, right,  &areas.Right,  &areas.Center);
            ImGui::DockBuilderSplitNode(areas.Center, ImGuiDir_Down,  bottom, &areas.Bottom, &areas.Center);
            return areas;
        }

        void BuildDefault(const ImGuiID root)
        {
            const Areas areas = SplitEditor(root, 0.18f, 0.26f, 0.30f);
            ImGui::DockBuilderDockWindow(HierarchyWindow, areas.Left);
            ImGui::DockBuilderDockWindow(InspectorWindow, areas.Right);
            ImGui::DockBuilderDockWindow(AssetsWindow,    areas.Bottom);
            ImGui::DockBuilderDockWindow(ConsoleWindow,   areas.Bottom);
            ImGui::DockBuilderDockWindow(SceneWindow,     areas.Center);
            ImGui::DockBuilderDockWindow(GameWindow,      areas.Center);
        }

        void BuildSideBySide(const ImGuiID root)
        {
            Areas areas = SplitEditor(root, 0.16f, 0.22f, 0.25f);

            ImGuiID game = 0;
            ImGui::DockBuilderSplitNode(areas.Center, ImGuiDir_Right, 0.5f, &game, &areas.Center);

            ImGui::DockBuilderDockWindow(HierarchyWindow, areas.Left);
            ImGui::DockBuilderDockWindow(InspectorWindow, areas.Right);
            ImGui::DockBuilderDockWindow(AssetsWindow,    areas.Bottom);
            ImGui::DockBuilderDockWindow(ConsoleWindow,   areas.Bottom);
            ImGui::DockBuilderDockWindow(SceneWindow,     areas.Center);
            ImGui::DockBuilderDockWindow(GameWindow,      game);
        }

        void BuildScripting(const ImGuiID root)
        {
            Areas areas = SplitEditor(root, 0.18f, 0.30f, 0.25f);

            ImGuiID leftBottom = 0;
            ImGui::DockBuilderSplitNode(areas.Left, ImGuiDir_Down, 0.5f, &leftBottom, &areas.Left);

            ImGuiID rightBottom = 0;
            ImGui::DockBuilderSplitNode(areas.Right, ImGuiDir_Down, 0.5f, &rightBottom, &areas.Right);

            ImGui::DockBuilderDockWindow(HierarchyWindow, areas.Left);
            ImGui::DockBuilderDockWindow(AssetsWindow,    leftBottom);
            ImGui::DockBuilderDockWindow(InspectorWindow, areas.Right);
            ImGui::DockBuilderDockWindow(SceneWindow,     rightBottom);
            ImGui::DockBuilderDockWindow(GameWindow,      rightBottom);
            ImGui::DockBuilderDockWindow(ConsoleWindow,   areas.Bottom);
            ImGui::DockBuilderDockWindow(GraphWindow,     areas.Center);
        }

        void Build(const LayoutPreset preset, const ImGuiID root, const ImVec2 size)
        {
            ImGui::DockBuilderRemoveNode(root);
            ImGui::DockBuilderAddNode(root, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(root, size);

            switch (preset)
            {
                case LayoutPreset::Default:    BuildDefault(root);    break;
                case LayoutPreset::SideBySide: BuildSideBySide(root); break;
                case LayoutPreset::Scripting:  BuildScripting(root);  break;
            }

            ImGui::DockBuilderFinish(root);
        }
    }

    void EditorLayouts::Init()
    {
        std::error_code error;
        fs::create_directories(GetLayoutDirectory(), error);

        m_IniPath = (GetStateDirectory() / "imgui.ini").string();
        ImGui::GetIO().IniFilename = m_IniPath.c_str();

        if (!fs::exists(m_IniPath))
            m_PendingPreset = LayoutPreset::Default;
    }

    void EditorLayouts::Update()
    {
        if (m_PendingIni.empty()) return;

        ImGui::LoadIniSettingsFromMemory(m_PendingIni.c_str(), m_PendingIni.size());
        ImGui::MarkIniSettingsDirty();
        m_PendingIni.clear();
    }

    void EditorLayouts::ApplyPendingPreset(EditorContext& context, const ImGuiID dockSpaceId, const ImVec2 size)
    {
        if (!m_PendingPreset) return;

        const LayoutPreset preset = *m_PendingPreset;
        m_PendingPreset.reset();

        Build(preset, dockSpaceId, size);

        context.Open<ScenePanel>();
        context.Open<GamePanel>();
        context.Open<SceneHierarchyPanel>();
        context.Open<InspectorPanel>();
        context.Open<AssetsPanel>();
        context.Open<ConsolePanel>();

        if (preset == LayoutPreset::Scripting)
            context.Open<GraphEditorPanel>();

        APP_INFO("Applied layout '{}'", GetPresetName(preset));
    }

    void EditorLayouts::OpenSaveDialog(PopupManager& popups)
    {
        std::array<char, 64> nameBuffer{};
        std::snprintf(nameBuffer.data(), nameBuffer.size(), "%s", "My Layout");

        popups.Open("Save Layout", [this, nameBuffer](Popup& popup) mutable
        {
            if (ImGui::IsWindowAppearing())
                ImGui::SetKeyboardFocusHere();

            const bool submitted = ImGui::InputTextWithHint("##name", "Layout name", nameBuffer.data(),
                                                            nameBuffer.size(), ImGuiInputTextFlags_EnterReturnsTrue);

            const std::string name = MakeLayoutName(nameBuffer.data());
            const bool valid = !name.empty();

            if (valid && fs::exists(GetLayoutPath(name)))
                ImGui::TextDisabled("'%s' will be overwritten", name.c_str());

            ImGui::BeginDisabled(!valid);
            if (ImGui::Button("Save") || (submitted && valid))
            {
                SaveUserLayout(name);
                popup.Close();
            }
            ImGui::EndDisabled();

            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                popup.Close();
        });
    }

    void EditorLayouts::RequestPreset(const LayoutPreset preset)
    {
        m_PendingIni.clear();
        m_PendingPreset = preset;
    }

    bool EditorLayouts::RequestUserLayout(const std::string& name)
    {
        std::ifstream file(GetLayoutPath(name), std::ios::binary);
        if (!file.is_open())
        {
            APP_ERROR("Cannot open layout '{}'", name);
            return false;
        }

        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if (content.empty())
        {
            APP_ERROR("Layout '{}' is empty", name);
            return false;
        }

        m_PendingPreset.reset();
        m_PendingIni = std::move(content);

        APP_INFO("Loaded layout '{}'", name);
        return true;
    }

    bool EditorLayouts::SaveUserLayout(const std::string& name) const
    {
        const fs::path path = GetLayoutPath(name);

        std::error_code error;
        fs::create_directories(path.parent_path(), error);

        usize size = 0;
        const char* data = ImGui::SaveIniSettingsToMemory(&size);

        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file.is_open())
        {
            APP_ERROR("Cannot write layout '{}'", path.generic_string());
            return false;
        }

        file.write(data, static_cast<std::streamsize>(size));

        APP_INFO("Saved layout '{}'", name);
        return true;
    }

    bool EditorLayouts::DeleteUserLayout(const std::string& name) const
    {
        std::error_code error;
        if (!fs::remove(GetLayoutPath(name), error) || error)
        {
            APP_ERROR("Cannot delete layout '{}'", name);
            return false;
        }

        APP_INFO("Deleted layout '{}'", name);
        return true;
    }

    std::vector<std::string> EditorLayouts::GetUserLayouts()
    {
        std::vector<std::string> names;

        std::error_code error;
        for (fs::directory_iterator it(GetLayoutDirectory(), error), end; !error && it != end; it.increment(error))
        {
            if (it->is_regular_file() && it->path().extension() == LayoutExtension)
                names.push_back(it->path().stem().string());
        }

        std::ranges::sort(names);
        return names;
    }
}

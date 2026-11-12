#pragma once

#include "Editor/PopupManager.h"

#include <imgui.h>

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace ByteForge
{
    class EditorContext;

    enum class LayoutPreset
    {
        Default,
        SideBySide,
        Scripting
    };

    struct LayoutPresetInfo
    {
        LayoutPreset Preset;
        const char* Name;
    };

    inline constexpr std::array<LayoutPresetInfo, 3> LayoutPresets{{
        { LayoutPreset::Default,    "Default"      },
        { LayoutPreset::SideBySide, "Side by Side" },
        { LayoutPreset::Scripting,  "Scripting"    }
    }};

    class EditorLayouts
    {
    public:
        void Init();

        void Update();

        void ApplyPendingPreset(EditorContext& context, ImGuiID dockSpaceId, ImVec2 size);

        void OpenSaveDialog(PopupManager& popups);

        void RequestPreset(LayoutPreset preset);
        bool RequestUserLayout(const std::string& name);

        bool SaveUserLayout(const std::string& name) const;
        bool DeleteUserLayout(const std::string& name) const;

        [[nodiscard]] static std::vector<std::string> GetUserLayouts();

    private:
        std::string m_IniPath;

        std::optional<LayoutPreset> m_PendingPreset;
        std::string m_PendingIni;
    };
}

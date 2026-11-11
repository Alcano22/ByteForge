#pragma once

#include "Editor/EditorPanel.h"

#include <Engine/Scene/UUID.h>

#include <array>
#include <filesystem>
#include <string>

namespace ByteForge
{
    class ProjectSettingsPanel : public EditorPanel
    {
    public:
        explicit ProjectSettingsPanel(EditorContext& context);

        void OnImGuiRender() override;

    private:
        void Reload();
        void Apply();

        [[nodiscard]] static std::filesystem::path GetFile();
        [[nodiscard]] static std::filesystem::file_time_type ReadFileTime();

    private:
        std::array<char, 128> m_Name{};
        std::array<char, 128> m_WindowTitle{};
        std::array<i32, 2> m_WindowSize{ 1280, 720 };
        UUID m_StartScene{ 0 };

        std::string m_Error;
        std::filesystem::file_time_type m_FileTime{};
        bool m_Loaded = false;
        bool m_Dirty = false;
    };
}

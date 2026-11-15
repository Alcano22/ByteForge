#pragma once

#include "Editor/Settings/SettingsPage.h"

#include <Engine/Core/Core.h>
#include <Engine/Scene/UUID.h>

#include <array>
#include <filesystem>
#include <string>

namespace ByteForge
{
    class ProjectGeneralPage final : public SettingsPage
    {
    public:
        [[nodiscard]] const char* GetName() const override { return "General"; }
        [[nodiscard]] SettingsGroup GetGroup() const override { return SettingsGroup::Project; }
        [[nodiscard]] bool HasUnsavedChanges() const override { return m_Dirty; }

        void Draw(EditorContext& context) override;

    private:
        void Reload();
        void Apply();

        [[nodiscard]] static fs::path GetFile();
        [[nodiscard]] static fs::file_time_type ReadFileTime();

    private:
        std::array<char, 128> m_Name{};
        std::array<char, 128> m_WindowTitle{};
        std::array<i32, 2> m_WindowSize{ 1280, 720 };
        UUID m_StartScene{ 0 };

        std::string m_Error;
        fs::file_time_type m_FileTime{};
        bool m_Loaded = false;
        bool m_Dirty = false;
    };
}

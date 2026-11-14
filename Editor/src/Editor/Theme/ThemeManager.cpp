#include "Editor/Theme/ThemeManager.h"
#include "Editor/Theme/ThemePresets.h"
#include "Editor/EditorPaths.h"

#include <Engine/Core/FileSystem.h>
#include <Engine/Core/Log.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <system_error>

namespace ByteForge
{
    namespace
    {
        constexpr const char* ThemeExtension = ".json";

        fs::path GetThemeDirectory() { return GetStateDirectory() / "themes"; }
        fs::path GetActiveThemePath() { return GetStateDirectory() / "theme.json"; }

        std::optional<nlohmann::json> ReadJson(const fs::path& file)
        {
            std::ifstream stream(file);
            if (!stream.is_open())
                return std::nullopt;

            nlohmann::json json = nlohmann::json::parse(stream, nullptr, false);
            if (json.is_discarded())
                return std::nullopt;

            return json;
        }
    }

    void ThemeManager::Init()
    {
        std::error_code error;
        fs::create_directories(GetThemeDirectory(), error);

        Reload();

        std::string wanted = DefaultThemeName;
        if (const auto saved = ReadJson(GetActiveThemePath()); saved && saved->is_object())
            wanted = saved->value("active", wanted);

        const Theme* theme = Find(wanted);
        if (theme == nullptr)
        {
            APP_ERROR("Saved theme '{}' does not exist, using '{}'", wanted, DefaultThemeName);
            theme = Find(DefaultThemeName);
        }

        Activate(*theme);
    }

    void ThemeManager::Reload()
    {
        m_Themes = CreateBuiltInThemes();
        LoadUserThemes();

        if (m_ActiveName.empty()) return;

        const Theme* active = Find(m_ActiveName);
        if (active == nullptr)
        {
            APP_ERROR("Theme '{}' no longer exists, using '{}'", m_ActiveName, DefaultThemeName);
            active = Find(DefaultThemeName);
        }

        Activate(*active);
    }

    bool ThemeManager::SetActive(const std::string_view name)
    {
        const Theme* theme = Find(name);
        if (theme == nullptr)
        {
            APP_ERROR("There is no theme named '{}'", std::string(name));
            return false;
        }

        Activate(*theme);
        SaveActiveName();
        return true;
    }

    const Theme* ThemeManager::Find(const std::string_view name) const
    {
        const auto it = std::ranges::find(m_Themes, name, &Theme::Name);
        return it != m_Themes.end() ? &*it : nullptr;
    }

    void ThemeManager::LoadUserThemes()
    {
        std::vector<fs::path> files;

        std::error_code error;
        for (fs::directory_iterator it(GetThemeDirectory(), error), end; !error && it != end;
             it.increment(error))
        {
            if (it->is_regular_file() && it->path().extension() == ThemeExtension)
                files.push_back(it->path());
        }

        std::ranges::sort(files);

        const Theme base = *Find(DefaultThemeName);

        for (const fs::path& file : files)
        {
            const std::string fileName = file.filename().generic_string();

            const auto json = ReadJson(file);
            if (!json)
            {
                APP_ERROR("Cannot read theme file '{}'", fileName);
                continue;
            }

            auto theme = Theme::FromJson(*json, file.stem().string(), base);
            if (!theme)
            {
                APP_ERROR("Theme file '{}' is not a valid theme", fileName);
                continue;
            }

            if (Find(theme->Name) != nullptr)
            {
                APP_ERROR("Skipping theme file '{}': a theme named '{}' already exists", fileName, theme->Name);
                continue;
            }

            m_Themes.push_back(std::move(*theme));
        }
    }

    void ThemeManager::Activate(const Theme& theme)
    {
        theme.Apply();
        m_ActiveName = theme.Name;
    }

    void ThemeManager::SaveActiveName() const
    {
        const nlohmann::json json{ { "active", m_ActiveName } };

        if (const auto written = WriteJsonFile(GetActiveThemePath(), json); !written)
            APP_ERROR("Cannot save the active theme: {}", written.error());
    }
}

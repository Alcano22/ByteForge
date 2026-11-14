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

        fs::path GetThemeFile(const std::string_view name)
        {
            return GetThemeDirectory() / (std::string(name) + ThemeExtension);
        }

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

    Theme* ThemeManager::FindEditable(const std::string_view name)
    {
        const auto it = std::ranges::find(m_Themes, name, &Theme::Name);
        return it != m_Themes.end() && !it->BuiltIn ? &*it : nullptr;
    }

    void ThemeManager::MarkEdited(Theme& theme)
    {
        theme.Modified = true;

        if (theme.Name == m_ActiveName)
            theme.Apply();
    }

    const char* ThemeManager::GetNameError(const std::string_view name) const
    {
        if (name.empty())
            return "Enter a name";

        if (name.find_first_of("\\/:*?\"<>|") != std::string_view::npos)
            return "The name must not contain \\ / : * ? \" < > |";

        if (name.front() == ' ' || name.back() == ' ' || name.back() == '.')
            return "The name must not start or end with a space or end with a dot";

        if (Find(name) != nullptr)
            return "A theme with this name already exists";

        std::error_code error;
        if (fs::exists(GetThemeFile(name), error))
            return "A theme file with this name already exists";

        return nullptr;
    }

    bool ThemeManager::Duplicate(const std::string_view sourceName, const std::string& newName)
    {
        const Theme* source = Find(sourceName);
        if (source == nullptr)
            return false;

        if (const char* error = GetNameError(newName))
        {
            APP_ERROR("Cannot create theme '{}': {}", newName, error);
            return false;
        }

        Theme copy = *source;
        copy.Name = newName;
        copy.BuiltIn = false;
        copy.Modified = false;
        copy.File = GetThemeFile(newName);

        if (const auto written = WriteJsonFile(copy.File, copy.ToJson()); !written)
        {
            APP_ERROR("Cannot save theme '{}': {}", newName, written.error());
            return false;
        }

        m_Themes.push_back(std::move(copy));
        return SetActive(newName);
    }

    bool ThemeManager::Save(const std::string_view name)
    {
        Theme* theme = FindEditable(name);
        if (theme == nullptr)
            return false;

        if (const auto written = WriteJsonFile(theme->File, theme->ToJson()); !written)
        {
            APP_ERROR("Cannot save theme '{}': {}", theme->Name, written.error());
            return false;
        }

        theme->Modified = false;
        return true;
    }

    bool ThemeManager::Revert(const std::string_view name)
    {
        Theme* theme = FindEditable(name);
        if (theme == nullptr)
            return false;

        const auto json = ReadJson(theme->File);
        if (!json)
        {
            APP_ERROR("Cannot read theme file '{}'", theme->File.filename().generic_string());
            return false;
        }

        const auto reloaded = Theme::FromJson(*json, theme->Name, *Find(DefaultThemeName));
        if (!reloaded)
        {
            APP_ERROR("Theme file '{}' is not a valid theme", theme->File.filename().generic_string());
            return false;
        }

        theme->Colors = reloaded->Colors;
        theme->Editor = reloaded->Editor;
        theme->Modified = false;

        if (theme->Name == m_ActiveName)
            theme->Apply();

        return true;
    }

    bool ThemeManager::Delete(const std::string_view name)
    {
        const auto it = std::ranges::find(m_Themes, name, &Theme::Name);
        if (it == m_Themes.end() || it->BuiltIn)
            return false;

        std::error_code error;
        std::filesystem::remove(it->File, error);
        if (error)
        {
            APP_ERROR("Cannot delete theme file '{}': {}", it->File.filename().generic_string(), error.message());
            return false;
        }

        const bool wasActive = it->Name == m_ActiveName;
        m_Themes.erase(it);

        if (wasActive)
            SetActive(DefaultThemeName);

        return true;
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

            theme->File = file;
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

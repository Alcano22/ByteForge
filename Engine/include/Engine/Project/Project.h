#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Core/FileLock.h"
#include "Engine/Project/ProjectSettings.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace ByteForge
{
    struct ProjectCreateInfo
    {
        std::string Name;
        fs::path Location;
        fs::path TemplateDirectory;
    };

    class BYTEFORGE_API Project
    {
    public:
        static constexpr const char* AssetDirectoryName = "assets";
        static constexpr const char* StateDirectoryName = ".byteforge";

        [[nodiscard]] static Result<Project> Open(const fs::path& path);

        [[nodiscard]] static Result<Project> Create(const ProjectCreateInfo& info);

        [[nodiscard]] static std::optional<std::string> ValidateName(std::string_view name);

        [[nodiscard]] static bool IsProjectDirectory(const fs::path& directory);

        [[nodiscard]] const fs::path& GetRoot() const { return m_Root; }

        [[nodiscard]] const std::string& GetName() const { return m_Name; }

        [[nodiscard]] fs::path GetProjectFile() const { return m_Root / ProjectSettings::FileName; }
        [[nodiscard]] fs::path GetAssetDirectory() const { return m_Root / AssetDirectoryName; }

        [[nodiscard]] fs::path GetStateDirectory() const { return m_Root / StateDirectoryName; }
        [[nodiscard]] fs::path GetScriptAssemblyDirectory() const { return GetStateDirectory() / "ScriptAssemblies"; }

        [[nodiscard]] fs::path GetEditorLockFile() const { return GetStateDirectory() / "editor.lock"; }

        [[nodiscard]] Result<Scope<FileLock>> AcquireEditorLock() const;

        [[nodiscard]] Result<ProjectSettings> LoadSettings() const;
        [[nodiscard]] Result<> SaveSettings(const ProjectSettings& settings) const;

        [[nodiscard]] ProjectSettings MakeDefaultSettings() const;

        static void SetActive(Project project);
        [[nodiscard]] static bool HasActive();
        [[nodiscard]] static const Project& GetActive();

    private:
        explicit Project(fs::path root);

    private:
        fs::path m_Root;
        std::string m_Name;
    };
}

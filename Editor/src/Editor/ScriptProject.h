#pragma once

#include <Engine/Core/Platform.h>
#include <Engine/Scene/UUID.h>

#include <chrono>
#include <expected>
#include <filesystem>
#include <future>
#include <string>

namespace ByteForge
{
    class ScriptProject
    {
    public:
        explicit ScriptProject(std::filesystem::path root);

        void GenerateProjectFile() const;

        void RequestReload();

        void Update(bool canLoad);

        [[nodiscard]] bool IsBuilding() const { return m_Build.valid(); }
        [[nodiscard]] float GetBuildSeconds() const;

        [[nodiscard]] static std::expected<std::string, std::string> FindClassForAsset(UUID scriptAsset);

    private:
        void StartBuild();
        void FinishBuild();

        [[nodiscard]] std::filesystem::path GetProjectFile() const { return m_Root / "GameScripts.csproj"; }
        [[nodiscard]] std::filesystem::path GetOutputDirectory() const { return m_Root / ".byteforge" / "ScriptAssemblies"; }

    private:
        std::filesystem::path m_Root;

        std::future<Platform::ProcessResult> m_Build;
        std::chrono::steady_clock::time_point m_BuildStart;
        bool m_ReloadQueued = false;
    };
}

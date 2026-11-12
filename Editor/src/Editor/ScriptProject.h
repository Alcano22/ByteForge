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
        explicit ScriptProject(fs::path root);

        void GenerateProjectFile() const;

        void RequestReload();

        void Update(bool canLoad);

        [[nodiscard]] bool IsBuilding() const { return m_Build.valid(); }
        [[nodiscard]] f32 GetBuildSeconds() const;

        [[nodiscard]] static Result<std::string> FindClassForAsset(UUID scriptAsset);

    private:
        void StartBuild();
        void FinishBuild();
        void LoadAssembly() const;

        [[nodiscard]] fs::path GetProjectFile() const { return m_Root / "GameScripts.csproj"; }
        [[nodiscard]] fs::path GetOutputDirectory() const { return m_Root / ".byteforge" / "ScriptAssemblies"; }
        [[nodiscard]] fs::path GetAssemblyFile() const { return GetOutputDirectory() / "GameScripts.dll"; }
        [[nodiscard]] fs::path GetStampFile() const { return GetOutputDirectory() / "build.stamp"; }

    private:
        fs::path m_Root;

        std::future<Platform::ProcessResult> m_Build;
        std::chrono::steady_clock::time_point m_BuildStart;
        bool m_ReloadQueued = false;

        std::string m_PendingFingerprint;
        bool m_LoadPending = false;
    };
}

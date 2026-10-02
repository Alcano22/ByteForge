#pragma once

#include <Engine/Scene/UUID.h>

#include <filesystem>
#include <expected>
#include <string>

namespace ByteForge
{
    class ScriptProject
    {
    public:
        explicit ScriptProject(std::filesystem::path root);

        void GenerateProjectFile() const;

        bool Reload() const;

        [[nodiscard]] static std::expected<std::string, std::string> FindClassForAsset(UUID scriptAsset);

    private:
        [[nodiscard]] std::filesystem::path GetProjectFile() const { return m_Root / "GameScripts.csproj"; }
        [[nodiscard]] std::filesystem::path GetOutputDirectory() const { return m_Root / ".byteforge" / "ScriptAssemblies"; }

    private:
        std::filesystem::path m_Root;
    };
}

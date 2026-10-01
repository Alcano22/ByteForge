#pragma once

#include <filesystem>

namespace ByteForge
{
    class ScriptProject
    {
    public:
        explicit ScriptProject(std::filesystem::path root);

        void GenerateProjectFile() const;

        bool Reload() const;

    private:
        [[nodiscard]] std::filesystem::path GetProjectFile() const { return m_Root / "GameScripts.csproj"; }
        [[nodiscard]] std::filesystem::path GetOutputDirectory() const { return m_Root / ".byteforge" / "ScriptAssemblies"; }

    private:
        std::filesystem::path m_Root;
    };
}

#pragma once

#include <Engine/Core/Core.h>

#include <array>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ByteForge
{
    struct CodeEditorInfo
    {
        std::string_view Id;
        std::string_view Name;
        std::array<std::string_view, 3> Executables;
        std::string_view ProjectArguments;
        std::string_view FileArguments;
    };

    class CodeEditorLauncher
    {
    public:
        static constexpr std::string_view CustomId = "custom";

        void Init();

        void Detect();

        void Save() const;

        [[nodiscard]] static std::span<const CodeEditorInfo> GetKnownEditors();

        [[nodiscard]] const std::string& GetSelectedId() const { return m_Selected; }
        void SetSelected(std::string_view id);

        [[nodiscard]] std::optional<fs::path> FindInstalled(std::string_view id) const;

        [[nodiscard]] const std::string& GetCustomExecutable() const { return m_CustomExecutable; }
        void SetCustomExecutable(std::string executable) { m_CustomExecutable = std::move(executable); }

        [[nodiscard]] const std::string& GetCustomArguments() const { return m_CustomArguments; }
        void SetCustomArguments(std::string arguments) { m_CustomArguments = std::move(arguments); }

        bool OpenProject() const;
        bool OpenFile(const fs::path& file, u32 line = 0) const;

        [[nodiscard]] Result<std::vector<std::string>> BuildCommand(const fs::path* file, u32 line = 0) const;

    private:
        void Load();
        [[nodiscard]] std::string PickDefault() const;

    private:
        std::string m_Selected;
        std::string m_CustomExecutable;
        std::string m_CustomArguments = "{project} {file}";

        std::unordered_map<std::string, fs::path> m_Detected;
    };
}

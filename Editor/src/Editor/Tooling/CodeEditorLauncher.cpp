#include "Editor/Tooling/CodeEditorLauncher.h"
#include "Editor/EditorPaths.h"

#include <Engine/Core/FileSystem.h>
#include <Engine/Core/Log.h>
#include <Engine/Core/Platform.h>
#include <Engine/Core/StringUtils.h>
#include <Engine/Project/Project.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <format>
#include <string>

namespace ByteForge
{
    namespace
    {
#if defined(_WIN32)
        constexpr std::string_view SystemOpener = "explorer";
#elif defined(__APPLE__)
        constexpr std::string_view SystemOpener = "open";
#else
        constexpr std::string_view SystemOpener = "xdg-open";
#endif

        constexpr std::array KnownEditors{
            CodeEditorInfo{
                .Id               = "vscode",
                .Name             = "Visual Studio Code",
                .Executables      = { "code", "code-insiders" },
                .ProjectArguments = "{project}",
                .FileArguments    = "{project} --goto {file}:{line}"
            },
            CodeEditorInfo{
                .Id               = "vscodium",
                .Name             = "VSCodium",
                .Executables      = { "codium" },
                .ProjectArguments = "{project}",
                .FileArguments    = "{project} --goto {file}:{line}"
            },
            CodeEditorInfo{
                .Id               = "rider",
                .Name             = "JetBrains Rider",
                .Executables      = { "rider", "rider64", "rider.sh" },
                .ProjectArguments = "{project}",
                .FileArguments    = "--line {line} {file}"
            },
            CodeEditorInfo{
                .Id               = "sublime",
                .Name             = "Sublime Text",
                .Executables      = { "subl" },
                .ProjectArguments = "{project}",
                .FileArguments    = "{project} {file}:{line}"
            },
            CodeEditorInfo{
                .Id               = "zed",
                .Name             = "Zed",
                .Executables      = { "zed", "zeditor" },
                .ProjectArguments = "{project}",
                .FileArguments    = "{project} {file}:{line}"
            },
            CodeEditorInfo{
                .Id               = "system",
                .Name             = "System Default",
                .Executables      = { SystemOpener },
                .ProjectArguments = "{project}",
                .FileArguments    = "{file}"
            }
        };

        constexpr std::string_view SystemId = "system";

        fs::path GetSettingsFile() { return GetUserSettingsDirectory() / "tooling.json"; }

        const CodeEditorInfo* FindInfo(const std::string_view id)
        {
            const auto it = std::ranges::find(KnownEditors, id, &CodeEditorInfo::Id);
            return it != KnownEditors.end() ? &*it : nullptr;
        }

        std::vector<std::string> SplitArguments(const std::string_view text)
        {
            std::vector<std::string> arguments;
            std::string current;
            bool quoted = false;
            bool hasToken = false;

            for (const char c : text)
            {
                if (c == '"')
                {
                    quoted = !quoted;
                    hasToken = true;
                } else if (!quoted && (c == ' ' || c == '\t'))
                {
                    if (hasToken)
                        arguments.push_back(std::exchange(current, {}));
                    hasToken = false;
                } else
                {
                    current.push_back(c);
                    hasToken = true;
                }
            }

            if (hasToken)
                arguments.push_back(std::move(current));

            return arguments;
        }
    }

    std::span<const CodeEditorInfo> CodeEditorLauncher::GetKnownEditors() { return KnownEditors; }

    void CodeEditorLauncher::Init()
    {
        Load();
        Detect();

        if (m_Selected.empty() || (m_Selected != CustomId && FindInfo(m_Selected) == nullptr))
            m_Selected = PickDefault();
    }

    void CodeEditorLauncher::Detect()
    {
        m_Detected.clear();

        for (const CodeEditorInfo& info : KnownEditors)
        {
            for (const std::string_view executable : info.Executables)
            {
                if (executable.empty()) continue;

                if (std::optional<fs::path> found = Platform::FindExecutable(executable))
                {
                    m_Detected.emplace(std::string(info.Id), std::move(*found));
                    break;
                }
            }
        }
    }

    std::string CodeEditorLauncher::PickDefault() const
    {
        for (const CodeEditorInfo& info : KnownEditors)
        {
            if (info.Id != SystemId && m_Detected.contains(std::string(info.Id)))
                return std::string(info.Id);
        }

        return std::string(SystemId);
    }

    void CodeEditorLauncher::Load()
    {
        const fs::path file = GetSettingsFile();
        if (!GetLastWriteTime(file))
            return;

        const Result<nlohmann::json> json = ReadJsonFile(file);
        if (!json || !json->is_object())
        {
            APP_ERROR("Cannot read '.byteforge/tooling.json', using the default editor");
            return;
        }

        m_Selected = json->value("editor", std::string());
        m_CustomExecutable = json->value("customExecutable", std::string());
        m_CustomArguments = json->value("customArguments", m_CustomArguments);
    }

    void CodeEditorLauncher::Save() const
    {
        const nlohmann::json json{
            { "editor",           m_Selected         },
            { "customExecutable", m_CustomExecutable },
            { "customArguments",  m_CustomArguments  }
        };

        if (const auto written = WriteJsonFile(GetSettingsFile(), json); !written)
            APP_ERROR("Cannot save the tooling settings: {}", written.error());
    }

    void CodeEditorLauncher::SetSelected(const std::string_view id)
    {
        if (m_Selected == id) return;

        m_Selected = id;
        Save();
    }

    std::optional<fs::path> CodeEditorLauncher::FindInstalled(const std::string_view id) const
    {
        const auto it = m_Detected.find(std::string(id));
        return it != m_Detected.end() ? std::optional(it->second) : std::nullopt;
    }

    Result<std::vector<std::string>> CodeEditorLauncher::BuildCommand(const fs::path* file, const u32 line) const
    {
        std::string executable;
        std::string_view arguments;

        if (m_Selected == CustomId)
        {
            if (m_CustomExecutable.empty())
                return Unexpected("No program set for the custom editor");

            const std::optional<fs::path> found = Platform::FindExecutable(m_CustomExecutable);
            if (!found)
                return Unexpected("'{}' was not found", m_CustomExecutable);

            executable = ToUtf8(*found);
            arguments = m_CustomArguments;
        } else
        {
            const CodeEditorInfo* info = FindInfo(m_Selected);
            if (info == nullptr)
                return Unexpected("Unknown editor '{}'", m_Selected);

            const std::optional<fs::path> found = FindInstalled(info->Id);
            if (!found)
            {
                return Unexpected("{} was not found on the PATH. Install it, press Refresh, "
                                  "or choose Custom and set its program.", info->Name);
            }

            executable = ToUtf8(*found);
            arguments = file != nullptr ? info->FileArguments : info->ProjectArguments;
        }

        const std::string project = ToUtf8(Project::GetActive().GetRoot());
        const std::string filePath = file != nullptr ? ToUtf8(fs::absolute(*file)) : project;
        const std::string lineText = std::to_string(std::max(line, 1u));

        std::vector<std::string> command{ std::move(executable) };
        for (std::string argument : SplitArguments(arguments))
        {
            ReplaceAll(argument, "{project}", project);
            ReplaceAll(argument, "{file}", filePath);
            ReplaceAll(argument, "{line}", lineText);
            command.push_back(std::move(argument));
        }

        return command;
    }

    bool CodeEditorLauncher::OpenProject() const
    {
        const auto command = BuildCommand(nullptr);
        if (!command)
        {
            APP_ERROR("Cannot open the script project: {}", command.error());
            return false;
        }

        if (const auto launched = Platform::LaunchProcess(*command); !launched)
        {
            APP_ERROR("Cannot open the script project: {}", launched.error());
            return false;
        }

        return true;
    }

    bool CodeEditorLauncher::OpenFile(const fs::path& file, const u32 line) const
    {
        const auto command = BuildCommand(&file, line);
        if (!command)
        {
            APP_ERROR("Cannot open '{}': {}", ToUtf8(file.filename()), command.error());
            return false;
        }

        if (const auto launched = Platform::LaunchProcess(*command); !launched)
        {
            APP_ERROR("Cannot open '{}': {}", ToUtf8(file.filename()), launched.error());
            return false;
        }

        return true;
    }
}

#include "Editor/GameExporter.h"
#include "Editor/EditorContext.h"
#include "Editor/FileDialogs.h"

#include <Engine/Assets/AssetRegistry.h>
#include <Engine/Core/Log.h>
#include <Engine/Core/Platform.h>
#include <Engine/Project/ProjectSettings.h>

#include <algorithm>
#include <format>
#include <string_view>
#include <system_error>

namespace ByteForge
{
    namespace
    {
#ifdef _WIN32
        constexpr const char* ExecutableSuffix = ".exe";
#else
        constexpr const char* ExecutableSuffix = "";
#endif

        std::string PlayerFileName() { return std::string("Player") + ExecutableSuffix; }

        std::string MakeFileName(const std::string& name)
        {
            std::string result = name;
            for (char& c : result)
            {
                if (std::string_view("/\\:*?\"<>|").find(c) != std::string_view::npos ||
                    static_cast<unsigned char>(c) < 32)
                    c = '_';
            }

            while (!result.empty() && (result.back() == ' ' || result.back() == '.'))
                result.pop_back();

            return result.empty() ? "Game" : result;
        }

        bool IsInsideOrSame(const fs::path& path, const fs::path& directory)
        {
            std::error_code error;
            const fs::path a = fs::weakly_canonical(path, error);
            if (error)
                return false;

            const fs::path b = fs::weakly_canonical(directory, error);
            if (error)
                return false;

            const fs::path relative = a.lexically_relative(b);
            return !relative.empty() && *relative.begin() != "..";
        }

        Result<> CopySingleFile(const fs::path& from, const fs::path& to)
        {
            std::error_code error;
            fs::create_directories(to.parent_path(), error);
            if (error)
            {
                return std::unexpected(std::format("cannot create '{}': {}",
                                                   to.parent_path().generic_string(), error.message()));
            }

            fs::copy_file(from, to, fs::copy_options::overwrite_existing, error);
            if (error)
            {
                return std::unexpected(std::format("cannot copy '{}' to '{}': {}",
                                                   from.generic_string(), to.generic_string(), error.message()));
            }

            return {};
        }

        template<typename Filter>
        Result<usize> CopyTree(const fs::path& from, const fs::path& to, Filter&& include)
        {
            std::error_code error;
            fs::recursive_directory_iterator it(from, error);
            if (error)
                return std::unexpected(std::format("cannot read '{}': {}", from.generic_string(), error.message()));

            usize copied = 0;
            for (const fs::recursive_directory_iterator end; it != end; it.increment(error))
            {
                if (error) break;
                if (!it->is_regular_file()) continue;

                const fs::path relative = fs::relative(it->path(), from);
                if (!include(relative)) continue;

                if (const auto result = CopySingleFile(it->path(), to / relative); !result)
                    return std::unexpected(result.error());

                ++copied;
            }

            if (error)
                return std::unexpected(std::format("cannot read '{}': {}", from.generic_string(), error.message()));

            return copied;
        }

        Result<usize> CopyRuntime(const fs::path& from, const fs::path& to,
                                                      const std::string& executableName)
        {
            std::error_code error;
            fs::directory_iterator it(from, error);
            if (error)
                return std::unexpected(std::format("cannot read '{}': {}", from.generic_string(), error.message()));

            usize copied = 0;
            for (const fs::directory_iterator end; it != end; it.increment(error))
            {
                if (error) break;

                const std::string fileName = it->path().filename().string();
                const fs::path target = to / (fileName == PlayerFileName() ? executableName + ExecutableSuffix
                                                                           : fileName);

                std::error_code entryError;
                if (it->is_symlink(entryError))
                {
                    fs::create_directories(to, entryError);
                    fs::remove(target, entryError);
                    fs::copy_symlink(it->path(), target, entryError);
                    if (entryError)
                        return std::unexpected(std::format("cannot copy '{}': {}", fileName, entryError.message()));

                    ++copied;
                    continue;
                }

                if (!it->is_regular_file(entryError)) continue;

                if (const auto result = CopySingleFile(it->path(), target); !result)
                    return std::unexpected(result.error());

                ++copied;
            }

            if (error)
                return std::unexpected(std::format("cannot read '{}': {}", from.generic_string(), error.message()));

            return copied;
        }

        bool IsScriptSource(const fs::path& relative)
        {
            const std::string name = relative.filename().string();
            return name.ends_with(".cs") || name.ends_with(".cs.meta");
        }
    }

    Result<ExportResult> GameExporter::Export(const fs::path& projectRoot,
                                                                  const fs::path& outputRoot)
    {
        const fs::path editorDirectory = Platform::GetExecutableDirectory();
        const fs::path assetsDirectory = projectRoot / "assets";
        const fs::path projectFile = projectRoot / ProjectSettings::FileName;
        const fs::path gameScripts = projectRoot / ".byteforge" / "ScriptAssemblies" / "GameScripts.dll";

        const auto settings = ProjectSettings::Load(projectFile);
        if (!settings)
            return std::unexpected(std::format("{} (use File > Set as Startup Scene first)", settings.error()));

        AssetMetadata startScene;
        if (static_cast<u64>(settings->StartScene) == 0 ||
            !AssetRegistry::TryGetMetadata(settings->StartScene, startScene) ||
            startScene.Type != AssetType::Scene)
        {
            return std::unexpected("the project has no valid startup scene (File > Set as Startup Scene)");
        }

        const fs::path runtimeDirectory = editorDirectory / "player";
        if (!fs::exists(runtimeDirectory / PlayerFileName()))
            return std::unexpected(std::format("'{}' is missing, build the 'Player' target", PlayerFileName()));

        for (const char* folder : { "engine", "ScriptCore" })
        {
            if (!fs::exists(editorDirectory / folder))
                return std::unexpected(std::format("'{}' is missing next to the editor", folder));
        }

        const bool hasScripts = !AssetRegistry::GetAssetsOfType(AssetType::Script).empty();
        const bool hasAssembly = fs::exists(gameScripts);
        if (hasScripts && !hasAssembly)
            return std::unexpected("the game scripts are not built yet (Scripts > Reload Scripts)");

        const std::string gameName = MakeFileName(settings->Name);
        std::string executableName = gameName;
        std::ranges::replace(executableName, ' ', '_');

        const fs::path output = outputRoot / gameName;

        if (IsInsideOrSame(output, assetsDirectory) || IsInsideOrSame(projectRoot, output))
        {
            return std::unexpected(std::format("'{}' would overwrite the project, choose another folder",
                                               output.generic_string()));
        }

        std::error_code error;
        if (fs::exists(output, error) && !fs::is_empty(output, error) && !fs::exists(output / ProjectSettings::FileName))
        {
            return std::unexpected(std::format("'{}' already exists and is not an exported game",
                                               output.generic_string()));
        }

        for (const char* folder : { "assets", "engine", "ScriptCore", ".byteforge" })
            fs::remove_all(output / folder, error);

        fs::create_directories(output, error);
        if (error)
            return std::unexpected(std::format("cannot create '{}': {}", output.generic_string(), error.message()));

        ExportResult result{ .Directory = output };

        const auto runtime = CopyRuntime(runtimeDirectory, output, executableName);
        if (!runtime)
            return std::unexpected(runtime.error());
        result.FileCount += *runtime;

        for (const char* folder : { "engine", "ScriptCore" })
        {
            const auto copied = CopyTree(editorDirectory / folder, output / folder,
                [](const fs::path&) { return true; });
            if (!copied)
                return std::unexpected(copied.error());
            result.FileCount += *copied;
        }

        const auto assets = CopyTree(assetsDirectory, output / "assets",
            [](const fs::path& relative) { return !IsScriptSource(relative); });
        if (!assets)
            return std::unexpected(assets.error());
        result.FileCount += *assets;

        if (hasAssembly)
        {
            if (const auto copied = CopySingleFile(gameScripts,
                                                   output / ".byteforge" / "ScriptAssemblies" / "GameScripts.dll");
                !copied)
            {
                return std::unexpected(copied.error());
            }
            ++result.FileCount;
        }

        if (const auto copied = CopySingleFile(projectFile, output / ProjectSettings::FileName); !copied)
            return std::unexpected(copied.error());
        ++result.FileCount;

        return result;
    }

    void ExportGameWithDialog(EditorContext& context)
    {
        if (!context.IsEditing())
        {
            APP_ERROR("Stop the game before exporting");
            return;
        }

        if (context.Document.IsDirty())
            APP_WARN("The scene has unsaved changes, the export uses the saved scene files");

        const std::optional<fs::path> folder = FileDialogs::PickFolder(fs::current_path());
        if (!folder) return;

        APP_INFO("Exporting the game to '{}'...", folder->generic_string());

        const auto result = GameExporter::Export(fs::current_path(), *folder);
        if (!result)
        {
            APP_ERROR("Export failed: {}", result.error());
            return;
        }

        APP_INFO("Exported {} file(s) to '{}'", result->FileCount, result->Directory.generic_string());
    }
}

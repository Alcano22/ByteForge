#include "Editor/GameExporter.h"
#include "Editor/EditorContext.h"
#include "Editor/FileDialogs.h"

#include <Engine/Assets/AssetRegistry.h>
#include <Engine/Assets/Pak.h>
#include <Engine/Core/Log.h>
#include <Engine/Core/Platform.h>
#include <Engine/Project/ProjectSettings.h>
#include <Engine/Renderer/ShaderLibrary.h>

#include <algorithm>
#include <charconv>
#include <format>
#include <optional>
#include <sstream>
#include <string_view>
#include <system_error>
#include <vector>

namespace ByteForge
{
    namespace
    {
#ifdef _WIN32
        constexpr const char* ExecutableSuffix = ".exe";
#else
        constexpr const char* ExecutableSuffix = "";
#endif

#ifdef BYTEFORGE_DEBUG
        constexpr const char* BuildConfiguration = "Debug";
#else
        constexpr const char* BuildConfiguration = "Release";
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

        bool IsScriptSource(const fs::path& relative)
        {
            const std::string name = relative.filename().string();
            return name.ends_with(".cs") || name.ends_with(".cs.meta");
        }

        bool IsRuntimeFile(const fs::path& relative)
        {
            const std::string name = relative.filename().string();
            return !name.ends_with(".pdb") && name != "cmake_install.cmake";
        }

        bool IsShaderCompilerLibrary(const std::string& fileName)
        {
            return fileName.starts_with("libdxcompiler") || fileName.starts_with("libdxil") ||
                   fileName.starts_with("dxcompiler") || fileName.starts_with("dxil");
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
                if (IsShaderCompilerLibrary(fileName)) continue;

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

        Result<PakStats> PackAssets(const fs::path& assetsDirectory, const fs::path& archive,
                                    const std::optional<fs::path>& gameScripts)
        {
            std::vector<fs::path> files;

            std::error_code error;
            fs::recursive_directory_iterator it(assetsDirectory, error);
            for (const fs::recursive_directory_iterator end; !error && it != end; it.increment(error))
            {
                if (!it->is_regular_file()) continue;

                fs::path relative = fs::relative(it->path(), assetsDirectory);
                if (!IsScriptSource(relative))
                    files.push_back(std::move(relative));
            }

            if (error)
            {
                return std::unexpected(std::format("cannot read '{}': {}",
                                                   assetsDirectory.generic_string(), error.message()));
            }

            std::ranges::sort(files, {}, [](const fs::path& path) { return path.generic_string(); });

            Result<Scope<PakWriter>> writer = PakWriter::Create(archive);
            if (!writer)
                return std::unexpected(writer.error());

            for (const fs::path& relative : files)
            {
                if (const auto added = (*writer)->AddFile(relative.generic_string(), assetsDirectory / relative); !added)
                    return std::unexpected(added.error());
            }

            if (gameScripts)
            {
                if (const auto added = (*writer)->AddFile(AssetRegistry::GameScriptsEntry, *gameScripts); !added)
                    return std::unexpected(added.error());
            }

            return (*writer)->Finish();
        }

        Result<> VerifyArchive(const fs::path& archive)
        {
            const Result<Scope<PakArchive>> opened = PakArchive::Open(archive);
            if (!opened)
                return std::unexpected(opened.error());

            for (const PakEntry& entry : (*opened)->GetEntries())
            {
                if (const auto data = (*opened)->Read(entry.Path); !data)
                    return std::unexpected(data.error());
            }
            return {};
        }

        Result<usize> PackShaders(const fs::path& archive)
        {
            Result<Scope<PakWriter>> writer = PakWriter::Create(archive);
            if (!writer)
                return std::unexpected(writer.error());

            const Result<usize> stages = ShaderLibrary::Precompile(**writer);
            if (!stages)
                return std::unexpected(stages.error());

            if (const auto finished = (*writer)->Finish(); !finished)
                return std::unexpected(finished.error());

            return *stages;
        }

        constexpr i32 DotNetMajorVersion = 10;

        struct DotNetRuntime
        {
            fs::path Root;
            std::string Version;
        };

        std::vector<i32> ParseVersion(const std::string_view text)
        {
            std::vector<i32> parts;
            usize start = 0;
            while (start < text.size())
            {
                const usize end = std::min(text.find('.', start), text.size());
                i32 value = 0;
                std::from_chars(text.data() + start, text.data() + end, value);
                parts.push_back(value);
                start = end + 1;
            }
            return parts;
        }

        std::optional<std::string> FindNewestVersion(const fs::path& directory, const i32 major)
        {
            std::optional<std::string> best;

            std::error_code error;
            for (fs::directory_iterator it(directory, error), end; !error && it != end; it.increment(error))
            {
                if (!it->is_directory()) continue;

                const std::string name = it->path().filename().string();
                const std::vector<i32> version = ParseVersion(name);
                if (version.empty() || version.front() != major) continue;

                if (!best || ParseVersion(*best) < version)
                    best = name;
            }
            return best;
        }

        Result<DotNetRuntime> FindDotNetRuntime()
        {
            const Platform::ProcessResult listed = Platform::RunProcess("dotnet --list-runtimes");
            if (listed.ExitCode != 0)
                return std::unexpected(std::format("'dotnet --list-runtimes' failed: {}", listed.Output));

            constexpr std::string_view Framework = "Microsoft.NETCore.App ";

            std::optional<DotNetRuntime> best;
            std::istringstream lines(listed.Output);
            for (std::string line; std::getline(lines, line);)
            {
                if (!line.starts_with(Framework)) continue;

                const usize versionEnd = line.find(' ', Framework.size());
                const usize pathStart = line.find('[', versionEnd);
                const usize pathEnd = line.rfind(']');
                if (versionEnd == std::string::npos || pathStart == std::string::npos ||
                    pathEnd == std::string::npos || pathEnd < pathStart) continue;

                std::string version = line.substr(Framework.size(), versionEnd - Framework.size());
                const std::vector<i32> parsed = ParseVersion(version);
                if (parsed.empty() || parsed.front() != DotNetMajorVersion) continue;

                if (best && ParseVersion(best->Version) >= parsed) continue;

                const fs::path frameworkDirectory = line.substr(pathStart + 1, pathEnd - pathStart - 1);
                best = DotNetRuntime{
                    .Root    = frameworkDirectory.parent_path().parent_path(),
                    .Version = std::move(version)
                };
            }

            if (!best)
                return std::unexpected(std::format("no .NET {} runtime is installed", DotNetMajorVersion));

            return *best;
        }

        Result<usize> BundleDotNetRuntime(const DotNetRuntime& runtime, const fs::path& target)
        {
            const std::optional<std::string> fxr = FindNewestVersion(runtime.Root / "host" / "fxr", DotNetMajorVersion);
            if (!fxr)
            {
                return std::unexpected(std::format("'{}' contains no host/fxr for .NET {}",
                                                   runtime.Root.generic_string(), DotNetMajorVersion));
            }

            usize copied = 0;

            const fs::path framework = fs::path("shared") / "Microsoft.NETCore.App" / runtime.Version;
            const fs::path hostFxr = fs::path("host") / "fxr" / *fxr;

            for (const fs::path& relative : { framework, hostFxr })
            {
                const auto result = CopyTree(runtime.Root / relative, target / relative,
                    [](const fs::path&) { return true; });
                if (!result)
                    return std::unexpected(result.error());
                copied += *result;
            }

            for (const char* notice : { "LICENSE.txt", "ThirdPartyNotices.txt" })
            {
                std::error_code error;
                if (!fs::exists(runtime.Root / notice, error)) continue;

                if (const auto result = CopySingleFile(runtime.Root / notice, target / notice); !result)
                    return std::unexpected(result.error());
                ++copied;
            }

            return copied;
        }
    }

    Result<ExportResult> GameExporter::Export(const fs::path& projectRoot, const fs::path& outputRoot)
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

        for (const char* folder : { "assets", "engine", "ScriptCore", ".byteforge", "dotnet" })
            fs::remove_all(output / folder, error);

        for (fs::directory_iterator it(output, error), end; !error && it != end; it.increment(error))
        {
            if (!IsShaderCompilerLibrary(it->path().filename().string())) continue;

            std::error_code removeError;
            fs::remove(it->path(), removeError);
        }
        error.clear();

        fs::create_directories(output, error);
        if (error)
            return std::unexpected(std::format("cannot create '{}': {}", output.generic_string(), error.message()));

        ExportResult result{ .Directory = output };

        const auto runtime = CopyRuntime(runtimeDirectory, output, executableName);
        if (!runtime)
            return std::unexpected(runtime.error());
        result.FileCount += *runtime;

        const auto scriptCore = CopyTree(editorDirectory / "ScriptCore", output / "ScriptCore",
            [](const fs::path& relative) { return IsRuntimeFile(relative); });
        if (!scriptCore)
            return std::unexpected(scriptCore.error());
        result.FileCount += *scriptCore;

        const Result<DotNetRuntime> dotnet = FindDotNetRuntime();
        if (!dotnet)
            return std::unexpected(std::format("cannot bundle the .NET runtime: {}", dotnet.error()));

        const Result<usize> bundledRuntime = BundleDotNetRuntime(*dotnet, output / "dotnet");
        if (!bundledRuntime)
            return std::unexpected(std::format("cannot bundle the .NET runtime: {}", bundledRuntime.error()));

        result.FileCount += *bundledRuntime;
        result.DotNetVersion = dotnet->Version;

        const fs::path shaderArchive = output / ShaderLibrary::ArchiveName;

        const Result<usize> shaders = PackShaders(shaderArchive);
        if (!shaders)
            return std::unexpected(std::format("cannot precompile the shaders: {}", shaders.error()));

        if (const auto verified = VerifyArchive(shaderArchive); !verified)
            return std::unexpected(std::format("the shader archive is broken: {}", verified.error()));

        result.ShaderStages = *shaders;
        ++result.FileCount;

        const fs::path archive = output / std::format("assets{}", PakExtension);

        const Result<PakStats> packed = PackAssets(assetsDirectory, archive,
                                                   hasAssembly ? std::optional(gameScripts) : std::nullopt);
        if (!packed)
            return std::unexpected(std::format("cannot pack the assets: {}", packed.error()));

        if (const auto verified = VerifyArchive(archive); !verified)
            return std::unexpected(std::format("the asset archive is broken: {}", verified.error()));

        result.Archive = *packed;
        ++result.FileCount;

        if (const auto copied = CopySingleFile(projectFile, output / ProjectSettings::FileName); !copied)
            return std::unexpected(copied.error());
        ++result.FileCount;

        return result;
    }

    void ExportGameWithDialog(const EditorContext& context)
    {
        if (!context.IsEditing())
        {
            APP_ERROR("Stop the game before exporting");
            return;
        }

        if (context.Document.IsDirty())
            APP_WARN("The scene has unsaved changes, the export uses the saved scene files");

#ifdef BYTEFORGE_DEBUG
        APP_WARN("This is a Debug editor, so the game is exported as a slow Debug build; "
                 "export from the Release editor to ship the game");
#endif

        const std::optional<fs::path> folder = FileDialogs::PickFolder(fs::current_path());
        if (!folder) return;

        APP_INFO("Exporting the game to '{}'...", folder->generic_string());

        const auto result = GameExporter::Export(fs::current_path(), *folder);
        if (!result)
        {
            APP_ERROR("Export failed: {}", result.error());
            return;
        }

        const PakStats& archive = result->Archive;
        const f64 ratio = archive.Size > 0
                        ? 100.0 * static_cast<f64>(archive.StoredSize) / static_cast<f64>(archive.Size)
                        : 100.0;
        APP_INFO("Packed {} asset file(s): {:.1f} KiB -> {:.1f} KiB ({:.0f}%)", archive.FileCount,
                 static_cast<f64>(archive.Size) / 1024.0, static_cast<f64>(archive.StoredSize) / 1024.0, ratio);

        APP_INFO("Bundled the .NET {} runtime", result->DotNetVersion);
        APP_INFO("Precompiled {} shader stage(s)", result->ShaderStages);
        APP_INFO("Exported {} file(s) as a {} build to '{}'",
                 result->FileCount, BuildConfiguration, result->Directory.generic_string());
    }
}

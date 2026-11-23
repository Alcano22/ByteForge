#include "Editor/GameExporter.h"
#include "Editor/EditorContext.h"
#include "Editor/FileDialogs.h"

#include <Engine/Assets/AssetRegistry.h>
#include <Engine/Assets/Pak.h>
#include <Engine/Core/DotNetLocator.h>
#include <Engine/Core/JobSystem.h>
#include <Engine/Core/Log.h>
#include <Engine/Core/Platform.h>
#include <Engine/Core/RuntimePaths.h>
#include <Engine/Project/Project.h>
#include <Engine/Project/ProjectSettings.h>
#include <Engine/Renderer/ShaderLibrary.h>

#include <imgui.h>

#include <algorithm>
#include <charconv>
#include <chrono>
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

        std::unexpected<std::string> Cancelled() { return std::unexpected<std::string>("the export was cancelled"); }

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
            const std::string name = ToUtf8(relative.filename());
            return name.ends_with(".cs") || name.ends_with(".cs.meta");
        }

        bool IsRuntimeFile(const fs::path& relative)
        {
            const std::string name = ToUtf8(relative.filename());
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
                return Unexpected("cannot create '{}': {}", ToGenericUtf8(to.parent_path()), error.message());

            fs::copy_file(from, to, fs::copy_options::overwrite_existing, error);
            if (error)
            {
                return Unexpected("cannot copy '{}' to '{}': {}",
                                  ToGenericUtf8(from), ToGenericUtf8(to), error.message());
            }

            return {};
        }

        template<typename Filter>
        Result<std::vector<fs::path>> ListFiles(const fs::path& directory, Filter&& include)
        {
            std::vector<fs::path> files;

            std::error_code error;
            fs::recursive_directory_iterator it(directory, error);
            for (const fs::recursive_directory_iterator end; !error && it != end; it.increment(error))
            {
                if (!it->is_regular_file()) continue;

                fs::path relative = it->path().lexically_relative(directory);
                if (include(relative))
                    files.push_back(std::move(relative));
            }

            if (error)
                return Unexpected("cannot read '{}': {}", ToGenericUtf8(directory), error.message());

            std::ranges::sort(files, {}, [](const fs::path& path) { return ToGenericUtf8(path); });
            return files;
        }

        template<typename Filter>
        Result<usize> CopyTree(const fs::path& from, const fs::path& to, Filter&& include, ExportProgress& progress)
        {
            const Result<std::vector<fs::path>> files = ListFiles(from, std::forward<Filter>(include));
            if (!files)
                return Unexpected(files.error());

            progress.AddWork(files->size());

            for (const fs::path& relative : *files)
            {
                if (progress.IsCancelled())
                    return Cancelled();

                if (const auto result = CopySingleFile(from / relative, to / relative); !result)
                    return Unexpected(result.error());

                progress.Advance();
            }

            return files->size();
        }

        Result<usize> CopyRuntime(const fs::path& from, const fs::path& to, const std::string& executableName)
        {
            std::error_code error;
            fs::directory_iterator it(from, error);
            if (error)
                return Unexpected("cannot read '{}': {}", ToGenericUtf8(from), error.message());

            usize copied = 0;
            for (const fs::directory_iterator end; it != end; it.increment(error))
            {
                if (error) break;

                const std::string fileName = ToUtf8(it->path().filename());
                if (IsShaderCompilerLibrary(fileName)) continue;

                const std::string targetName = fileName == PlayerFileName() ? executableName + ExecutableSuffix
                                                                            : fileName;
                const fs::path target = to / PathFromUtf8(targetName);

                std::error_code entryError;
                if (it->is_symlink(entryError))
                {
                    fs::create_directories(to, entryError);
                    fs::remove(target, entryError);
                    fs::copy_symlink(it->path(), target, entryError);
                    if (entryError)
                        return Unexpected("cannot copy '{}': {}", fileName, entryError.message());

                    ++copied;
                    continue;
                }

                if (!it->is_regular_file(entryError)) continue;

                if (const auto result = CopySingleFile(it->path(), target); !result)
                    return Unexpected(result.error());

                ++copied;
            }

            if (error)
                return Unexpected("cannot read '{}': {}", ToGenericUtf8(from), error.message());

            return copied;
        }

        Result<PakStats> PackAssets(const fs::path& assetsDirectory, const fs::path& archive,
                                    const std::optional<fs::path>& gameScripts, ExportProgress& progress)
        {
            const Result<std::vector<fs::path>> files = ListFiles(assetsDirectory,
                [](const fs::path& relative) { return !IsScriptSource(relative); });
            if (!files)
                return Unexpected(files.error());

            progress.AddWork(files->size() + (gameScripts ? 1 : 0));

            Result<Scope<PakWriter>> writer = PakWriter::Create(archive);
            if (!writer)
                return Unexpected(writer.error());

            for (const fs::path& relative : *files)
            {
                if (progress.IsCancelled())
                    return Cancelled();

                if (const auto added = (*writer)->AddFile(ToGenericUtf8(relative), assetsDirectory / relative); !added)
                    return Unexpected(added.error());

                progress.Advance();
            }

            if (gameScripts)
            {
                if (const auto added = (*writer)->AddFile(AssetRegistry::GameScriptsEntry, *gameScripts); !added)
                    return Unexpected(added.error());

                progress.Advance();
            }

            return (*writer)->Finish();
        }

        Result<> VerifyArchive(const fs::path& archive, ExportProgress& progress)
        {
            const Result<Scope<PakArchive>> opened = PakArchive::Open(archive);
            if (!opened)
                return Unexpected(opened.error());

            progress.AddWork((*opened)->GetEntries().size());

            for (const PakEntry& entry : (*opened)->GetEntries())
            {
                if (progress.IsCancelled())
                    return Cancelled();

                if (const auto data = (*opened)->Read(entry.Path); !data)
                    return Unexpected(data.error());

                progress.Advance();
            }
            return {};
        }

        Result<usize> PackShaders(const fs::path& archive)
        {
            Result<Scope<PakWriter>> writer = PakWriter::Create(archive);
            if (!writer)
                return Unexpected(writer.error());

            const Result<usize> stages = ShaderLibrary::Precompile(**writer);
            if (!stages)
                return Unexpected(stages.error());

            if (const auto finished = (*writer)->Finish(); !finished)
                return Unexpected(finished.error());

            return *stages;
        }

        Result<usize> BundleDotNetRuntime(const DotNetInstallation& runtime,
                                          const fs::path& target, ExportProgress& progress)
        {
            if (!runtime.Runtime)
                return Unexpected("'{}' has no .NET {} runtime", ToGenericUtf8(runtime.Root), DotNetMajorVersion);

            usize copied = 0;

            const fs::path framework = fs::path("shared") / "Microsoft.NETCore.App" / runtime.Runtime->ToString();
            const fs::path hostFxr = fs::path("host") / "fxr" / runtime.HostResolver.ToString();

            for (const fs::path& relative : { framework, hostFxr })
            {
                const auto result = CopyTree(runtime.Root / relative, target / relative,
                    [](const fs::path&) { return true; }, progress);
                if (!result)
                    return Unexpected(result.error());
                copied += *result;
            }

            for (const char* notice : { "LICENSE.txt", "ThirdPartyNotices.txt" })
            {
                std::error_code error;
                if (!fs::exists(runtime.Root / notice, error)) continue;

                if (const auto result = CopySingleFile(runtime.Root / notice, target / notice); !result)
                    return Unexpected(result.error());
                ++copied;
            }

            return copied;
        }

        void LogResult(const ExportResult& result)
        {
            const PakStats& archive = result.Archive;
            const f64 ratio = archive.Size > 0
                            ? 100.0 * static_cast<f64>(archive.StoredSize) / static_cast<f64>(archive.Size)
                            : 100.0;
            APP_INFO("Packed {} asset file(s): {:.1f} KiB -> {:.1f} KiB ({:.0f}%)", archive.FileCount,
                     static_cast<f64>(archive.Size) / 1024.0, static_cast<f64>(archive.StoredSize) / 1024.0, ratio);

            APP_INFO("Bundled the .NET {} runtime", result.DotNetVersion);
            APP_INFO("Precompiled {} shader stage(s)", result.ShaderStages);
            APP_INFO("Exported {} file(s) as a {} build to '{}'",
                     result.FileCount, BuildConfiguration, ToGenericUtf8(result.Directory));
        }
    }

    Result<ExportPlan> GameExporter::Prepare(const Project& project, const fs::path& outputRoot)
    {
        ExportPlan plan;
        plan.PlayerDirectory = RuntimePaths::GetRoot() / "player";
        plan.ScriptCoreDirectory = RuntimePaths::GetDataDirectory() / "ScriptCore";
        plan.FontsDirectory = RuntimePaths::GetDataDirectory() / "fonts";
        plan.AssetsDirectory = project.GetAssetDirectory();
        plan.ProjectFile = project.GetProjectFile();

        const fs::path gameScripts = project.GetScriptAssemblyDirectory() / "GameScripts.dll";

        const auto settings = ProjectSettings::Load(plan.ProjectFile);
        if (!settings)
            return Unexpected("{} (use File > Set as Startup Scene first)", settings.error());

        AssetMetadata startScene;
        if (!settings->StartScene.IsSet() || !AssetRegistry::TryGetMetadata(settings->StartScene, startScene) ||
            startScene.Type != AssetType::Scene)
        {
            return Unexpected("the project has no valid startup scene (File > Set as Startup Scene)");
        }

        if (!fs::exists(plan.PlayerDirectory / PlayerFileName()))
            return Unexpected("'{}' is missing, build the 'Player' target", PlayerFileName());

        for (const fs::path& required : { RuntimePaths::GetDataDirectory() / "shaders",
                                          plan.ScriptCoreDirectory, plan.FontsDirectory })
        {
            if (!fs::exists(required))
                return Unexpected("'{}' is missing", ToGenericUtf8(required));
        }

        if (const fs::path input = RuntimePaths::GetDataDirectory() / "input"; fs::exists(input))
            plan.InputDirectory = input;
        else
        {
            APP_WARN("'{}' is missing, the exported game will only know the controllers built into GLFW",
                     ToGenericUtf8(input));
        }

        const bool hasScripts = !AssetRegistry::GetAssetsOfType(AssetType::Script).empty();
        const bool hasAssembly = fs::exists(gameScripts);
        if (hasScripts && !hasAssembly)
            return Unexpected("the game scripts are not built yet (Scripts > Reload Scripts)");
        if (hasAssembly)
            plan.GameScripts = gameScripts;

        const std::string gameName = MakeFileName(settings->Name);
        plan.ExecutableName = gameName;
        std::ranges::replace(plan.ExecutableName, ' ', '_');

        plan.Output = outputRoot / PathFromUtf8(gameName);

        if (IsInsideOrSame(plan.Output, plan.AssetsDirectory) || IsInsideOrSame(project.GetRoot(), plan.Output))
            return Unexpected("'{}' would overwrite the project, choose another folder", ToGenericUtf8(plan.Output));

        std::error_code error;
        if (fs::exists(plan.Output, error) && !fs::is_empty(plan.Output, error) &&
            !fs::exists(plan.Output / ProjectSettings::FileName))
        {
            return Unexpected("'{}' already exists and is not an exported game", ToGenericUtf8(plan.Output));
        }

        return plan;
    }

    Result<ExportResult> GameExporter::Run(const ExportPlan& plan, ExportProgress& progress)
    {
        const fs::path& output = plan.Output;

        progress.BeginStep("Copying the player");

        std::error_code error;
        for (const char* folder : { "assets", "engine", "shaders", "fonts", "input", "ScriptCore", ".byteforge", "dotnet" })
            fs::remove_all(output / folder, error);

        for (fs::directory_iterator it(output, error), end; !error && it != end; it.increment(error))
        {
            if (!IsShaderCompilerLibrary(ToUtf8(it->path().filename()))) continue;

            std::error_code removeError;
            fs::remove(it->path(), removeError);
        }
        error.clear();

        fs::create_directories(output, error);
        if (error)
            return Unexpected("cannot create '{}': {}", ToGenericUtf8(output), error.message());

        ExportResult result{ .Directory = output };

        const auto runtime = CopyRuntime(plan.PlayerDirectory, output, plan.ExecutableName);
        if (!runtime)
            return Unexpected(runtime.error());
        result.FileCount += *runtime;

        const auto scriptCore = CopyTree(plan.ScriptCoreDirectory, output / "ScriptCore",
            [](const fs::path& relative) { return IsRuntimeFile(relative); }, progress);
        if (!scriptCore)
            return Unexpected(scriptCore.error());
        result.FileCount += *scriptCore;

        const auto fonts = CopyTree(plan.FontsDirectory, output / "fonts",
            [](const fs::path&) { return true; }, progress);
        if (!fonts)
            return Unexpected(fonts.error());
        result.FileCount += *fonts;

        if (plan.InputDirectory)
        {
            const auto input = CopyTree(*plan.InputDirectory, output / "input",
                [](const fs::path&) { return true; }, progress);
            if (!input)
                return Unexpected(input.error());
            result.FileCount += *input;
        }

        progress.BeginStep("Bundling the .NET runtime");

        const std::optional<DotNetInstallation> dotnet = DotNetLocator::Find(DotNetComponent::Runtime);
        if (!dotnet)
            return Unexpected("cannot bundle the .NET runtime: no .NET {} runtime found", DotNetMajorVersion);

        const Result<usize> bundledRuntime = BundleDotNetRuntime(*dotnet, output / "dotnet", progress);
        if (!bundledRuntime)
        {
            return Unexpected(bundledRuntime.error() == Cancelled().error()
                              ? bundledRuntime.error()
                              : std::format("cannot bundle the .NET runtime: {}", bundledRuntime.error()));
        }

        result.FileCount += *bundledRuntime;
        result.DotNetVersion = dotnet->Runtime->ToString();

        progress.BeginStep("Compiling shaders");
        if (progress.IsCancelled())
            return Cancelled();

        const fs::path shaderArchive = output / ShaderLibrary::ArchiveName;

        const Result<usize> shaders = PackShaders(shaderArchive);
        if (!shaders)
            return Unexpected("cannot precompile the shaders: {}", shaders.error());

        result.ShaderStages = *shaders;
        ++result.FileCount;

        progress.BeginStep("Packing assets");

        const fs::path archive = output / std::format("assets{}", PakExtension);

        const Result<PakStats> packed = PackAssets(plan.AssetsDirectory, archive, plan.GameScripts, progress);
        if (!packed)
            return Unexpected(packed.error());

        result.Archive = *packed;
        ++result.FileCount;

        progress.BeginStep("Verifying the archives");

        for (const fs::path& written : { shaderArchive, archive })
        {
            if (const auto verified = VerifyArchive(written, progress); !verified)
                return Unexpected("'{}' is broken: {}", ToUtf8(written.filename()), verified.error());
        }

        if (const auto copied = CopySingleFile(plan.ProjectFile, output / ProjectSettings::FileName); !copied)
            return Unexpected(copied.error());
        ++result.FileCount;

        return result;
    }

    ExportTask::~ExportTask()
    {
        if (m_Progress)
            m_Progress->Cancel();
    }

    bool ExportTask::Start(ExportPlan plan, PopupManager& popups)
    {
        if (IsRunning())
            return false;

        m_Progress = MakeRef<ExportProgress>();
        m_Future = JobSystem::Submit([plan = std::move(plan), progress = m_Progress]
        {
            return GameExporter::Run(plan, *progress);
        });

        popups.Open("Exporting Game", [this](Popup& popup)
        {
            if (!IsRunning())
            {
                popup.Close();
                return;
            }

            const ExportProgress::State state = m_Progress->Get();
            ImGui::TextUnformatted(state.Step.c_str());
            ImGui::ProgressBar(state.Fraction, ImVec2(360.0f, 0.0f));

            const bool cancelling = m_Progress->IsCancelled();
            ImGui::BeginDisabled(cancelling);
            if (ImGui::Button(cancelling ? "Cancelling..." : "Cancel"))
                m_Progress->Cancel();
            ImGui::EndDisabled();
        }, { .Dismissable = false });

        return true;
    }

    void ExportTask::Update()
    {
        if (!IsRunning() || m_Future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;

        const Result<ExportResult> result = m_Future.get();
        const bool cancelled = m_Progress->IsCancelled();
        m_Future = {};

        if (result)
            LogResult(*result);
        else if (cancelled)
            APP_WARN("Export cancelled, the output folder is incomplete; the next export cleans it up");
        else
            APP_ERROR("Export failed: {}", result.error());
    }

    void ExportGameWithDialog(EditorContext& context)
    {
        if (!context.IsEditing())
        {
            APP_ERROR("Stop the game before exporting");
            return;
        }

        if (context.Export.IsRunning()) return;

        if (context.Document.IsDirty())
            APP_WARN("The scene has unsaved changes, the export uses the saved scene files");

#ifdef BYTEFORGE_DEBUG
        APP_WARN("This is a Debug editor, so the game is exported as a slow Debug build; "
                 "export from the Release editor to ship the game");
#endif

        const Project& project = Project::GetActive();

        const std::optional<fs::path> folder = FileDialogs::PickFolder(project.GetRoot());
        if (!folder) return;

        Result<ExportPlan> plan = GameExporter::Prepare(project, *folder);
        if (!plan)
        {
            APP_ERROR("Export failed: {}", plan.error());
            return;
        }

        APP_INFO("Exporting the game to '{}'...", ToGenericUtf8(plan->Output));
        context.Export.Start(std::move(*plan), context.Popups);
    }
}

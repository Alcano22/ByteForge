#include "Editor/SceneDocument.h"
#include "Editor/Commands/EntityCommands.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorPaths.h"

#include <Engine/Assets/AssetRegistry.h>
#include <Engine/Assets/Prefab.h>
#include <Engine/Core/Application.h>
#include <Engine/Core/Log.h>
#include <Engine/Core/FileSystem.h>
#include <Engine/Project/ProjectSettings.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Scene/SceneSerializer.h>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <exception>
#include <format>
#include <system_error>
#include <utility>
#include <vector>

namespace ByteForge
{
    namespace
    {
        constexpr const char* WindowTitle = "ByteForge Editor";

        fs::path GetStateFile()
        {
            return GetStateDirectory() / "EditorState.json";
        }

        fs::path GetAssetPath(const UUID handle)
        {
            AssetMetadata metadata;
            return AssetRegistry::TryGetMetadata(handle, metadata) ? metadata.Path : fs::path{};
        }

        nlohmann::json ReadState()
        {
            Result<nlohmann::json> state = ReadJsonFile(GetStateFile());
            return state && state->is_object() ? std::move(*state) : nlohmann::json::object();
        }

        void WriteState(const nlohmann::json& state)
        {
            const fs::path file = GetStateFile();

            std::error_code error;
            fs::create_directories(file.parent_path(), error);

            if (const auto written = WriteJsonFile(file, state); !written)
                APP_ERROR("Cannot save the editor state: {}", written.error());
        }
    }

    void SceneDocument::New()
    {
        m_Context.ActiveScene->Clear();
        m_Handle.reset();
        ResetEditorState();
    }

    bool SceneDocument::Open(const UUID scene)
    {
        AssetMetadata metadata;
        if (!AssetRegistry::TryGetMetadata(scene, metadata) || metadata.Type != AssetType::Scene)
        {
            APP_ERROR("Cannot open asset {}: it is not a scene", static_cast<u64>(scene));
            return false;
        }

        const std::string name = metadata.Path.generic_string();

        try
        {
            if (!SceneSerializer::DeserializeFromFile(*m_Context.ActiveScene, AssetRegistry::Resolve(scene).string()))
            {
                APP_ERROR("Could not read scene '{}'", name);
                return false;
            }

            Prefab::SyncAllInstances(*m_Context.ActiveScene);
        } catch (const std::exception& e)
        {
            APP_ERROR("Scene '{}' is broken: {}", name, e.what());
            return false;
        }

        m_Handle = scene;
        ResetEditorState();
        RememberAsLast();

        APP_INFO("Opened scene '{}'", name);
        return true;
    }

    bool SceneDocument::OpenLast()
    {
        const nlohmann::json state = ReadState();
        if (!state.contains("lastScene") || !state.at("lastScene").is_number_unsigned())
            return false;

        const UUID scene(state.at("lastScene").get<u64>());

        AssetMetadata metadata;
        if (!AssetRegistry::TryGetMetadata(scene, metadata))
            return false;

        return Open(scene);
    }

    bool SceneDocument::Save()
    {
        if (m_PrefabStage)
            return SavePrefab();

        if (!m_Handle)
            return false;

        return Write(AssetRegistry::Resolve(*m_Handle));
    }

    bool SceneDocument::SaveAs(const fs::path& relativePath)
    {
        if (m_PrefabStage)
        {
            APP_WARN("Close the prefab before saving the scene under a new name");
            return false;
        }

        const fs::path absolutePath = AssetRegistry::GetAssetRoot() / relativePath;

        std::error_code error;
        fs::create_directories(absolutePath.parent_path(), error);

        if (!Write(absolutePath))
            return false;

        m_Handle = AssetRegistry::Import(relativePath);
        RememberAsLast();
        return true;
    }

    bool SceneDocument::OpenPrefab(const UUID prefab)
    {
        if (!m_Context.IsEditing() || m_Context.ActiveScene == nullptr)
        {
            APP_WARN("Stop the play session to edit a prefab");
            return false;
        }

        if (m_PrefabStage && m_PrefabStage->Prefab == prefab)
            return true;

        ClosePrefab();

        Scene& scene = *m_Context.ActiveScene;

        Prefab::RecordOverrides(scene);
        nlohmann::json snapshot = SceneSerializer::Serialize(scene);

        scene.Clear();
        const Result<Entity> root = Prefab::LoadForEditing(scene, prefab);
        if (!root)
        {
            SceneSerializer::Deserialize(scene, snapshot);
            APP_ERROR("Cannot open prefab: {}", root.error());
            return false;
        }

        m_PrefabStage = PrefabStage{
            .Prefab        = prefab,
            .SceneSnapshot = std::move(snapshot),
            .SceneHistory  = std::exchange(m_Context.History, CommandHistory{})
        };

        m_Context.SelectionContext.ClearEntity();
        m_Context.SelectionContext.Select(*root);

        APP_INFO("Editing prefab '{}'", GetAssetPath(prefab).generic_string());
        return true;
    }

    void SceneDocument::ClosePrefab()
    {
        if (!m_PrefabStage) return;

        PrefabStage stage = std::move(*m_PrefabStage);
        m_PrefabStage.reset();

        Scene& scene = *m_Context.ActiveScene;
        SceneSerializer::Deserialize(scene, stage.SceneSnapshot);

        m_Context.History = std::move(stage.SceneHistory);
        m_Context.SelectionContext.ClearEntity();

        if (!stage.Saved) return;

        const usize updated = Prefab::SyncInstances(scene, stage.Prefab);
        if (updated == 0) return;

        const std::string name = GetAssetPath(stage.Prefab).stem().string();
        m_Context.History.Record(MakeScope<SceneSnapshotCommand>(scene, std::format("Update '{}' instances", name),
                                                                 std::move(stage.SceneSnapshot),
                                                                 SceneSerializer::Serialize(scene)));

        APP_INFO("Updated {} instance(s) of '{}'", updated, name);
    }

    std::optional<UUID> SceneDocument::GetPrefab() const
    {
        return m_PrefabStage ? std::optional(m_PrefabStage->Prefab) : std::nullopt;
    }

    std::string SceneDocument::GetSceneName() const
    {
        const fs::path path = m_Handle ? GetAssetPath(*m_Handle) : fs::path{};
        return path.empty() ? "Untitled" : path.stem().string();
    }

    bool SceneDocument::SetAsStartupScene() const
    {
        if (!m_Handle)
            return false;

        const fs::path root = fs::current_path();
        const fs::path file = root / ProjectSettings::FileName;

        ProjectSettings settings;
        if (fs::exists(file))
        {
            auto loaded = ProjectSettings::Load(file);
            if (!loaded)
            {
                APP_ERROR("Could not read project file: {}", loaded.error());
                return false;
            }
            settings = std::move(*loaded);
        } else
        {
            settings.Name = root.filename().string();
            settings.WindowTitle = settings.Name;
        }

        settings.StartScene = *m_Handle;

        if (const auto saved = settings.Save(file); !saved)
        {
            APP_ERROR("Could not write project file: {}", saved.error());
            return false;
        }

        APP_INFO("'{}' is now the startup scene", GetAssetPath(*m_Handle).generic_string());
        return true;
    }

    bool SceneDocument::IsDirty() const { return m_Context.History.IsDirty(); }

    fs::path SceneDocument::GetPath() const
    {
        if (m_PrefabStage)
            return GetAssetPath(m_PrefabStage->Prefab);

        return m_Handle ? GetAssetPath(*m_Handle) : fs::path{};
    }

    std::string SceneDocument::GetName() const
    {
        const fs::path path = GetPath();
        return path.empty() ? "Untitled" : path.stem().string();
    }

    void SceneDocument::Update() const
    {
        Application::Get().GetWindow().SetTitle(std::format("{} - {}{}{}", WindowTitle, GetName(),
                                                            IsEditingPrefab() ? " (Prefab)" : "",
                                                            IsDirty() ? "*" : ""));
    }

    fs::path SceneDocument::ToScenePath(const std::string_view name)
    {
        fs::path path(name);
        if (path.extension().string() != Extension)
            path += Extension;
        return path.lexically_normal();
    }

    bool SceneDocument::Write(const fs::path& absolutePath)
    {
        if (!m_Context.IsEditing())
        {
            APP_ERROR("Scenes can only be saved while editing");
            return false;
        }

        try
        {
            Prefab::RecordOverrides(*m_Context.ActiveScene);
            SceneSerializer::SerializeToFile(*m_Context.ActiveScene, absolutePath.string());
        } catch (const std::exception& e)
        {
            APP_ERROR("Could not save scene: {}", e.what());
            return false;
        }

        m_Context.History.MarkClean();
        APP_INFO("Scene saved to '{}'", absolutePath.generic_string());
        return true;
    }

    bool SceneDocument::SavePrefab()
    {
        const std::vector<Entity> roots = m_Context.ActiveScene->GetRootEntities();
        if (roots.size() != 1)
        {
            APP_ERROR("A prefab needs exactly one root entity (found {}); move the others below the root",
                      roots.size());
            return false;
        }

        if (const auto saved = Prefab::Save(m_PrefabStage->Prefab, roots.front()); !saved)
        {
            APP_ERROR("Could not save prefab: {}", saved.error());
            return false;
        }

        m_PrefabStage->Saved = true;
        m_Context.History.MarkClean();

        APP_INFO("Prefab saved to '{}'", GetPath().generic_string());
        return true;
    }

    void SceneDocument::ResetEditorState() const
    {
        m_Context.SelectionContext.ClearEntity();
        m_Context.History.Clear();
        m_Context.History.MarkClean();
    }

    void SceneDocument::RememberAsLast() const
    {
        nlohmann::json state = ReadState();
        state["lastScene"] = static_cast<u64>(*m_Handle);
        WriteState(state);
    }
}

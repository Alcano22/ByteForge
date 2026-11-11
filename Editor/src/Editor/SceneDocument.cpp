#include "Editor/SceneDocument.h"
#include "Editor/EditorContext.h"

#include <Engine/Assets/AssetRegistry.h>
#include <Engine/Core/Application.h>
#include <Engine/Core/Log.h>
#include <Engine/Project/ProjectSettings.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Scene/SceneSerializer.h>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <exception>
#include <format>
#include <fstream>
#include <system_error>

namespace ByteForge
{
    namespace
    {
        constexpr const char* WindowTitle = "ByteForge Editor";

        fs::path GetStateFile()
        {
            return fs::current_path() / ".byteforge" / "EditorState.json";
        }

        nlohmann::json ReadState()
        {
            std::ifstream file(GetStateFile());
            if (!file.is_open())
                return nlohmann::json::object();

            nlohmann::json state = nlohmann::json::parse(file, nullptr, false);
            return state.is_object() ? state : nlohmann::json::object();
        }

        void WriteState(const nlohmann::json& state)
        {
            const fs::path file = GetStateFile();

            std::error_code error;
            fs::create_directories(file.parent_path(), error);

            std::ofstream(file) << state.dump(4);
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
        if (!m_Handle)
            return false;

        return Write(AssetRegistry::Resolve(*m_Handle));
    }

    bool SceneDocument::SaveAs(const fs::path& relativePath)
    {
        const fs::path absolutePath = AssetRegistry::GetAssetRoot() / relativePath;

        std::error_code error;
        fs::create_directories(absolutePath.parent_path(), error);

        if (!Write(absolutePath))
            return false;

        m_Handle = AssetRegistry::Import(relativePath);
        RememberAsLast();
        return true;
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

        APP_INFO("'{}' is now the startup scene", GetPath().generic_string());
        return true;
    }

    bool SceneDocument::IsDirty() const { return m_Context.History.IsDirty(); }

    fs::path SceneDocument::GetPath() const
    {
        AssetMetadata metadata;
        if (!m_Handle || !AssetRegistry::TryGetMetadata(*m_Handle, metadata))
            return {};
        return metadata.Path;
    }

    std::string SceneDocument::GetName() const
    {
        const fs::path path = GetPath();
        return path.empty() ? "Untitled" : path.stem().string();
    }

    void SceneDocument::Update() const
    {
        Application::Get().GetWindow().SetTitle(std::format("{} - {}{}", WindowTitle, GetName(), IsDirty() ? "*" : ""));
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

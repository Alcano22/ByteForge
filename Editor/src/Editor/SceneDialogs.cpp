#include "Editor/SceneDialogs.h"
#include "Editor/EditorContext.h"
#include "Editor/FileDialogs.h"

#include <Engine/Assets/AssetRegistry.h>
#include <Engine/Core/Application.h>
#include <Engine/Core/Log.h>

#include <imgui.h>

#include <array>
#include <optional>
#include <system_error>
#include <utility>

namespace ByteForge
{
    namespace
    {
        constexpr const char* UnsavedChangesId = "Unsaved Changes";

        constexpr std::array<FileFilter, 1> SceneFilters{ { { "ByteForge Scene", "bfscene" } } };

        std::optional<std::filesystem::path> ToAssetPath(const std::filesystem::path& absolutePath)
        {
            std::error_code error;
            const std::filesystem::path root =
                std::filesystem::weakly_canonical(AssetRegistry::GetAssetRoot(), error);
            const std::filesystem::path relative =
                std::filesystem::weakly_canonical(absolutePath, error).lexically_relative(root);

            if (relative.empty() || relative.begin()->string() == "..")
                return std::nullopt;
            return relative;
        }

        void CenterNextWindow()
        {
            ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        }
    }

    void SceneDialogs::RequestNew()
    {
        if (EnsureEditing("create a scene"))
            Guard([this] { m_Context.Document.New(); });
    }

    void SceneDialogs::RequestOpen(const UUID scene)
    {
        if (EnsureEditing("open a scene"))
            Guard([this, scene] { m_Context.Document.Open(scene); });
    }

    void SceneDialogs::RequestOpenFromDisk()
    {
        if (!EnsureEditing("open a scene")) return;

        const std::optional<std::filesystem::path> chosen = FileDialogs::OpenFile(SceneFilters, GetDefaultDirectory());
        if (!chosen) return;

        const std::optional<std::filesystem::path> relative = ToAssetPath(*chosen);
        if (!relative)
        {
            APP_ERROR("Scenes must be inside the asset folder: '{}'", chosen->string());
            return;
        }

        RequestOpen(AssetRegistry::Import(*relative));
    }

    bool SceneDialogs::RequestSave()
    {
        if (!EnsureEditing("save"))
            return false;

        return m_Context.Document.HasFile() ? m_Context.Document.Save() : RequestSaveAs();
    }

    bool SceneDialogs::RequestSaveAs()
    {
        if (!EnsureEditing("save"))
            return false;

        const std::string defaultName = m_Context.Document.GetName() + SceneDocument::Extension;
        const std::optional<std::filesystem::path> chosen =
            FileDialogs::SaveFile(SceneFilters, GetDefaultDirectory(), defaultName);
        if (!chosen)
            return false;

        const std::optional<std::filesystem::path> relative = ToAssetPath(*chosen);
        if (!relative)
        {
            APP_ERROR("Scenes must be saved inside the asset folder: '{}'", chosen->string());
            return false;
        }

        return m_Context.Document.SaveAs(SceneDocument::ToScenePath(relative->generic_string()));
    }

    bool SceneDialogs::RequestQuit()
    {
        if (!m_Context.Document.IsDirty())
            return false;

        if (!m_Context.IsEditing())
            m_Context.OnSceneStop();

        Guard([] { Application::Get().Close(); });
        return true;
    }

    void SceneDialogs::Draw()
    {
        if (std::exchange(m_PromptRequested, false))
            ImGui::OpenPopup(UnsavedChangesId);

        CenterNextWindow();
        if (!ImGui::BeginPopupModal(UnsavedChangesId, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;

        ImGui::Text("Save changes to '%s'?", m_Context.Document.GetName().c_str());
        ImGui::TextDisabled("Your changes are lost if you don't save them.");
        ImGui::Spacing();

        if (ImGui::Button("Save"))
        {
            ImGui::CloseCurrentPopup();

            if (RequestSave())
                RunContinuation();
            else
                m_Continuation = nullptr;
        }

        ImGui::SameLine();
        if (ImGui::Button("Don't Save"))
        {
            ImGui::CloseCurrentPopup();
            RunContinuation();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            ImGui::CloseCurrentPopup();
            m_Continuation = nullptr;
        }

        ImGui::EndPopup();
    }

    void SceneDialogs::Guard(std::function<void()> action)
    {
        if (!m_Context.Document.IsDirty())
        {
            action();
            return;
        }

        m_Continuation = std::move(action);
        m_PromptRequested = true;
    }

    void SceneDialogs::RunContinuation()
    {
        if (const std::function<void()> action = std::exchange(m_Continuation, {}))
            action();
    }

    bool SceneDialogs::EnsureEditing(const char* what) const
    {
        if (m_Context.IsEditing())
            return true;

        APP_WARN("Stop the play session to {}", what);
        return false;
    }

    std::filesystem::path SceneDialogs::GetDefaultDirectory() const
    {
        const std::filesystem::path root = std::filesystem::absolute(AssetRegistry::GetAssetRoot());

        if (const std::filesystem::path current = m_Context.Document.GetPath(); !current.empty())
            return (root / current).parent_path();

        std::error_code error;
        const std::filesystem::path scenes = root / "scenes";
        return std::filesystem::is_directory(scenes, error) ? scenes : root;
    }
}

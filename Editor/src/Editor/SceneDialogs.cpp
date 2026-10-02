#include "Editor/SceneDialogs.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorWidgets.h"

#include <Engine/Assets/AssetRegistry.h>
#include <Engine/Core/Application.h>
#include <Engine/Core/Log.h>

#include <imgui.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace ByteForge
{
    namespace
    {
        constexpr const char* UnsavedChangesId = "Unsaved Changes";
        constexpr const char* SaveAsId = "Save Scene As";
        constexpr const char* OpenSceneId = "Open Scene";

        const char* GetPopupId(const auto popup)
        {
            switch (popup)
            {
                case decltype(popup)::UnsavedChanges: return UnsavedChangesId;
                case decltype(popup)::SaveAs:         return SaveAsId;
                case decltype(popup)::OpenScene:      return OpenSceneId;
                case decltype(popup)::None:           break;
            }
            return nullptr;
        }

        void CenterNextWindow()
        {
            ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        }

        void CopyToBuffer(std::array<char, 128>& buffer, const std::string& text)
        {
            const size_t length = std::min(text.size(), buffer.size() - 1);
            std::memcpy(buffer.data(), text.data(), length);
            buffer[length] = '\0';
        }

        const char* ValidateScenePath(const std::string_view input, const std::filesystem::path& path,
                                      const std::filesystem::path& currentPath)
        {
            if (input.empty() || path.filename().string() == SceneDocument::Extension)
                return "Enter a name";

            if (path.is_absolute() || (path.begin() != path.end() && path.begin()->string() == ".."))
                return "The scene must be inside the asset folder";

            std::error_code error;
            if (path != currentPath && std::filesystem::exists(AssetRegistry::GetAssetRoot() / path, error))
                return "A file with this name already exists";

            return nullptr;
        }

        bool CancelPressed()
        {
            return ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape);
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

    void SceneDialogs::RequestOpenPicker()
    {
        if (EnsureEditing("open a scene"))
            m_Requested = Popup::OpenScene;
    }

    void SceneDialogs::RequestSave()
    {
        if (!EnsureEditing("save")) return;

        if (m_Context.Document.HasFile())
            m_Context.Document.Save();
        else
            RequestSaveAs();
    }

    void SceneDialogs::RequestSaveAs()
    {
        if (!EnsureEditing("save")) return;

        std::filesystem::path suggestion = m_Context.Document.GetPath();
        if (suggestion.empty())
            suggestion = std::filesystem::path("Scenes") / "Untitled";
        suggestion.replace_extension();

        CopyToBuffer(m_NameBuffer, suggestion.generic_string());
        m_Requested = Popup::SaveAs;
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
        if (m_Requested != Popup::None)
        {
            ImGui::OpenPopup(GetPopupId(m_Requested));
            m_Requested = Popup::None;
        }

        DrawUnsavedChanges();
        DrawSaveAs();
        DrawOpenScene();
    }

    void SceneDialogs::Guard(std::function<void()> action)
    {
        if (!m_Context.Document.IsDirty())
        {
            action();
            return;
        }

        m_Continuation = std::move(action);
        m_Requested = Popup::UnsavedChanges;
    }

    void SceneDialogs::RunContinuation()
    {
        if (std::function<void()> action = std::exchange(m_Continuation, {}))
            action();
    }

    bool SceneDialogs::EnsureEditing(const char* what) const
    {
        if (m_Context.IsEditing())
            return true;

        APP_WARN("Stop the play session to {}", what);
        return false;
    }

    void SceneDialogs::DrawUnsavedChanges()
    {
        CenterNextWindow();
        if (!ImGui::BeginPopupModal(UnsavedChangesId, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;

        ImGui::Text("Save changes to '%s'?", m_Context.Document.GetName().c_str());
        ImGui::TextDisabled("Your changes are lost if you don't save them.");
        ImGui::Spacing();

        if (ImGui::Button("Save"))
        {
            ImGui::CloseCurrentPopup();

            if (!m_Context.Document.HasFile())
                RequestSaveAs();
            else if (m_Context.Document.Save())
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
        if (CancelPressed())
        {
            ImGui::CloseCurrentPopup();
            m_Continuation = nullptr;
        }

        ImGui::EndPopup();
    }

    void SceneDialogs::DrawSaveAs()
    {
        CenterNextWindow();
        if (!ImGui::BeginPopupModal(SaveAsId, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;

        ImGui::TextUnformatted("Path inside the asset folder");

        if (ImGui::IsWindowAppearing())
            ImGui::SetKeyboardFocusHere();

        ImGui::SetNextItemWidth(320.0f);
        const bool submitted = ImGui::InputText("##path", m_NameBuffer.data(), m_NameBuffer.size(),
                                                ImGuiInputTextFlags_EnterReturnsTrue);

        const std::string_view input(m_NameBuffer.data());
        const std::filesystem::path path = SceneDocument::ToScenePath(input);
        const char* error = ValidateScenePath(input, path, m_Context.Document.GetPath());

        if (error != nullptr)
            ImGui::TextColored(EditorUI::ErrorColor, "%s", error);
        else
            ImGui::TextDisabled("Assets/%s", path.generic_string().c_str());

        ImGui::Spacing();

        ImGui::BeginDisabled(error != nullptr);
        const bool save = ImGui::Button("Save") || (submitted && error == nullptr);
        ImGui::EndDisabled();

        if (save)
        {
            ImGui::CloseCurrentPopup();

            if (m_Context.Document.SaveAs(path))
                RunContinuation();
            else
                m_Continuation = nullptr;
        }

        ImGui::SameLine();
        if (CancelPressed())
        {
            ImGui::CloseCurrentPopup();
            m_Continuation = nullptr;
        }

        ImGui::EndPopup();
    }

    void SceneDialogs::DrawOpenScene()
    {
        CenterNextWindow();
        ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f), ImGuiCond_Appearing);
        if (!ImGui::BeginPopupModal(OpenSceneId, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;

        const std::vector<AssetMetadata> scenes = AssetRegistry::GetAssetsOfType(AssetType::Scene);
        const std::filesystem::path current = m_Context.Document.GetPath();

        if (scenes.empty())
            ImGui::TextDisabled("No scenes in the asset folder yet");

        for (const AssetMetadata& scene : scenes)
        {
            const std::string label = scene.Path.generic_string();
            if (ImGui::Selectable(label.c_str(), scene.Path == current))
            {
                ImGui::CloseCurrentPopup();
                RequestOpen(scene.Handle);
            }
        }

        ImGui::Separator();
        if (CancelPressed())
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
}

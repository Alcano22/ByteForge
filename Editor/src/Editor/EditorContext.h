#pragma once

#include "Editor/EditorPanel.h"
#include "Editor/SceneState.h"
#include "Editor/Selection.h"
#include "Editor/EditorIcons.h"
#include "Editor/EditorFonts.h"
#include "Editor/ScriptProject.h"
#include "Editor/SceneDocument.h"
#include "Editor/SceneDialogs.h"
#include "Editor/Commands/CommandHistory.h"

#include <Engine/Core/Core.h>
#include <Engine/Core/Timestep.h>
#include <Engine/Scene/Entity.h>
#include <Engine/Scene/UUID.h>
#include <Engine/Renderer/Texture2D.h>

#include <nlohmann/json.hpp>

#include <type_traits>
#include <typeindex>
#include <utility>
#include <vector>
#include <functional>
#include <string>
#include <filesystem>

namespace ByteForge
{
    class Scene;

    class EditorContext
    {
    public:
        template<typename T, typename... Args>
        T& Open(Args&&... args)
        {
            static_assert(std::is_base_of_v<EditorPanel, T>, "T must derive from EditorPanel");

            if (T* existing = TryGet<T>())
            {
                existing->SetOpen(true);
                return *existing;
            }

            auto panel = MakeScope<T>(*this, std::forward<Args>(args)...);
            T& ref = *panel;
            m_Panels.emplace_back(std::type_index(typeid(T)), std::move(panel));
            return ref;
        }

        template<typename T>
        void Close()
        {
            if (T* existing = TryGet<T>())
                existing->SetOpen(false);
        }

        template<typename T>
        [[nodiscard]] bool IsOpen() const
        {
            const T* existing = TryGet<T>();
            return existing != nullptr && existing->IsOpen();
        }

        void OnUpdate(const Timestep ts)
        {
            Scripts.Update(IsEditing());
            Document.Update();
            FlushDeferred();

            for (const auto& [type, panel] : m_Panels)
            {
                if (panel->IsOpen())
                    panel->OnUpdate(ts);
            }
        }

        void OnImGuiRender() const
        {
            for (const auto& [type, panel] : m_Panels)
            {
                if (panel->IsOpen())
                    panel->OnImGuiRender();
            }
        }

        void OnScenePlay();
        void OnSceneStop();
        void OnScenePause();
        void OnSceneResume();
        void OnSceneStep();

        [[nodiscard]] bool IsPlaying() const { return State == SceneState::Play; }
        [[nodiscard]] bool IsPaused() const { return State == SceneState::Pause; }
        [[nodiscard]] bool IsEditing() const { return State == SceneState::Edit; }
        [[nodiscard]] bool CanPlay() const { return IsEditing() && ActiveScene != nullptr && !Scripts.IsBuilding(); }

        [[nodiscard]] bool ConsumeStepRequest()
        {
            if (!m_StepRequested)
                return false;

            m_StepRequested = false;
            return true;
        }

        static void ApplyTextureSettings(UUID handle, const TextureSettings& settings);

        void CreateEntity(std::string name, std::function<void(Entity)> setup = {});
        void RenameEntity(Entity entity, std::string name);
        void DuplicateEntity(Entity entity);
        void DestroyEntity(Entity entity);

        bool Undo();
        bool Redo();

        template<typename Fn>
        void ForEachPanel(Fn&& fn) const
        {
            for (const auto& [type, panel] : m_Panels)
                fn(*panel);
        }

    private:
        template<typename T>
        [[nodiscard]] T* TryGet() const
        {
            const auto typeIndex = std::type_index(typeid(T));
            for (const auto& [type, panel] : m_Panels)
            {
                if (type == typeIndex)
                    return static_cast<T*>(panel.get());
            }
            return nullptr;
        }

        void Defer(std::function<void()> action);
        void FlushDeferred();

        [[nodiscard]] std::string MakeUniqueName(const std::string& name) const;

        void RecordIfEditing(Scope<EditorCommand> command);
        void DropInvalidSelection();

    public:
        Scene* ActiveScene = nullptr;
        SceneDocument Document{ *this };
        SceneDialogs Dialogs{ *this };
        Selection SelectionContext;
        CommandHistory History;
        SceneState State = SceneState::Edit;
        EditorIcons Icons;
        EditorFonts Fonts;
        ScriptProject Scripts{ std::filesystem::current_path() };

    private:
        std::vector<std::pair<std::type_index, Scope<EditorPanel>>> m_Panels;

        nlohmann::json m_EditSceneSnapshot;
        bool m_StepRequested = false;

        std::vector<std::function<void()>> m_Deferred;
    };
}

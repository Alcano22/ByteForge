#include "Editor/EditorContext.h"

#include <Engine/Assets/AssetManager.h>
#include <Engine/Assets/AssetRegistry.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Scene/SceneSerializer.h>
#include <Engine/Scene/Components.h>

#include <algorithm>
#include <cctype>
#include <format>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace ByteForge
{
    void EditorContext::OnScenePlay()
    {
        if (State != SceneState::Edit || ActiveScene == nullptr) return;

        m_EditSceneSnapshot = SceneSerializer::Serialize(*ActiveScene);
        SelectionContext.ClearEntity();
        State = SceneState::Play;
    }

    void EditorContext::OnSceneStop()
    {
        if (State == SceneState::Edit || ActiveScene == nullptr) return;

        SceneSerializer::Deserialize(*ActiveScene, m_EditSceneSnapshot);
        m_EditSceneSnapshot = nlohmann::json();
        m_StepRequested = false;
        SelectionContext.ClearEntity();
        State = SceneState::Edit;
    }

    void EditorContext::OnScenePause()
    {
        if (State == SceneState::Play)
            State = SceneState::Pause;
    }

    void EditorContext::OnSceneResume()
    {
        if (State == SceneState::Pause)
            State = SceneState::Play;
    }

    void EditorContext::OnSceneStep()
    {
        if (State == SceneState::Pause)
            m_StepRequested = true;
    }

    void EditorContext::ApplyTextureSettings(const UUID handle, const TextureSettings& settings)
    {
        if (AssetRegistry::SetSettings(handle, settings))
            AssetManager::Reload(handle);
    }

    void EditorContext::CreateEntity(std::string name, std::function<void(Entity)> setup)
    {
        Defer([this, name = std::move(name), setup = std::move(setup)]
        {
            if (ActiveScene == nullptr) return;

            const Entity entity = ActiveScene->CreateEntity(MakeUniqueName(name));
            if (setup)
                setup(entity);

            SelectionContext.Select(entity);
        });
    }

    void EditorContext::DuplicateEntity(const Entity entity)
    {
        Defer([this, entity]
        {
            if (ActiveScene == nullptr || !entity.IsValid()) return;

            const Entity copy = ActiveScene->DuplicateEntity(entity);
            copy.GetComponent<TagComponent>().Tag = MakeUniqueName(entity.GetTag());
            SelectionContext.Select(copy);
        });
    }

    void EditorContext::DestroyEntity(const Entity entity)
    {
        Defer([this, entity]
        {
            if (ActiveScene == nullptr || !entity.IsValid()) return;

            if (SelectionContext.IsEntity(entity))
                SelectionContext.Clear();

            ActiveScene->DestroyEntity(entity);
        });
    }

    void EditorContext::Defer(std::function<void()> action)
    {
        m_Deferred.push_back(std::move(action));
    }

    void EditorContext::FlushDeferred()
    {
        const std::vector<std::function<void()>> actions = std::exchange(m_Deferred, {});
        for (const auto& action : actions)
            action();
    }

    std::string EditorContext::MakeUniqueName(const std::string& name) const
    {
        std::string_view base = name;

        if (base.ends_with(')'))
        {
            if (const size_t open = base.rfind(" ("); open != std::string_view::npos)
            {
                const std::string_view digits = base.substr(open + 2, base.size() - open - 3);
                const auto isDigit = [](const char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; };
                if (!digits.empty() && std::ranges::all_of(digits, isDigit))
                    base = base.substr(0, open);
            }
        }

        std::unordered_set<std::string> taken;
        ActiveScene->Each<TagComponent>([&taken](const Entity, const TagComponent& tag) { taken.insert(tag.Tag); });

        if (!taken.contains(std::string(base)))
            return std::string(base);

        for (int index = 1;; ++index)
        {
            std::string candidate = std::format("{} ({})", base, index);
            if (!taken.contains(candidate))
                return candidate;
        }
    }
}

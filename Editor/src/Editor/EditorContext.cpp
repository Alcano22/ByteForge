#include "Editor/EditorContext.h"
#include "Editor/Commands/EntityCommands.h"

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
#include <variant>

namespace ByteForge
{
    void EditorContext::OnScenePlay()
    {
        if (State != SceneState::Edit || ActiveScene == nullptr) return;

        m_EditSceneSnapshot = SceneSerializer::Serialize(*ActiveScene);
        ActiveScene->OnRuntimeStart();
        SelectionContext.ClearEntity();
        State = SceneState::Play;
    }

    void EditorContext::OnSceneStop()
    {
        if (State == SceneState::Edit || ActiveScene == nullptr) return;

        ActiveScene->OnRuntimeStop();
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

            RecordIfEditing(MakeScope<CreateEntityCommand>(*ActiveScene, std::format("Create '{}'", entity.GetTag()),
                                                           SceneSerializer::SerializeEntity(entity)));
            SelectionContext.Select(entity);
        });
    }

    void EditorContext::RenameEntity(const Entity entity, std::string name)
    {
        if (!entity.IsValid() || name.empty() || entity.GetTag() == name) return;

        nlohmann::json before = SceneSerializer::SerializeEntity(entity);
        const std::string label = std::format("Rename '{}' to '{}'", entity.GetTag(), name);

        entity.GetComponent<TagComponent>().Tag = std::move(name);

        RecordIfEditing(MakeScope<ModifyEntityCommand>(entity.GetScene(), label, std::move(before),
                                                       SceneSerializer::SerializeEntity(entity)));
    }

    void EditorContext::DuplicateEntity(const Entity entity)
    {
        Defer([this, entity]
        {
            if (ActiveScene == nullptr || !entity.IsValid()) return;

            const Entity copy = ActiveScene->DuplicateEntity(entity);
            copy.GetComponent<TagComponent>().Tag = MakeUniqueName(entity.GetTag());

            RecordIfEditing(MakeScope<CreateEntityCommand>(*ActiveScene, std::format("Duplicate '{}'", entity.GetTag()),
                                                           SceneSerializer::SerializeEntity(copy)));
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

            if (!IsEditing())
            {
                ActiveScene->DestroyEntity(entity);
                return;
            }

            History.Execute(MakeScope<DestroyEntityCommand>(*ActiveScene, std::format("Delete '{}'", entity.GetTag()),
                                                            SceneSerializer::SerializeEntity(entity)));
        });
    }

    bool EditorContext::Undo()
    {
        if (!IsEditing())
            return false;

        const bool undone = History.Undo();
        DropInvalidSelection();
        return undone;
    }

    bool EditorContext::Redo()
    {
        if (!IsEditing())
            return false;

        const bool redone = History.Redo();
        DropInvalidSelection();
        return redone;
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

    void EditorContext::RecordIfEditing(Scope<EditorCommand> command)
    {
        if (IsEditing())
            History.Record(std::move(command));
    }

    void EditorContext::DropInvalidSelection()
    {
        if (std::holds_alternative<Entity>(SelectionContext.Get()) && !SelectionContext.GetEntity().IsValid())
            SelectionContext.Clear();
    }
}

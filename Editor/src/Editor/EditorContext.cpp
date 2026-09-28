#include "Editor/EditorContext.h"

#include <Engine/Core/Log.h>
#include <Engine/Assets/AssetManager.h>
#include <Engine/Assets/AssetRegistry.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Scene/SceneSerializer.h>
#include <Engine/Scene/Components.h>

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
        try
        {
            const Ref<Texture2D> oldTexture = AssetManager::LoadTexture2D(handle);

            if (!AssetRegistry::SetSettings(handle, settings)) return;

            const Ref<Texture2D> newTexture = AssetManager::Reload(handle);
            if (ActiveScene == nullptr || !oldTexture || !newTexture || oldTexture == newTexture) return;

            ActiveScene->Each<SpriteRendererComponent>([&](Entity, SpriteRendererComponent& sprite)
            {
                if (sprite.SubTexture && sprite.SubTexture->GetTexture() == oldTexture)
                {
                    sprite.SubTexture = MakeRef<SubTexture2D>(newTexture, sprite.SubTexture->GetUVMin(),
                                                              sprite.SubTexture->GetUVMax());
                }
            });
        } catch (const std::exception& e)
        {
            APP_ERROR("Failed to apply texture settings: {}", e.what());
        }
    }
}

#include "Editor/EditorContext.h"

#include <Engine/Scene/Scene.h>
#include <Engine/Scene/SceneSerializer.h>

namespace ByteForge
{
    void EditorContext::OnScenePlay()
    {
        if (State != SceneState::Edit || ActiveScene == nullptr) return;

        m_EditSceneSnapshot = SceneSerializer::Serialize(*ActiveScene);
        SelectionContext = Entity{};
        State = SceneState::Play;
    }

    void EditorContext::OnSceneStop()
    {
        if (State == SceneState::Edit || ActiveScene == nullptr) return;

        SceneSerializer::Deserialize(*ActiveScene, m_EditSceneSnapshot);
        m_EditSceneSnapshot = nlohmann::json();
        m_StepRequested = false;
        SelectionContext = Entity{};
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
}

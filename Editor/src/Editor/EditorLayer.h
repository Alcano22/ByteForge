#pragma once

#include "Editor/EditorContext.h"

#include <Engine/Core/Layer.h>
#include <Engine/Scene/Scene.h>

namespace ByteForge
{
    class EditorLayer : public Layer
    {
    public:
        EditorLayer()
            : Layer("EditorLayer") {}

        void OnAttach() override;

        void OnUpdate(Timestep ts) override;
        void OnImGuiRender() override;

    private:
        EditorContext m_Context;
        Scene m_Scene;
    };
}

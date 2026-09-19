#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/Mesh.h"

#include <glm/glm.hpp>

namespace ByteForge
{
    class BYTEFORGE_API Renderer
    {
    public:
        static void BeginFrame();
        static void Submit(const Ref<Shader>& shader, const Ref<Mesh>& mesh,
                           const glm::mat4& transform = glm::mat4(1.0f));
        static void EndFrame();

        static void OnWindowResized();

        static void WaitIdle();
    };
}

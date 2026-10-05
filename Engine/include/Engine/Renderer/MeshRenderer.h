#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Core/NonCopyable.h"
#include "Engine/Renderer/Camera.h"
#include "Engine/Renderer/ImageFormat.h"
#include "Engine/Renderer/Material.h"
#include "Engine/Renderer/Mesh.h"
#include "Engine/Renderer/Pipeline.h"
#include "Engine/Renderer/SceneLighting.h"

#include <glm/glm.hpp>

namespace ByteForge
{
    struct MeshRendererSpec
    {
        ImageFormat ColorFormat = ImageFormat::Swapchain;
        ImageFormat DepthFormat = ImageFormat::None;
    };

    class BYTEFORGE_API MeshRenderer : NonCopyable
    {
    public:
        explicit MeshRenderer(const MeshRendererSpec& spec = {});

        void BeginScene(const Camera& camera, const SceneLighting& lighting = {});
        void EndScene();

        void DrawMesh(const Ref<Mesh>& mesh, const glm::mat4& transform, const glm::vec4& color = glm::vec4(1.0f));

    private:
        struct DrawData
        {
            glm::mat4 Model;
            glm::vec4 Color;
        };
        static_assert(sizeof(DrawData) == 80);

        MeshRendererSpec m_Spec;
        Ref<Pipeline> m_Pipeline;
        Ref<Material> m_Material;

        bool m_InScene = false;
        bool m_LinearizeColors = false;
    };
}

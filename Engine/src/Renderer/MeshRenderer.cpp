#include "Engine/Renderer/MeshRenderer.h"
#include "Engine/Renderer/MeshVertex.h"
#include "Engine/Renderer/Renderer.h"
#include "Renderer/BuiltinShaders.h"
#include "Renderer/ColorSpace.h"

#include <stdexcept>

namespace ByteForge
{
    MeshRenderer::MeshRenderer(const MeshRendererSpec& spec)
        : m_Spec(spec)
    {
        m_LinearizeColors = Renderer::IsSrgb(spec.ColorFormat);

        const bool hasDepth = spec.DepthFormat != ImageFormat::None;

        m_Pipeline = Pipeline::Create({
            .Shader           = Shader::Load(GetBuiltinShaderPath("Mesh.hlsl")),
            .VertexLayout     = MeshVertex::GetLayout(),
            .Cull             = CullMode::Back,
            .Front            = FrontFace::CounterClockwise,
            .ColorAttachments = { ColorAttachment{ .Format = spec.ColorFormat } },
            .DepthFormat      = spec.DepthFormat,
            .DepthTest        = hasDepth,
            .DepthWrite       = hasDepth
        });
        m_Material = Material::Create(m_Pipeline);
    }

    void MeshRenderer::BeginScene(const Camera& camera, const SceneLighting& lighting)
    {
        if (m_InScene)
            throw std::runtime_error("MeshRenderer::BeginScene: the previous scene has not been ended with EndScene");

        Renderer::BeginScene(camera, lighting);
        m_InScene = true;
    }

    void MeshRenderer::EndScene()
    {
        if (!m_InScene)
            throw std::runtime_error("MeshRenderer::EndScene: BeginScene has not been called");

        m_InScene = false;
    }

    void MeshRenderer::DrawMesh(const Ref<Mesh>& mesh, const glm::mat4& transform, const glm::vec4& color)
    {
        if (!m_InScene)
            throw std::runtime_error("MeshRenderer: meshes can only be drawn between BeginScene and EndScene");

        if (!mesh)
            throw std::runtime_error("MeshRenderer::DrawMesh: the mesh must not be null");

        const DrawData data{
            .Model = transform,
            .Color = m_LinearizeColors ? SrgbToLinear(color) : color
        };

        Renderer::Submit(m_Material, mesh, data);
    }
}

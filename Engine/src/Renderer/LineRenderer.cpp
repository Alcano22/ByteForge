#include "Engine/Renderer/LineRenderer.h"
#include "Engine/Renderer/Renderer.h"
#include "Renderer/ColorSpace.h"

#include <array>
#include <cmath>
#include <format>
#include <stdexcept>

namespace ByteForge
{
    namespace
    {
        constexpr const char* VertexSource = R"(
            cbuffer CameraUBO : register(b0)
            {
                float4x4 u_ViewProjection;
            };

            struct VSInput
            {
                float3 Position : POSITION;
                float4 Color    : COLOR;
            };

            struct VSOutput
            {
                float4 Position : SV_Position;
                float4 Color    : COLOR;
            };

            VSOutput main(VSInput input)
            {
                VSOutput output;
                output.Position = mul(u_ViewProjection, float4(input.Position, 1.0));
                output.Color = input.Color;
                return output;
            }
        )";

        constexpr const char* FragmentSource = R"(
            float4 main(float4 color : COLOR) : SV_Target
            {
                return color;
            }
        )";
    }

    LineRenderer::LineRenderer(const LineRendererSpec& spec)
        : m_Spec(spec)
    {
        static_assert(sizeof(Vertex) == 7 * sizeof(float), "Vertex must be tightly packed to match the buffer layout");

        if (spec.MaxLines == 0)
            throw std::runtime_error("LineRendererSpec::MaxLines must be at least 1");

        m_LinearizeColors = Renderer::IsSrgb(spec.ColorFormat);

        const BufferLayout layout = {
            { ShaderDataType::Float3, "Position" },
            { ShaderDataType::Float4, "Color"    }
        };

        const uint32_t vertexCount = spec.MaxLines * VerticesPerLine;
        m_VertexBuffer = VertexBuffer::Create(static_cast<uint32_t>(vertexCount * sizeof(Vertex)));
        m_VertexBuffer->SetLayout(layout);
        m_Mesh = MakeRef<Mesh>(m_VertexBuffer, vertexCount);

        std::vector<ColorAttachment> attachments{ { .Format = spec.ColorFormat, .Blend = BlendMode::Alpha } };
        if (spec.EntityIdFormat != ImageFormat::None)
            attachments.push_back({ .Format = spec.EntityIdFormat, .WriteEnabled = false });

        m_Pipeline = Pipeline::Create({
            .Shader           = Shader::Create(VertexSource, FragmentSource),
            .VertexLayout     = layout,
            .Topology         = PrimitiveTopology::LineList,
            .ColorAttachments = std::move(attachments),
            .DepthFormat      = spec.DepthFormat,
            .DepthTest        = spec.DepthFormat != ImageFormat::None,
            .DepthWrite       = false
        });
        m_Material = Material::Create(m_Pipeline);

        m_Vertices.resize(vertexCount);
    }

    void LineRenderer::BeginScene(const Camera& camera)
    {
        if (m_InScene)
            throw std::runtime_error("LineRenderer::BeginScene: the previous scene has not been ended with EndScene");

        const uint64_t frame = Renderer::GetFrameNumber();
        if (frame != m_Frame)
        {
            m_Frame = frame;
            m_FrameLines = 0;
            m_BatchStart = 0;
        }

        Renderer::BeginScene(camera);
        m_InScene = true;
    }

    void LineRenderer::EndScene()
    {
        if (!m_InScene)
            throw std::runtime_error("LineRenderer::EndScene: BeginScene has not been called");

        Flush();
        m_InScene = false;
    }

    void LineRenderer::DrawLine(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color)
    {
        if (!m_InScene)
            throw std::runtime_error("LineRenderer: lines can only be drawn between BeginScene and EndScene");

        if (m_FrameLines == m_Spec.MaxLines)
        {
            throw std::runtime_error(std::format("LineRenderer: the budget of {} lines per frame is used up; "
                                                 "raise LineRendererSpec::MaxLines", m_Spec.MaxLines));
        }

        const glm::vec4 vertexColor = m_LinearizeColors ? SrgbToLinear(color) : color;

        Vertex* vertices = &m_Vertices[static_cast<size_t>(m_FrameLines) * VerticesPerLine];
        vertices[0] = { .Position = from, .Color = vertexColor };
        vertices[1] = { .Position = to, .Color = vertexColor };

        ++m_FrameLines;
    }

    void LineRenderer::DrawRect(const glm::vec3& center, const glm::vec2& size, const float rotation,
                                const glm::vec4& color)
    {
        const glm::vec2 half = size * 0.5f;
        const float cosine = std::cos(rotation);
        const float sine = std::sin(rotation);

        const auto corner = [&](const float x, const float y)
        {
            return glm::vec3(center.x + x * cosine - y * sine, center.y + x * sine + y * cosine, center.z);
        };

        const std::array<glm::vec3, 4> corners{
            corner(-half.x, -half.y), corner(half.x, -half.y), corner(half.x, half.y), corner(-half.x, half.y)
        };

        for (size_t i = 0; i < corners.size(); ++i)
            DrawLine(corners[i], corners[(i + 1) % corners.size()], color);
    }

    void LineRenderer::Flush()
    {
        const uint32_t lineCount = m_FrameLines - m_BatchStart;
        if (lineCount == 0) return;

        const uint32_t firstVertex = m_BatchStart * VerticesPerLine;
        const uint32_t vertexCount = lineCount * VerticesPerLine;

        m_VertexBuffer->SetData(&m_Vertices[firstVertex], static_cast<uint32_t>(vertexCount * sizeof(Vertex)),
                                static_cast<uint32_t>(firstVertex * sizeof(Vertex)));

        Renderer::Submit(m_Material, m_Mesh, DrawRange{ .Count = vertexCount, .First = firstVertex });
        m_BatchStart = m_FrameLines;
    }
}

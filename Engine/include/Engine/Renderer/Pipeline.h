#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Renderer/Buffer.h"
#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/ImageFormat.h"

#include <cstdint>
#include <vector>

namespace ByteForge
{
    enum class PrimitiveTopology { TriangleList, TriangleStrip, LineList };
    enum class CullMode { None, Front, Back };
    enum class FrontFace { Clockwise, CounterClockwise };
    enum class BlendMode { None, Alpha };
    enum class CompareOp { Less, LessOrEqual, Always };

    struct ColorAttachment
    {
        ImageFormat Format = ImageFormat::Swapchain;
        BlendMode Blend = BlendMode::None;

        bool WriteEnabled = true;
    };

    struct PipelineSpec
    {
        Ref<ByteForge::Shader> Shader;
        BufferLayout VertexLayout;
        PrimitiveTopology Topology = PrimitiveTopology::TriangleList;
        CullMode Cull = CullMode::None;
        FrontFace Front = FrontFace::Clockwise;

        std::vector<ColorAttachment> ColorAttachments{ ColorAttachment{} };
        ImageFormat DepthFormat = ImageFormat::None;

        bool DepthTest = false;
        bool DepthWrite = false;
        CompareOp DepthCompare = CompareOp::Less;
    };

    class BYTEFORGE_API Pipeline
    {
    public:
        virtual ~Pipeline() = default;

        static Ref<Pipeline> Create(const PipelineSpec& spec);
    };
}

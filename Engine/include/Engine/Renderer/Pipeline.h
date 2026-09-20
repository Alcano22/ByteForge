#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Renderer/Buffer.h"
#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/ImageFormat.h"

#include <cstdint>

namespace ByteForge
{
    enum class PrimitiveTopology { TriangleList, TriangleStrip, LineList };
    enum class CullMode { None, Front, Back };
    enum class FrontFace { Clockwise, CounterClockwise };
    enum class BlendMode { None, Alpha };
    enum class CompareOp { Less, LessOrEqual, Always };

    struct PipelineSpec
    {
        Ref<Shader> Shader;
        BufferLayout VertexLayout;
        PrimitiveTopology Topology = PrimitiveTopology::TriangleList;
        CullMode Cull = CullMode::None;
        FrontFace Front = FrontFace::Clockwise;
        BlendMode Blend = BlendMode::None;

        ImageFormat ColorFormat = ImageFormat::Swapchain;
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

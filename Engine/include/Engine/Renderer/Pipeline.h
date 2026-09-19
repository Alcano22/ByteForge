#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Renderer/Buffer.h"
#include "Engine/Renderer/Shader.h"

#include <cstdint>

namespace ByteForge
{
    enum class PrimitiveTopology { TriangleList, TriangleStrip, LineList };
    enum class CullMode { None, Front, Back };
    enum class FrontFace { Clockwise, CounterClockwise };
    enum class BlendMode { None, Alpha };

    struct PipelineSpec
    {
        Ref<Shader> Shader;
        BufferLayout VertexLayout;
        PrimitiveTopology Topology = PrimitiveTopology::TriangleList;
        CullMode Cull = CullMode::None;
        FrontFace Front = FrontFace::Clockwise;
        BlendMode Blend = BlendMode::None;
    };

    class BYTEFORGE_API Pipeline
    {
    public:
        virtual ~Pipeline() = default;

        static Ref<Pipeline> Create(const PipelineSpec& spec);
    };
}

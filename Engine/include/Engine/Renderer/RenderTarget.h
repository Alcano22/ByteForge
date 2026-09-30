#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Renderer/ImageFormat.h"

#include <glm/glm.hpp>

#include <cstdint>

namespace ByteForge
{
    struct RenderTargetSpec
    {
        uint32_t Width = 1280;
        uint32_t Height = 720;
        ImageFormat ColorFormat = ImageFormat::RGBA8_SRGB;
        ImageFormat DepthFormat = ImageFormat::Depth32F;
        glm::vec4 ClearColor{ 0.01f, 0.01f, 0.01f, 1.0f };
    };

    class BYTEFORGE_API RenderTarget
    {
    public:
        virtual ~RenderTarget() = default;

        [[nodiscard]] virtual uint32_t GetWidth() const = 0;
        [[nodiscard]] virtual uint32_t GetHeight() const = 0;

        static Ref<RenderTarget> Create(const RenderTargetSpec& spec);
    };
}

#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Renderer/ImageFormat.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace ByteForge
{
    struct RenderTargetSpec
    {
        u32 Width = 1280;
        u32 Height = 720;

        std::vector<ImageFormat> ColorFormats{ ImageFormat::RGBA8_SRGB };
        ImageFormat DepthFormat = ImageFormat::Depth32F;
        glm::vec4 ClearColor{ 0.01f, 0.01f, 0.01f, 1.0f };
    };

    class BYTEFORGE_API RenderTarget
    {
    public:
        virtual ~RenderTarget() = default;

        [[nodiscard]] virtual u32 GetWidth() const = 0;
        [[nodiscard]] virtual u32 GetHeight() const = 0;

        [[nodiscard]] virtual u32 ReadPixel(u32 attachment, u32 x, u32 y) const = 0;

        static Ref<RenderTarget> Create(const RenderTargetSpec& spec);
    };
}

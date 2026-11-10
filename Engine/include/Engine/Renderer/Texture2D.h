#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Renderer/ImageFormat.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>

namespace ByteForge
{
    enum class TextureFilter { Nearest, Linear };
    enum class TextureWrap { Repeat, ClampToEdge };

    struct TextureSettings
    {
        ImageFormat Format = ImageFormat::RGBA8_SRGB;
        TextureFilter Filter = TextureFilter::Linear;
        TextureWrap Wrap = TextureWrap::Repeat;
        bool GenerateMips = true;
    };

    class BYTEFORGE_API Texture2D
    {
    public:
        virtual ~Texture2D() = default;

        [[nodiscard]] virtual u32 GetWidth() const = 0;
        [[nodiscard]] virtual u32 GetHeight() const = 0;
        [[nodiscard]] virtual u32 GetMipLevels() const = 0;
        [[nodiscard]] virtual TextureFilter GetFilter() const = 0;

        static Ref<Texture2D> Create(u32 width, u32 height, std::span<const std::byte> pixels,
                                     const TextureSettings& settings = {});

        static Ref<Texture2D> Load(const std::filesystem::path& path, const TextureSettings& settings = {});
    };
}

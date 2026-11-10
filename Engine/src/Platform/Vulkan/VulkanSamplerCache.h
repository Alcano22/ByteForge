#pragma once

#include "Engine/Core/NonCopyable.h"
#include "Engine/Renderer/Texture2D.h"

#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>

namespace ByteForge
{
    class VulkanDevice;

    class VulkanSamplerCache : NonCopyable
    {
    public:
        explicit VulkanSamplerCache(const VulkanDevice& device);
        ~VulkanSamplerCache();

        [[nodiscard]] VkSampler Get(TextureFilter filter, TextureWrap wrap);

    private:
        static constexpr usize FilterCount = 2;
        static constexpr usize WrapCount = 2;

        const VulkanDevice& m_Device;
        std::array<VkSampler, FilterCount * WrapCount> m_Samplers{};
    };
}

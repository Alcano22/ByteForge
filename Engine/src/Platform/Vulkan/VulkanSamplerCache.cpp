#include "Platform/Vulkan/VulkanSamplerCache.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanHelpers.h"

namespace ByteForge
{
    VulkanSamplerCache::VulkanSamplerCache(const VulkanDevice& device)
        : m_Device(device) {}

    VulkanSamplerCache::~VulkanSamplerCache()
    {
        for (const VkSampler sampler : m_Samplers)
        {
            if (sampler != nullptr)
                vkDestroySampler(m_Device.GetHandle(), sampler, nullptr);
        }
    }

    VkSampler VulkanSamplerCache::Get(const TextureFilter filter, const TextureWrap wrap)
    {
        const size_t index = static_cast<size_t>(filter) * WrapCount + static_cast<size_t>(wrap);

        VkSampler& sampler = m_Samplers.at(index);
        if (sampler != nullptr)
            return sampler;

        const bool nearest = filter == TextureFilter::Nearest;
        const VkFilter vkFilter = nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
        const VkSamplerAddressMode addressMode = wrap == TextureWrap::Repeat
                                               ? VK_SAMPLER_ADDRESS_MODE_REPEAT
                                               : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

        const VkSamplerCreateInfo samplerInfo{
            .sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter    = vkFilter,
            .minFilter    = vkFilter,
            .mipmapMode   = nearest ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = addressMode,
            .addressModeV = addressMode,
            .addressModeW = addressMode,
            .maxLod       = VK_LOD_CLAMP_NONE
        };

        VK_CHECK(vkCreateSampler(m_Device.GetHandle(), &samplerInfo, nullptr, &sampler));
        return sampler;
    }
}

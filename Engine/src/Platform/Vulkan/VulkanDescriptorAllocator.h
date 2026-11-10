#pragma once

#include "Engine/Core/NonCopyable.h"

#include <vulkan/vulkan.h>

#include <span>
#include <unordered_map>
#include <vector>

namespace ByteForge
{
    class VulkanDevice;

    class VulkanDescriptorAllocator : NonCopyable
    {
    public:
        explicit VulkanDescriptorAllocator(const VulkanDevice& device);
        ~VulkanDescriptorAllocator();

        [[nodiscard]] std::vector<VkDescriptorSet> Allocate(VkDescriptorSetLayout layout, u32 count);

        void Free(std::span<const VkDescriptorSet> sets);

    private:
        VkDescriptorPool CreatePool() const;
        VkResult TryAllocate(VkDescriptorPool pool, VkDescriptorSetLayout layout,
                             std::vector<VkDescriptorSet>& sets) const;

    public:
        static constexpr u32 SetsPerPool = 64;
        static constexpr u32 MaxUniformBuffersPerSet = 4;
        static constexpr u32 MaxSampledImagesPerSet = 8;
        static constexpr u32 MaxSamplersPerSet = 8;

    private:
        const VulkanDevice& m_Device;
        std::vector<VkDescriptorPool> m_Pools;
        std::unordered_map<VkDescriptorSet, VkDescriptorPool> m_PoolOfSet;
    };
}

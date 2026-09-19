#include "Platform/Vulkan/VulkanDescriptorAllocator.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanHelpers.h"
#include "Engine/Core/Log.h"

#include <format>
#include <stdexcept>

namespace ByteForge
{
    VulkanDescriptorAllocator::VulkanDescriptorAllocator(const VulkanDevice& device)
        : m_Device(device) {}

    VulkanDescriptorAllocator::~VulkanDescriptorAllocator()
    {
        for (const VkDescriptorPool pool : m_Pools)
            vkDestroyDescriptorPool(m_Device.GetHandle(), pool, nullptr);
    }

    std::vector<VkDescriptorSet> VulkanDescriptorAllocator::Allocate(const VkDescriptorSetLayout layout,
                                                                     const uint32_t count)
    {
        if (count == 0 || count > SetsPerPool)
        {
            throw std::runtime_error(std::format("Cannot allocate {} descriptor sets at once (maximum is {})",
                                                 count, SetsPerPool));
        }

        std::vector<VkDescriptorSet> sets(count);

        for (auto it = m_Pools.rbegin(); it != m_Pools.rend(); ++it)
        {
            const VkResult result = TryAllocate(*it, layout, sets);

            if (result == VK_SUCCESS)
            {
                for (const VkDescriptorSet set : sets)
                    m_PoolOfSet[set] = *it;

                return sets;
            }

            if (result != VK_ERROR_OUT_OF_POOL_MEMORY && result != VK_ERROR_FRAGMENTED_POOL)
                throw VulkanException(result, "vkAllocateDescriptorSets", __FILE__, __LINE__);
        }

        const VkDescriptorPool pool = CreatePool();
        m_Pools.push_back(pool);

        VK_CHECK(TryAllocate(pool, layout, sets));

        for (const VkDescriptorSet set : sets)
            m_PoolOfSet[set] = pool;

        return sets;
    }

    void VulkanDescriptorAllocator::Free(const std::span<const VkDescriptorSet> sets)
    {
        for (const VkDescriptorSet set : sets)
        {
            const auto it = m_PoolOfSet.find(set);
            if (it == m_PoolOfSet.end())
                throw std::runtime_error("Tried to free a descriptor set that was not allocated here");

            VK_CHECK(vkFreeDescriptorSets(m_Device.GetHandle(), it->second, 1, &set));
            m_PoolOfSet.erase(it);
        }
    }

    VkDescriptorPool VulkanDescriptorAllocator::CreatePool() const
    {
        constexpr VkDescriptorPoolSize poolSize{
            .type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = SetsPerPool * UniformBuffersPerSet
        };

        const VkDescriptorPoolCreateInfo poolInfo{
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
            .maxSets       = SetsPerPool,
            .poolSizeCount = 1,
            .pPoolSizes    = &poolSize
        };

        VkDescriptorPool pool = nullptr;
        VK_CHECK(vkCreateDescriptorPool(m_Device.GetHandle(), &poolInfo, nullptr, &pool));

        CORE_INFO("Created descriptor pool for {} sets", SetsPerPool);
        return pool;
    }

    VkResult VulkanDescriptorAllocator::TryAllocate(const VkDescriptorPool pool, const VkDescriptorSetLayout layout,
                                                    std::vector<VkDescriptorSet>& sets) const
    {
        const std::vector<VkDescriptorSetLayout> layouts(sets.size(), layout);

        const VkDescriptorSetAllocateInfo allocInfo{
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool     = pool,
            .descriptorSetCount = static_cast<uint32_t>(sets.size()),
            .pSetLayouts        = layouts.data()
        };

        return vkAllocateDescriptorSets(m_Device.GetHandle(), &allocInfo, sets.data());
    }
}

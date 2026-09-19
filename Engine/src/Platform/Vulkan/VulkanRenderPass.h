#pragma once

#include "Engine/Core/NonCopyable.h"

#include <vulkan/vulkan.h>

namespace ByteForge
{
    class VulkanDevice;

    class VulkanRenderPass : NonCopyable
    {
    public:
        VulkanRenderPass(const VulkanDevice& device, VkFormat colorAttachmentFormat,
                         bool isSecondaryPass = false);
        ~VulkanRenderPass();

        [[nodiscard]] VkRenderPass GetHandle() const { return m_RenderPass; }

    private:
        const VulkanDevice& m_Device;
        VkRenderPass m_RenderPass = nullptr;
    };
}

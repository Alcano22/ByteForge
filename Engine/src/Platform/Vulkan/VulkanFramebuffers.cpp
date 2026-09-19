#include "Platform/Vulkan/VulkanFramebuffers.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanSwapchain.h"
#include "Platform/Vulkan/VulkanRenderPass.h"
#include "Platform/Vulkan/VulkanHelpers.h"
#include "Engine/Core/Log.h"

namespace ByteForge
{
    VulkanFramebuffers::VulkanFramebuffers(const VulkanDevice& device, const VulkanSwapchain& swapchain,
                                           const VulkanRenderPass& renderPass, const bool useImGuiViews)
        : m_Device(device)
    {
        const auto& imageViews = useImGuiViews ? swapchain.GetImGuiImageViews() : swapchain.GetImageViews();
        m_Framebuffers.resize(imageViews.size());

        for (size_t i = 0; i < imageViews.size(); ++i)
        {
            const VkImageView attachments[] = { imageViews[i] };

            const VkFramebufferCreateInfo createInfo{
                .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .renderPass      = renderPass.GetHandle(),
                .attachmentCount = 1,
                .pAttachments    = attachments,
                .width           = swapchain.GetExtent().width,
                .height          = swapchain.GetExtent().height,
                .layers          = 1
            };

            VK_CHECK(vkCreateFramebuffer(m_Device.GetHandle(), &createInfo, nullptr, &m_Framebuffers[i]));
        }

        CORE_INFO("Created {} framebuffers", m_Framebuffers.size());
    }

    VulkanFramebuffers::~VulkanFramebuffers()
    {
        for (const VkFramebuffer framebuffer : m_Framebuffers)
            vkDestroyFramebuffer(m_Device.GetHandle(), framebuffer, nullptr);
    }
}

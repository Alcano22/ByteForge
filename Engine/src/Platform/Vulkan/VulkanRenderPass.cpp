#include "Platform/Vulkan/VulkanRenderPass.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanHelpers.h"
#include "Engine/Core/Log.h"

namespace ByteForge
{
    VulkanRenderPass::VulkanRenderPass(const VulkanDevice& device, const VkFormat colorAttachmentFormat,
                                       const bool isSecondaryPass)
        : m_Device(device)
    {
        const VkAttachmentDescription colorAttachment{
            .format         = colorAttachmentFormat,
            .samples        = VK_SAMPLE_COUNT_1_BIT,
            .loadOp         = isSecondaryPass ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = isSecondaryPass ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        };

        constexpr VkAttachmentReference colorAttachmentRef{
            .attachment = 0,
            .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        };

        const VkSubpassDescription subpass{
            .pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &colorAttachmentRef
        };

        const VkAccessFlags dstAccessMask = isSecondaryPass
                                          ? (VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT)
                                          : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        const VkSubpassDependency dependency{
            .srcSubpass    = VK_SUBPASS_EXTERNAL,
            .dstSubpass    = 0,
            .srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = isSecondaryPass ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : 0u,
            .dstAccessMask = dstAccessMask
        };

        const VkRenderPassCreateInfo renderPassInfo{
            .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments    = &colorAttachment,
            .subpassCount    = 1,
            .pSubpasses      = &subpass,
            .dependencyCount = 1,
            .pDependencies   = &dependency
        };

        VK_CHECK(vkCreateRenderPass(m_Device.GetHandle(), &renderPassInfo, nullptr, &m_RenderPass));

        CORE_INFO("Vulkan render pass created ({})", isSecondaryPass ? "secondary" : "main");
    }

    VulkanRenderPass::~VulkanRenderPass()
    {
        if (m_RenderPass != nullptr)
            vkDestroyRenderPass(m_Device.GetHandle(), m_RenderPass, nullptr);
    }
}

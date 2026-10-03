#include "Platform/Vulkan/VulkanRenderTarget.h"
#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanUploader.h"
#include "Platform/Vulkan/VulkanHelpers.h"
#include "Engine/Core/Log.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace ByteForge
{
    namespace
    {
        constexpr VkPipelineStageFlags2 DepthStages = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT
                                                    | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    }

    VulkanRenderTarget::VulkanRenderTarget(const RenderTargetSpec& spec)
        : m_Spec(spec)
    {
        if (spec.Width == 0 || spec.Height == 0)
            throw std::runtime_error("RenderTargetSpec: width and height must be greater than zero");

        if (spec.ColorFormats.empty())
            throw std::runtime_error("RenderTargetSpec: at least one color format is required");

        VulkanContext& context = VulkanContext::Get();
        const VulkanDevice& device = context.GetDevice();

        for (const ImageFormat imageFormat : spec.ColorFormats)
        {
            const VkFormat format = ImageFormatToVk(imageFormat);
            if (format == VK_FORMAT_UNDEFINED)
                throw std::runtime_error("RenderTargetSpec: color formats must not be None");

            const bool integer = IsIntegerFormat(format);
            const VkFormat unormFormat = ToUnormEquivalent(format);

            const VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                                          | (integer ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                                                     : VK_IMAGE_USAGE_SAMPLED_BIT);

            m_Colors.push_back(ColorAttachment{
                .Image = MakeScope<VulkanImage>(device, context.GetAllocator(), VulkanImageSpec{
                    .Width               = spec.Width,
                    .Height              = spec.Height,
                    .Format              = format,
                    .Usage               = usage,
                    .Aspect              = VK_IMAGE_ASPECT_COLOR_BIT,
                    .AlternateViewFormat = !integer && unormFormat != format ? unormFormat : VK_FORMAT_UNDEFINED
                }),
                .IsInteger = integer
            });
            m_ColorFormats.push_back(format);
        }

        if (spec.DepthFormat != ImageFormat::None)
        {
            m_Depth = MakeScope<VulkanImage>(device, context.GetAllocator(), VulkanImageSpec{
                .Width  = spec.Width,
                .Height = spec.Height,
                .Format = ImageFormatToVk(spec.DepthFormat),
                .Usage  = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                .Aspect = VK_IMAGE_ASPECT_DEPTH_BIT
            });
        }

        if (std::ranges::any_of(m_Colors, &ColorAttachment::IsInteger))
        {
            m_ReadbackBuffer = MakeScope<VulkanBuffer>(context.GetAllocator(), sizeof(uint32_t),
                                                       VK_BUFFER_USAGE_TRANSFER_DST_BIT, VulkanBufferMemory::Readback);
        }

        InitializeLayouts();

        CORE_INFO("Render target created ({}x{}, {} color attachment(s))", spec.Width, spec.Height, m_Colors.size());
    }

    uint32_t VulkanRenderTarget::ReadPixel(const uint32_t attachment, const uint32_t x, const uint32_t y) const
    {
        if (attachment >= m_Colors.size() || !m_Colors[attachment].IsInteger)
            throw std::runtime_error("RenderTarget::ReadPixel: only integer color attachments can be read");

        if (x >= m_Spec.Width || y >= m_Spec.Height)
            throw std::runtime_error("RenderTarget::ReadPixel: the pixel is outside the render target");

        const VkImage image = m_Colors[attachment].Image->GetHandle();
        const VkBuffer buffer = m_ReadbackBuffer->GetHandle();

        VulkanContext::Get().GetUploader().SubmitAndWait([&](const VkCommandBuffer commandBuffer)
        {
            CmdImageBarrier(commandBuffer, image,
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT,
                            VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);

            const VkBufferImageCopy region{
                .bufferOffset     = 0,
                .imageSubresource = {
                    .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel       = 0,
                    .baseArrayLayer = 0,
                    .layerCount     = 1
                },
                .imageOffset      = { static_cast<int32_t>(x), static_cast<int32_t>(y), 0 },
                .imageExtent      = { 1, 1, 1 }
            };
            vkCmdCopyImageToBuffer(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &region);

            const VkBufferMemoryBarrier2 hostBarrier{
                .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                .srcStageMask        = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                .srcAccessMask       = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .dstStageMask        = VK_PIPELINE_STAGE_2_HOST_BIT,
                .dstAccessMask       = VK_ACCESS_2_HOST_READ_BIT,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .buffer              = buffer,
                .offset              = 0,
                .size                = VK_WHOLE_SIZE
            };

            const VkDependencyInfo dependency{
                .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .bufferMemoryBarrierCount = 1,
                .pBufferMemoryBarriers    = &hostBarrier
            };
            vkCmdPipelineBarrier2(commandBuffer, &dependency);
        });

        uint32_t value = 0;
        m_ReadbackBuffer->GetData(&value, sizeof(value));
        return value;
    }

    VkImageView VulkanRenderTarget::GetColorDisplayView() const
    {
        const VulkanImage& image = *m_Colors.front().Image;
        return image.GetAlternateView() != nullptr ? image.GetAlternateView() : image.GetView();
    }

    std::vector<VkRenderingAttachmentInfo> VulkanRenderTarget::GetColorAttachmentInfos() const
    {
        const glm::vec4& color = m_Spec.ClearColor;

        std::vector<VkRenderingAttachmentInfo> infos;
        infos.reserve(m_Colors.size());

        for (const ColorAttachment& attachment : m_Colors)
        {
            const VkClearColorValue clearValue = attachment.IsInteger
                ? VkClearColorValue{ .uint32 = { 0, 0, 0, 0 } }
                : VkClearColorValue{ .float32 = { color.r, color.g, color.b, color.a } };

            infos.push_back({
                .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView   = attachment.Image->GetView(),
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue  = { .color = clearValue }
            });
        }

        return infos;
    }

    void VulkanRenderTarget::CmdTransitionForRendering(const VkCommandBuffer commandBuffer) const
    {
        for (const ColorAttachment& attachment : m_Colors)
        {
            const VkPipelineStageFlags2 previousReads = attachment.IsInteger ? VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT
                                                                             : VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;

            CmdImageBarrier(commandBuffer, attachment.Image->GetHandle(),
                            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                            previousReads | VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                            VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
        }

        if (m_Depth)
        {
            CmdImageBarrier(commandBuffer, m_Depth->GetHandle(),
                            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                            DepthStages, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                            DepthStages, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                                       | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                            VK_IMAGE_ASPECT_DEPTH_BIT);
        }
    }

    void VulkanRenderTarget::CmdTransitionAfterRendering(const VkCommandBuffer commandBuffer) const
    {
        for (const ColorAttachment& attachment : m_Colors)
        {
            if (attachment.IsInteger)
            {
                CmdImageBarrier(commandBuffer, attachment.Image->GetHandle(),
                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
            } else
            {
                CmdImageBarrier(commandBuffer, attachment.Image->GetHandle(),
                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
            }
        }
    }

    void VulkanRenderTarget::InitializeLayouts() const
    {
        VulkanContext::Get().GetUploader().SubmitAndWait([&](const VkCommandBuffer commandBuffer)
        {
            for (const ColorAttachment& attachment : m_Colors)
            {
                const VkImage image = attachment.Image->GetHandle();

                if (!attachment.IsInteger)
                {
                    CmdImageBarrier(commandBuffer, image,
                                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                    VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
                                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
                    continue;
                }

                CmdImageBarrier(commandBuffer, image,
                                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
                                VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);

                constexpr VkClearColorValue zero{ .uint32 = { 0, 0, 0, 0 } };
                constexpr VkImageSubresourceRange range{
                    .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel   = 0,
                    .levelCount     = 1,
                    .baseArrayLayer = 0,
                    .layerCount     = 1
                };
                vkCmdClearColorImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &zero, 1, &range);

                CmdImageBarrier(commandBuffer, image,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
            }
        });
    }
}

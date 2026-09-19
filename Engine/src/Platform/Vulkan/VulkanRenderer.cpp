#include "Platform/Vulkan/VulkanRenderer.h"

#include "VulkanFrameData.h"
#include "VulkanPipeline.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanSwapchain.h"
#include "Platform/Vulkan/VulkanRenderPass.h"
#include "Platform/Vulkan/VulkanFramebuffers.h"
#include "Platform/Vulkan/VulkanCommandPool.h"
#include "Platform/Vulkan/VulkanSyncObjects.h"
#include "Platform/Vulkan/VulkanHelpers.h"

namespace ByteForge
{
    VulkanRenderer::VulkanRenderer(VulkanDevice& device, VulkanSwapchain& swapchain, VulkanRenderPass& renderPass,
                                   VulkanFramebuffers& framebuffers, VulkanRenderPass& imguiRenderPass,
                                   VulkanFramebuffers& imguiFramebuffers, VulkanCommandPool& commandPool,
                                   VulkanSyncObjects& syncObjects, const VulkanFrameData& frameData,
                                   const uint32_t framesInFlight)
        : m_Device(device), m_Swapchain(&swapchain), m_RenderPass(renderPass), m_Framebuffers(&framebuffers),
          m_ImGuiRenderPass(imguiRenderPass), m_ImGuiFramebuffers(&imguiFramebuffers), m_CommandPool(commandPool),
          m_SyncObjects(syncObjects), m_FrameData(frameData), m_FramesInFlight(framesInFlight) {}

    VulkanRenderer::FrameResult VulkanRenderer::BeginFrame()
    {
        const VkFence fence = m_SyncObjects.GetInFlightFence(m_CurrentFrame);
        vkWaitForFences(m_Device.GetHandle(), 1, &fence, VK_TRUE, UINT64_MAX);

        const VkResult acquireResult = vkAcquireNextImageKHR(m_Device.GetHandle(),
                                                             m_Swapchain->GetHandle(),
                                                             UINT64_MAX,
                                                             m_SyncObjects.GetImageAvailable(m_CurrentFrame),
                                                             nullptr,
                                                             &m_CurrentImageIndex);

        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
        {
            m_FrameSkipped = true;
            return FrameResult::NeedsRecreation;
        }
        if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR)
            throw VulkanException(acquireResult, "vkAcquireNextImageKHR", __FILE__, __LINE__);

        m_FrameSkipped = false;
        m_MainPassEnded = false;

        vkResetFences(m_Device.GetHandle(), 1, &fence);

        m_CurrentCommandBuffer = m_CommandPool.Get(m_CurrentFrame);
        vkResetCommandBuffer(m_CurrentCommandBuffer, 0);

        constexpr VkCommandBufferBeginInfo beginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
        };
        VK_CHECK(vkBeginCommandBuffer(m_CurrentCommandBuffer, &beginInfo));

        constexpr VkClearValue clearColor{ .color = { { 0.01f, 0.01f, 0.01f, 1.0f } } };

        const VkRenderPassBeginInfo renderPassInfo{
            .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass      = m_RenderPass.GetHandle(),
            .framebuffer     = m_Framebuffers->Get(m_CurrentImageIndex),
            .renderArea      = { .offset = { 0, 0 }, .extent = m_Swapchain->GetExtent() },
            .clearValueCount = 1,
            .pClearValues    = &clearColor,
        };

        vkCmdBeginRenderPass(m_CurrentCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        const VkViewport viewport{
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = static_cast<float>(m_Swapchain->GetExtent().width),
            .height   = static_cast<float>(m_Swapchain->GetExtent().height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        vkCmdSetViewport(m_CurrentCommandBuffer, 0, 1, &viewport);

        const VkRect2D scissor{ .offset = { 0, 0 }, .extent = m_Swapchain->GetExtent() };
        vkCmdSetScissor(m_CurrentCommandBuffer, 0, 1, &scissor);

        return FrameResult::Ok;
    }

    void VulkanRenderer::Submit(const VulkanPipeline& pipeline, const std::span<const std::byte> pushConstants,
                                const VkBuffer vertexBuffer, const uint32_t vertexCount,
                                const VkBuffer indexBuffer, const uint32_t indexCount) const
    {
        if (m_FrameSkipped) return;

        const VkPipelineLayout pipelineLayout = pipeline.GetLayoutHandle();
        const VkDescriptorSet frameSet = m_FrameData.GetSet(m_CurrentFrame);
        const uint32_t dynamicOffset = m_FrameData.GetDynamicOffset();

        vkCmdBindPipeline(m_CurrentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetHandle());

        vkCmdBindDescriptorSets(m_CurrentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipelineLayout, 0, 1, &frameSet, 1, &dynamicOffset);

        if (!pushConstants.empty())
        {
            vkCmdPushConstants(m_CurrentCommandBuffer, pipelineLayout, pipeline.GetPushConstantStages(),
                               0, static_cast<uint32_t>(pushConstants.size()), pushConstants.data());
        }

        const VkBuffer vertexBuffers[] = { vertexBuffer };
        constexpr VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(m_CurrentCommandBuffer, 0, 1, vertexBuffers, offsets);

        if (indexBuffer != nullptr)
        {
            vkCmdBindIndexBuffer(m_CurrentCommandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(m_CurrentCommandBuffer, indexCount, 1, 0, 0, 0);
        } else
            vkCmdDraw(m_CurrentCommandBuffer, vertexCount, 1, 0, 0);
    }

    VulkanRenderer::FrameResult VulkanRenderer::EndFrame()
    {
        if (m_FrameSkipped)
            return FrameResult::Ok;

        EndMainRenderPass();

        VK_CHECK(vkEndCommandBuffer(m_CurrentCommandBuffer));

        const VkSemaphore waitSemaphores[] = { m_SyncObjects.GetImageAvailable(m_CurrentFrame) };
        constexpr VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        const VkSemaphore signalSemaphores[] = { m_SyncObjects.GetRenderFinished(m_CurrentImageIndex) };

        const VkSubmitInfo submitInfo{
            .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount   = 1,
            .pWaitSemaphores      = waitSemaphores,
            .pWaitDstStageMask    = waitStages,
            .commandBufferCount   = 1,
            .pCommandBuffers      = &m_CurrentCommandBuffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores    = signalSemaphores,
        };

        VK_CHECK(vkQueueSubmit(m_Device.GetGraphicsQueue(), 1, &submitInfo,
                               m_SyncObjects.GetInFlightFence(m_CurrentFrame)));

        const VkSwapchainKHR swapchains[] = { m_Swapchain->GetHandle() };
        const VkPresentInfoKHR presentInfo{
            .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores    = signalSemaphores,
            .swapchainCount     = 1,
            .pSwapchains        = swapchains,
            .pImageIndices      = &m_CurrentImageIndex,
        };

        const VkResult presentResult = vkQueuePresentKHR(m_Device.GetPresentQueue(), &presentInfo);
        m_CurrentFrame = (m_CurrentFrame + 1) % m_FramesInFlight;

        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR)
            return FrameResult::NeedsRecreation;
        if (presentResult != VK_SUCCESS)
            throw VulkanException(presentResult, "vkQueuePresentKHR", __FILE__, __LINE__);

        return FrameResult::Ok;
    }

    void VulkanRenderer::EndMainRenderPass()
    {
        if (m_FrameSkipped || m_MainPassEnded) return;

        vkCmdEndRenderPass(m_CurrentCommandBuffer);
        m_MainPassEnded = true;
    }

    void VulkanRenderer::BeginImGuiRenderPass()
    {
        if (m_FrameSkipped) return;

        EndMainRenderPass();

        const VkRenderPassBeginInfo renderPassInfo{
            .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass      = m_ImGuiRenderPass.GetHandle(),
            .framebuffer     = m_ImGuiFramebuffers->Get(m_CurrentImageIndex),
            .renderArea      = { .offset = { 0, 0 }, .extent = m_Swapchain->GetExtent() },
            .clearValueCount = 0,
            .pClearValues    = nullptr,
        };

        vkCmdBeginRenderPass(m_CurrentCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    void VulkanRenderer::EndImGuiRenderPass() const
    {
        if (m_FrameSkipped) return;

        vkCmdEndRenderPass(m_CurrentCommandBuffer);
    }
}

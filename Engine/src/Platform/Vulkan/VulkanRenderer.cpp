#include "Platform/Vulkan/VulkanRenderer.h"
#include "Platform/Vulkan/VulkanFrameData.h"
#include "Platform/Vulkan/VulkanPipeline.h"
#include "Platform/Vulkan/VulkanMaterial.h"
#include "Platform/Vulkan/VulkanRenderTarget.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanSwapchain.h"
#include "Platform/Vulkan/VulkanCommandPool.h"
#include "Platform/Vulkan/VulkanSyncObjects.h"
#include "Platform/Vulkan/VulkanVertexBuffer.h"
#include "Platform/Vulkan/VulkanIndexBuffer.h"
#include "Platform/Vulkan/VulkanHelpers.h"
#include "Engine/Renderer/Mesh.h"

#include <format>
#include <stdexcept>

namespace ByteForge
{
    namespace
    {
        constexpr VkClearColorValue SwapchainClearColor{ .float32 = { 0.01f, 0.01f, 0.01f, 1.0f } };
    }

    VulkanRenderer::VulkanRenderer(VulkanDevice& device, VulkanSwapchain& swapchain,
                                   VulkanCommandPool& commandPool, VulkanSyncObjects& syncObjects,
                                   const VulkanFrameData& frameData, const uint32_t framesInFlight)
        : m_Device(device), m_Swapchain(&swapchain), m_CommandPool(commandPool),
          m_SyncObjects(syncObjects), m_FrameData(frameData), m_FramesInFlight(framesInFlight) {}

    VulkanRenderer::FrameResult VulkanRenderer::BeginFrame()
    {
        const VkFence fence = m_SyncObjects.GetInFlightFence(m_CurrentFrame);
        VK_CHECK(vkWaitForFences(m_Device.GetHandle(), 1, &fence, VK_TRUE, UINT64_MAX));

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
        m_ActivePass = Pass::None;
        m_SwapchainCleared = false;
        m_ActiveTarget = nullptr;
        m_ActiveColorFormat = VK_FORMAT_UNDEFINED;
        m_ActiveDepthFormat = VK_FORMAT_UNDEFINED;

        VK_CHECK(vkResetFences(m_Device.GetHandle(), 1, &fence));

        m_CurrentCommandBuffer = m_CommandPool.Get(m_CurrentFrame);
        VK_CHECK(vkResetCommandBuffer(m_CurrentCommandBuffer, 0));

        constexpr VkCommandBufferBeginInfo beginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
        };
        VK_CHECK(vkBeginCommandBuffer(m_CurrentCommandBuffer, &beginInfo));

        CmdImageBarrier(m_CurrentCommandBuffer, m_Swapchain->GetImages()[m_CurrentImageIndex],
                        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_NONE,
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

        return FrameResult::Ok;
    }

    void VulkanRenderer::Submit(Material& material, const Mesh& mesh, const std::span<const std::byte> pushConstants)
    {
        auto& vulkanMaterial = static_cast<VulkanMaterial&>(material);
        const VulkanPipeline& pipeline = vulkanMaterial.GetVulkanPipeline();

        if (pushConstants.size() != pipeline.GetPushConstantSize())
        {
            throw std::runtime_error(std::format("Renderer::Submit: got {} bytes of push constant data, "
                                                 "but the shader expects {}",
                                                 pushConstants.size(), pipeline.GetPushConstantSize()));
        }

        const auto& vertexBuffer = static_cast<const VulkanVertexBuffer&>(*mesh.GetVertexBuffer());

        const VkBuffer vertexBufferHandle = vertexBuffer.GetHandleForDraw();

        VkBuffer indexBufferHandle = nullptr;
        uint32_t indexCount = 0;
        if (mesh.HasIndexBuffer())
        {
            const auto& indexBuffer = static_cast<const VulkanIndexBuffer&>(*mesh.GetIndexBuffer());
            indexBufferHandle = indexBuffer.GetHandle();
            indexCount = indexBuffer.GetCount();
        }

        vulkanMaterial.Flush();

        if (m_FrameSkipped) return;

        RecordDraw(vulkanMaterial, pushConstants, vertexBufferHandle,
                   mesh.GetVertexCount(), indexBufferHandle, indexCount);
    }

    VulkanRenderer::FrameResult VulkanRenderer::EndFrame()
    {
        if (m_FrameSkipped)
            return FrameResult::Ok;

        if (m_ActivePass == Pass::Target)
            throw std::runtime_error("Renderer::EndFrame: EndRenderTarget was not called");

        EndRendering();
        EnsureSwapchainCleared();

        CmdImageBarrier(m_CurrentCommandBuffer, m_Swapchain->GetImages()[m_CurrentImageIndex],
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE);

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

    void VulkanRenderer::BeginRenderTarget(const RenderTarget& renderTarget)
    {
        if (m_FrameSkipped) return;

        if (m_ActivePass == Pass::Target)
        {
            throw std::runtime_error("Renderer::BeginRenderTarget: render targets cannot be nested, "
                                     "call EndRenderTarget first");
        }

        const auto& target = static_cast<const VulkanRenderTarget&>(renderTarget);

        EndRendering();

        target.CmdTransitionForRendering(m_CurrentCommandBuffer);

        const glm::vec4& color = target.GetClearColor();
        const VkClearColorValue clearColor{ .float32 = { color.r, color.g, color.b, color.a } };

        BeginRendering(target.GetExtent(), target.GetColorView(), VK_ATTACHMENT_LOAD_OP_CLEAR,
                       clearColor, target.GetDepthView());

        m_ActivePass = Pass::Target;
        m_ActiveTarget = &target;
        m_ActiveColorFormat = target.GetColorFormat();
        m_ActiveDepthFormat = target.GetDepthFormat();
    }

    void VulkanRenderer::EndRenderTarget()
    {
        if (m_FrameSkipped) return;

        if (m_ActivePass != Pass::Target)
            throw std::runtime_error("Renderer::EndRenderTarget: no render target is active");

        EndRendering();

        m_ActiveTarget->CmdTransitionForSampling(m_CurrentCommandBuffer);
        m_ActiveTarget = nullptr;
    }

    void VulkanRenderer::BeginImGuiRendering()
    {
        if (m_FrameSkipped) return;

        if (m_ActivePass == Pass::Target)
            throw std::runtime_error("Renderer: EndRenderTarget must be called before ImGui is rendered");

        EndRendering();

        EnsureSwapchainCleared();
        BarrierSwapchainWrites();

        BeginRendering(m_Swapchain->GetExtent(), m_Swapchain->GetImGuiImageViews()[m_CurrentImageIndex],
                       VK_ATTACHMENT_LOAD_OP_LOAD, SwapchainClearColor);

        m_ActivePass = Pass::ImGui;
        m_ActiveColorFormat = m_Swapchain->GetImGuiImageFormat();
        m_ActiveDepthFormat = VK_FORMAT_UNDEFINED;
    }

    void VulkanRenderer::EndImGuiRendering()
    {
        if (!m_FrameSkipped)
            EndRendering();
    }

    void VulkanRenderer::RecordDraw(const VulkanMaterial& material, const std::span<const std::byte> pushConstants,
                                    const VkBuffer vertexBuffer, const uint32_t vertexCount,
                                    const VkBuffer indexBuffer, const uint32_t indexCount)
    {
        const VulkanPipeline& pipeline = material.GetVulkanPipeline();

        if (m_ActivePass == Pass::None)
            BeginSwapchainPass();

        if (pipeline.GetColorFormat() != m_ActiveColorFormat || pipeline.GetDepthFormat() != m_ActiveDepthFormat)
        {
            throw std::runtime_error(std::format(
                "Renderer::Submit: the pipeline renders into {} / {} (color / depth), but the active pass has "
                "{} / {}; set PipelineSpec::ColorFormat and DepthFormat to match the render target",
                VkFormatToString(pipeline.GetColorFormat()), VkFormatToString(pipeline.GetDepthFormat()),
                VkFormatToString(m_ActiveColorFormat), VkFormatToString(m_ActiveDepthFormat)));
        }

        const VkPipelineLayout pipelineLayout = pipeline.GetLayoutHandle();
        const VkDescriptorSet frameSet = m_FrameData.GetSet(m_CurrentFrame);
        const uint32_t dynamicOffset = m_FrameData.GetDynamicOffset();

        vkCmdBindPipeline(m_CurrentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetHandle());

        vkCmdBindDescriptorSets(m_CurrentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipelineLayout, 0, 1, &frameSet, 1, &dynamicOffset);

        if (const VkDescriptorSet materialSet = material.GetDescriptorSet(m_CurrentFrame);
            materialSet != nullptr)
        {
            vkCmdBindDescriptorSets(m_CurrentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipelineLayout, 1, 1, &materialSet, 0, nullptr);
        }

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

    void VulkanRenderer::BeginSwapchainPass()
    {
        const bool resuming = m_SwapchainCleared;
        if (resuming)
            BarrierSwapchainWrites();

        BeginRendering(m_Swapchain->GetExtent(), m_Swapchain->GetImageViews()[m_CurrentImageIndex],
                       resuming ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR, SwapchainClearColor);

        m_SwapchainCleared = true;
        m_ActivePass = Pass::Swapchain;
        m_ActiveColorFormat = m_Swapchain->GetImageFormat();
        m_ActiveDepthFormat = VK_FORMAT_UNDEFINED;
    }

    void VulkanRenderer::EnsureSwapchainCleared()
    {
        if (m_SwapchainCleared) return;

        BeginSwapchainPass();
        EndRendering();
    }

    void VulkanRenderer::BarrierSwapchainWrites() const
    {
        CmdImageBarrier(m_CurrentCommandBuffer, m_Swapchain->GetImages()[m_CurrentImageIndex],
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
    }

    void VulkanRenderer::BeginRendering(const VkExtent2D extent, const VkImageView colorView,
                                        const VkAttachmentLoadOp loadOp, const VkClearColorValue& clearColor,
                                        const VkImageView depthView)
    {
        const VkRenderingAttachmentInfo colorAttachment{
            .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView   = colorView,
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .loadOp      = loadOp,
            .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue  = { .color = clearColor }
        };

        const VkRenderingAttachmentInfo depthAttachment{
            .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView   = depthView,
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            .loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp     = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .clearValue  = { .depthStencil = { .depth = 1.0f, .stencil = 0 } }
        };

        const VkRenderingInfo renderingInfo{
            .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea           = { .offset = { 0, 0 }, .extent = extent },
            .layerCount           = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &colorAttachment,
            .pDepthAttachment     = depthView != nullptr ? &depthAttachment : nullptr
        };

        vkCmdBeginRendering(m_CurrentCommandBuffer, &renderingInfo);

        const VkViewport viewport{
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = static_cast<float>(extent.width),
            .height   = static_cast<float>(extent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        vkCmdSetViewport(m_CurrentCommandBuffer, 0, 1, &viewport);

        const VkRect2D scissor{ .offset = { 0, 0 }, .extent = extent };
        vkCmdSetScissor(m_CurrentCommandBuffer, 0, 1, &scissor);
    }

    void VulkanRenderer::EndRendering()
    {
        if (m_ActivePass == Pass::None) return;

        vkCmdEndRendering(m_CurrentCommandBuffer);

        m_ActivePass = Pass::None;
        m_ActiveColorFormat = VK_FORMAT_UNDEFINED;
        m_ActiveDepthFormat = VK_FORMAT_UNDEFINED;
    }
}

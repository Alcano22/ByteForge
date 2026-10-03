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
#include <string>
#include <vector>

namespace ByteForge
{
    namespace
    {
        constexpr VkClearColorValue SwapchainClearColor{ .float32 = { 0.01f, 0.01f, 0.01f, 1.0f } };

        DrawRange ResolveDrawRange(const DrawRange& range, const uint32_t available, const bool indexed)
        {
            const char* unit = indexed ? "indices" : "vertices";

            if (range.First > available)
            {
                throw std::runtime_error(std::format("Renderer::Submit: the draw range starts at {}, but the "
                                                     "mesh has only {} {}", range.First, available, unit));
            }

            DrawRange resolved = range;
            if (resolved.Count == 0)
                resolved.Count = available - range.First;
            else if (range.Count > available - range.First)
            {
                throw std::runtime_error(std::format("Renderer::Submit: the draw range [{}, {}) exceeds the {} {} "
                                                     "of the mesh", range.First,
                                                     static_cast<uint64_t>(range.First) + range.Count,
                                                     available, unit));
            }

            return resolved;
        }

        VkRenderingAttachmentInfo MakeColorAttachment(const VkImageView view, const VkAttachmentLoadOp loadOp,
                                                      const VkClearColorValue& clearColor)
        {
            return {
                .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView   = view,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp      = loadOp,
                .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue  = { .color = clearColor }
            };
        }

        std::string FormatList(const std::vector<VkFormat>& formats)
        {
            if (formats.empty())
                return "none";

            std::string text;
            for (const VkFormat format : formats)
            {
                if (!text.empty())
                    text += ", ";
                text += VkFormatToString(format);
            }
            return text;
        }
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
        m_ActiveColorFormats.clear();
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

    void VulkanRenderer::Submit(Material& material, const Mesh& mesh, const std::span<const std::byte> pushConstants,
                                const DrawRange& range)
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
        uint32_t available = mesh.GetVertexCount();
        if (mesh.HasIndexBuffer())
        {
            const auto& indexBuffer = static_cast<const VulkanIndexBuffer&>(*mesh.GetIndexBuffer());
            indexBufferHandle = indexBuffer.GetHandle();
            available = indexBuffer.GetCount();
        }

        const DrawRange resolved = ResolveDrawRange(range, available, mesh.HasIndexBuffer());

        vulkanMaterial.Flush();

        if (m_FrameSkipped || resolved.Count == 0) return;

        RecordDraw(vulkanMaterial, pushConstants, vertexBufferHandle, indexBufferHandle, resolved);
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

        const std::vector<VkRenderingAttachmentInfo> colorAttachments = target.GetColorAttachmentInfos();
        BeginRendering(target.GetExtent(), colorAttachments, target.GetDepthView());

        m_ActivePass = Pass::Target;
        m_ActiveTarget = &target;
        m_ActiveColorFormats = target.GetColorFormats();
        m_ActiveDepthFormat = target.GetDepthFormat();
    }

    void VulkanRenderer::EndRenderTarget()
    {
        if (m_FrameSkipped) return;

        if (m_ActivePass != Pass::Target)
            throw std::runtime_error("Renderer::EndRenderTarget: no render target is active");

        EndRendering();

        m_ActiveTarget->CmdTransitionAfterRendering(m_CurrentCommandBuffer);
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

        const VkRenderingAttachmentInfo attachment = MakeColorAttachment(
            m_Swapchain->GetImGuiImageViews()[m_CurrentImageIndex], VK_ATTACHMENT_LOAD_OP_LOAD, SwapchainClearColor);
        BeginRendering(m_Swapchain->GetExtent(), std::span(&attachment, 1));

        m_ActivePass = Pass::ImGui;
        m_ActiveColorFormats = { m_Swapchain->GetImGuiImageFormat() };
        m_ActiveDepthFormat = VK_FORMAT_UNDEFINED;
    }

    void VulkanRenderer::EndImGuiRendering()
    {
        if (!m_FrameSkipped)
            EndRendering();
    }

    void VulkanRenderer::RecordDraw(const VulkanMaterial& material, const std::span<const std::byte> pushConstants,
                                    const VkBuffer vertexBuffer, const VkBuffer indexBuffer,
                                    const DrawRange& range)
    {
        const VulkanPipeline& pipeline = material.GetVulkanPipeline();

        if (m_ActivePass == Pass::None)
            BeginSwapchainPass();

        if (pipeline.GetColorFormats() != m_ActiveColorFormats || pipeline.GetDepthFormat() != m_ActiveDepthFormat)
        {
            throw std::runtime_error(std::format(
                "Renderer::Submit: the pipeline renders into [{}] / {} (color / depth), but the active pass has "
                "[{}] / {}; set PipelineSpec::ColorAttachments and DepthFormat to match the render target",
                FormatList(pipeline.GetColorFormats()), VkFormatToString(pipeline.GetDepthFormat()),
                FormatList(m_ActiveColorFormats), VkFormatToString(m_ActiveDepthFormat)));
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
            vkCmdDrawIndexed(m_CurrentCommandBuffer, range.Count, 1, range.First, range.VertexOffset, 0);
        } else
            vkCmdDraw(m_CurrentCommandBuffer, range.Count, 1, range.First, 0);
    }

    void VulkanRenderer::BeginSwapchainPass()
    {
        const bool resuming = m_SwapchainCleared;
        if (resuming)
            BarrierSwapchainWrites();

        const VkRenderingAttachmentInfo attachment = MakeColorAttachment(
            m_Swapchain->GetImageViews()[m_CurrentImageIndex],
            resuming ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR, SwapchainClearColor);
        BeginRendering(m_Swapchain->GetExtent(), std::span(&attachment, 1));

        m_SwapchainCleared = true;
        m_ActivePass = Pass::Swapchain;
        m_ActiveColorFormats = { m_Swapchain->GetImageFormat() };
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

    void VulkanRenderer::BeginRendering(const VkExtent2D extent,
                                        const std::span<const VkRenderingAttachmentInfo> colorAttachments,
                                        const VkImageView depthView)
    {
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
            .colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size()),
            .pColorAttachments    = colorAttachments.data(),
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
        m_ActiveColorFormats.clear();
        m_ActiveDepthFormat = VK_FORMAT_UNDEFINED;
    }
}

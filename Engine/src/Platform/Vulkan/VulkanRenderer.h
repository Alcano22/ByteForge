#pragma once

#include "Engine/Core/NonCopyable.h"

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

#include <span>
#include <cstddef>

namespace ByteForge
{
    class VulkanDevice;
    class VulkanSwapchain;
    class VulkanRenderPass;
    class VulkanFramebuffers;
    class VulkanCommandPool;
    class VulkanSyncObjects;
    class VulkanPipeline;
    class VulkanFrameData;

    class VulkanRenderer : NonCopyable
    {
    public:
        enum class FrameResult { Ok, NeedsRecreation };

        VulkanRenderer(VulkanDevice& device, VulkanSwapchain& swapchain, VulkanRenderPass& renderPass,
                       VulkanFramebuffers& framebuffers, VulkanRenderPass& imguiRenderPass,
                       VulkanFramebuffers& imguiFramebuffers, VulkanCommandPool& commandPool,
                       VulkanSyncObjects& syncObjects, const VulkanFrameData& frameData, uint32_t framesInFlight);

        FrameResult BeginFrame();
        void Submit(const VulkanPipeline& pipeline, std::span<const std::byte> pushConstants,
                    VkBuffer vertexBuffer, uint32_t vertexCount,
                    VkBuffer indexBuffer = nullptr, uint32_t indexCount = 0) const;
        FrameResult EndFrame();

        void EndMainRenderPass();
        void BeginImGuiRenderPass();
        void EndImGuiRenderPass() const;

        void UpdateSwapchainTargets(VulkanSwapchain& swapchain, VulkanFramebuffers& framebuffers,
                                    VulkanFramebuffers& imguiFramebuffers)
        {
            m_Swapchain = &swapchain;
            m_Framebuffers = &framebuffers;
            m_ImGuiFramebuffers = &imguiFramebuffers;
        }

        [[nodiscard]] uint32_t GetCurrentFrameIndex() const { return m_CurrentFrame; }
        [[nodiscard]] VkCommandBuffer GetCurrentCommandBuffer() const { return m_CurrentCommandBuffer; }
        [[nodiscard]] bool IsFrameSkipped() const { return m_FrameSkipped; }

    private:
        VulkanDevice& m_Device;
        VulkanSwapchain* m_Swapchain;
        VulkanRenderPass& m_RenderPass;
        VulkanFramebuffers* m_Framebuffers;
        VulkanRenderPass& m_ImGuiRenderPass;
        VulkanFramebuffers* m_ImGuiFramebuffers;
        VulkanCommandPool& m_CommandPool;
        VulkanSyncObjects& m_SyncObjects;
        const VulkanFrameData& m_FrameData;

        uint32_t m_FramesInFlight;
        uint32_t m_CurrentFrame = 0;
        uint32_t m_CurrentImageIndex = 0;
        VkCommandBuffer m_CurrentCommandBuffer = nullptr;
        bool m_FrameSkipped = false;
        bool m_MainPassEnded = false;
    };
}

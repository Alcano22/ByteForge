#pragma once

#include "Engine/Core/NonCopyable.h"
#include "Engine/Renderer/DrawRange.h"

#include <vulkan/vulkan.h>

#include <cstddef>
#include <span>
#include <vector>

namespace ByteForge
{
    class Material;
    class Mesh;
    class RenderTarget;

    class VulkanDevice;
    class VulkanSwapchain;
    class VulkanCommandPool;
    class VulkanSyncObjects;
    class VulkanMaterial;
    class VulkanFrameData;
    class VulkanRenderTarget;

    class VulkanRenderer : NonCopyable
    {
    public:
        enum class FrameResult { Ok, NeedsRecreation };

        VulkanRenderer(VulkanDevice& device, VulkanSwapchain& swapchain, VulkanCommandPool& commandPool,
                       VulkanSyncObjects& syncObjects, const VulkanFrameData& frameData, uint32_t framesInFlight);

        FrameResult BeginFrame();
        void Submit(Material& material, const Mesh& mesh, std::span<const std::byte> pushConstants,
                    const DrawRange& range);
        FrameResult EndFrame();

        void BeginRenderTarget(const RenderTarget& renderTarget);
        void EndRenderTarget();

        void BeginImGuiRendering();
        void EndImGuiRendering();

        void UpdateSwapchain(VulkanSwapchain& swapchain) { m_Swapchain = &swapchain; }

        [[nodiscard]] uint32_t GetCurrentFrameIndex() const { return m_CurrentFrame; }
        [[nodiscard]] VkCommandBuffer GetCurrentCommandBuffer() const { return m_CurrentCommandBuffer; }
        [[nodiscard]] bool IsFrameSkipped() const { return m_FrameSkipped; }

    private:
        enum class Pass { None, Swapchain, Target, ImGui };

        void RecordDraw(const VulkanMaterial& material, std::span<const std::byte> pushConstants,
                        VkBuffer vertexBuffer, VkBuffer indexBuffer, const DrawRange& range);

        void BeginSwapchainPass();
        void EnsureSwapchainCleared();
        void BarrierSwapchainWrites() const;

        void BeginRendering(VkExtent2D extent, std::span<const VkRenderingAttachmentInfo> colorAttachments,
                            VkImageView depthView = nullptr);
        void EndRendering();

    private:
        VulkanDevice& m_Device;
        VulkanSwapchain* m_Swapchain;
        VulkanCommandPool& m_CommandPool;
        VulkanSyncObjects& m_SyncObjects;
        const VulkanFrameData& m_FrameData;

        uint32_t m_FramesInFlight;
        uint32_t m_CurrentFrame = 0;
        uint32_t m_CurrentImageIndex = 0;
        VkCommandBuffer m_CurrentCommandBuffer = nullptr;
        bool m_FrameSkipped = false;

        Pass m_ActivePass = Pass::None;
        bool m_SwapchainCleared = false;
        const VulkanRenderTarget* m_ActiveTarget = nullptr;
        std::vector<VkFormat> m_ActiveColorFormats;
        VkFormat m_ActiveDepthFormat = VK_FORMAT_UNDEFINED;
    };
}

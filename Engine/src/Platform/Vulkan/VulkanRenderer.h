#pragma once

#include "Engine/Core/NonCopyable.h"
#include "Engine/Renderer/DrawRange.h"
#include "Engine/Renderer/ViewportRect.h"

#include <vulkan/vulkan.h>

#include <cstddef>
#include <optional>
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
                       VulkanSyncObjects& syncObjects, const VulkanFrameData& frameData, u32 framesInFlight);

        FrameResult BeginFrame();
        void Submit(Material& material, const Mesh& mesh, std::span<const std::byte> pushConstants,
                    const DrawRange& range);
        FrameResult EndFrame();

        void BeginRenderTarget(const RenderTarget& renderTarget);
        void EndRenderTarget();

        void BeginImGuiRendering();
        void EndImGuiRendering();

        void SetSwapchainViewport(const std::optional<ViewportRect>& rect);

        void UpdateSwapchain(VulkanSwapchain& swapchain) { m_Swapchain = &swapchain; }

        [[nodiscard]] u32 GetCurrentFrameIndex() const { return m_CurrentFrame; }
        [[nodiscard]] VkCommandBuffer GetCurrentCommandBuffer() const { return m_CurrentCommandBuffer; }
        [[nodiscard]] bool IsFrameSkipped() const { return m_FrameSkipped; }

    private:
        enum class Pass { None, Swapchain, Target, ImGui };

        void RecordDraw(const VulkanMaterial& material, std::span<const std::byte> pushConstants,
                        VkBuffer vertexBuffer, VkBuffer indexBuffer, const DrawRange& range);

        void BeginSwapchainPass(bool withDepth);
        void EnsureSwapchainCleared();
        void BarrierSwapchainWrites() const;
        void PrepareSwapchainDepth() const;
        void ApplySwapchainViewport() const;

        void BeginRendering(VkExtent2D extent, std::span<const VkRenderingAttachmentInfo> colorAttachments,
                            VkImageView depthView = nullptr,
                            VkAttachmentLoadOp depthLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                            VkAttachmentStoreOp depthStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE);
        void EndRendering();

    private:
        VulkanDevice& m_Device;
        VulkanSwapchain* m_Swapchain;
        VulkanCommandPool& m_CommandPool;
        VulkanSyncObjects& m_SyncObjects;
        const VulkanFrameData& m_FrameData;

        u32 m_FramesInFlight;
        u32 m_CurrentFrame = 0;
        u32 m_CurrentImageIndex = 0;
        VkCommandBuffer m_CurrentCommandBuffer = nullptr;
        bool m_FrameSkipped = false;

        Pass m_ActivePass = Pass::None;
        bool m_SwapchainCleared = false;
        bool m_SwapchainDepthCleared = false;
        const VulkanRenderTarget* m_ActiveTarget = nullptr;
        std::vector<VkFormat> m_ActiveColorFormats;
        VkFormat m_ActiveDepthFormat = VK_FORMAT_UNDEFINED;

        std::optional<ViewportRect> m_SwapchainViewport;
    };
}

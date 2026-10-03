#pragma once

#include "Engine/Renderer/RenderTarget.h"
#include "Platform/Vulkan/VulkanBuffer.h"
#include "Platform/Vulkan/VulkanImage.h"

#include <vulkan/vulkan.h>

#include <vector>

namespace ByteForge
{
    class VulkanRenderTarget : public RenderTarget
    {
    public:
        explicit VulkanRenderTarget(const RenderTargetSpec& spec);

        [[nodiscard]] uint32_t GetWidth() const override { return m_Spec.Width; }
        [[nodiscard]] uint32_t GetHeight() const override { return m_Spec.Height; }
        [[nodiscard]] VkExtent2D GetExtent() const { return { m_Spec.Width, m_Spec.Height }; }

        [[nodiscard]] uint32_t ReadPixel(uint32_t attachment, uint32_t x, uint32_t y) const override;

        [[nodiscard]] const std::vector<VkFormat>& GetColorFormats() const { return m_ColorFormats; }
        [[nodiscard]] VkImageView GetDepthView() const { return m_Depth ? m_Depth->GetView() : nullptr; }
        [[nodiscard]] VkFormat GetDepthFormat() const { return m_Depth ? m_Depth->GetFormat() : VK_FORMAT_UNDEFINED; }

        [[nodiscard]] VkImageView GetColorDisplayView() const;

        [[nodiscard]] std::vector<VkRenderingAttachmentInfo> GetColorAttachmentInfos() const;

        void CmdTransitionForRendering(VkCommandBuffer commandBuffer) const;
        void CmdTransitionAfterRendering(VkCommandBuffer commandBuffer) const;

    private:
        void InitializeLayouts() const;

    private:
        struct ColorAttachment
        {
            Scope<VulkanImage> Image;
            bool IsInteger = false;
        };

        RenderTargetSpec m_Spec;
        std::vector<ColorAttachment> m_Colors;
        std::vector<VkFormat> m_ColorFormats;
        Scope<VulkanImage> m_Depth;
        Scope<VulkanBuffer> m_ReadbackBuffer;
    };
}

#pragma once

#include "Engine/Renderer/RenderTarget.h"
#include "Platform/Vulkan/VulkanImage.h"

#include <vulkan/vulkan.h>

namespace ByteForge
{
    class VulkanRenderTarget : public RenderTarget
    {
    public:
        explicit VulkanRenderTarget(const RenderTargetSpec& spec);

        [[nodiscard]] uint32_t GetWidth() const override { return m_Spec.Width; }
        [[nodiscard]] uint32_t GetHeight() const override { return m_Spec.Height; }
        [[nodiscard]] VkExtent2D GetExtent() const { return { m_Spec.Width, m_Spec.Height }; }
        [[nodiscard]] VkImageView GetColorView() const { return m_Color->GetView(); }
        [[nodiscard]] VkImageView GetDepthView() const { return m_Depth ? m_Depth->GetView() : nullptr; }
        [[nodiscard]] VkFormat GetColorFormat() const { return m_Color->GetFormat(); }
        [[nodiscard]] VkFormat GetDepthFormat() const { return m_Depth ? m_Depth->GetFormat() : VK_FORMAT_UNDEFINED; }
        [[nodiscard]] const glm::vec4& GetClearColor() const { return m_Spec.ClearColor; }

        [[nodiscard]] VkImageView GetColorDisplayView() const
        {
            return m_Color->GetAlternateView() != nullptr ? m_Color->GetAlternateView() : m_Color->GetView();
        }

        void CmdTransitionForRendering(VkCommandBuffer commandBuffer) const;
        void CmdTransitionForSampling(VkCommandBuffer commandBuffer) const;


    private:
        RenderTargetSpec m_Spec;
        Scope<VulkanImage> m_Color;
        Scope<VulkanImage> m_Depth;
    };
}

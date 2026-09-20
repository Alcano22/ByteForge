#include "Platform/Vulkan/VulkanRenderTarget.h"
#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanUploader.h"
#include "Platform/Vulkan/VulkanSamplerCache.h"
#include "Platform/Vulkan/VulkanHelpers.h"
#include "Engine/Core/Log.h"

#include <imgui.h>
#include <imgui_impl_vulkan.h>

#include <stdexcept>

namespace ByteForge
{
    VulkanRenderTarget::VulkanRenderTarget(const RenderTargetSpec& spec)
        : m_Spec(spec)
    {
        if (spec.Width == 0 || spec.Height == 0)
            throw std::runtime_error("RenderTargetSpec: width and height must be greater than zero");

        VulkanContext& context = VulkanContext::Get();
        const VulkanDevice& device = context.GetDevice();

        const VkFormat colorFormat = ImageFormatToVk(spec.ColorFormat);
        if (colorFormat == VK_FORMAT_UNDEFINED)
            throw std::runtime_error("RenderTargetSpec: ColorFormat must be set");

        const VkFormat unormFormat = ToUnormEquivalent(colorFormat);

        m_Color = MakeScope<VulkanImage>(device, context.GetAllocator(), VulkanImageSpec{
            .Width               = spec.Width,
            .Height              = spec.Height,
            .Format              = colorFormat,
            .Usage               = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .Aspect              = VK_IMAGE_ASPECT_COLOR_BIT,
            .AlternateViewFormat = unormFormat != colorFormat ? unormFormat : VK_FORMAT_UNDEFINED
        });

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

        m_Sampler = context.GetSamplerCache().Get(TextureFilter::Linear, TextureWrap::ClampToEdge);

        context.GetUploader().TransitionToShaderRead(m_Color->GetHandle());

        CORE_INFO("Render target created ({}x{})", spec.Width, spec.Height);
    }

    VulkanRenderTarget::~VulkanRenderTarget()
    {
        if (m_ImGuiTexture != nullptr && ImGui::GetCurrentContext() != nullptr)
            ImGui_ImplVulkan_RemoveTexture(m_ImGuiTexture);
    }

    uint64_t VulkanRenderTarget::GetImGuiTextureId()
    {
        if (m_ImGuiTexture == nullptr)
        {
            if (ImGui::GetCurrentContext() == nullptr)
            {
                throw std::runtime_error("RenderTarget::GetImGuiTextureId: ImGui is not enabled, "
                                         "call Application::EnableImGui() first");
            }

            const VkImageView displayView = m_Color->GetAlternateView() != nullptr
                                          ? m_Color->GetAlternateView() : m_Color->GetView();

            m_ImGuiTexture = ImGui_ImplVulkan_AddTexture(m_Sampler, displayView,
                                                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        return reinterpret_cast<uint64_t>(m_ImGuiTexture);
    }

    void VulkanRenderTarget::CmdTransitionForRendering(const VkCommandBuffer commandBuffer) const
    {
        CmdImageBarrier(commandBuffer, m_Color->GetHandle(),
                        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

        if (m_Depth)
        {
            constexpr VkPipelineStageFlags2 depthStages = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT
                                                        | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;

            CmdImageBarrier(commandBuffer, m_Depth->GetHandle(),
                            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                            depthStages, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                            depthStages, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                                       | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                            VK_IMAGE_ASPECT_DEPTH_BIT);
        }
    }

    void VulkanRenderTarget::CmdTransitionForSampling(const VkCommandBuffer commandBuffer) const
    {
        CmdImageBarrier(commandBuffer, m_Color->GetHandle(),
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
    }
}

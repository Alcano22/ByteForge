#include "Platform/Vulkan/VulkanImGuiRenderer.h"
#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanSwapchain.h"
#include "Platform/Vulkan/VulkanRenderer.h"
#include "Platform/Vulkan/VulkanDeletionQueue.h"
#include "Platform/Vulkan/VulkanRenderTarget.h"
#include "Platform/Vulkan/VulkanTexture2D.h"
#include "Platform/Vulkan/VulkanHelpers.h"
#include "Engine/Core/Log.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <GLFW/glfw3.h>

#include <stdexcept>

namespace ByteForge
{
    void VulkanImGuiRenderer::Init(GLFWwindow* windowHandle)
    {
        const auto& context = VulkanContext::Get();
        const auto& device = context.GetDevice();

        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForVulkan(windowHandle, true);

        m_ColorFormat = context.GetSwapchain().GetImGuiImageFormat();

        ImGui_ImplVulkan_InitInfo initInfo{
            .ApiVersion = VK_API_VERSION_1_3,
            .Instance = context.GetInstanceHandle(),
            .PhysicalDevice = device.GetPhysicalDevice(),
            .Device = device.GetHandle(),
            .QueueFamily = device.GetQueueFamilyIndices().GraphicsFamily.value(),
            .Queue = device.GetGraphicsQueue(),
            .DescriptorPoolSize = 1000,
            .MinImageCount = 2,
            .ImageCount = static_cast<uint32_t>(context.GetSwapchain().GetImages().size()),
            .PipelineInfoMain = {
                .MSAASamples = VK_SAMPLE_COUNT_1_BIT
            },
            .CheckVkResultFn = [](const VkResult result)
            {
                if (result != VK_SUCCESS)
                    throw VulkanException(result, "ImGui Vulkan backend call", __FILE__, __LINE__);
            }
        };

        initInfo.UseDynamicRendering = true;
        initInfo.PipelineInfoMain.PipelineRenderingCreateInfo = {
            .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount    = 1,
            .pColorAttachmentFormats = &m_ColorFormat
        };

        ImGui_ImplVulkan_Init(&initInfo);

        CORE_INFO("ImGui Vulkan backend initialized");
    }

    void VulkanImGuiRenderer::Shutdown()
    {
        VulkanContext::Get().GetDevice().WaitIdle();

        m_Textures.clear();

        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void VulkanImGuiRenderer::NewFrame()
    {
        ReleaseExpired();

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
    }

    void VulkanImGuiRenderer::RenderDrawData()
    {
        auto& renderer = VulkanContext::Get().GetRenderer();
        if (renderer.IsFrameSkipped()) return;

        renderer.BeginImGuiRendering();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), renderer.GetCurrentCommandBuffer());
        renderer.EndImGuiRendering();
    }

    ImTextureID VulkanImGuiRenderer::GetTextureId(const Ref<Texture2D>& texture)
    {
        if (!texture)
            throw std::runtime_error("ImGuiRenderer::GetTextureId: the texture must not be null");

        return Resolve(texture, static_cast<const VulkanTexture2D&>(*texture).GetDisplayView());
    }

    ImTextureID VulkanImGuiRenderer::GetTextureId(const Ref<RenderTarget>& target)
    {
        if (!target)
            throw std::runtime_error("ImGuiRenderer::GetTextureId: the render target must not be null");

        return Resolve(target, static_cast<const VulkanRenderTarget&>(*target).GetColorDisplayView());
    }

    ImTextureID VulkanImGuiRenderer::Resolve(const std::shared_ptr<const void>& owner, const VkImageView view)
    {
        const void* key = owner.get();

        if (const auto it = m_Textures.find(key); it != m_Textures.end())
        {
            if (!it->second.Owner.expired())
                return reinterpret_cast<ImTextureID>(it->second.Set);

            Release(it->second.Set);
            m_Textures.erase(it);
        }

        const VkDescriptorSet set = ImGui_ImplVulkan_AddTexture(view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        m_Textures.emplace(key, CachedTexture{ .Owner = owner, .Set = set });
        return reinterpret_cast<ImTextureID>(set);
    }

    void VulkanImGuiRenderer::ReleaseExpired()
    {
        std::erase_if(m_Textures, [](const auto& entry)
        {
            if (!entry.second.Owner.expired())
                return false;

            Release(entry.second.Set);
            return true;
        });
    }

    void VulkanImGuiRenderer::Release(const VkDescriptorSet set)
    {
        VulkanContext::Get().GetDeletionQueue().Push([set]
        {
            if (ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().BackendRendererUserData != nullptr)
                ImGui_ImplVulkan_RemoveTexture(set);
        });
    }
}

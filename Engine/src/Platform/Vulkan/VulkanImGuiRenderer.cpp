#include "Platform/Vulkan/VulkanImGuiRenderer.h"
#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanSwapchain.h"
#include "Platform/Vulkan/VulkanRenderPass.h"
#include "Platform/Vulkan/VulkanRenderer.h"
#include "Platform/Vulkan/VulkanHelpers.h"
#include "Engine/Core/Log.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <GLFW/glfw3.h>

namespace ByteForge
{
    void VulkanImGuiRenderer::Init(GLFWwindow* windowHandle)
    {
        const auto& context = VulkanContext::Get();
        const auto& device = context.GetDevice();

        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForVulkan(windowHandle, true);

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
                .RenderPass = context.GetImGuiRenderPass().GetHandle(),
                .MSAASamples = VK_SAMPLE_COUNT_1_BIT
            },
            .CheckVkResultFn = [](const VkResult result)
            {
                if (result != VK_SUCCESS)
                    throw VulkanException(result, "ImGui Vulkan backend call", __FILE__, __LINE__);
            }
        };

        ImGui_ImplVulkan_Init(&initInfo);

        CORE_INFO("ImGui Vulkan backend initialized");
    }

    void VulkanImGuiRenderer::Shutdown()
    {
        VulkanContext::Get().GetDevice().WaitIdle();

        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void VulkanImGuiRenderer::NewFrame()
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
    }

    void VulkanImGuiRenderer::RenderDrawData()
    {
        auto& renderer = VulkanContext::Get().GetRenderer();
        if (renderer.IsFrameSkipped()) return;

        renderer.BeginImGuiRenderPass();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), renderer.GetCurrentCommandBuffer());
        renderer.EndImGuiRenderPass();
    }
}

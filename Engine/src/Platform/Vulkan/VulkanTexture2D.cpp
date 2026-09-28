#include "Platform/Vulkan/VulkanTexture2D.h"
#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanSamplerCache.h"
#include "Platform/Vulkan/VulkanUploader.h"
#include "Platform/Vulkan/VulkanHelpers.h"
#include "Engine/Core/Log.h"

#include <imgui.h>
#include <imgui_impl_vulkan.h>

#include <algorithm>
#include <bit>
#include <format>
#include <stdexcept>

namespace ByteForge
{
    VulkanTexture2D::VulkanTexture2D(const uint32_t width, const uint32_t height,
                                     const std::span<const std::byte> pixels, const TextureSettings& settings)
        : m_Width(width), m_Height(height), m_Filter(settings.Filter)
    {
        if (width == 0 || height == 0)
            throw std::runtime_error("Texture2D: width and height must be greater than zero");

        if (settings.Format != ImageFormat::RGBA8_SRGB && settings.Format != ImageFormat::RGBA8_UNORM)
            throw std::runtime_error("Texture2D: only RGBA8_SRGB and RGBA8_UNORM are supported for now");

        constexpr size_t bytesPerPixel = 4;
        const size_t expectedSize = static_cast<size_t>(width) * height * bytesPerPixel;
        if (pixels.size() != expectedSize)
        {
            throw std::runtime_error(std::format("Texture2D: {}x{} RGBA8 needs {} bytes of pixel data, got {}",
                                                 width, height, expectedSize, pixels.size()));
        }

        VulkanContext& context = VulkanContext::Get();
        const VulkanDevice& device = context.GetDevice();

        VkPhysicalDeviceProperties deviceProps;
        vkGetPhysicalDeviceProperties(device.GetPhysicalDevice(), &deviceProps);
        const uint32_t maxDimension = deviceProps.limits.maxImageDimension2D;
        if (width > maxDimension || height > maxDimension)
        {
            throw std::runtime_error(std::format("Texture2D: {}x{} exceeds the maximum image size of this GPU ({})",
                                                 width, height, maxDimension));
        }

        m_MipLevels = settings.GenerateMips ? static_cast<uint32_t>(std::bit_width(std::max(width, height))) : 1u;

        const VkFormat format = ImageFormatToVk(settings.Format);
        const VkFormat unormFormat = ToUnormEquivalent(format);

        m_Image = MakeScope<VulkanImage>(device, context.GetAllocator(), VulkanImageSpec{
            .Width               = width,
            .Height              = height,
            .Format              = format,
            .Usage               = VK_IMAGE_USAGE_SAMPLED_BIT
                                 | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                                 | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .Aspect              = VK_IMAGE_ASPECT_COLOR_BIT,
            .AlternateViewFormat = unormFormat != format ? unormFormat : VK_FORMAT_UNDEFINED,
            .MipLevels           = m_MipLevels,
            .DedicatedMemory     = false
        });

        context.GetUploader().UploadTexture(*m_Image, width, height, m_MipLevels, pixels.data(), pixels.size());
        m_Sampler = context.GetSamplerCache().Get(settings.Filter, settings.Wrap);

        CORE_INFO("Texture created ({}x{}, mip levels: {})", width, height, m_MipLevels);
    }

    VulkanTexture2D::~VulkanTexture2D()
    {
        if (m_ImGuiTexture != nullptr && ImGui::GetCurrentContext() != nullptr)
            ImGui_ImplVulkan_RemoveTexture(m_ImGuiTexture);
    }

    uint64_t VulkanTexture2D::GetImGuiTextureId()
    {
        if (m_ImGuiTexture == nullptr)
        {
            if (ImGui::GetCurrentContext() == nullptr)
            {
                throw std::runtime_error("Texture2D::GetImGuiTextureId: ImGui is not enabled, "
                                         "call Application::EnableImGui() first");
            }

            const VkImageView displayView = m_Image->GetAlternateView() != nullptr
                                          ? m_Image->GetAlternateView() : m_Image->GetView();

            m_ImGuiTexture = ImGui_ImplVulkan_AddTexture(displayView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        return reinterpret_cast<uint64_t>(m_ImGuiTexture);
    }
}

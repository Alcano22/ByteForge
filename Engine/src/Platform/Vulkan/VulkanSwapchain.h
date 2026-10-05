#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Core/NonCopyable.h"
#include "Platform/Vulkan/VulkanImage.h"

#include <vulkan/vulkan.h>

#include <vector>

struct GLFWwindow;

namespace ByteForge
{
    class VulkanDevice;

    struct SwapchainSupportDetails
    {
        VkSurfaceCapabilitiesKHR Capabilities{};
        std::vector<VkSurfaceFormatKHR> Formats;
        std::vector<VkPresentModeKHR> PresentModes;
    };

    class VulkanSwapchain : NonCopyable
    {
    public:
        VulkanSwapchain(const VulkanDevice& device, const VulkanAllocator& allocator, VkSurfaceKHR surface,
                        GLFWwindow* windowHandle, VkSwapchainKHR oldSwapchain = nullptr);
        ~VulkanSwapchain();

        [[nodiscard]] VkSwapchainKHR GetHandle() const { return m_Swapchain; }
        [[nodiscard]] VkFormat GetImageFormat() const { return m_ImageFormat; }
        [[nodiscard]] VkFormat GetImGuiImageFormat() const { return m_ImGuiImageFormat; }
        [[nodiscard]] const std::vector<VkImage>& GetImages() const { return m_Images; }
        [[nodiscard]] const std::vector<VkImageView>& GetImageViews() const { return m_ImageViews; }
        [[nodiscard]] const std::vector<VkImageView>& GetImGuiImageViews() const { return m_ImGuiImageViews; }
        [[nodiscard]] VkExtent2D GetExtent() const { return m_Extent; }

        [[nodiscard]] VkImage GetDepthImage() const { return m_DepthImage->GetHandle(); }
        [[nodiscard]] VkImageView GetDepthView() const { return m_DepthImage->GetView(); }

        [[nodiscard]] static SwapchainSupportDetails QuerySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

    private:
        void CreateImageViews();

        [[nodiscard]] static VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available);
        [[nodiscard]] static VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& available);
        [[nodiscard]] static VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* windowHandle);

    public:
        static constexpr VkFormat DepthFormat = VK_FORMAT_D32_SFLOAT;

    private:
        const VulkanDevice& m_Device;
        VkSurfaceKHR m_Surface;

        VkSwapchainKHR m_Swapchain = nullptr;
        VkFormat m_ImageFormat = VK_FORMAT_UNDEFINED;
        VkFormat m_ImGuiImageFormat = VK_FORMAT_UNDEFINED;
        std::vector<VkImage> m_Images;
        std::vector<VkImageView> m_ImageViews;
        std::vector<VkImageView> m_ImGuiImageViews;
        VkExtent2D m_Extent{};

        Scope<VulkanImage> m_DepthImage;
    };
}

#pragma once

#include "Engine/Core/NonCopyable.h"

#include <vulkan/vulkan.h>

#include <array>
#include <optional>

namespace ByteForge
{
    struct QueueFamilyIndices
    {
        std::optional<uint32_t> GraphicsFamily;
        std::optional<uint32_t> PresentFamily;

        [[nodiscard]] bool IsComplete() const { return GraphicsFamily.has_value() && PresentFamily.has_value(); }
    };

    class VulkanDevice : NonCopyable
    {
    public:
        VulkanDevice(VkInstance instance, VkSurfaceKHR surface);
        ~VulkanDevice();

        void WaitIdle() const { vkDeviceWaitIdle(m_Device); }

        [[nodiscard]] VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
        [[nodiscard]] VkDevice GetHandle() const { return m_Device; }
        [[nodiscard]] VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
        [[nodiscard]] VkQueue GetPresentQueue() const { return m_PresentQueue; }
        [[nodiscard]] const QueueFamilyIndices& GetQueueFamilyIndices() const { return m_QueueFamilyIndices; }

        static constexpr std::array<const char*, 3> DeviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,
            VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME
        };

    private:
        void PickPhysicalDevice();
        void CreateLogicalDevice();

        [[nodiscard]] bool IsDeviceSuitable(VkPhysicalDevice device) const;
        [[nodiscard]] QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device) const;
        [[nodiscard]] uint64_t RateDeviceSuitability(VkPhysicalDevice device) const;

        [[nodiscard]] static bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
        [[nodiscard]] static bool CheckFeatureSupport(VkPhysicalDevice device);

    private:
        VkInstance m_Instance;
        VkSurfaceKHR m_Surface;

        VkPhysicalDevice m_PhysicalDevice = nullptr;
        VkDevice m_Device = nullptr;
        VkQueue m_GraphicsQueue = nullptr;
        VkQueue m_PresentQueue = nullptr;
        QueueFamilyIndices m_QueueFamilyIndices;
    };
}

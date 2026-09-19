#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanSwapchain.h"
#include "Platform/Vulkan/VulkanHelpers.h"
#include "Engine/Core/Log.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

namespace ByteForge
{
    VulkanDevice::VulkanDevice(const VkInstance instance, const VkSurfaceKHR surface)
        : m_Instance(instance), m_Surface(surface)
    {
        PickPhysicalDevice();
        CreateLogicalDevice();
    }

    VulkanDevice::~VulkanDevice()
    {
        if (m_Device != nullptr)
            vkDestroyDevice(m_Device, nullptr);
    }

    void VulkanDevice::PickPhysicalDevice()
    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);

        if (deviceCount == 0)
            throw std::runtime_error("Failed to find a GPU with Vulkan support");

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

        std::multimap<uint64_t, VkPhysicalDevice, std::greater<>> candidates;

        for (VkPhysicalDevice device : devices)
        {
            const uint64_t score = RateDeviceSuitability(device);

            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(device, &props);
            CORE_INFO("  Candidate: {} (score: {})", props.deviceName, score);

            if (score > 0)
                candidates.insert({ score, device });
        }

        if (candidates.empty())
            throw std::runtime_error("Failed to find a suitable GPU");

        m_PhysicalDevice = candidates.begin()->second;
        m_QueueFamilyIndices = FindQueueFamilies(m_PhysicalDevice);

        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(m_PhysicalDevice, &props);
        CORE_INFO("Selected GPU: {} (score: {})", props.deviceName, candidates.begin()->first);
    }

    uint64_t VulkanDevice::RateDeviceSuitability(const VkPhysicalDevice device) const
    {
        if (!IsDeviceSuitable(device))
            return 0;

        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(device, &props);

        uint64_t typeRank = 1;

        switch (props.deviceType)
        {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   typeRank = 5; break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: typeRank = 4; break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    typeRank = 3; break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU:            typeRank = 2; break;
            default: break;
        }

        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(device, &memProps);

        VkDeviceSize deviceLocalMemory = 0;
        for (uint32_t i = 0; i < memProps.memoryHeapCount; ++i)
        {
            if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
                deviceLocalMemory += memProps.memoryHeaps[i].size;
        }

        const uint64_t deviceLocalMebibytes = deviceLocalMemory / (1024 * 1024);

        return (typeRank << 32) | deviceLocalMebibytes;
    }

    bool VulkanDevice::IsDeviceSuitable(const VkPhysicalDevice device) const
    {
        const QueueFamilyIndices indices = FindQueueFamilies(device);
        const bool extensionsSupported = CheckDeviceExtensionSupport(device);

        bool swapchainAdequate = false;
        if (extensionsSupported)
        {
            const SwapchainSupportDetails swapchainSupport = VulkanSwapchain::QuerySwapchainSupport(device, m_Surface);
            swapchainAdequate = !swapchainSupport.Formats.empty() && !swapchainSupport.PresentModes.empty();
        }

        return indices.IsComplete() && extensionsSupported && swapchainAdequate;
    }

    bool VulkanDevice::CheckDeviceExtensionSupport(const VkPhysicalDevice device) const
    {
        uint32_t extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        return std::ranges::all_of(DeviceExtensions,
            [&availableExtensions](const char* required)
            {
                return std::ranges::any_of(availableExtensions,
                    [required](const VkExtensionProperties& ext)
                    {
                        return std::strcmp(ext.extensionName, required) == 0;
                    });
            });
    }

    QueueFamilyIndices VulkanDevice::FindQueueFamilies(const VkPhysicalDevice device) const
    {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        for (uint32_t i = 0; i < queueFamilyCount; ++i)
        {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                indices.GraphicsFamily = i;

            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &presentSupport);
            if (presentSupport)
                indices.PresentFamily = i;

            if (indices.IsComplete()) break;
        }

        return indices;
    }

    void VulkanDevice::CreateLogicalDevice()
    {
        const std::set<uint32_t> uniqueQueueFamilies = {
            m_QueueFamilyIndices.GraphicsFamily.value(),
            m_QueueFamilyIndices.PresentFamily.value()
        };

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        constexpr float queuePriority = 1.0f;

        for (const uint32_t queueFamily : uniqueQueueFamilies)
        {
            queueCreateInfos.push_back({
                .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = queueFamily,
                .queueCount       = 1,
                .pQueuePriorities = &queuePriority
            });
        }

        constexpr VkPhysicalDeviceFeatures deviceFeatures{};

        VkDeviceCreateInfo createInfo{
            .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .queueCreateInfoCount    = static_cast<uint32_t>(queueCreateInfos.size()),
            .pQueueCreateInfos       = queueCreateInfos.data(),
            .enabledExtensionCount   = static_cast<uint32_t>(DeviceExtensions.size()),
            .ppEnabledExtensionNames = DeviceExtensions.data(),
            .pEnabledFeatures        = &deviceFeatures,
        };

        VK_CHECK(vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device));

        vkGetDeviceQueue(m_Device, m_QueueFamilyIndices.GraphicsFamily.value(), 0, &m_GraphicsQueue);
        vkGetDeviceQueue(m_Device, m_QueueFamilyIndices.PresentFamily.value(), 0, &m_PresentQueue);

        CORE_INFO("Vulkan logical device created");
    }
}

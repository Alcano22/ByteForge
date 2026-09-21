#include "Platform/Vulkan/VulkanInstance.h"
#include "Platform/Vulkan/VulkanHelpers.h"
#include "Engine/Core/Log.h"

#include <GLFW/glfw3.h>

#include <cstring>
#include <stdexcept>

namespace ByteForge
{
    namespace
    {
        VkResult CreateDebugUtilsMessengerEXT(const VkInstance instance,
                                              const VkDebugUtilsMessengerCreateInfoEXT* createInfo,
                                              const VkAllocationCallbacks* allocator,
                                              VkDebugUtilsMessengerEXT* debugMessenger)
        {
            const auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));

            if (func)
                return func(instance, createInfo, allocator, debugMessenger);

            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        void DestroyDebugUtilsMessengerEXT(const VkInstance instance,
                                           const VkDebugUtilsMessengerEXT debugMessenger,
                                           const VkAllocationCallbacks* allocator)
        {
            const auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));

            if (func)
                func(instance, debugMessenger, allocator);
        }
    }

    VulkanInstance::VulkanInstance()
    {
        CreateInstance();
        SetupDebugMessenger();
    }

    VulkanInstance::~VulkanInstance()
    {
        if (m_DebugMessenger != nullptr)
            DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);

        if (m_Instance != nullptr)
            vkDestroyInstance(m_Instance, nullptr);
    }

    void VulkanInstance::CreateInstance()
    {
#ifdef BYTEFORGE_DEBUG
        m_ValidationEnabled = CheckValidationLayerSupport();
        if (!m_ValidationEnabled)
            CORE_WARN("Continuing without Vulkan validation layers (install them, e.g. with the Vulkan SDK)");
#endif

        constexpr VkApplicationInfo appInfo{
            .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName   = "ByteForge Application",
            .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
            .pEngineName        = "ByteForge",
            .engineVersion      = VK_MAKE_VERSION(0, 1, 0),
            .apiVersion         = VK_API_VERSION_1_3
        };

        const std::vector<const char*> extensions = GetRequiredExtensions(m_ValidationEnabled);

        VkInstanceCreateInfo createInfo{
            .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo        = &appInfo,
            .enabledExtensionCount   = static_cast<uint32_t>(extensions.size()),
            .ppEnabledExtensionNames = extensions.data()
        };

        if (m_ValidationEnabled)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(s_ValidationLayers.size());
            createInfo.ppEnabledLayerNames = s_ValidationLayers.data();
        }

        VK_CHECK(vkCreateInstance(&createInfo, nullptr, &m_Instance));

        CORE_INFO("Vulkan instance created successfully");
    }

    void VulkanInstance::SetupDebugMessenger()
    {
        if (!m_ValidationEnabled) return;

        constexpr VkDebugUtilsMessengerCreateInfoEXT createInfo{
            .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                             | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                             | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                             | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = DebugCallback
        };

        VK_CHECK(CreateDebugUtilsMessengerEXT(m_Instance, &createInfo, nullptr, &m_DebugMessenger));

        CORE_INFO("Vulkan debug messenger attached");
    }

    VkBool32 VulkanInstance::DebugCallback(const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                           const VkDebugUtilsMessageTypeFlagsEXT,
                                           const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
                                           void*)
    {
        switch (messageSeverity)
        {
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
                CORE_ERROR("[Vulkan] {}", callbackData->pMessage);
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
                CORE_WARN("[Vulkan] {}", callbackData->pMessage);
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
                CORE_INFO("[Vulkan] {}", callbackData->pMessage);
                break;
            default:
                CORE_TRACE("[Vulkan] {}", callbackData->pMessage);
                break;
        }

        return VK_FALSE;
    }

    bool VulkanInstance::CheckValidationLayerSupport()
    {
        uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char* layerName : s_ValidationLayers)
        {
            bool layerFound = false;
            for (const auto& layerProps : availableLayers)
            {
                if (std::strcmp(layerName, layerProps.layerName) != 0) continue;

                layerFound = true;
                break;
            }

            if (!layerFound)
            {
                CORE_ERROR("Validation layer not found: {}", layerName);
                return false;
            }
        }

        return true;
    }

    std::vector<const char*> VulkanInstance::GetRequiredExtensions(const bool validationEnabled)
    {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        if (!glfwExtensions)
            throw std::runtime_error("GLFW reports no Vulkan surface support - "
                                     "check the Vulkan loader's WSI extensions");

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (validationEnabled)
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        return extensions;
    }
}

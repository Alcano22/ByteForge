#pragma once

#include "Engine/Core/NonCopyable.h"

#include <vulkan/vulkan.h>

#include <array>
#include <vector>

namespace ByteForge
{
    class VulkanInstance : NonCopyable
    {
    public:
        VulkanInstance();
        ~VulkanInstance();

        [[nodiscard]] VkInstance GetHandle() const { return m_Instance; }

    private:
        void CreateInstance();
        void SetupDebugMessenger();

        [[nodiscard]] bool CheckValidationLayerSupport() const;
        [[nodiscard]] std::vector<const char*> GetRequiredExtensions() const;

        static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                            VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                            const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
                                                            void* userData);

    private:
        VkInstance m_Instance = nullptr;
        VkDebugUtilsMessengerEXT m_DebugMessenger = nullptr;

        static constexpr std::array<const char*, 1> s_ValidationLayers = {
            "VK_LAYER_KHRONOS_validation"
        };
    };
}

#pragma once

#include "Engine/Renderer/ShaderStage.h"
#include "Platform/Vulkan/VulkanShaderReflection.h"

#include <vulkan/vulkan.h>

#include <string>

namespace ByteForge
{
    class VulkanDevice;
    struct ShaderSource;

    class VulkanShader
    {
    public:
        VulkanShader(const VulkanDevice& device, const ShaderSource& source,
                     ShaderStage stage, std::string entryPoint);
        ~VulkanShader();

        VulkanShader(const VulkanShader&) = delete;
        VulkanShader& operator=(const VulkanShader&) = delete;
        VulkanShader(VulkanShader&&) = delete;
        VulkanShader& operator=(VulkanShader&&) = delete;

        [[nodiscard]] VkShaderModule GetHandle() const { return m_Module; }
        [[nodiscard]] const ShaderReflection& GetReflection() const { return m_Reflection; }

        [[nodiscard]] const std::string& GetEntryPoint() const { return m_EntryPoint; }

    private:
        const VulkanDevice& m_Device;
        std::string m_EntryPoint;
        VkShaderModule m_Module = nullptr;
        ShaderReflection m_Reflection;
    };
}

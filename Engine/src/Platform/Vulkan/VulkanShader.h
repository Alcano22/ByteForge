#pragma once

#include "Engine/Renderer/ShaderStage.h"
#include "Platform/Vulkan/VulkanShaderReflection.h"

#include <vulkan/vulkan.h>

#include <string>

namespace ByteForge
{
    class VulkanDevice;
    class VulkanShaderCompiler;

    class VulkanShader
    {
    public:
        VulkanShader(const VulkanDevice& device, const std::string& source,
                     ShaderStage stage, const std::string& entryPoint = "main");
        ~VulkanShader();

        VulkanShader(const VulkanShader&) = delete;
        VulkanShader& operator=(const VulkanShader&) = delete;
        VulkanShader(VulkanShader&&) = delete;
        VulkanShader& operator=(VulkanShader&&) = delete;

        [[nodiscard]] VkShaderModule GetHandle() const { return m_Module; }
        [[nodiscard]] const ShaderReflection& GetReflection() const { return m_Reflection; }

    private:
        const VulkanDevice& m_Device;
        VkShaderModule m_Module = nullptr;
        ShaderReflection m_Reflection;
    };
}

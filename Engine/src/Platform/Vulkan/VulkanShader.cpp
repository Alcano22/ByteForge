#include "Platform/Vulkan/VulkanShader.h"
#include "Platform/Vulkan/VulkanShaderCompiler.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanHelpers.h"
#include "Renderer/ShaderSource.h"
#include "Engine/Core/Log.h"

#include <utility>
#include <vector>

namespace ByteForge
{
    VulkanShader::VulkanShader(const VulkanDevice& device, const ShaderSource& source,
                               const ShaderStage stage, std::string entryPoint)
        : m_Device(device), m_EntryPoint(std::move(entryPoint))
    {
        const std::vector<uint32_t> spirv = VulkanShaderCompiler::Get().Compile(source, stage, m_EntryPoint);
        m_Reflection = ShaderReflection::Reflect(spirv, stage);

        const VkShaderModuleCreateInfo createInfo{
            .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = spirv.size() * sizeof(uint32_t),
            .pCode    = spirv.data()
        };

        VK_CHECK(vkCreateShaderModule(m_Device.GetHandle(), &createInfo, nullptr, &m_Module));

        CORE_TRACE("Shader module created for '{}' ({}, {} bytes SPIR-V)",
                   source.Path.empty() ? std::string("<inline>") : source.Path.filename().string(),
                   m_EntryPoint, createInfo.codeSize);
    }

    VulkanShader::~VulkanShader()
    {
        if (m_Module != nullptr)
            vkDestroyShaderModule(m_Device.GetHandle(), m_Module, nullptr);
    }
}

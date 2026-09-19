#include "Platform/Vulkan/VulkanShader.h"
#include "Platform/Vulkan/VulkanShaderCompiler.h"
#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanHelpers.h"
#include "Engine/Core/Log.h"

#include <vector>

namespace ByteForge
{
    VulkanShader::VulkanShader(const VulkanDevice& device, const std::string& source,
                               const ShaderStage stage, const std::string& entryPoint)
        : m_Device(device)
    {
        const std::vector<uint32_t> spirv = VulkanShaderCompiler::Get().Compile(source, stage, entryPoint);

        const VkShaderModuleCreateInfo createInfo{
            .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = spirv.size() * sizeof(uint32_t),
            .pCode    = spirv.data()
        };

        VK_CHECK(vkCreateShaderModule(m_Device.GetHandle(), &createInfo, nullptr, &m_Module));

        CORE_INFO("Vulkan shader module created ({} bytes SPIR-V)", createInfo.codeSize);
    }

    VulkanShader::~VulkanShader()
    {
        if (m_Module != nullptr)
            vkDestroyShaderModule(m_Device.GetHandle(), m_Module, nullptr);
    }
}

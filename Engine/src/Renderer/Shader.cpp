#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/RendererAPI.h"

#include "Platform/Vulkan/VulkanShaderProgram.h"

namespace ByteForge
{
    Ref<Shader> Shader::Create(const std::string& vertexSrc, const std::string& fragmentSrc,
                               const BufferLayout& vertexLayout, const uint32_t uniformBufferSize,
                               const uint32_t uniformStageFlags, const uint32_t pushConstantStageFlags,
                               const uint32_t pushConstantSize)
    {
        return CreateRHIObject<VulkanShaderProgram, Shader>(vertexSrc, fragmentSrc, vertexLayout,
            uniformBufferSize, uniformStageFlags, pushConstantStageFlags, pushConstantSize);
    }
}

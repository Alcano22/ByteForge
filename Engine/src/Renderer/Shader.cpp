#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/RendererAPI.h"

#include "Platform/Vulkan/VulkanShaderProgram.h"

namespace ByteForge
{
    Ref<Shader> Shader::Create(const std::string& vertexSrc, const std::string& fragmentSrc)
    {
        return CreateRHIObject<VulkanShaderProgram, Shader>(vertexSrc, fragmentSrc);
    }
}

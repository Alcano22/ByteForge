#pragma once

#include "Engine/Renderer/Shader.h"
#include "Platform/Vulkan/VulkanShader.h"
#include "Platform/Vulkan/VulkanShaderReflection.h"

#include <vulkan/vulkan.h>

#include <string>

namespace ByteForge
{
    class VulkanShaderProgram : public Shader
    {
    public:
        VulkanShaderProgram(const std::string& vertexSrc, const std::string& fragmentSrc);

        [[nodiscard]] VkShaderModule GetVertexModule() const { return m_Vertex.GetHandle(); }
        [[nodiscard]] VkShaderModule GetFragmentModule() const { return m_Fragment.GetHandle(); }
        [[nodiscard]] const ShaderReflection& GetReflection() const { return m_Reflection; }

    private:
        VulkanShader m_Vertex;
        VulkanShader m_Fragment;
        ShaderReflection m_Reflection;
    };
}

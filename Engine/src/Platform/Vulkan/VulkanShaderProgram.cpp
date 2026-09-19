#include "Platform/Vulkan/VulkanShaderProgram.h"
#include "Platform/Vulkan/VulkanContext.h"

namespace ByteForge
{
    VulkanShaderProgram::VulkanShaderProgram(const std::string& vertexSrc, const std::string& fragmentSrc)
        : m_Vertex(VulkanContext::Get().GetDevice(), vertexSrc, ShaderStage::Vertex),
          m_Fragment(VulkanContext::Get().GetDevice(), fragmentSrc, ShaderStage::Fragment),
          m_Reflection(ShaderReflection::Merge(m_Vertex.GetReflection(), m_Fragment.GetReflection())) {}
}

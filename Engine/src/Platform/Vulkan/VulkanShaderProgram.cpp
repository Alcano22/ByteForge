#include "Platform/Vulkan/VulkanShaderProgram.h"
#include "Platform/Vulkan/VulkanContext.h"
#include "Renderer/ShaderSource.h"

namespace ByteForge
{
    namespace
    {
        constexpr const char* InlineEntryPoint  = "main";
        constexpr const char* FileVertexEntry   = "VSMain";
        constexpr const char* FileFragmentEntry = "PSMain";
    }

    VulkanShaderProgram::VulkanShaderProgram(const std::string& vertexSrc, const std::string& fragmentSrc)
        : m_Vertex(VulkanContext::Get().GetDevice(), ShaderSource{ .Code = vertexSrc },
                   ShaderStage::Vertex, InlineEntryPoint),
          m_Fragment(VulkanContext::Get().GetDevice(), ShaderSource{ .Code = fragmentSrc },
                   ShaderStage::Fragment, InlineEntryPoint),
          m_Reflection(ShaderReflection::Merge(m_Vertex.GetReflection(), m_Fragment.GetReflection())) {}

    VulkanShaderProgram::VulkanShaderProgram(const ShaderSource& source)
        : m_Vertex(VulkanContext::Get().GetDevice(), source, ShaderStage::Vertex, FileVertexEntry),
          m_Fragment(VulkanContext::Get().GetDevice(), source, ShaderStage::Fragment, FileFragmentEntry),
          m_Reflection(ShaderReflection::Merge(m_Vertex.GetReflection(), m_Fragment.GetReflection())) {}
}

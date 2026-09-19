#include "Engine/Renderer/UniformBuffer.h"
#include "Engine/Renderer/RendererAPI.h"

#include "Platform/Vulkan/VulkanUniformBuffer.h"

namespace ByteForge
{
    Ref<UniformBuffer> UniformBuffer::Create(const uint32_t size)
    {
        return CreateRHIObject<VulkanUniformBuffer, UniformBuffer>(size);
    }
}

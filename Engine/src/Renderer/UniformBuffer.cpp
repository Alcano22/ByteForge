#include "Engine/Renderer/UniformBuffer.h"
#include "Renderer/RenderBackend.h"

namespace ByteForge
{
    Ref<UniformBuffer> UniformBuffer::Create(const uint32_t size)
    {
        return RenderBackend::Get().CreateUniformBuffer(size);
    }
}

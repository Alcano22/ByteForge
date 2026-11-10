#include "Engine/Renderer/UniformBuffer.h"
#include "Renderer/RenderBackend.h"

namespace ByteForge
{
    Ref<UniformBuffer> UniformBuffer::Create(const u32 size)
    {
        return RenderBackend::Get().CreateUniformBuffer(size);
    }
}

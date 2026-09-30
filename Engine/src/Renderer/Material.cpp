#include "Engine/Renderer/Material.h"
#include "Renderer/RenderBackend.h"

namespace ByteForge
{
    Ref<Material> Material::Create(const Ref<Pipeline>& pipeline)
    {
        return RenderBackend::Get().CreateMaterial(pipeline);
    }
}

#include "Engine/Renderer/Material.h"
#include "Engine/Renderer/RendererAPI.h"

#include "Platform/Vulkan/VulkanMaterial.h"

namespace ByteForge
{
    Ref<Material> Material::Create(const Ref<Pipeline>& pipeline)
    {
        return CreateRHIObject<VulkanMaterial, Material>(pipeline);
    }
}

#include "Engine/Renderer/Pipeline.h"
#include "Engine/Renderer/RendererAPI.h"

#include "Platform/Vulkan/VulkanPipeline.h"

namespace ByteForge
{
    Ref<Pipeline> Pipeline::Create(const PipelineSpec& spec)
    {
        return CreateRHIObject<VulkanPipeline, Pipeline>(spec);
    }
}

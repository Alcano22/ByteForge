#include "Engine/Renderer/Pipeline.h"
#include "Renderer/RenderBackend.h"

namespace ByteForge
{
    Ref<Pipeline> Pipeline::Create(const PipelineSpec& spec)
    {
        return RenderBackend::Get().CreatePipeline(spec);
    }
}

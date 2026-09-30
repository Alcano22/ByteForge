#include "Engine/Renderer/RenderTarget.h"
#include "Renderer/RenderBackend.h"

namespace ByteForge
{
    Ref<RenderTarget> RenderTarget::Create(const RenderTargetSpec& spec)
    {
        return RenderBackend::Get().CreateRenderTarget(spec);
    }
}

#include "Engine/Renderer/RenderTarget.h"
#include "Engine/Renderer/RendererAPI.h"

#include "Platform/Vulkan/VulkanRenderTarget.h"

namespace ByteForge
{
    Ref<RenderTarget> RenderTarget::Create(const RenderTargetSpec& spec)
    {
        return CreateRHIObject<VulkanRenderTarget, RenderTarget>(spec);
    }
}

#include "Engine/Renderer/RendererAPI.h"

#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanDeletionQueue.h"

namespace ByteForge
{
    RendererAPI::API RendererAPI::s_API = RendererAPI::API::Vulkan;

    void DeferRHIDestroy(std::function<void()> destroy)
    {
        DispatchRHICall([&]
        {
            VulkanContext* context = VulkanContext::TryGet();
            if (context == nullptr)
            {
                CORE_CRITICAL("RHI object released after the graphics context was destroyed, leaking it");
                return;
            }

            context->GetDeletionQueue().Push(std::move(destroy));
        });
    }
}

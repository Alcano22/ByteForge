#include "Engine/Renderer/GraphicsContext.h"
#include "Engine/Renderer/RendererAPI.h"
#include "Engine/Core/Log.h"

#include "Platform/Vulkan/VulkanContext.h"

namespace ByteForge
{
    Scope<GraphicsContext> GraphicsContext::Create(GLFWwindow* windowHandle)
    {
        switch (RendererAPI::GetAPI())
        {
            case RendererAPI::API::None:
                CORE_CRITICAL("RendererAPI::None is not supported");
                return nullptr;
            case RendererAPI::API::Vulkan:
                return std::make_unique<VulkanContext>(windowHandle);
        }

        CORE_CRITICAL("Unknown RendererAPI");
        return nullptr;
    }
}

#include "Engine/Renderer/GraphicsContext.h"
#include "Engine/Renderer/RendererAPI.h"

#include "Platform/Vulkan/VulkanContext.h"

#include <stdexcept>

namespace ByteForge
{
    Scope<GraphicsContext> GraphicsContext::Create(GLFWwindow* windowHandle)
    {
        switch (RendererAPI::GetAPI())
        {
            case RendererAPI::API::Vulkan: return MakeScope<VulkanContext>(windowHandle);
            case RendererAPI::API::None:   break;
        }

        throw std::runtime_error("GraphicsContext::Create: no supported renderer API selected");
    }
}

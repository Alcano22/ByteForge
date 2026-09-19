#include "Engine/ImGui/ImGuiRenderer.h"
#include "Engine/Renderer/RendererAPI.h"
#include "Engine/Core/Log.h"

#include "Platform/Vulkan/VulkanImGuiRenderer.h"

namespace ByteForge
{
    Scope<ImGuiRenderer> ImGuiRenderer::Create()
    {
        switch (RendererAPI::GetAPI())
        {
            case RendererAPI::API::None:
                CORE_CRITICAL("RendererAPI::None is not supported");
                return nullptr;
            case RendererAPI::API::Vulkan:
                return MakeScope<VulkanImGuiRenderer>();
        }

        CORE_CRITICAL("Unknown RendererAPI");
        return nullptr;
    }
}

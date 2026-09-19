#pragma once

#include "Engine/ImGui/ImGuiRenderer.h"

#include <vulkan/vulkan.h>

namespace ByteForge
{
    class VulkanImGuiRenderer : public ImGuiRenderer
    {
    public:
        void Init(GLFWwindow* windowHandle) override;
        void Shutdown() override;

        void NewFrame() override;
        void RenderDrawData() override;
    };
}

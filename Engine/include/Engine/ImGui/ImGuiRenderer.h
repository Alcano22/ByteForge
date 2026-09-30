#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Renderer/RenderTarget.h"
#include "Engine/Renderer/Texture2D.h"

#include <imgui.h>

struct GLFWwindow;

namespace ByteForge
{
    class ImGuiRenderer
    {
    public:
        virtual ~ImGuiRenderer() = default;

        virtual void Init(GLFWwindow* windowHandle) = 0;
        virtual void Shutdown() = 0;

        virtual void NewFrame() = 0;
        virtual void RenderDrawData() = 0;

        [[nodiscard]] virtual ImTextureID GetTextureId(const Ref<Texture2D>& texture) = 0;
        [[nodiscard]] virtual ImTextureID GetTextureId(const Ref<RenderTarget>& target) = 0;

        [[nodiscard]] static ImGuiRenderer& Get();

        static Scope<ImGuiRenderer> Create();

    private:
        friend class ImGuiLayer;

        static ImGuiRenderer* s_Active;
    };
}

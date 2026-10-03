#pragma once

#include <Engine/Core/Core.h>
#include <Engine/Renderer/LineRenderer.h>
#include <Engine/Renderer/Renderer.h>
#include <Engine/Renderer/Renderer2D.h>
#include <Engine/Renderer/RenderTarget.h>

#include <glm/glm.hpp>
#include <imgui.h>

#include <cstdint>
#include <optional>
#include <utility>

namespace ByteForge
{
    struct ViewportCanvasSpec
    {
        glm::vec4 ClearColor{ 0.01f, 0.01f, 0.01f, 1.0f };

        std::optional<float> FixedAspect;

        bool EntityIds = false;
    };

    class ViewportCanvas
    {
    public:
        explicit ViewportCanvas(ViewportCanvasSpec spec = {});

        template<typename Fn>
        void Render(Fn&& draw)
        {
            EnsureTargetSize();

            Renderer::BeginRenderTarget(m_Target);
            std::forward<Fn>(draw)(*m_Renderer2D, *m_LineRenderer);
            Renderer::EndRenderTarget();
        }

        void Draw();

        [[nodiscard]] uint32_t ReadEntityId(const glm::vec2& viewportPoint) const;

        [[nodiscard]] bool IsHovered() const { return m_Hovered; }
        [[nodiscard]] glm::vec2 GetSize() const { return glm::vec2(m_Size); }
        [[nodiscard]] ImVec2 GetScreenMin() const { return m_ScreenMin; }

        [[nodiscard]] glm::vec2 ToViewport(const ImVec2& screenPoint) const
        {
            return { screenPoint.x - m_ScreenMin.x, screenPoint.y - m_ScreenMin.y };
        }

    private:
        void EnsureTargetSize();

    private:
        ViewportCanvasSpec m_Spec;
        Scope<Renderer2D> m_Renderer2D;
        Scope<LineRenderer> m_LineRenderer;
        Ref<RenderTarget> m_Target;

        glm::uvec2 m_Size{ 1280, 720 };
        ImVec2 m_ScreenMin{};
        bool m_Hovered = false;
    };
}

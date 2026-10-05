#include "Editor/Viewport/ViewportCanvas.h"

#include <Engine/ImGui/ImGuiWidgets.h>

#include <vector>

namespace ByteForge
{
    namespace
    {
        constexpr ImageFormat ColorFormat    = ImageFormat::RGBA8_SRGB;
        constexpr ImageFormat DepthFormat    = ImageFormat::Depth32F;
        constexpr ImageFormat EntityIdFormat = ImageFormat::R32_UINT;

        constexpr uint32_t EntityIdAttachment = 1;
    }

    ViewportCanvas::ViewportCanvas(ViewportCanvasSpec spec)
        : m_Spec(std::move(spec))
    {
        const ImageFormat idFormat = m_Spec.EntityIds ? EntityIdFormat : ImageFormat::None;

        m_Renderer2D = MakeScope<Renderer2D>(Renderer2DSpec{
            .ColorFormat    = ColorFormat,
            .DepthFormat    = DepthFormat,
            .EntityIdFormat = idFormat
        });

        m_LineRenderer = MakeScope<LineRenderer>(LineRendererSpec{
            .ColorFormat    = ColorFormat,
            .DepthFormat    = DepthFormat,
            .EntityIdFormat = idFormat
        });

        EnsureTargetSize();
    }

    void ViewportCanvas::Draw()
    {
        m_Hovered = false;

        const ImVec2 available = ImGui::GetContentRegionAvail();
        if (available.x < 1.0f || available.y < 1.0f) return;

        ImVec2 imageSize = available;
        if (m_Spec.FixedAspect)
        {
            const float aspect = *m_Spec.FixedAspect;
            imageSize = available.x / available.y > aspect ? ImVec2(available.y * aspect, available.y)
                                                           : ImVec2(available.x, available.x / aspect);

            const ImVec2 regionMin = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(regionMin,
                ImVec2(regionMin.x + available.x, regionMin.y + available.y), IM_COL32_BLACK);
            ImGui::SetCursorScreenPos(ImVec2(regionMin.x + (available.x - imageSize.x) * 0.5f,
                                             regionMin.y + (available.y - imageSize.y) * 0.5f));
        }

        m_ScreenMin = ImGui::GetCursorScreenPos();
        m_Size = { static_cast<uint32_t>(imageSize.x), static_cast<uint32_t>(imageSize.y) };

        UI::Image(m_Target, imageSize);
        m_Hovered = ImGui::IsItemHovered();
    }

    uint32_t ViewportCanvas::ReadEntityId(const glm::vec2& viewportPoint) const
    {
        if (!m_Spec.EntityIds || viewportPoint.x < 0.0f || viewportPoint.y < 0.0f)
            return 0;

        const uint32_t x = static_cast<uint32_t>(viewportPoint.x);
        const uint32_t y = static_cast<uint32_t>(viewportPoint.y);
        if (x >= m_Target->GetWidth() || y >= m_Target->GetHeight())
            return 0;

        return m_Target->ReadPixel(EntityIdAttachment, x, y);
    }

    void ViewportCanvas::EnsureTargetSize()
    {
        if (m_Target && m_Target->GetWidth() == m_Size.x && m_Target->GetHeight() == m_Size.y) return;

        std::vector<ImageFormat> colorFormats{ ColorFormat };
        if (m_Spec.EntityIds)
            colorFormats.push_back(EntityIdFormat);

        m_Target = RenderTarget::Create({
            .Width        = m_Size.x,
            .Height       = m_Size.y,
            .ColorFormats = std::move(colorFormats),
            .DepthFormat  = DepthFormat,
            .ClearColor   = m_Spec.ClearColor
        });
    }
}

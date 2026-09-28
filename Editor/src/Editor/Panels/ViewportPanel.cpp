#include "Editor/Panels/ViewportPanel.h"
#include "Editor/EditorContext.h"

#include <Engine/Renderer/Renderer.h>
#include <Engine/Scene/Scene.h>

#include <imgui.h>

namespace ByteForge
{
    ViewportPanel::ViewportPanel(EditorContext& context)
        : EditorPanel(context, "Viewport"), m_Camera(6.0f, 16.0f / 9.0f, -1.0f, 1.0f)
    {
        m_Renderer2D = MakeScope<Renderer2D>(Renderer2DSpec{
            .ColorFormat = ImageFormat::RGBA8_SRGB,
            .DepthFormat = ImageFormat::Depth32F
        });
        m_Target = CreateTarget(m_ViewportSize.x, m_ViewportSize.y);
    }

    void ViewportPanel::OnUpdate(const Timestep ts)
    {
        if (m_ViewportSize.x != m_Target->GetWidth() || m_ViewportSize.y != m_Target->GetHeight())
            m_Target = CreateTarget(m_ViewportSize.x, m_ViewportSize.y);

        Scene* scene = GetContext().ActiveScene;
        if (scene == nullptr) return;

        Renderer::BeginRenderTarget(m_Target);

        if (GetContext().IsPlaying())
            scene->OnUpdateRuntime(ts, *m_Renderer2D, m_Camera);
        else if (GetContext().IsPaused())
        {
            if (GetContext().ConsumeStepRequest())
                scene->OnUpdateRuntime(ts, *m_Renderer2D, m_Camera);
            else
                scene->OnUpdateEditor(ts, *m_Renderer2D, m_Camera);
        } else
            scene->OnUpdateEditor(ts, *m_Renderer2D, m_Camera);

        Renderer::EndRenderTarget();
    }

    void ViewportPanel::OnImGuiRender()
    {
        if (!m_Open) return;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        if (!ImGui::Begin(GetName().c_str(), &m_Open))
        {
            ImGui::End();
            ImGui::PopStyleVar();
            return;
        }

        m_ViewportHovered = ImGui::IsWindowHovered();

        const ImVec2 available = ImGui::GetContentRegionAvail();
        if (available.x >= 1.0f && available.y >= 1.0f)
        {
            constexpr float targetAspect = 16.0f / 9.0f;
            const float availableAspect = available.x / available.y;

            const ImVec2 imageSize = availableAspect > targetAspect
                                   ? ImVec2{ available.y * targetAspect, available.y }
                                   : ImVec2{ available.x, available.x / targetAspect };

            const ImVec2 offset{ (available.x - imageSize.x) * 0.5f, (available.y - imageSize.y) * 0.5f };

            const ImVec2 regionStart = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(regionStart,
                                                      { regionStart.x + available.x, regionStart.y + available.y },
                                                      IM_COL32_BLACK);

            ImGui::SetCursorPos({ ImGui::GetCursorPos().x + offset.x, ImGui::GetCursorPos().y + offset.y });

            m_ViewportSize = { static_cast<uint32_t>(imageSize.x), static_cast<uint32_t>(imageSize.y) };

            const auto textureId = static_cast<ImTextureID>(m_Target->GetImGuiTextureId());
            ImGui::Image(textureId, imageSize);
        }

        ImGui::End();
        ImGui::PopStyleVar();
    }

    Ref<RenderTarget> ViewportPanel::CreateTarget(const uint32_t width, const uint32_t height)
    {
        return RenderTarget::Create({ .Width = width, .Height = height });
    }
}

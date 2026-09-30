#include "Engine/ImGui/ImGuiWidgets.h"
#include "Engine/ImGui/ImGuiRenderer.h"

namespace ByteForge::UI
{
    namespace
    {
        class ScopedSamplerFilter
        {
        public:
            explicit ScopedSamplerFilter(ImDrawList& drawList, const TextureFilter filter)
                : m_DrawList(drawList)
            {
                if (filter != TextureFilter::Nearest) return;

                const ImGuiPlatformIO& platformIO = ImGui::GetPlatformIO();
                if (platformIO.DrawCallback_SetSamplerNearest == nullptr ||
                    platformIO.DrawCallback_SetSamplerLinear == nullptr) return;

                m_DrawList.AddCallback(platformIO.DrawCallback_SetSamplerNearest, nullptr);
                m_Restore = platformIO.DrawCallback_SetSamplerLinear;
            }

            ~ScopedSamplerFilter()
            {
                if (m_Restore != nullptr)
                    m_DrawList.AddCallback(m_Restore, nullptr);
            }

            ScopedSamplerFilter(const ScopedSamplerFilter&) = delete;
            ScopedSamplerFilter& operator=(const ScopedSamplerFilter&) = delete;

        private:
            ImDrawList& m_DrawList;
            ImDrawCallback m_Restore = nullptr;
        };
    }

    void Image(const Ref<Texture2D>& texture, const ImVec2& size, const ImVec4& tint)
    {
        const ScopedSamplerFilter filter(*ImGui::GetWindowDrawList(), texture->GetFilter());
        ImGui::ImageWithBg(ImGuiRenderer::Get().GetTextureId(texture), size,
                           ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), ImVec4(0.0f, 0.0f, 0.0f, 0.0f), tint);
    }

    void Image(const Ref<RenderTarget>& target, const ImVec2& size)
    {
        ImGui::Image(ImGuiRenderer::Get().GetTextureId(target), size);
    }

    bool ImageButton(const char* id, const Ref<Texture2D>& texture, const ImVec2& size,
                     const ImVec4& tint, const ImVec4& background)
    {
        const ScopedSamplerFilter filter(*ImGui::GetWindowDrawList(), texture->GetFilter());
        return ImGui::ImageButton(id, ImGuiRenderer::Get().GetTextureId(texture), size,
                                  ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), background, tint);
    }

    void DrawImage(ImDrawList& drawList, const Ref<Texture2D>& texture,
                   const ImVec2& min, const ImVec2& max, const ImU32 tint)
    {
        const ScopedSamplerFilter filter(drawList, texture->GetFilter());
        drawList.AddImage(ImGuiRenderer::Get().GetTextureId(texture), min, max,
                          ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), tint);
    }
}

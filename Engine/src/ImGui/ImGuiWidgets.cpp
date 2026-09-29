#include "Engine/ImGui/ImGuiWidgets.h"

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

    void Image(const Ref<Texture2D>& texture, const ImVec2& size)
    {
        const ScopedSamplerFilter filter(*ImGui::GetWindowDrawList(), texture->GetFilter());
        ImGui::Image(static_cast<ImTextureID>(texture->GetImGuiTextureId()), size);
    }

    bool ImageButton(const char* id, const Ref<Texture2D>& texture, const ImVec2& size)
    {
        const ScopedSamplerFilter filter(*ImGui::GetWindowDrawList(), texture->GetFilter());
        return ImGui::ImageButton(id, static_cast<ImTextureID>(texture->GetImGuiTextureId()), size);
    }

    void DrawImage(ImDrawList& drawList, const Ref<Texture2D>& texture, const ImVec2& min, const ImVec2& max)
    {
        const ScopedSamplerFilter filter(drawList, texture->GetFilter());
        drawList.AddImage(static_cast<ImTextureID>(texture->GetImGuiTextureId()), min, max);
    }
}

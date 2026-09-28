#include "Engine/ImGui/ImGuiWidgets.h"

namespace ByteForge::UI
{
    namespace
    {
        class ScopedSamplerFilter
        {
        public:
            explicit ScopedSamplerFilter(const TextureFilter filter)
            {
                if (filter != TextureFilter::Nearest) return;

                const ImGuiPlatformIO& platformIO = ImGui::GetPlatformIO();
                if (platformIO.DrawCallback_SetSamplerNearest == nullptr ||
                    platformIO.DrawCallback_SetSamplerLinear == nullptr) return;

                ImGui::GetWindowDrawList()->AddCallback(platformIO.DrawCallback_SetSamplerNearest, nullptr);
                m_Restore = platformIO.DrawCallback_SetSamplerLinear;
            }

            ~ScopedSamplerFilter()
            {
                if (m_Restore != nullptr)
                    ImGui::GetWindowDrawList()->AddCallback(m_Restore, nullptr);
            }

            ScopedSamplerFilter(const ScopedSamplerFilter&) = delete;
            ScopedSamplerFilter& operator=(const ScopedSamplerFilter&) = delete;

        private:
            ImDrawCallback m_Restore = nullptr;
        };
    }

    void Image(const Ref<Texture2D>& texture, const ImVec2& size)
    {
        const ScopedSamplerFilter filter(texture->GetFilter());
        ImGui::Image(static_cast<ImTextureID>(texture->GetImGuiTextureId()), size);
    }

    bool ImageButton(const char* id, const Ref<Texture2D>& texture, const ImVec2& size)
    {
        const ScopedSamplerFilter filter(texture->GetFilter());
        return ImGui::ImageButton(id, static_cast<ImTextureID>(texture->GetImGuiTextureId()), size);
    }
}

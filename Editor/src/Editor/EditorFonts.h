#pragma once

#include <imgui.h>

#include <array>
#include <cstddef>
#include <filesystem>

namespace ByteForge
{
    enum class EditorFont
    {
        Regular,
        SemiBold,
        Mono,
        Count
    };

    class EditorFonts
    {
    public:
        void Load(const std::filesystem::path& directory, float size);

        [[nodiscard]] ImFont* Get(EditorFont font) const;

    private:
        std::array<ImFont*, static_cast<size_t>(EditorFont::Count)> m_Fonts{};
    };

    class ScopedFont
    {
    public:
        explicit ScopedFont(ImFont* font, const float size = 0.0f) { ImGui::PushFont(font, size); }
        ~ScopedFont() { ImGui::PopFont(); }

        ScopedFont(const ScopedFont&) = delete;
        ScopedFont& operator=(const ScopedFont&) = delete;
    };
}

#include "Editor/EditorFonts.h"

#include <Engine/Core/Log.h>

#include <stdexcept>
#include <string_view>
#include <system_error>

namespace ByteForge
{
    namespace
    {
        constexpr size_t FontCount = static_cast<size_t>(EditorFont::Count);

        constexpr std::array<std::string_view, FontCount> FontFiles{
            "inter_regular.ttf",
            "inter_semibold.ttf",
            "jetbrainsmono_regular.ttf"
        };
    }

    void EditorFonts::Load(const std::filesystem::path& directory, const float size)
    {
        ImGuiIO& io = ImGui::GetIO();
        ImFont* fallback = nullptr;

        for (size_t i = 0; i < FontCount; ++i)
        {
            const std::filesystem::path path = directory / FontFiles[i];

            std::error_code error;
            if (std::filesystem::is_regular_file(path, error))
            {
                m_Fonts[i] = io.Fonts->AddFontFromFileTTF(path.string().c_str(), size);
                if (m_Fonts[i] != nullptr) continue;
            }

            APP_ERROR("EditorFonts: cannot load '{}', using the built-in font", path.string());

            if (fallback == nullptr)
                fallback = io.Fonts->AddFontDefault();
            m_Fonts[i] = fallback;
        }

        io.FontDefault = m_Fonts[static_cast<size_t>(EditorFont::Regular)];
    }

    ImFont* EditorFonts::Get(const EditorFont font) const
    {
        ImFont* result = m_Fonts.at(static_cast<size_t>(font));
        if (result == nullptr)
            throw std::runtime_error("EditorFonts::Get: fonts are not loaded, call Load first");

        return result;
    }
}

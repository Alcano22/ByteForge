#include "Editor/Inspectors/TextureSettingsEditor.h"
#include "Editor/EditorWidgets.h"

#include <imgui.h>
#include <magic_enum/magic_enum.hpp>

#include <array>

namespace ByteForge::EditorUI
{
    bool EditTextureSettings(TextureSettings& settings)
    {
        constexpr std::array<ImageFormat, 2> formats{ ImageFormat::RGBA8_SRGB, ImageFormat::RGBA8_UNORM };

        bool changed = false;
        changed |= EnumCombo<ImageFormat>("Format", settings.Format, formats);
        changed |= EnumCombo<TextureFilter>("Filter", settings.Filter, magic_enum::enum_values<TextureFilter>());
        changed |= EnumCombo<TextureWrap>("Wrap", settings.Wrap, magic_enum::enum_values<TextureWrap>());
        changed |= ImGui::Checkbox("Generate Mips", &settings.GenerateMips);
        return changed;
    }
}

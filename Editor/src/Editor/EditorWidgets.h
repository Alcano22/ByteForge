#pragma once

#include "Editor/Selection.h"

#include <Engine/Assets/TextureAsset.h>

#include <imgui.h>
#include <magic_enum/magic_enum.hpp>

#include <span>
#include <string>

namespace ByteForge::EditorUI
{
    inline constexpr ImVec4 ErrorColor{ 0.90f, 0.35f, 0.35f, 1.0f };

    bool TextureAssetField(const char* label, Ref<TextureAsset>& asset, Selection& selection);

    template<typename T>
    bool EnumCombo(const char* label, T& value, const std::span<const T> options)
    {
        bool changed = false;

        if (ImGui::BeginCombo(label, std::string(magic_enum::enum_name(value)).c_str()))
        {
            for (const T option : options)
            {
                const bool selected = option == value;
                if (ImGui::Selectable(std::string(magic_enum::enum_name(option)).c_str(), selected) && !selected)
                {
                    value = option;
                    changed = true;
                }

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        return changed;
    }
}

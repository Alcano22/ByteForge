#pragma once

#include "Editor/Selection.h"

#include <Engine/Assets/AssetType.h>
#include <Engine/Assets/TextureAsset.h>
#include <Engine/Scene/UUID.h>
#include <Engine/Renderer/Texture2D.h>

#include <imgui.h>
#include <magic_enum/magic_enum.hpp>

#include <algorithm>
#include <cstddef>
#include <format>
#include <span>
#include <string>
#include <type_traits>

namespace ByteForge::EditorUI
{
    inline constexpr ImVec4 ErrorColor{ 0.90f, 0.35f, 0.35f, 1.0f };

    bool TextureAssetField(const char* label, Ref<TextureAsset>& asset, Selection& selection);

    bool AssetReferenceField(const char* label, AssetType type, UUID& handle,
                             Selection& selection, const char* noneText = "None");

    bool IconButton(const char* id, const Ref<Texture2D>& icon, bool active = false);

    template<typename T>
    struct ToggleOption
    {
        T Value;
        const char* Label;
        Ref<Texture2D> Icon = nullptr;
    };

    template<typename T>
    using ToggleOptions = std::type_identity_t<std::span<const ToggleOption<T>>>;

    template<typename T>
    [[nodiscard]] size_t FindOption(const T& value, ToggleOptions<T> options)
    {
        const auto it = std::ranges::find(options, value, &ToggleOption<T>::Value);
        return it != options.end() ? static_cast<size_t>(it - options.begin()) : 0;
    }

    template<typename T>
    [[nodiscard]] T NextOption(const T& value, ToggleOptions<T> options)
    {
        return options[(FindOption(value, options) + 1) % options.size()].Value;
    }

    namespace Detail
    {
        template<typename T, typename DrawFn>
        bool Toggle(T& value, ToggleOptions<T> options, const char* shortcut, DrawFn&& drawButton)
        {
            if (options.empty())
                return false;

            const size_t index = FindOption(value, options);
            const ToggleOption<T>& current = options[index];
            const ToggleOption<T>& next = options[(index + 1) % options.size()];

            const bool pressed = drawButton(current);

            if (shortcut != nullptr)
                ImGui::SetItemTooltip("%s (%s), click for %s", current.Label, shortcut, next.Label);
            else
                ImGui::SetItemTooltip("%s, click for %s", current.Label, next.Label);

            if (pressed)
                value = next.Value;

            return pressed;
        }
    }

    template<typename T>
    bool ToggleButton(const char* id, T& value, ToggleOptions<T> options, const char* shortcut = nullptr)
    {
        float width = 0.0f;
        for (const ToggleOption<T>& option : options)
            width = std::max(width, ImGui::CalcTextSize(option.Label).x);
        width += ImGui::GetStyle().FramePadding.x * 2.0f;

        return Detail::Toggle(value, options, shortcut, [&](const ToggleOption<T>& current)
        {
            const std::string label = std::format("{}###{}", current.Label, id);
            return ImGui::Button(label.c_str(), ImVec2(width, 0.0f));
        });
    }

    template<typename T>
    bool ToggleIconButton(const char* id, T& value, ToggleOptions<T> options, const char* shortcut = nullptr)
    {
        return Detail::Toggle(value, options, shortcut, [&](const ToggleOption<T>& current)
        {
            return IconButton(id, current.Icon);
        });
    }

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

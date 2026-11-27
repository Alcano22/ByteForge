#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Input/KeyCodes.h"
#include "Engine/Input/MouseButtons.h"
#include "Engine/Input/GamepadCodes.h"
#include "Engine/Input/GamepadFeatures.h"
#include "Engine/Input/TriggerEffect.h"

#include <magic_enum/magic_enum.hpp>

#include <array>
#include <concepts>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>

namespace ByteForge
{
    struct ScriptEnumValue
    {
        std::string_view Name;
        i32 Value = 0;
    };

    struct ScriptEnum
    {
        std::string_view Name;
        std::span<const ScriptEnumValue> Values;

        [[nodiscard]] constexpr const ScriptEnumValue* Find(const i32 value) const
        {
            for (const ScriptEnumValue& entry : Values)
            {
                if (entry.Value == value)
                    return &entry;
            }
            return nullptr;
        }
    };

    template<typename T>
    struct ScriptEnumOf;

    template<typename T>
    concept ScriptEnumType = std::is_enum_v<T> && requires
    {
        { ScriptEnumOf<T>::Value } -> std::convertible_to<const ScriptEnum&>;
    };

    namespace Detail
    {
        template<typename E>
        constexpr auto ReflectScriptEnumValues()
        {
            constexpr auto entries = magic_enum::enum_entries<E>();

            std::array<ScriptEnumValue, entries.size()> values{};
            for (usize i = 0; i < entries.size(); ++i)
                values[i] = { entries[i].second, static_cast<i32>(entries[i].first) };
            return values;
        }

        constexpr auto MakeMouseButtonValues()
        {
            std::array<ScriptEnumValue, MouseButtonNames.size()> values{};
            for (usize i = 0; i < MouseButtonNames.size(); ++i)
                values[i] = { MouseButtonNames[i], static_cast<i32>(i) };
            return values;
        }

        inline constexpr auto KeyCodeValues = ReflectScriptEnumValues<KeyCode>();
        inline constexpr auto MouseButtonValues = MakeMouseButtonValues();
        inline constexpr auto GamepadTypeValues = ReflectScriptEnumValues<GamepadType>();
        inline constexpr auto GamepadButtonValues = ReflectScriptEnumValues<GamepadButton>();
        inline constexpr auto GamepadAxisValues = ReflectScriptEnumValues<GamepadAxis>();
        inline constexpr auto GamepadTriggerValues = ReflectScriptEnumValues<GamepadTrigger>();
    }

    template<>
    struct ScriptEnumOf<KeyCode>
    {
        static constexpr ScriptEnum Value{ "Key", Detail::KeyCodeValues };
    };

    template<>
    struct ScriptEnumOf<MouseButton>
    {
        static constexpr ScriptEnum Value{ "Mouse Button", Detail::MouseButtonValues };
    };

    template<>
    struct ScriptEnumOf<GamepadType>
    {
        static constexpr ScriptEnum Value{ "Gamepad Type", Detail::GamepadTypeValues };
    };

    template<>
    struct ScriptEnumOf<GamepadButton>
    {
        static constexpr ScriptEnum Value{ "Gamepad Button", Detail::GamepadButtonValues };
    };

    template<>
    struct ScriptEnumOf<GamepadAxis>
    {
        static constexpr ScriptEnum Value{ "Gamepad Axis", Detail::GamepadAxisValues };
    };

    template<>
    struct ScriptEnumOf<GamepadTrigger>
    {
        static constexpr ScriptEnum Value{ "Gamepad Trigger", Detail::GamepadTriggerValues };
    };
}

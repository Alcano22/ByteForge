#include "Engine/Input/InputControl.h"

#include <magic_enum/magic_enum.hpp>

#include <algorithm>
#include <array>
#include <format>

template<>
struct magic_enum::customize::enum_range<ByteForge::KeyCode>
{
    static constexpr int min = 0;
    static constexpr int max = 350;
};

namespace ByteForge
{
    namespace
    {
        constexpr std::array<std::string_view, static_cast<usize>(MouseButton::ButtonLast) + 1> MouseButtonNames{
            "Left", "Right", "Middle", "Button3", "Button4", "Button5", "Button6", "Button7"
        };

        template<typename E>
        std::string_view NameOf(const u16 code)
        {
            return magic_enum::enum_name(static_cast<E>(code));
        }

        template<typename E>
        std::optional<u16> CodeOf(const std::string_view name)
        {
            const std::optional<E> value = magic_enum::enum_cast<E>(name);
            return value ? std::optional<u16>(static_cast<u16>(*value)) : std::nullopt;
        }

        std::string_view CodeName(const InputControl& control)
        {
            switch (control.Type)
            {
                case InputControlType::Key:           return NameOf<KeyCode>(control.Code);
                case InputControlType::GamepadButton: return NameOf<GamepadButton>(control.Code);
                case InputControlType::GamepadAxis:   return NameOf<GamepadAxis>(control.Code);
                case InputControlType::GamepadStick:  return NameOf<GamepadStick>(control.Code);
                case InputControlType::MouseButton:   return control.Code < MouseButtonNames.size()
                                                           ? MouseButtonNames[control.Code] : std::string_view{};
                case InputControlType::None:          break;
            }
            return {};
        }

        std::optional<u16> CodeFromName(const InputControlType type, const std::string_view name)
        {
            switch (type)
            {
                case InputControlType::Key:           return CodeOf<KeyCode>(name);
                case InputControlType::GamepadButton: return CodeOf<GamepadButton>(name);
                case InputControlType::GamepadAxis:   return CodeOf<GamepadAxis>(name);
                case InputControlType::GamepadStick:  return CodeOf<GamepadStick>(name);
                case InputControlType::MouseButton:
                {
                    const auto it = std::ranges::find(MouseButtonNames, name);
                    if (it == MouseButtonNames.end())
                        return std::nullopt;
                    return static_cast<u16>(std::distance(MouseButtonNames.begin(), it));
                }
                case InputControlType::None:
                    break;
            }
            return std::nullopt;
        }
    }

    std::string InputControlToString(const InputControl& control)
    {
        const std::string_view name = CodeName(control);
        if (name.empty())
            return {};

        return std::format("{}/{}", magic_enum::enum_name(control.Type), name);
    }

    std::optional<InputControl> InputControlFromString(const std::string_view text)
    {
        if (text.empty())
            return InputControl{};

        const usize separator = text.find('/');
        if (separator == std::string_view::npos)
            return std::nullopt;

        const std::optional<InputControlType> type = magic_enum::enum_cast<InputControlType>(text.substr(0, separator));
        if (!type || *type == InputControlType::None)
            return std::nullopt;

        const std::optional<u16> code = CodeFromName(*type, text.substr(separator + 1));
        if (!code)
            return std::nullopt;

        return InputControl{ *type, *code };
    }
}

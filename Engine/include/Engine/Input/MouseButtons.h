#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace ByteForge
{
    enum class MouseButton : u16
    {
        Button0 = 0, Button1 = 1, Button2 = 2, Button3 = 3,
        Button4 = 4, Button5 = 5, Button6 = 6, Button7 = 7,

        ButtonLast   = Button7,
        ButtonLeft   = Button0,
        ButtonRight  = Button1,
        ButtonMiddle = Button2
    };

    inline constexpr std::array<std::string_view, static_cast<usize>(MouseButton::ButtonLast) + 1> MouseButtonNames{
        "Left", "Right", "Middle", "Button3", "Button4", "Button5", "Button6", "Button7"
    };
}

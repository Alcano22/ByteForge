#pragma once

#include "Engine/Core/Core.h"

namespace ByteForge
{
    enum class HorizontalAlignment : u8 { Left, Center, Right };
    enum class VerticalAlignment : u8 { Top, Middle, Bottom };

    enum class TextOverflow : u8 { Overflow, Clip, Shrink };
}

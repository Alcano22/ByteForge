#pragma once

#include <cstddef>
#include <span>

namespace ByteForge::Embedded
{
    [[nodiscard]] std::span<const std::byte> SplashLogo();
}

#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Input/TriggerEffect.h"

#include <array>

namespace ByteForge
{
    using DualSenseTriggerPacket = std::array<u8, 11>;

    [[nodiscard]] BYTEFORGE_API DualSenseTriggerPacket EncodeDualSenseTriggerEffect(const TriggerEffect& effect);
}

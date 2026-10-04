#pragma once

#include "Engine/Scripting/ScriptField.h"

#include <cstddef>

namespace ByteForge::ManagedValue
{
    inline constexpr size_t SlotSize = 16;

    [[nodiscard]] ScriptValue Read(ScriptFieldType type, const void* slot);
    void Write(const ScriptValue& value, void* slot);
}

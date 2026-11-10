#pragma once

#include "Engine/Scripting/ScriptField.h"

#include <cstddef>

namespace ByteForge::ManagedValue
{
    inline constexpr usize SlotSize = 16;

    [[nodiscard]] ScriptValue Read(ScriptFieldType type, const void* slot);
    void Write(const ScriptValue& value, void* slot);
}

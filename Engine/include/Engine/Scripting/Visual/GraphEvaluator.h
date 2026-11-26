#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Scripting/ScriptField.h"
#include "Engine/Scripting/Visual/ScriptGraph.h"

#include <optional>

namespace ByteForge
{
    [[nodiscard]] BYTEFORGE_API std::optional<ScriptValue> EvaluateConstant(const ScriptGraph& graph,
                                                                            const PinRef& output);

    [[nodiscard]] BYTEFORGE_API PinRef FindValueSource(const ScriptGraph& graph, PinRef output);
}

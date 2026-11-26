#pragma once

#include "Engine/Core/Core.h"
#include "Engine/Scripting/ScriptField.h"
#include "Engine/Scripting/Visual/ScriptGraph.h"

#include <functional>
#include <optional>

namespace ByteForge
{
    using LivePinValue = std::function<std::optional<ScriptValue>(const PinRef&)>;

    [[nodiscard]] BYTEFORGE_API std::optional<ScriptValue> EvaluatePin(const ScriptGraph& graph, const PinRef& output,
                                                                       const LivePinValue& live);

    [[nodiscard]] BYTEFORGE_API std::optional<ScriptValue> EvaluateConstant(const ScriptGraph& graph,
                                                                            const PinRef& output);

    [[nodiscard]] BYTEFORGE_API PinRef FindValueSource(const ScriptGraph& graph, PinRef output);
}

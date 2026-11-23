#include "Scripting/API/StringAPI.h"

#include "Engine/Scripting/API/ScriptAPI.h"

#include <algorithm>
#include <format>
#include <string>

namespace ByteForge
{
    namespace
    {
        constexpr i32 MaxDecimals = 9;

        std::string StringFromInt(const ScriptCallContext&, const i32 value) { return std::to_string(value); }

        std::string StringFromFloat(const ScriptCallContext&, const f32 value, const i32 decimals)
        {
            return std::format("{:.{}f}", value, std::clamp(decimals, 0, MaxDecimals));
        }

        std::string StringAppend(const ScriptCallContext&, const std::string& a, const std::string& b) { return a + b; }
    }

    void RegisterStringAPI(ScriptAPI& api)
    {
        using enum ScriptPurity;

        api.Register<&StringFromInt>("String.FromInt", "Int To Text", Pure, { "Value" });
        api.Register<&StringFromFloat>("String.FromFloat", "Float To Text", Pure, { "Value", "Decimals" });
        api.Register<&StringAppend>("String.Append", "Append Text", Pure, { "A", "B" });
    }
}

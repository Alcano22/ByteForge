#include "Scripting/API/LogicAPI.h"

#include "Engine/Scripting/API/ScriptAPI.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace ByteForge
{
    namespace
    {
        constexpr f32 EqualRelativeTolerance = 1e-6f;
        constexpr f32 EqualAbsoluteTolerance = 1e-6f;

        bool AreNearlyEqual(const f32 a, const f32 b, const f32 tolerance)
        {
            if (!std::isfinite(a) || !std::isfinite(b))
                return a == b;

            return std::abs(a - b) <= tolerance;
        }

        bool LogicAnd(const ScriptCallContext&, const bool a, const bool b) { return a && b; }
        bool LogicOr(const ScriptCallContext&, const bool a, const bool b) { return a || b; }
        bool LogicXor(const ScriptCallContext&, const bool a, const bool b) { return a != b; }
        bool LogicNot(const ScriptCallContext&, const bool value) { return !value; }

        bool LogicEqualFloat(const ScriptCallContext&, const f32 a, const f32 b)
        {
            const f32 tolerance = std::max(EqualAbsoluteTolerance,
                                           EqualRelativeTolerance * std::max(std::abs(a), std::abs(b)));
            return AreNearlyEqual(a, b, tolerance);
        }

        bool LogicNotEqualFloat(const ScriptCallContext& context, const f32 a, const f32 b)
        {
            return !LogicEqualFloat(context, a, b);
        }

        bool LogicNearlyEqualFloat(const ScriptCallContext&, const f32 a, const f32 b, const f32 tolerance)
        {
            if (!(tolerance >= 0.0f))
                throw ScriptError("Logic.NearlyEqualFloat: Tolerance must not be negative, got {}", tolerance);

            return AreNearlyEqual(a, b, tolerance);
        }

        bool LogicGreaterOrEqualFloat(const ScriptCallContext&, const f32 a, const f32 b) { return a >= b; }
        bool LogicLessOrEqualFloat(const ScriptCallContext&, const f32 a, const f32 b) { return a <= b; }

        bool LogicEqualInt(const ScriptCallContext&, const i32 a, const i32 b) { return a == b; }
        bool LogicNotEqualInt(const ScriptCallContext&, const i32 a, const i32 b) { return a != b; }
        bool LogicGreaterInt(const ScriptCallContext&, const i32 a, const i32 b) { return a > b; }
        bool LogicGreaterOrEqualInt(const ScriptCallContext&, const i32 a, const i32 b) { return a >= b; }
        bool LogicLessInt(const ScriptCallContext&, const i32 a, const i32 b) { return a < b; }
        bool LogicLessOrEqualInt(const ScriptCallContext&, const i32 a, const i32 b) { return a <= b; }

        bool LogicEqualString(const ScriptCallContext&, const std::string& a, const std::string& b) { return a == b; }
        bool LogicNotEqualString(const ScriptCallContext&, const std::string& a, const std::string& b) { return a != b; }
    }

    void RegisterLogicAPI(ScriptAPI& api)
    {
        using enum ScriptPurity;

        api.Register<&LogicAnd>("Logic.And", "And", Pure, { "A", "B" });
        api.Register<&LogicOr>("Logic.Or", "Or", Pure, { "A", "B" });
        api.Register<&LogicXor>("Logic.Xor", "Xor", Pure, { "A", "B" });
        api.Register<&LogicNot>("Logic.Not", "Not", Pure, { "Value" });

        api.Register<&LogicEqualFloat>("Logic.EqualFloat", "Equal", Pure, { "A", "B" });
        api.Register<&LogicNotEqualFloat>("Logic.NotEqualFloat", "Not Equal", Pure, { "A", "B" });
        api.Register<&LogicNearlyEqualFloat>("Logic.NearlyEqualFloat", "Nearly Equal", Pure, { "A", "B", "Tolerance" });
        api.Register<&LogicGreaterOrEqualFloat>("Logic.GreaterOrEqualFloat", "Greater Or Equal", Pure, { "A", "B" });
        api.Register<&LogicLessOrEqualFloat>("Logic.LessOrEqualFloat", "Less Or Equal", Pure, { "A", "B" });

        api.Register<&LogicEqualInt>("Logic.EqualInt", "Equal (Int)", Pure, { "A", "B" });
        api.Register<&LogicNotEqualInt>("Logic.NotEqualInt", "Not Equal (Int)", Pure, { "A", "B" });
        api.Register<&LogicGreaterInt>("Logic.GreaterInt", "Greater (Int)", Pure, { "A", "B" });
        api.Register<&LogicGreaterOrEqualInt>("Logic.GreaterOrEqualInt", "Greater Or Equal (Int)", Pure, { "A", "B" });
        api.Register<&LogicLessInt>("Logic.LessInt", "Less (Int)", Pure, { "A", "B" });
        api.Register<&LogicLessOrEqualInt>("Logic.LessOrEqualInt", "Less Or Equal (Int)", Pure, { "A", "B" });

        api.Register<&LogicEqualString>("Logic.EqualString", "Equal (Text)", Pure, { "A", "B" });
        api.Register<&LogicNotEqualString>("Logic.NotEqualString", "Not Equal (Text)", Pure, { "A", "B" });
    }
}

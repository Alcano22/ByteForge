#include "Scripting/API/MathAPI.h"

#include "Engine/Scripting/API/ScriptAPI.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace ByteForge
{
    namespace
    {
        constexpr i32 IntMin = std::numeric_limits<i32>::min();
        constexpr i32 IntMax = std::numeric_limits<i32>::max();

        constexpr f32 MinNormalizeLength = 1e-6f;

        i32 WrapAdd(const i32 a, const i32 b) { return static_cast<i32>(static_cast<u32>(a) + static_cast<u32>(b)); }
        i32 WrapSubtract(const i32 a, const i32 b) { return static_cast<i32>(static_cast<u32>(a) - static_cast<u32>(b)); }
        i32 WrapMultiply(const i32 a, const i32 b) { return static_cast<i32>(static_cast<u32>(a) * static_cast<u32>(b)); }

        void RequireDivisible(const char* function, const i32 a, const i32 b)
        {
            if (b == 0)
                throw ScriptError("{}: division by zero", function);

            if (a == IntMin && b == -1)
                throw ScriptError("{}: {} / -1 does not fit into an Int", function, a);
        }

        template<typename T>
        void RequireRange(const char* function, const T min, const T max)
        {
            if (max < min)
                throw ScriptError("{}: Min ({}) is greater than Max ({})", function, min, max);
        }

        i32 ToInt(const f64 value, const char* function)
        {
            if (!std::isfinite(value) || value < static_cast<f64>(IntMin) || value > static_cast<f64>(IntMax))
                throw ScriptError("{}: {} does not fit into an Int", function, value);

            return static_cast<i32>(value);
        }

        template<typename Vector>
        Vector SafeNormalize(const Vector& vector)
        {
            const f32 length = glm::length(vector);
            return length > MinNormalizeLength ? vector / length : Vector(0.0f);
        }

        f32 MathAddFloat(const ScriptCallContext&, const f32 a, const f32 b) { return a + b; }
        f32 MathSubtractFloat(const ScriptCallContext&, const f32 a, const f32 b) { return a - b; }
        f32 MathMultiplyFloat(const ScriptCallContext&, const f32 a, const f32 b) { return a * b; }
        f32 MathDivideFloat(const ScriptCallContext&, const f32 a, const f32 b) { return a / b; }
        f32 MathModuloFloat(const ScriptCallContext&, const f32 a, const f32 b) { return std::fmod(a, b); }
        bool MathGreaterFloat(const ScriptCallContext&, const f32 a, const f32 b) { return a > b; }
        bool MathLessFloat(const ScriptCallContext&, const f32 a, const f32 b) { return a < b; }

        glm::vec3 MathAddVector3(const ScriptCallContext&, const glm::vec3& a, const glm::vec3& b) { return a + b; }
        glm::vec3 MathScaleVector3(const ScriptCallContext&, const glm::vec3& v, const f32 s) { return v * s; }

        f32 MathMinFloat(const ScriptCallContext&, const f32 a, const f32 b) { return std::min(a, b); }
        f32 MathMaxFloat(const ScriptCallContext&, const f32 a, const f32 b) { return std::max(a, b); }

        f32 MathClampFloat(const ScriptCallContext&, const f32 value, const f32 min, const f32 max)
        {
            RequireRange("Math.ClampFloat", min, max);
            return std::clamp(value, min, max);
        }

        f32 MathLerpFloat(const ScriptCallContext&, const f32 a, const f32 b, const f32 t)
        {
            return std::lerp(a, b, std::clamp(t, 0.0f, 1.0f));
        }

        f32 MathInverseLerpFloat(const ScriptCallContext&, const f32 a, const f32 b, const f32 value)
        {
            if (a == b)
                return 0.0f;

            return std::clamp((value - a) / (b - a), 0.0f, 1.0f);
        }

        f32 MathAbsFloat(const ScriptCallContext&, const f32 value) { return std::abs(value); }

        f32 MathSignFloat(const ScriptCallContext&, const f32 value)
        {
            if (value > 0.0f) return 1.0f;
            if (value < 0.0f) return -1.0f;
            return 0.0f;
        }

        f32 MathFloorFloat(const ScriptCallContext&, const f32 value) { return std::floor(value); }
        f32 MathCeilFloat(const ScriptCallContext&, const f32 value) { return std::ceil(value); }
        f32 MathRoundFloat(const ScriptCallContext&, const f32 value) { return std::round(value); }

        f32 MathSqrtFloat(const ScriptCallContext&, const f32 value) { return std::sqrt(value); }
        f32 MathPowFloat(const ScriptCallContext&, const f32 base, const f32 exponent) { return std::pow(base, exponent); }

        f32 MathSinFloat(const ScriptCallContext&, const f32 radians) { return std::sin(radians); }
        f32 MathCosFloat(const ScriptCallContext&, const f32 radians) { return std::cos(radians); }
        f32 MathTanFloat(const ScriptCallContext&, const f32 radians) { return std::tan(radians); }

        f32 MathAtan2Float(const ScriptCallContext&, const f32 y, const f32 x) { return std::atan2(y, x); }

        f32 MathDegreesToRadians(const ScriptCallContext&, const f32 degrees) { return glm::radians(degrees); }
        f32 MathRadiansToDegrees(const ScriptCallContext&, const f32 radians) { return glm::degrees(radians); }

        f32 MathPi(const ScriptCallContext&) { return glm::pi<f32>(); }

        i32 MathAddInt(const ScriptCallContext&, const i32 a, const i32 b) { return WrapAdd(a, b); }
        i32 MathSubtractInt(const ScriptCallContext&, const i32 a, const i32 b) { return WrapSubtract(a, b); }
        i32 MathMultiplyInt(const ScriptCallContext&, const i32 a, const i32 b) { return WrapMultiply(a, b); }

        i32 MathDivideInt(const ScriptCallContext&, const i32 a, const i32 b)
        {
            RequireDivisible("Math.DivideInt", a, b);
            return a / b;
        }

        i32 MathModuloInt(const ScriptCallContext&, const i32 a, const i32 b)
        {
            RequireDivisible("Math.ModuloInt", a, b);
            return a % b;
        }

        i32 MathMinInt(const ScriptCallContext&, const i32 a, const i32 b) { return std::min(a, b); }
        i32 MathMaxInt(const ScriptCallContext&, const i32 a, const i32 b) { return std::max(a, b); }

        i32 MathClampInt(const ScriptCallContext&, const i32 value, const i32 min, const i32 max)
        {
            RequireRange("Math.ClampInt", min, max);
            return std::clamp(value, min, max);
        }

        i32 MathAbsInt(const ScriptCallContext&, const i32 value)
        {
            if (value == IntMin)
                throw ScriptError("Math.AbsInt: the absolute value of {} does not fit into an Int", value);

            return std::abs(value);
        }

        f32 MathIntToFloat(const ScriptCallContext&, const i32 value) { return static_cast<f32>(value); }

        i32 MathFloorToInt(const ScriptCallContext&, const f32 value)
        {
            return ToInt(std::floor(static_cast<f64>(value)), "Math.FloorToInt");
        }

        i32 MathCeilToInt(const ScriptCallContext&, const f32 value)
        {
            return ToInt(std::ceil(static_cast<f64>(value)), "Math.CeilToInt");
        }

        i32 MathRoundToInt(const ScriptCallContext&, const f32 value)
        {
            return ToInt(std::round(static_cast<f64>(value)), "Math.RoundToInt");
        }

        i32 MathTruncateToInt(const ScriptCallContext&, const f32 value)
        {
            return ToInt(std::trunc(static_cast<f64>(value)), "Math.TruncateToInt");
        }

        glm::vec2 MathMakeVector2(const ScriptCallContext&, const f32 x, const f32 y) { return { x, y }; }
        f32 MathGetXVector2(const ScriptCallContext&, const glm::vec2& v) { return v.x; }
        f32 MathGetYVector2(const ScriptCallContext&, const glm::vec2& v) { return v.y; }

        glm::vec2 MathAddVector2(const ScriptCallContext&, const glm::vec2& a, const glm::vec2& b) { return a + b; }
        glm::vec2 MathSubtractVector2(const ScriptCallContext&, const glm::vec2& a, const glm::vec2& b) { return a - b; }
        glm::vec2 MathScaleVector2(const ScriptCallContext&, const glm::vec2& v, const f32 s) { return v * s; }

        glm::vec2 MathMultiplyVector2(const ScriptCallContext&, const glm::vec2& a, const glm::vec2& b) { return a * b; }

        f32 MathLengthVector2(const ScriptCallContext&, const glm::vec2& v) { return glm::length(v); }
        glm::vec2 MathNormalizeVector2(const ScriptCallContext&, const glm::vec2& v) { return SafeNormalize(v); }
        f32 MathDotVector2(const ScriptCallContext&, const glm::vec2& a, const glm::vec2& b) { return glm::dot(a, b); }
        f32 MathDistanceVector2(const ScriptCallContext&, const glm::vec2& a, const glm::vec2& b) { return glm::distance(a, b); }

        glm::vec2 MathLerpVector2(const ScriptCallContext&, const glm::vec2& a, const glm::vec2& b, const f32 t)
        {
            return glm::mix(a, b, std::clamp(t, 0.0f, 1.0f));
        }

        glm::vec2 MathRotateVector2(const ScriptCallContext&, const glm::vec2& v, const f32 radians)
        {
            const f32 cos = std::cos(radians);
            const f32 sin = std::sin(radians);
            return { v.x * cos - v.y * sin, v.x * sin + v.y * cos };
        }

        f32 MathAngleVector2(const ScriptCallContext&, const glm::vec2& v) { return std::atan2(v.y, v.x); }

        glm::vec2 MathClampLengthVector2(const ScriptCallContext&, const glm::vec2& v, const f32 maxLength)
        {
            if (maxLength < 0.0f)
                throw ScriptError("Math.ClampLengthVector2: Max Length must not be negative, got {}", maxLength);

            const f32 length = glm::length(v);
            return length > maxLength ? v * (maxLength / length) : v;
        }

        glm::vec3 MathMakeVector3(const ScriptCallContext&, const f32 x, const f32 y, const f32 z) { return { x, y, z }; }
        f32 MathGetXVector3(const ScriptCallContext&, const glm::vec3& v) { return v.x; }
        f32 MathGetYVector3(const ScriptCallContext&, const glm::vec3& v) { return v.y; }
        f32 MathGetZVector3(const ScriptCallContext&, const glm::vec3& v) { return v.z; }

        glm::vec3 MathSubtractVector3(const ScriptCallContext&, const glm::vec3& a, const glm::vec3& b) { return a - b; }
        f32 MathLengthVector3(const ScriptCallContext&, const glm::vec3& v) { return glm::length(v); }
        glm::vec3 MathNormalizeVector3(const ScriptCallContext&, const glm::vec3& v) { return SafeNormalize(v); }
        f32 MathDotVector3(const ScriptCallContext&, const glm::vec3& a, const glm::vec3& b) { return glm::dot(a, b); }
        glm::vec3 MathCrossVector3(const ScriptCallContext&, const glm::vec3& a, const glm::vec3& b) { return glm::cross(a, b); }
        f32 MathDistanceVector3(const ScriptCallContext&, const glm::vec3& a, const glm::vec3& b) { return glm::distance(a, b); }

        glm::vec3 MathLerpVector3(const ScriptCallContext&, const glm::vec3& a, const glm::vec3& b, const f32 t)
        {
            return glm::mix(a, b, std::clamp(t, 0.0f, 1.0f));
        }

        glm::vec3 MathVector2ToVector3(const ScriptCallContext&, const glm::vec2& v, const f32 z) { return { v.x, v.y, z }; }
        glm::vec2 MathVector3ToVector2(const ScriptCallContext&, const glm::vec3& v) { return { v.x, v.y }; }
    }

    void RegisterMathAPI(ScriptAPI& api)
    {
        using enum ScriptPurity;

        api.Register<&MathAddFloat>("Math.AddFloat", "Add", Pure, { "A", "B" });
        api.Register<&MathSubtractFloat>("Math.SubtractFloat", "Subtract", Pure, { "A", "B" });
        api.Register<&MathMultiplyFloat>("Math.MultiplyFloat", "Multiply", Pure, { "A", "B" });
        api.Register<&MathDivideFloat>("Math.DivideFloat", "Divide", Pure, { "A", "B" });
        api.Register<&MathGreaterFloat>("Math.GreaterFloat", "Greater", Pure, { "A", "B" });
        api.Register<&MathLessFloat>("Math.LessFloat", "Less", Pure, { "A", "B" });
        api.Register<&MathAddVector3>("Math.AddVector3", "Add (Vector3)", Pure, { "A", "B" });
        api.Register<&MathScaleVector3>("Math.ScaleVector3", "Scale (Vector3)", Pure, { "Vector", "Scale" });

        api.Register<&MathModuloFloat>("Math.ModuloFloat", "Modulo", Pure, { "A", "B" });
        api.Register<&MathMinFloat>("Math.MinFloat", "Min", Pure, { "A", "B" });
        api.Register<&MathMaxFloat>("Math.MaxFloat", "Max", Pure, { "A", "B" });
        api.Register<&MathClampFloat>("Math.ClampFloat", "Clamp", Pure, { "Value", "Min", "Max" });
        api.Register<&MathLerpFloat>("Math.LerpFloat", "Lerp", Pure, { "A", "B", "T" });
        api.Register<&MathInverseLerpFloat>("Math.InverseLerpFloat", "Inverse Lerp", Pure, { "A", "B", "Value" });
        api.Register<&MathAbsFloat>("Math.AbsFloat", "Abs", Pure, { "Value" });
        api.Register<&MathSignFloat>("Math.SignFloat", "Sign", Pure, { "Value" });
        api.Register<&MathFloorFloat>("Math.FloorFloat", "Floor", Pure, { "Value" });
        api.Register<&MathCeilFloat>("Math.CeilFloat", "Ceil", Pure, { "Value" });
        api.Register<&MathRoundFloat>("Math.RoundFloat", "Round", Pure, { "Value" });
        api.Register<&MathSqrtFloat>("Math.SqrtFloat", "Square Root", Pure, { "Value" });
        api.Register<&MathPowFloat>("Math.PowFloat", "Power", Pure, { "Base", "Exponent" });
        api.Register<&MathSinFloat>("Math.SinFloat", "Sin", Pure, { "Radians" });
        api.Register<&MathCosFloat>("Math.CosFloat", "Cos", Pure, { "Radians" });
        api.Register<&MathTanFloat>("Math.TanFloat", "Tan", Pure, { "Radians" });
        api.Register<&MathAtan2Float>("Math.Atan2Float", "Atan2", Pure, { "Y", "X" });
        api.Register<&MathDegreesToRadians>("Math.DegreesToRadians", "Degrees To Radians", Pure, { "Degrees" });
        api.Register<&MathRadiansToDegrees>("Math.RadiansToDegrees", "Radians To Degrees", Pure, { "Radians" });
        api.Register<&MathPi>("Math.Pi", "Pi", Pure);

        api.Register<&MathAddInt>("Math.AddInt", "Add (Int)", Pure, { "A", "B" });
        api.Register<&MathSubtractInt>("Math.SubtractInt", "Subtract (Int)", Pure, { "A", "B" });
        api.Register<&MathMultiplyInt>("Math.MultiplyInt", "Multiply (Int)", Pure, { "A", "B" });
        api.Register<&MathDivideInt>("Math.DivideInt", "Divide (Int)", Pure, { "A", "B" });
        api.Register<&MathModuloInt>("Math.ModuloInt", "Modulo (Int)", Pure, { "A", "B" });
        api.Register<&MathMinInt>("Math.MinInt", "Min (Int)", Pure, { "A", "B" });
        api.Register<&MathMaxInt>("Math.MaxInt", "Max (Int)", Pure, { "A", "B" });
        api.Register<&MathClampInt>("Math.ClampInt", "Clamp (Int)", Pure, { "Value", "Min", "Max" });
        api.Register<&MathAbsInt>("Math.AbsInt", "Abs (Int)", Pure, { "Value" });

        api.Register<&MathIntToFloat>("Math.IntToFloat", "Int To Float", Pure, { "Value" });
        api.Register<&MathFloorToInt>("Math.FloorToInt", "Floor To Int", Pure, { "Value" });
        api.Register<&MathCeilToInt>("Math.CeilToInt", "Ceil To Int", Pure, { "Value" });
        api.Register<&MathRoundToInt>("Math.RoundToInt", "Round To Int", Pure, { "Value" });
        api.Register<&MathTruncateToInt>("Math.TruncateToInt", "Truncate To Int", Pure, { "Value" });

        api.Register<&MathMakeVector2>("Math.MakeVector2", "Make Vector2", Pure, { "X", "Y" });
        api.Register<&MathGetXVector2>("Math.GetXVector2", "Get X (Vector2)", Pure, { "Vector" });
        api.Register<&MathGetYVector2>("Math.GetYVector2", "Get Y (Vector2)", Pure, { "Vector" });
        api.Register<&MathAddVector2>("Math.AddVector2", "Add (Vector2)", Pure, { "A", "B" });
        api.Register<&MathSubtractVector2>("Math.SubtractVector2", "Subtract (Vector2)", Pure, { "A", "B" });
        api.Register<&MathScaleVector2>("Math.ScaleVector2", "Scale (Vector2)", Pure, { "Vector", "Scale" });
        api.Register<&MathMultiplyVector2>("Math.MultiplyVector2", "Multiply (Vector2)", Pure, { "A", "B" });
        api.Register<&MathLengthVector2>("Math.LengthVector2", "Length (Vector2)", Pure, { "Vector" });
        api.Register<&MathNormalizeVector2>("Math.NormalizeVector2", "Normalize (Vector2)", Pure, { "Vector" });
        api.Register<&MathDotVector2>("Math.DotVector2", "Dot (Vector2)", Pure, { "A", "B" });
        api.Register<&MathDistanceVector2>("Math.DistanceVector2", "Distance (Vector2)", Pure, { "A", "B" });
        api.Register<&MathLerpVector2>("Math.LerpVector2", "Lerp (Vector2)", Pure, { "A", "B", "T" });
        api.Register<&MathRotateVector2>("Math.RotateVector2", "Rotate (Vector2)", Pure, { "Vector", "Radians" });
        api.Register<&MathAngleVector2>("Math.AngleVector2", "Angle (Vector2)", Pure, { "Vector" });
        api.Register<&MathClampLengthVector2>("Math.ClampLengthVector2", "Clamp Length (Vector2)", Pure, { "Vector", "Max Length" });

        api.Register<&MathMakeVector3>("Math.MakeVector3", "Make Vector3", Pure, { "X", "Y", "Z" });
        api.Register<&MathGetXVector3>("Math.GetXVector3", "Get X (Vector3)", Pure, { "Vector" });
        api.Register<&MathGetYVector3>("Math.GetYVector3", "Get Y (Vector3)", Pure, { "Vector" });
        api.Register<&MathGetZVector3>("Math.GetZVector3", "Get Z (Vector3)", Pure, { "Vector" });
        api.Register<&MathSubtractVector3>("Math.SubtractVector3", "Subtract (Vector3)", Pure, { "A", "B" });
        api.Register<&MathLengthVector3>("Math.LengthVector3", "Length (Vector3)", Pure, { "Vector" });
        api.Register<&MathNormalizeVector3>("Math.NormalizeVector3", "Normalize (Vector3)", Pure, { "Vector" });
        api.Register<&MathDotVector3>("Math.DotVector3", "Dot (Vector3)", Pure, { "A", "B" });
        api.Register<&MathCrossVector3>("Math.CrossVector3", "Cross (Vector3)", Pure, { "A", "B" });
        api.Register<&MathDistanceVector3>("Math.DistanceVector3", "Distance (Vector3)", Pure, { "A", "B" });
        api.Register<&MathLerpVector3>("Math.LerpVector3", "Lerp (Vector3)", Pure, { "A", "B", "T" });

        api.Register<&MathVector2ToVector3>("Math.Vector2ToVector3", "Vector2 To Vector3", Pure, { "Vector", "Z" });
        api.Register<&MathVector3ToVector2>("Math.Vector3ToVector2", "Vector3 To Vector2", Pure, { "Vector" });
    }
}

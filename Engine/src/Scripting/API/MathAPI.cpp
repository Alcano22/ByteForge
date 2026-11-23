#include "Scripting/API/MathAPI.h"

#include "Engine/Scripting/API/ScriptAPI.h"

#include <glm/glm.hpp>

namespace ByteForge
{
    namespace
    {
        f32 MathAddFloat(const ScriptCallContext&, const f32 a, const f32 b) { return a + b; }
        f32 MathSubtractFloat(const ScriptCallContext&, const f32 a, const f32 b) { return a - b; }
        f32 MathMultiplyFloat(const ScriptCallContext&, const f32 a, const f32 b) { return a * b; }
        f32 MathDivideFloat(const ScriptCallContext&, const f32 a, const f32 b) { return a / b; }
        bool MathGreaterFloat(const ScriptCallContext&, const f32 a, const f32 b) { return a > b; }
        bool MathLessFloat(const ScriptCallContext&, const f32 a, const f32 b) { return a < b; }

        glm::vec3 MathAddVector3(const ScriptCallContext&, const glm::vec3& a, const glm::vec3& b) { return a + b; }
        glm::vec3 MathScaleVector3(const ScriptCallContext&, const glm::vec3& v, const f32 s) { return v * s; }
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
    }
}

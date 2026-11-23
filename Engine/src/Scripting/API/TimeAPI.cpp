#include "Scripting/API/TimeAPI.h"

#include "Engine/Scene/Scene.h"
#include "Engine/Scene/SceneTime.h"
#include "Engine/Scripting/API/ScriptAPI.h"

#include <algorithm>
#include <limits>

namespace ByteForge
{
    namespace
    {
        const SceneTime& RequireTime(const ScriptCallContext& context)
        {
            if (context.ActiveScene == nullptr)
                throw ScriptError("No scene is running");

            return context.ActiveScene->GetTime();
        }

        f32 TimeDeltaTime(const ScriptCallContext& context) { return RequireTime(context).GetDeltaTime(); }

        f32 TimeElapsed(const ScriptCallContext& context)
        {
            return static_cast<f32>(RequireTime(context).GetElapsed());
        }

        i32 TimeFrameCount(const ScriptCallContext& context)
        {
            constexpr u64 Max = static_cast<u64>(std::numeric_limits<i32>::max());
            return static_cast<i32>(std::min(RequireTime(context).GetFrameCount(), Max));
        }
    }

    void RegisterTimeAPI(ScriptAPI& api)
    {
        using enum ScriptPurity;

        api.Register<&TimeDeltaTime>("Time.DeltaTime", "Delta Time", Pure);
        api.Register<&TimeElapsed>("Time.Elapsed", "Time Since Start", Pure);
        api.Register<&TimeFrameCount>("Time.FrameCount", "Frame Count", Pure);
    }
}

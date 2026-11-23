#include "Scripting/ScriptTestHelpers.h"

#include <Engine/Scene/Scene.h>
#include <Engine/Scripting/API/ScriptAPI.h>

#include <catch2/catch_test_macros.hpp>

#include <limits>

using namespace ByteForge;
using namespace ByteForge::Tests;

TEST_CASE("Scene time starts at zero", "[scripting][time]")
{
    Scene scene;
    const ScriptCallContext context{ .ActiveScene = &scene };

    CHECK(CallScriptAs<f32>(context, "Time.DeltaTime", {}) == 0.0f);
    CHECK(CallScriptAs<f32>(context, "Time.Elapsed", {}) == 0.0f);
    CHECK(CallScriptAs<i32>(context, "Time.FrameCount", {}) == 0);
}

TEST_CASE("Scene time advances once per frame", "[scripting][time]")
{
    Scene scene;
    const ScriptCallContext context{ .ActiveScene = &scene };

    scene.GetTime().Advance(Timestep(0.25f));
    scene.GetTime().Advance(Timestep(0.5f));

    CHECK(CallScriptAs<f32>(context, "Time.DeltaTime", {}) == 0.5f);
    CHECK(CallScriptAs<f32>(context, "Time.Elapsed", {}) == 0.75f);
    CHECK(CallScriptAs<i32>(context, "Time.FrameCount", {}) == 2);
}

TEST_CASE("Negative and NaN frame times count as zero", "[scripting][time]")
{
    SceneTime time;
    time.Advance(Timestep(1.0f));
    time.Advance(Timestep(-1.0f));
    time.Advance(Timestep(std::numeric_limits<f32>::quiet_NaN()));

    CHECK(time.GetDeltaTime() == 0.0f);
    CHECK(time.GetElapsed() == 1.0);
    CHECK(time.GetFrameCount() == 3);
}

TEST_CASE("Resetting the time starts over", "[scripting][time]")
{
    SceneTime time;
    time.Advance(Timestep(1.0f));
    time.Reset();

    CHECK(time.GetDeltaTime() == 0.0f);
    CHECK(time.GetElapsed() == 0.0);
    CHECK(time.GetFrameCount() == 0);
}

TEST_CASE("Summing many short frames stays exact", "[scripting][time]")
{
    SceneTime time;
    for (i32 i = 0; i < 36000; ++i)
        time.Advance(Timestep(0.1f));

    CHECK(time.GetElapsed() > 3599.99);
    CHECK(time.GetElapsed() < 3600.01);
}

TEST_CASE("Time needs a scene", "[scripting][time]")
{
    CHECK_THROWS_AS(CallPure<f32>("Time.DeltaTime", {}), ScriptError);
}

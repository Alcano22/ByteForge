#include "Scripting/ScriptTestHelpers.h"

#include <Engine/Scripting/API/ScriptAPI.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string>
#include <string_view>
#include <vector>

using namespace ByteForge;
using namespace ByteForge::Tests;

namespace
{
    struct PinnedFunction
    {
        std::string_view Id;
        std::string_view Signature;
        std::vector<std::string_view> Pins;
    };

    const std::array<PinnedFunction, 11> PinnedFunctions{ {
        { "Math.AddFloat",      "(Float,Float)->Float",     { "A", "B" } },
        { "Math.SubtractFloat", "(Float,Float)->Float",     { "A", "B" } },
        { "Math.MultiplyFloat", "(Float,Float)->Float",     { "A", "B" } },
        { "Math.DivideFloat",   "(Float,Float)->Float",     { "A", "B" } },
        { "Math.GreaterFloat",  "(Float,Float)->Bool",      { "A", "B" } },
        { "Math.LessFloat",     "(Float,Float)->Bool",      { "A", "B" } },
        { "Math.AddVector3",    "(Vector3,Vector3)->Vector3", { "A", "B" } },
        { "Math.ScaleVector3",  "(Vector3,Float)->Vector3", { "Vector", "Scale" } },
        { "String.FromInt",     "(Int)->String",            { "Value" } },
        { "String.FromFloat",   "(Float,Int)->String",      { "Value", "Decimals" } },
        { "String.Append",      "(String,String)->String",  { "A", "B" } }
    } };
}

TEST_CASE("Functions used by saved graphs keep their ids, signatures and pins", "[scripting][api]")
{
    for (const PinnedFunction& pinned : PinnedFunctions)
    {
        INFO(pinned.Id);

        const ScriptFunction* function = ScriptAPI::Get().Find(pinned.Id);
        REQUIRE(function != nullptr);
        CHECK(GetSignature(*function) == pinned.Signature);

        REQUIRE(function->Parameters.size() == pinned.Pins.size());
        for (usize i = 0; i < pinned.Pins.size(); ++i)
            CHECK(function->Parameters[i].Name == pinned.Pins[i]);
    }
}

TEST_CASE("The moved functions still compute the same results", "[scripting][api]")
{
    CHECK(CallPure<f32>("Math.AddFloat", { 1.5f, 2.0f }) == 3.5f);
    CHECK(CallPure<bool>("Math.GreaterFloat", { 2.0f, 1.0f }));
    CHECK(CallPure<glm::vec3>("Math.ScaleVector3", { glm::vec3(1.0f, 2.0f, 3.0f), 2.0f }) == glm::vec3(2.0f, 4.0f, 6.0f));

    CHECK(CallPure<std::string>("String.FromInt", { 42 }) == "42");
    CHECK(CallPure<std::string>("String.Append", { Text("Byte"), Text("Forge") }) == "ByteForge");
}

TEST_CASE("Float to text clamps the number of decimals", "[scripting][api]")
{
    CHECK(CallPure<std::string>("String.FromFloat", { 3.0f, -5 }) == "3");
    CHECK(CallPure<std::string>("String.FromFloat", { 3.0f, 2 }) == "3.00");
    CHECK(CallPure<std::string>("String.FromFloat", { 3.0f, 50 }) == "3.000000000");
}

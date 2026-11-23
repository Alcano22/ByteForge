#include "Scripting/ScriptTestHelpers.h"

#include <Engine/Scripting/API/ScriptAPI.h>

#include <catch2/catch_test_macros.hpp>

#include <glm/glm.hpp>

#include <array>
#include <cmath>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using namespace ByteForge;
using namespace ByteForge::Tests;

namespace
{
    struct FunctionContract
    {
        std::string_view Id;
        std::string_view Signature;
        std::vector<std::string_view> Pins;
        bool Pure = true;
    };

    const std::vector<FunctionContract>& ContractsV040()
    {
        static const std::vector<FunctionContract> contracts{
            { "Math.AddFloat",      "(Float,Float)->Float",       { "A", "B" } },
            { "Math.SubtractFloat", "(Float,Float)->Float",       { "A", "B" } },
            { "Math.MultiplyFloat", "(Float,Float)->Float",       { "A", "B" } },
            { "Math.DivideFloat",   "(Float,Float)->Float",       { "A", "B" } },
            { "Math.GreaterFloat",  "(Float,Float)->Bool",        { "A", "B" } },
            { "Math.LessFloat",     "(Float,Float)->Bool",        { "A", "B" } },
            { "Math.AddVector3",    "(Vector3,Vector3)->Vector3", { "A", "B" } },
            { "Math.ScaleVector3",  "(Vector3,Float)->Vector3",   { "Vector", "Scale" } },
            { "String.FromInt",     "(Int)->String",              { "Value" } },
            { "String.FromFloat",   "(Float,Int)->String",        { "Value", "Decimals" } },
            { "String.Append",      "(String,String)->String",    { "A", "B" } },
        };
        return contracts;
    }

    const std::vector<FunctionContract>& ContractsV050()
    {
        static const std::vector<FunctionContract> contracts{
            { "Math.ModuloFloat",       "(Float,Float)->Float",       { "A", "B" } },
            { "Math.MinFloat",          "(Float,Float)->Float",       { "A", "B" } },
            { "Math.MaxFloat",          "(Float,Float)->Float",       { "A", "B" } },
            { "Math.ClampFloat",        "(Float,Float,Float)->Float", { "Value", "Min", "Max" } },
            { "Math.LerpFloat",         "(Float,Float,Float)->Float", { "A", "B", "T" } },
            { "Math.InverseLerpFloat",  "(Float,Float,Float)->Float", { "A", "B", "Value" } },
            { "Math.AbsFloat",          "(Float)->Float",             { "Value" } },
            { "Math.SignFloat",         "(Float)->Float",             { "Value" } },
            { "Math.FloorFloat",        "(Float)->Float",             { "Value" } },
            { "Math.CeilFloat",         "(Float)->Float",             { "Value" } },
            { "Math.RoundFloat",        "(Float)->Float",             { "Value" } },
            { "Math.SqrtFloat",         "(Float)->Float",             { "Value" } },
            { "Math.PowFloat",          "(Float,Float)->Float",       { "Base", "Exponent" } },
            { "Math.SinFloat",          "(Float)->Float",             { "Radians" } },
            { "Math.CosFloat",          "(Float)->Float",             { "Radians" } },
            { "Math.TanFloat",          "(Float)->Float",             { "Radians" } },
            { "Math.Atan2Float",        "(Float,Float)->Float",       { "Y", "X" } },
            { "Math.DegreesToRadians",  "(Float)->Float",             { "Degrees" } },
            { "Math.RadiansToDegrees",  "(Float)->Float",             { "Radians" } },
            { "Math.Pi",                "()->Float",                  {} },

            { "Math.AddInt",      "(Int,Int)->Int",     { "A", "B" } },
            { "Math.SubtractInt", "(Int,Int)->Int",     { "A", "B" } },
            { "Math.MultiplyInt", "(Int,Int)->Int",     { "A", "B" } },
            { "Math.DivideInt",   "(Int,Int)->Int",     { "A", "B" } },
            { "Math.ModuloInt",   "(Int,Int)->Int",     { "A", "B" } },
            { "Math.MinInt",      "(Int,Int)->Int",     { "A", "B" } },
            { "Math.MaxInt",      "(Int,Int)->Int",     { "A", "B" } },
            { "Math.ClampInt",    "(Int,Int,Int)->Int", { "Value", "Min", "Max" } },
            { "Math.AbsInt",      "(Int)->Int",         { "Value" } },

            { "Math.IntToFloat",    "(Int)->Float", { "Value" } },
            { "Math.FloorToInt",    "(Float)->Int", { "Value" } },
            { "Math.CeilToInt",     "(Float)->Int", { "Value" } },
            { "Math.RoundToInt",    "(Float)->Int", { "Value" } },
            { "Math.TruncateToInt", "(Float)->Int", { "Value" } },

            { "Math.MakeVector2",        "(Float,Float)->Vector2",           { "X", "Y" } },
            { "Math.GetXVector2",        "(Vector2)->Float",                 { "Vector" } },
            { "Math.GetYVector2",        "(Vector2)->Float",                 { "Vector" } },
            { "Math.AddVector2",         "(Vector2,Vector2)->Vector2",       { "A", "B" } },
            { "Math.SubtractVector2",    "(Vector2,Vector2)->Vector2",       { "A", "B" } },
            { "Math.ScaleVector2",       "(Vector2,Float)->Vector2",         { "Vector", "Scale" } },
            { "Math.MultiplyVector2",    "(Vector2,Vector2)->Vector2",       { "A", "B" } },
            { "Math.LengthVector2",      "(Vector2)->Float",                 { "Vector" } },
            { "Math.NormalizeVector2",   "(Vector2)->Vector2",               { "Vector" } },
            { "Math.DotVector2",         "(Vector2,Vector2)->Float",         { "A", "B" } },
            { "Math.DistanceVector2",    "(Vector2,Vector2)->Float",         { "A", "B" } },
            { "Math.LerpVector2",        "(Vector2,Vector2,Float)->Vector2", { "A", "B", "T" } },
            { "Math.RotateVector2",      "(Vector2,Float)->Vector2",         { "Vector", "Radians" } },
            { "Math.AngleVector2",       "(Vector2)->Float",                 { "Vector" } },
            { "Math.ClampLengthVector2", "(Vector2,Float)->Vector2",         { "Vector", "Max Length" } },

            { "Math.MakeVector3",      "(Float,Float,Float)->Vector3",     { "X", "Y", "Z" } },
            { "Math.GetXVector3",      "(Vector3)->Float",                 { "Vector" } },
            { "Math.GetYVector3",      "(Vector3)->Float",                 { "Vector" } },
            { "Math.GetZVector3",      "(Vector3)->Float",                 { "Vector" } },
            { "Math.SubtractVector3",  "(Vector3,Vector3)->Vector3",       { "A", "B" } },
            { "Math.LengthVector3",    "(Vector3)->Float",                 { "Vector" } },
            { "Math.NormalizeVector3", "(Vector3)->Vector3",               { "Vector" } },
            { "Math.DotVector3",       "(Vector3,Vector3)->Float",         { "A", "B" } },
            { "Math.CrossVector3",     "(Vector3,Vector3)->Vector3",       { "A", "B" } },
            { "Math.DistanceVector3",  "(Vector3,Vector3)->Float",         { "A", "B" } },
            { "Math.LerpVector3",      "(Vector3,Vector3,Float)->Vector3", { "A", "B", "T" } },

            { "Math.Vector2ToVector3", "(Vector2,Float)->Vector3", { "Vector", "Z" } },
            { "Math.Vector3ToVector2", "(Vector3)->Vector2",       { "Vector" } },

            { "Logic.And",                 "(Bool,Bool)->Bool",         { "A", "B" } },
            { "Logic.Or",                  "(Bool,Bool)->Bool",         { "A", "B" } },
            { "Logic.Xor",                 "(Bool,Bool)->Bool",         { "A", "B" } },
            { "Logic.Not",                 "(Bool)->Bool",              { "Value" } },
            { "Logic.EqualFloat",          "(Float,Float)->Bool",       { "A", "B" } },
            { "Logic.NotEqualFloat",       "(Float,Float)->Bool",       { "A", "B" } },
            { "Logic.NearlyEqualFloat",    "(Float,Float,Float)->Bool", { "A", "B", "Tolerance" } },
            { "Logic.GreaterOrEqualFloat", "(Float,Float)->Bool",       { "A", "B" } },
            { "Logic.LessOrEqualFloat",    "(Float,Float)->Bool",       { "A", "B" } },
            { "Logic.EqualInt",            "(Int,Int)->Bool",           { "A", "B" } },
            { "Logic.NotEqualInt",         "(Int,Int)->Bool",           { "A", "B" } },
            { "Logic.GreaterInt",          "(Int,Int)->Bool",           { "A", "B" } },
            { "Logic.GreaterOrEqualInt",   "(Int,Int)->Bool",           { "A", "B" } },
            { "Logic.LessInt",             "(Int,Int)->Bool",           { "A", "B" } },
            { "Logic.LessOrEqualInt",      "(Int,Int)->Bool",           { "A", "B" } },
            { "Logic.EqualString",         "(String,String)->Bool",     { "A", "B" } },
            { "Logic.NotEqualString",      "(String,String)->Bool",     { "A", "B" } },

            { "String.FromBool",    "(Bool)->String",                 { "Value" } },
            { "String.FromVector2", "(Vector2,Int)->String",          { "Vector", "Decimals" } },
            { "String.FromVector3", "(Vector3,Int)->String",          { "Vector", "Decimals" } },
            { "String.Length",      "(String)->Int",                  { "Text" } },
            { "String.Contains",    "(String,String)->Bool",          { "Text", "Search" } },
            { "String.StartsWith",  "(String,String)->Bool",          { "Text", "Prefix" } },
            { "String.EndsWith",    "(String,String)->Bool",          { "Text", "Suffix" } },
            { "String.ToUpper",     "(String)->String",               { "Text" } },
            { "String.ToLower",     "(String)->String",               { "Text" } },
            { "String.Trim",        "(String)->String",               { "Text" } },
            { "String.Replace",     "(String,String,String)->String", { "Text", "From", "To" } },
            { "String.Substring",   "(String,Int,Int)->String",       { "Text", "Start", "Length" } },
            { "String.ParseInt",    "(String,Int)->Int",              { "Text", "Fallback" } },
            { "String.ParseFloat",  "(String,Float)->Float",          { "Text", "Fallback" } },

            { "Time.DeltaTime",  "()->Float", {} },
            { "Time.Elapsed",    "()->Float", {} },
            { "Time.FrameCount", "()->Int",   {} },

            { "Random.Float",         "()->Float",           {},                         false },
            { "Random.FloatInRange",  "(Float,Float)->Float", { "Min", "Max" },          false },
            { "Random.IntInRange",    "(Int,Int)->Int",       { "Min", "Max Exclusive" }, false },
            { "Random.Chance",        "(Float)->Bool",        { "Probability" },         false },
            { "Random.Direction2D",   "()->Vector2",          {},                         false },
            { "Random.PointInCircle", "(Float)->Vector2",     { "Radius" },              false },
            { "Random.SetSeed",       "(Int)->Void",          { "Seed" },                false },
        };
        return contracts;
    }

    constexpr std::array<std::string_view, 5> PinnedCategories{ "Math", "Logic", "String", "Time", "Random" };

    std::vector<std::string_view> GetPinNames(const ScriptFunction& function)
    {
        std::vector<std::string_view> names;
        names.reserve(function.Parameters.size());
        for (const ScriptParameter& parameter : function.Parameters)
            names.emplace_back(parameter.Name);
        return names;
    }

    void CheckContracts(const std::span<const FunctionContract> contracts)
    {
        for (const FunctionContract& contract : contracts)
        {
            INFO("Function: " << contract.Id);

            const ScriptFunction* function = ScriptAPI::Get().Find(contract.Id);
            REQUIRE(function != nullptr);

            CHECK(GetSignature(*function) == contract.Signature);
            CHECK(GetPinNames(*function) == contract.Pins);
            CHECK(function->Pure == contract.Pure);
        }
    }
}

TEST_CASE("Functions released in v0.4.0 keep their contract", "[scripting][compatibility]")
{
    CheckContracts(ContractsV040());
}

TEST_CASE("Functions released in v0.5.0 keep their contract", "[scripting][compatibility]")
{
    CheckContracts(ContractsV050());
}

TEST_CASE("Every function in a pinned category has a contract", "[scripting][compatibility]")
{
    std::set<std::string_view> pinned;
    for (const FunctionContract& contract : ContractsV040())
        pinned.insert(contract.Id);
    for (const FunctionContract& contract : ContractsV050())
        pinned.insert(contract.Id);

    for (const ScriptFunction& function : ScriptAPI::Get().GetFunctions())
    {
        if (std::ranges::find(PinnedCategories, function.Category) == PinnedCategories.end())
            continue;

        INFO("Function without a contract: " << function.Id);
        CHECK(pinned.contains(function.Id));
    }
}

TEST_CASE("Every contract is listed once", "[scripting][compatibility]")
{
    std::set<std::string_view> seen;
    for (const auto* contracts : { &ContractsV040(), &ContractsV050() })
    {
        for (const FunctionContract& contract : *contracts)
        {
            INFO("Duplicate contract: " << contract.Id);
            CHECK(seen.insert(contract.Id).second);
        }
    }
}

TEST_CASE("The original Math functions still compute the same results", "[scripting][compatibility]")
{
    CHECK(CallPure<f32>("Math.AddFloat", { 2.0f, 3.0f }) == 5.0f);
    CHECK(CallPure<f32>("Math.SubtractFloat", { 2.0f, 3.0f }) == -1.0f);
    CHECK(CallPure<f32>("Math.MultiplyFloat", { 2.0f, 3.0f }) == 6.0f);
    CHECK(CallPure<f32>("Math.DivideFloat", { 3.0f, 2.0f }) == 1.5f);
    CHECK(std::isinf(CallPure<f32>("Math.DivideFloat", { 1.0f, 0.0f })));

    CHECK(CallPure<bool>("Math.GreaterFloat", { 3.0f, 2.0f }));
    CHECK_FALSE(CallPure<bool>("Math.GreaterFloat", { 2.0f, 2.0f }));
    CHECK(CallPure<bool>("Math.LessFloat", { 2.0f, 3.0f }));
    CHECK_FALSE(CallPure<bool>("Math.LessFloat", { 2.0f, 2.0f }));

    CHECK(CallPure<glm::vec3>("Math.AddVector3", { glm::vec3(1.0f, 2.0f, 3.0f), glm::vec3(1.0f) })
          == glm::vec3(2.0f, 3.0f, 4.0f));
    CHECK(CallPure<glm::vec3>("Math.ScaleVector3", { glm::vec3(1.0f, 2.0f, 3.0f), 2.0f })
          == glm::vec3(2.0f, 4.0f, 6.0f));
}

TEST_CASE("The original String functions still compute the same results", "[scripting][compatibility]")
{
    CHECK(CallPure<std::string>("String.FromInt", { -42 }) == "-42");
    CHECK(CallPure<std::string>("String.Append", { Text("Score: "), Text("10") }) == "Score: 10");

    CHECK(CallPure<std::string>("String.FromFloat", { 3.0f, -1 }) == "3");
    CHECK(CallPure<std::string>("String.FromFloat", { 3.0f, 2 }) == "3.00");
    CHECK(CallPure<std::string>("String.FromFloat", { 3.0f, 42 }) == "3.000000000");
}

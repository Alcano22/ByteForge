#include "Scripting/ScriptTestHelpers.h"
#include "TestUtilities.h"

#include <Engine/Scripting/API/ScriptAPI.h>

#include <catch2/catch_test_macros.hpp>

#include <glm/glm.hpp>

#include <limits>
#include <string>
#include <string_view>

using namespace ByteForge;
using namespace ByteForge::Tests;

namespace
{
    ScriptValue Utf8(const std::u8string_view text) { return Tests::Text(Tests::Text(text)); }

    std::string String(const char* id, const std::vector<ScriptValue>& arguments) { return CallPure<std::string>(id, arguments); }
    bool Bool(const char* id, const std::vector<ScriptValue>& arguments) { return CallPure<bool>(id, arguments); }
    i32 Int(const char* id, const std::vector<ScriptValue>& arguments) { return CallPure<i32>(id, arguments); }
    f32 Float(const char* id, const std::vector<ScriptValue>& arguments) { return CallPure<f32>(id, arguments); }
}

TEST_CASE("Bools and vectors convert to text", "[scripting][string]")
{
    CHECK(String("String.FromBool", { true }) == "True");
    CHECK(String("String.FromBool", { false }) == "False");

    CHECK(String("String.FromVector2", { glm::vec2(1.0f, 2.5f), 2 }) == "(1.00, 2.50)");
    CHECK(String("String.FromVector3", { glm::vec3(1.0f, 2.0f, 3.0f), 0 }) == "(1, 2, 3)");
    CHECK(String("String.FromVector2", { glm::vec2(1.5f, -1.5f), -5 }) == "(2, -2)");
}

TEST_CASE("Length counts characters, not bytes", "[scripting][string]")
{
    CHECK(Int("String.Length", { Text("Hello") }) == 5);
    CHECK(Int("String.Length", { Text("") }) == 0);
    CHECK(Int("String.Length", { Utf8(u8"Gr\u00F6\u00DFe") }) == 5);
    CHECK(Int("String.Length", { Utf8(u8"\U0001F3AE") }) == 1);
}

TEST_CASE("Contains, starts with and ends with", "[scripting][string]")
{
    CHECK(Bool("String.Contains", { Text("Hello World"), Text("o W") }));
    CHECK_FALSE(Bool("String.Contains", { Text("Hello World"), Text("world") }));
    CHECK(Bool("String.Contains", { Text("Hello"), Text("") }));
    CHECK(Bool("String.Contains", { Utf8(u8"Gr\u00F6\u00DFe"), Utf8(u8"\u00F6\u00DF") }));

    CHECK(Bool("String.StartsWith", { Text("Player_01"), Text("Player") }));
    CHECK_FALSE(Bool("String.StartsWith", { Text("Player_01"), Text("01") }));

    CHECK(Bool("String.EndsWith", { Text("Player_01"), Text("01") }));
    CHECK_FALSE(Bool("String.EndsWith", { Text("01"), Text("Player_01") }));
}

TEST_CASE("Upper and lower case cover ASCII and Latin-1", "[scripting][string]")
{
    CHECK(String("String.ToUpper", { Utf8(u8"Hello, J\u00F6rg!") }) == Tests::Text(u8"HELLO, J\u00D6RG!"));
    CHECK(String("String.ToLower", { Utf8(u8"\u00C4\u00D6\u00DC ABC") }) == Tests::Text(u8"\u00E4\u00F6\u00FC abc"));

    CHECK(String("String.ToUpper", { Utf8(u8"Gr\u00F6\u00DFe") }) == Tests::Text(u8"GR\u00D6\u00DFE"));

    CHECK(String("String.ToUpper", { Utf8(u8"\u00FF") }) == Tests::Text(u8"\u0178"));
    CHECK(String("String.ToLower", { Utf8(u8"\u0178") }) == Tests::Text(u8"\u00FF"));

    CHECK(String("String.ToUpper", { Utf8(u8"\u00F7") }) == Tests::Text(u8"\u00F7"));
    CHECK(String("String.ToLower", { Utf8(u8"\u00D7") }) == Tests::Text(u8"\u00D7"));

    CHECK(String("String.ToUpper", { Utf8(u8"\u03A9mega") }) == Tests::Text(u8"\u03A9MEGA"));
}

TEST_CASE("Trim removes whitespace and line breaks at both ends", "[scripting][string]")
{
    CHECK(String("String.Trim", { Text("  \t Hi there \r\n") }) == "Hi there");
    CHECK(String("String.Trim", { Text(" \n ") }).empty());
    CHECK(String("String.Trim", { Text("Hi") }) == "Hi");
}

TEST_CASE("Replace replaces every occurrence", "[scripting][string]")
{
    CHECK(String("String.Replace", { Text("a-b-c"), Text("-"), Text("+") }) == "a+b+c");
    CHECK(String("String.Replace", { Text("aaa"), Text("a"), Text("aa") }) == "aaaaaa");
    CHECK(String("String.Replace", { Text("Hello"), Text("l"), Text("") }) == "Heo");
    CHECK(String("String.Replace", { Text("Hello"), Text("x"), Text("y") }) == "Hello");

    CHECK_THROWS_AS(CallPure<std::string>("String.Replace", { Text("Hello"), Text(""), Text("x") }), ScriptError);
}

TEST_CASE("Substring counts characters", "[scripting][string]")
{
    CHECK(String("String.Substring", { Text("Hello"), 1, 3 }) == "ell");
    CHECK(String("String.Substring", { Text("Hello"), 0, 5 }) == "Hello");
    CHECK(String("String.Substring", { Text("Hello"), 5, 0 }).empty());
    CHECK(String("String.Substring", { Utf8(u8"Gr\u00F6\u00DFe"), 2, 2 }) == Tests::Text(u8"\u00F6\u00DF"));
}

TEST_CASE("Substring rejects ranges outside the text", "[scripting][string]")
{
    CHECK_THROWS_AS(CallPure<std::string>("String.Substring", { Text("Hello"), 3, 3 }), ScriptError);
    CHECK_THROWS_AS(CallPure<std::string>("String.Substring", { Text("Hello"), -1, 1 }), ScriptError);
    CHECK_THROWS_AS(CallPure<std::string>("String.Substring", { Text("Hello"), 0, -1 }), ScriptError);
    CHECK_THROWS_AS(CallPure<std::string>("String.Substring", { Text("Hello"), std::numeric_limits<i32>::max(), 1 }),
                    ScriptError);
}

TEST_CASE("Text to int parses whole numbers and falls back otherwise", "[scripting][string]")
{
    CHECK(Int("String.ParseInt", { Text("42"), -1 }) == 42);
    CHECK(Int("String.ParseInt", { Text("  -7 \n"), -1 }) == -7);
    CHECK(Int("String.ParseInt", { Text("+5"), -1 }) == 5);
    CHECK(Int("String.ParseInt", { Text("-2147483648"), -1 }) == std::numeric_limits<i32>::min());

    CHECK(Int("String.ParseInt", { Text(""), -1 }) == -1);
    CHECK(Int("String.ParseInt", { Text("abc"), -1 }) == -1);
    CHECK(Int("String.ParseInt", { Text("12abc"), -1 }) == -1);
    CHECK(Int("String.ParseInt", { Text("3.5"), -1 }) == -1);
    CHECK(Int("String.ParseInt", { Text("+-5"), -1 }) == -1);
    CHECK(Int("String.ParseInt", { Text("2147483648"), -1 }) == -1);
}

TEST_CASE("Text to float always uses a dot and rejects NaN and infinity", "[scripting][string]")
{
    CHECK(Float("String.ParseFloat", { Text("3.5"), -1.0f }) == 3.5f);
    CHECK(Float("String.ParseFloat", { Text(" -0.25 "), -1.0f }) == -0.25f);
    CHECK(Float("String.ParseFloat", { Text("+1e3"), -1.0f }) == 1000.0f);
    CHECK(Float("String.ParseFloat", { Text("7"), -1.0f }) == 7.0f);

    CHECK(Float("String.ParseFloat", { Text("3,5"), -1.0f }) == -1.0f);
    CHECK(Float("String.ParseFloat", { Text("abc"), -1.0f }) == -1.0f);
    CHECK(Float("String.ParseFloat", { Text("nan"), -1.0f }) == -1.0f);
    CHECK(Float("String.ParseFloat", { Text("inf"), -1.0f }) == -1.0f);
    CHECK(Float("String.ParseFloat", { Text("1e999"), -1.0f }) == -1.0f);
}

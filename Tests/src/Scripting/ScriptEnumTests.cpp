#include "Scripting/ScriptTestHelpers.h"
#include "TestUtilities.h"

#include <Engine/Input/KeyCodes.h>
#include <Engine/Input/MouseButtons.h>
#include <Engine/Scripting/API/ScriptAPI.h>
#include <Engine/Scripting/API/ScriptEnum.h>
#include <Engine/Scripting/Visual/GraphSchema.h>
#include <Engine/Scripting/Visual/ScriptGraph.h>

#include <catch2/catch_test_macros.hpp>
#include <magic_enum/magic_enum.hpp>

#include <glm/glm.hpp>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

using namespace ByteForge;
using namespace ByteForge::Tests;

namespace
{
    const ScriptParameter& OnlyParameter(const std::string_view function)
    {
        const ScriptFunction* found = ScriptAPI::Get().Find(function);
        REQUIRE(found != nullptr);
        REQUIRE(found->Parameters.size() == 1);
        return found->Parameters.front();
    }

    i32 Code(const KeyCode key) { return static_cast<i32>(key); }
}

TEST_CASE("Key and mouse button parameters name their enum but stay Ints", "[scripting][enum]")
{
    const ScriptParameter& key = OnlyParameter("Input.IsKeyPressed");
    CHECK(key.Type == ScriptFieldType::Int);
    REQUIRE(key.Enum != nullptr);
    CHECK(key.Enum->Name == "Key");

    const ScriptParameter& button = OnlyParameter("Input.IsMouseButtonDown");
    REQUIRE(button.Enum != nullptr);
    CHECK(button.Enum->Name == "Mouse Button");

    CHECK(GetSignature(*ScriptAPI::Get().Find("Input.IsKeyPressed")) == "(Int)->Bool");
    CHECK(GetSignature(*ScriptAPI::Get().Find("Input.IsMouseButtonDown")) == "(Int)->Bool");

    CHECK(ScriptAPI::Get().FindEnum("Key") == key.Enum);
    CHECK(ScriptAPI::Get().FindEnum("Nonsense") == nullptr);
}

TEST_CASE("Enum values carry the enumerator names", "[scripting][enum]")
{
    const ScriptEnum& keys = ScriptEnumOf<KeyCode>::Value;

    CHECK(keys.Find(Code(KeyCode::A))->Name == "A");
    CHECK(keys.Find(Code(KeyCode::D0))->Name == "D0");
    CHECK(keys.Find(Code(KeyCode::LeftShift))->Name == "LeftShift");
    CHECK(keys.Find(0) == nullptr);

    CHECK(ScriptEnumOf<MouseButton>::Value.Find(static_cast<i32>(MouseButton::ButtonRight))->Name == "Right");
}

TEST_CASE("magic_enum finds the key codes above 127", "[scripting][enum]")
{
    const ScriptEnum& keys = ScriptEnumOf<KeyCode>::Value;

    CHECK(keys.Find(Code(KeyCode::Escape)) != nullptr);
    CHECK(keys.Find(Code(KeyCode::F12)) != nullptr);
    CHECK(keys.Find(Code(KeyCode::Menu)) != nullptr);
    CHECK(keys.Values.size() == magic_enum::enum_count<KeyCode>());
    CHECK(keys.Values.size() > 100);
}

TEST_CASE("Every key code has exactly one entry", "[scripting][enum]")
{
    const ScriptEnum& keys = ScriptEnumOf<KeyCode>::Value;

    for (const ScriptEnumValue& entry : keys.Values)
    {
        const auto count = std::ranges::count(keys.Values, entry.Value, &ScriptEnumValue::Value);
        CHECK(count == 1);
    }
}

TEST_CASE("Values outside the enum are rejected when the function runs", "[scripting][enum]")
{
    const ScriptCallContext context{};

    CHECK_THROWS_AS(CallScript(context, "Input.IsKeyDown", { 9999 }), ScriptError);
    CHECK_THROWS_AS(CallScript(context, "Input.IsKeyDown", { 0 }), ScriptError);
    CHECK_THROWS_AS(CallScript(context, "Input.IsMouseButtonDown", { 42 }), ScriptError);

    CHECK_NOTHROW(CallScript(context, "Input.IsKeyDown", { Code(KeyCode::Space) }));
    CHECK_NOTHROW(CallScript(context, "Input.IsMouseButtonDown", { 7 }));
}

TEST_CASE("Enum pins accept plain Ints but not other enums", "[scripting][enum][graph]")
{
    const PinType key = PinType::OfEnum("Key");
    const PinType button = PinType::OfEnum("Mouse Button");
    const PinType integer = PinType::Of(ScriptFieldType::Int);
    const PinType real = PinType::Of(ScriptFieldType::Float);

    CHECK(IsAssignable(integer, key));
    CHECK(IsAssignable(key, integer));
    CHECK(IsAssignable(key, key));
    CHECK_FALSE(IsAssignable(key, button));
    CHECK_FALSE(IsAssignable(key, real));
    CHECK(IsAssignable(integer, real));

    CHECK(DescribePinType(key) == "Key");
    CHECK(GetValueTypeName(button) == "Mouse Button");
}

TEST_CASE("An enum pin starts at the first value of its enum", "[scripting][enum][graph]")
{
    CHECK(DefaultValueFor(PinType::OfEnum("Key")) == ScriptValue(Code(KeyCode::Space)));
    CHECK(DefaultValueFor(PinType::OfEnum("Mouse Button")) == ScriptValue(0));
}

TEST_CASE("A key input is an enum pin and an invalid stored key is reported", "[scripting][enum][graph]")
{
    ScriptGraph graph;
    const UUID node = graph.AddNode(CallNode{ "Input.IsKeyPressed" }, glm::vec2(0.0f));

    const NodeSignature signature = DescribeNode(graph, *graph.FindNode(node));
    const PinInfo* pin = FindPin(signature, "Key", PinDirection::Input);
    REQUIRE(pin != nullptr);
    CHECK(pin->Type == PinType::OfEnum("Key"));

    graph.FindNode(node)->Defaults.insert_or_assign(std::string("Key"), ScriptValue(Code(KeyCode::W)));
    CHECK(ValidateGraph(graph).empty());

    graph.FindNode(node)->Defaults.insert_or_assign(std::string("Key"), ScriptValue(9999));
    const std::vector<GraphDiagnostic> diagnostics = ValidateGraph(graph);
    REQUIRE(diagnostics.size() == 1);
    CHECK(diagnostics.front().Severity == GraphSeverity::Warning);
}

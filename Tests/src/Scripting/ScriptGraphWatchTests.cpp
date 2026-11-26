#include "Scripting/GraphTestHelpers.h"
#include "TestUtilities.h"

#include <Engine/Scripting/ScriptBackend.h>
#include <Engine/Scripting/Visual/GraphEvaluator.h>
#include <Engine/Scripting/Visual/GraphSchema.h>
#include <Engine/Scripting/Visual/ScriptGraph.h>

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <vector>

using namespace ByteForge;
using namespace ByteForge::Tests;

namespace
{
    PinRef Pin(const UUID node, const std::string_view name)
    {
        return { node, std::string(name) };
    }

    UUID DeclareSquare(GraphBuilder& builder)
    {
        const Result<UUID> id = builder.Graph.AddFunction("Square");
        RequireSuccess(id);

        GraphFunction function = *builder.Graph.FindFunction(*id);
        function.Pure = true;
        function.Inputs = { { .Id = UUID(), .Name = "X", .Default = 0.0f } };
        function.Outputs = { { .Id = UUID(), .Name = "Result", .Default = 0.0f } };
        RequireSuccess(builder.Graph.UpdateFunction(std::move(function)));

        UUID entry{ 0 };
        UUID result{ 0 };
        for (const GraphNode& node : builder.Graph.GetNodes())
        {
            if (std::holds_alternative<FunctionEntryNode>(node.Data))
                entry = node.Id;
            else if (std::holds_alternative<FunctionReturnNode>(node.Data))
                result = node.Id;
        }

        const UUID power = builder.Graph.AddNode(CallNode{ "Math.PowFloat" }, glm::vec2(0.0f), *id);
        builder.SetDefault(power, "Exponent", 2.0f);
        builder.Link(entry, "X", power, "Base");
        builder.Link(power, "Result", result, "Result");
        return *id;
    }
}

TEST_CASE("Constant math is worked out without running the graph", "[scripting][graph][watch]")
{
    GraphBuilder builder;
    const UUID literal = builder.Add(LiteralNode{ 2 });
    const UUID add = builder.Add(CallNode{ "Math.AddInt" });
    builder.SetDefault(add, "B", 3);
    builder.Link(literal, GraphPin::Value, add, "A");

    const std::optional<ScriptValue> value = EvaluateConstant(builder.Graph, Pin(add, "Result"));
    REQUIRE(value.has_value());
    CHECK(*value == ScriptValue(5));
}

TEST_CASE("A pure graph function is worked out through its call", "[scripting][graph][watch]")
{
    GraphBuilder builder;
    const UUID square = DeclareSquare(builder);

    const UUID inner = builder.Add(CallFunctionNode{ square });
    const UUID outer = builder.Add(CallFunctionNode{ square });
    builder.SetDefault(inner, "X", 2.0f);
    builder.Link(inner, "Result", outer, "X");

    const std::optional<ScriptValue> value = EvaluateConstant(builder.Graph, Pin(outer, "Result"));
    REQUIRE(value.has_value());
    CHECK(*value == ScriptValue(16.0f));
}

TEST_CASE("Select and reroutes are followed for constants", "[scripting][graph][watch]")
{
    GraphBuilder builder;
    const UUID flag = builder.Add(LiteralNode{ false });
    const UUID yes = builder.Add(LiteralNode{ 1 });
    const UUID no = builder.Add(LiteralNode{ 2 });
    const UUID select = builder.Add(SelectNode{});
    const UUID reroute = builder.Add(RerouteNode{});

    builder.Link(flag, GraphPin::Value, select, GraphPin::Condition);
    builder.Link(yes, GraphPin::Value, select, GraphPin::IfTrue);
    builder.Link(no, GraphPin::Value, select, GraphPin::IfFalse);
    builder.Link(select, GraphPin::Value, reroute, GraphPin::In);

    const std::optional<ScriptValue> value = EvaluateConstant(builder.Graph, Pin(reroute, GraphPin::Out));
    REQUIRE(value.has_value());
    CHECK(*value == ScriptValue(2));

    const PinRef source = FindValueSource(builder.Graph, Pin(reroute, GraphPin::Out));
    CHECK(source.Node == select);
    CHECK(source.Pin == GraphPin::Value);
}

TEST_CASE("Values that depend on the game are not constant", "[scripting][graph][watch]")
{
    GraphBuilder builder;
    const UUID variable = builder.Variable("Speed", 1);
    const UUID get = builder.Add(GetVariableNode{ variable });
    const UUID add = builder.Add(CallNode{ "Math.AddInt" });
    builder.Link(get, GraphPin::Value, add, "A");

    CHECK_FALSE(EvaluateConstant(builder.Graph, Pin(get, GraphPin::Value)).has_value());
    CHECK_FALSE(EvaluateConstant(builder.Graph, Pin(add, "Result")).has_value());
}

TEST_CASE("A running graph reports the last value of a pin", "[scripting][graph][watch]")
{
    GraphFixture fixture;
    GraphBuilder builder;
    const UUID total = builder.Variable("Total", 0);
    const UUID unused = builder.Add(LiteralNode{ 9 });

    const UUID start = builder.Add(EventNode{ GraphEvent::OnCreate });
    const UUID add = builder.Add(CallNode{ "Math.AddInt" });
    const UUID set = builder.Add(SetVariableNode{ total });
    builder.SetDefault(add, "A", 2);
    builder.SetDefault(add, "B", 3);
    builder.Link(add, "Result", set, GraphPin::Value);
    builder.Link(start, GraphPin::Then, set, GraphPin::Exec);

    const Scope<ScriptInstance> instance = fixture.Instantiate(fixture.Import(builder.Graph, "Watch.bfgraph"));
    instance->OnCreate();

    const std::optional<ScriptValue> value = instance->GetPinValue(add, "Result");
    REQUIRE(value.has_value());
    CHECK(*value == ScriptValue(5));

    CHECK_FALSE(instance->GetPinValue(unused, GraphPin::Value).has_value());
}

TEST_CASE("A pure chain is worked out from the current values of a running graph", "[scripting][graph][watch]")
{
    GraphFixture fixture;
    GraphBuilder builder;
    const UUID x = builder.Variable("X", 8.0f);
    const UUID square = DeclareSquare(builder);

    const UUID get = builder.Add(GetVariableNode{ x });
    const UUID call = builder.Add(CallFunctionNode{ square });
    const UUID text = builder.Add(CallNode{ "String.FromFloat" });
    const UUID log = builder.Add(CallNode{ "Debug.Log" });
    const UUID start = builder.Add(EventNode{ GraphEvent::OnCreate });
    builder.Link(get, GraphPin::Value, call, "X");
    builder.Link(call, "Result", text, "Value");
    builder.Link(text, "Result", log, "Message");
    builder.Link(start, GraphPin::Then, log, GraphPin::Exec);

    const Scope<ScriptInstance> instance = fixture.Instantiate(fixture.Import(builder.Graph, "LiveChain.bfgraph"));
    instance->OnCreate();
    REQUIRE(instance->SetField("X", 3.0f));

    CHECK(instance->GetPinValue(call, "Result") == std::optional<ScriptValue>(64.0f));

    const LivePinValue live = [&](const PinRef& pin) { return instance->GetPinValue(pin.Node, pin.Pin); };
    CHECK(EvaluatePin(builder.Graph, Pin(call, "Result"), live) == std::optional<ScriptValue>(9.0f));
}

#include "Scripting/GraphTestHelpers.h"
#include "TestUtilities.h"

#include <Engine/Scripting/ScriptBackend.h>
#include <Engine/Scripting/Visual/GraphSchema.h>
#include <Engine/Scripting/Visual/ScriptGraph.h>
#include <Engine/Scripting/Visual/ScriptGraphSerializer.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

using namespace ByteForge;
using namespace ByteForge::Tests;

namespace
{
    GraphParameter Parameter(std::string name, ScriptValue type)
    {
        return { .Id = UUID(), .Name = std::move(name), .Default = std::move(type) };
    }

    UUID DeclareFunction(GraphBuilder& builder, std::string name, std::vector<GraphParameter> inputs,
                         std::vector<GraphParameter> outputs, const bool pure = false)
    {
        const Result<UUID> id = builder.Graph.AddFunction(std::move(name));
        RequireSuccess(id);

        GraphFunction function = *builder.Graph.FindFunction(*id);
        function.Inputs = std::move(inputs);
        function.Outputs = std::move(outputs);
        function.Pure = pure;
        RequireSuccess(builder.Graph.UpdateFunction(std::move(function)));
        return *id;
    }

    UUID AddIn(GraphBuilder& builder, const UUID scope, NodeData data)
    {
        return builder.Graph.AddNode(std::move(data), glm::vec2(0.0f), scope);
    }

    template<typename T>
    UUID FindIn(const ScriptGraph& graph, const UUID function)
    {
        for (const GraphNode& node : graph.GetNodes())
        {
            if (node.Scope == function && std::holds_alternative<T>(node.Data))
                return node.Id;
        }
        FAIL("The function has no such node");
        return UUID(0);
    }

    UUID DeclareDouble(GraphBuilder& builder, const bool pure)
    {
        const UUID function = DeclareFunction(builder, "Double", { Parameter("X", 0) }, { Parameter("Result", 0) }, pure);
        const UUID entry = FindIn<FunctionEntryNode>(builder.Graph, function);
        const UUID result = FindIn<FunctionReturnNode>(builder.Graph, function);

        const UUID add = AddIn(builder, function, CallNode{ "Math.AddInt" });
        builder.Link(entry, "X", add, "A");
        builder.Link(entry, "X", add, "B");
        builder.Link(add, "Result", result, "Result");
        return function;
    }

    bool HasLink(const ScriptGraph& graph, const UUID node, const std::string_view pin, const PinDirection direction)
    {
        return std::ranges::any_of(graph.GetLinks(), [&](const GraphLink& link)
        {
            const PinRef& end = direction == PinDirection::Output ? link.From : link.To;
            return end.Node == node && end.Pin == pin;
        });
    }
}

TEST_CASE("A new function starts with a connected entry and return node", "[scripting][graph][functions]")
{
    GraphBuilder builder;
    const Result<UUID> function = builder.Graph.AddFunction("Empty");
    RequireSuccess(function);

    const UUID entry = FindIn<FunctionEntryNode>(builder.Graph, *function);
    const UUID result = FindIn<FunctionReturnNode>(builder.Graph, *function);
    CHECK(HasLink(builder.Graph, entry, GraphPin::Then, PinDirection::Output));
    CHECK(HasLink(builder.Graph, result, GraphPin::Exec, PinDirection::Input));
    CHECK_FALSE(HasError(builder.Graph));
}

TEST_CASE("A function call passes inputs and returns outputs", "[scripting][graph][functions]")
{
    GraphFixture fixture;
    GraphBuilder builder;
    const UUID total = builder.Variable("Total", 0);
    const UUID function = DeclareDouble(builder, false);

    const UUID start = builder.Add(EventNode{ GraphEvent::OnCreate });
    const UUID call = builder.Add(CallFunctionNode{ function });
    const UUID set = builder.Add(SetVariableNode{ total });
    builder.SetDefault(call, "X", 21);
    builder.Link(start, GraphPin::Then, call, GraphPin::Exec);
    builder.Link(call, GraphPin::Then, set, GraphPin::Exec);
    builder.Link(call, "Result", set, GraphPin::Value);

    REQUIRE_FALSE(HasError(builder.Graph));
    const Scope<ScriptInstance> instance = fixture.Instantiate(fixture.Import(builder.Graph, "Double.bfgraph"));
    instance->OnCreate();

    CHECK(Field<i32>(*instance, "Total") == 42);
}

TEST_CASE("Two calls of a pure function keep their own results", "[scripting][graph][functions]")
{
    GraphFixture fixture;
    GraphBuilder builder;
    const UUID total = builder.Variable("Total", 0);
    const UUID function = DeclareDouble(builder, true);

    const UUID one = builder.Add(CallFunctionNode{ function });
    const UUID five = builder.Add(CallFunctionNode{ function });
    builder.SetDefault(one, "X", 1);
    builder.SetDefault(five, "X", 5);

    const UUID add = builder.Add(CallNode{ "Math.AddInt" });
    builder.Link(one, "Result", add, "A");
    builder.Link(five, "Result", add, "B");

    const UUID start = builder.Add(EventNode{ GraphEvent::OnCreate });
    const UUID set = builder.Add(SetVariableNode{ total });
    builder.Link(add, "Result", set, GraphPin::Value);
    builder.Link(start, GraphPin::Then, set, GraphPin::Exec);

    REQUIRE_FALSE(HasError(builder.Graph));
    const Scope<ScriptInstance> instance = fixture.Instantiate(fixture.Import(builder.Graph, "PureDouble.bfgraph"));
    instance->OnCreate();

    CHECK(Field<i32>(*instance, "Total") == 12);
}

TEST_CASE("Local variables start over at every call", "[scripting][graph][functions]")
{
    GraphFixture fixture;
    GraphBuilder builder;
    const UUID total = builder.Variable("Total", 0);

    const UUID function = DeclareFunction(builder, "Counter", {}, { Parameter("Result", 0) });
    const Result<UUID> local = builder.Graph.AddVariable("N", 0, function);
    RequireSuccess(local);

    const UUID entry = FindIn<FunctionEntryNode>(builder.Graph, function);
    const UUID result = FindIn<FunctionReturnNode>(builder.Graph, function);
    const UUID get = AddIn(builder, function, GetVariableNode{ *local });
    const UUID add = AddIn(builder, function, CallNode{ "Math.AddInt" });
    const UUID set = AddIn(builder, function, SetVariableNode{ *local });
    builder.SetDefault(add, "B", 1);
    builder.Link(get, GraphPin::Value, add, "A");
    builder.Link(add, "Result", set, GraphPin::Value);
    builder.Link(entry, GraphPin::Then, set, GraphPin::Exec);
    builder.Link(set, GraphPin::Then, result, GraphPin::Exec);
    builder.Link(set, GraphPin::Value, result, "Result");

    const UUID start = builder.Add(EventNode{ GraphEvent::OnCreate });
    const UUID first = builder.Add(CallFunctionNode{ function });
    const UUID second = builder.Add(CallFunctionNode{ function });
    const UUID store = builder.Add(SetVariableNode{ total });
    builder.Link(start, GraphPin::Then, first, GraphPin::Exec);
    builder.Link(first, GraphPin::Then, second, GraphPin::Exec);
    builder.Link(second, GraphPin::Then, store, GraphPin::Exec);
    builder.Link(second, "Result", store, GraphPin::Value);

    REQUIRE_FALSE(HasError(builder.Graph));
    const Scope<ScriptInstance> instance = fixture.Instantiate(fixture.Import(builder.Graph, "Counter.bfgraph"));
    instance->OnCreate();

    CHECK(Field<i32>(*instance, "Total") == 1);
    CHECK_FALSE(instance->GetField("N").has_value());
}

TEST_CASE("Functions reject what they cannot run", "[scripting][graph][functions]")
{
    GraphBuilder builder;
    const UUID function = DeclareFunction(builder, "Strict", {}, {});

    SECTION("A Delay")
    {
        AddIn(builder, function, DelayNode{});
        CHECK(HasError(builder.Graph));
    }

    SECTION("A call of itself")
    {
        AddIn(builder, function, CallFunctionNode{ function });
        CHECK(HasError(builder.Graph));
    }

    SECTION("An event node")
    {
        AddIn(builder, function, EventNode{ GraphEvent::OnCreate });
        CHECK(HasError(builder.Graph));
    }

    SECTION("A local of the function used in the event graph")
    {
        const Result<UUID> local = builder.Graph.AddVariable("Hidden", 0, function);
        RequireSuccess(local);
        builder.Add(GetVariableNode{ *local });
        CHECK(HasError(builder.Graph));
    }
}

TEST_CASE("Nodes of different canvases cannot be linked", "[scripting][graph][functions]")
{
    GraphBuilder builder;
    const UUID function = DeclareFunction(builder, "Inner", {}, {});

    const UUID outside = builder.Add(LiteralNode{ 1 });
    const UUID inside = AddIn(builder, function, CallNode{ "Math.AddInt" });

    CHECK_FALSE(CanConnect(builder.Graph, { outside, std::string(GraphPin::Value) }, { inside, "A" }).has_value());
}

TEST_CASE("Editing a function repairs the nodes that use it", "[scripting][graph][functions]")
{
    GraphBuilder builder;
    const UUID function = DeclareDouble(builder, false);
    const UUID entry = FindIn<FunctionEntryNode>(builder.Graph, function);

    const UUID start = builder.Add(EventNode{ GraphEvent::OnCreate });
    const UUID call = builder.Add(CallFunctionNode{ function });
    builder.SetDefault(call, "X", 3);
    builder.Link(start, GraphPin::Then, call, GraphPin::Exec);

    GraphFunction renamed = *builder.Graph.FindFunction(function);
    renamed.Inputs.front().Name = "Value";
    RequireSuccess(builder.Graph.UpdateFunction(renamed));

    CHECK(HasLink(builder.Graph, entry, "Value", PinDirection::Output));
    CHECK(builder.Graph.FindNode(call)->Defaults.contains("Value"));

    GraphFunction pure = *builder.Graph.FindFunction(function);
    pure.Pure = true;
    RequireSuccess(builder.Graph.UpdateFunction(pure));

    CHECK_FALSE(HasLink(builder.Graph, call, GraphPin::Exec, PinDirection::Input));
    CHECK_FALSE(HasError(builder.Graph));
}

TEST_CASE("Removing a function removes its graph, its locals and its calls", "[scripting][graph][functions]")
{
    GraphBuilder builder;
    const UUID function = DeclareDouble(builder, true);
    const UUID entry = FindIn<FunctionEntryNode>(builder.Graph, function);
    const Result<UUID> local = builder.Graph.AddVariable("Temp", 0, function);
    RequireSuccess(local);
    const UUID call = builder.Add(CallFunctionNode{ function });

    builder.Graph.RemoveFunction(function);

    CHECK(builder.Graph.FindFunction(function) == nullptr);
    CHECK(builder.Graph.FindNode(entry) == nullptr);
    CHECK(builder.Graph.FindNode(call) == nullptr);
    CHECK(builder.Graph.FindVariable(*local) == nullptr);
}

TEST_CASE("Locals of different functions may share a name, but not with a member", "[scripting][graph][functions]")
{
    GraphBuilder builder;
    const UUID first = DeclareFunction(builder, "First", {}, {});
    const UUID second = DeclareFunction(builder, "Second", {}, {});
    builder.Variable("Speed", 0.0f);

    CHECK(builder.Graph.AddVariable("Count", 0, first).has_value());
    CHECK(builder.Graph.AddVariable("Count", 0, second).has_value());
    CHECK_FALSE(builder.Graph.AddVariable("Speed", 0, first).has_value());
    CHECK_FALSE(builder.Graph.AddVariable("Count", 0).has_value());
}

TEST_CASE("Functions survive saving a graph", "[scripting][graph][functions][serializer]")
{
    GraphBuilder builder;
    const UUID function = DeclareDouble(builder, true);
    const UUID entry = FindIn<FunctionEntryNode>(builder.Graph, function);
    const Result<UUID> local = builder.Graph.AddVariable("Temp", 2.0f, function);
    RequireSuccess(local);

    const Result<ScriptGraph> loaded = DeserializeScriptGraph(SerializeScriptGraph(builder.Graph));
    RequireSuccess(loaded);

    const GraphFunction* saved = loaded->FindFunction(function);
    REQUIRE(saved != nullptr);
    CHECK(saved->Name == "Double");
    CHECK(saved->Pure);
    CHECK(saved->Inputs.size() == 1);
    CHECK(saved->Outputs.size() == 1);

    CHECK(loaded->FindNode(entry)->Scope == function);

    const GraphVariable* temp = loaded->FindVariable(*local);
    REQUIRE(temp != nullptr);
    CHECK(temp->Scope == function);
    CHECK_FALSE(temp->Exposed);
}

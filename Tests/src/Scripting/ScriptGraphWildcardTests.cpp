#include "Scripting/GraphTestHelpers.h"
#include "TestUtilities.h"

#include <Engine/Scripting/ScriptBackend.h>
#include <Engine/Scripting/Visual/GraphSchema.h>
#include <Engine/Scripting/Visual/ScriptGraph.h>
#include <Engine/Scripting/Visual/ScriptGraphSerializer.h>

#include <catch2/catch_test_macros.hpp>

#include <glm/glm.hpp>

#include <optional>
#include <string>
#include <string_view>

using namespace ByteForge;
using namespace ByteForge::Tests;

namespace
{
    PinRef Pin(const UUID node, const std::string_view name)
    {
        return { node, std::string(name) };
    }

    template<typename T>
    std::optional<PinType> StoredType(const ScriptGraph& graph, const UUID node)
    {
        const GraphNode* found = graph.FindNode(node);
        REQUIRE(found != nullptr);
        return std::get<T>(found->Data).Type;
    }
}

TEST_CASE("A reroute takes the type of its first link and loses it with its last", "[scripting][graph][wildcard]")
{
    GraphBuilder builder;
    const UUID literal = builder.Add(LiteralNode{ 3 });
    const UUID reroute = builder.Add(RerouteNode{});

    CHECK_FALSE(StoredType<RerouteNode>(builder.Graph, reroute).has_value());

    builder.Link(literal, GraphPin::Value, reroute, GraphPin::In);
    CHECK(StoredType<RerouteNode>(builder.Graph, reroute) == PinType::Of(ScriptFieldType::Int));

    builder.Graph.Disconnect(Pin(reroute, GraphPin::In), PinDirection::Input);
    CHECK_FALSE(StoredType<RerouteNode>(builder.Graph, reroute).has_value());
}

TEST_CASE("Two untyped wildcard pins cannot be connected", "[scripting][graph][wildcard]")
{
    GraphBuilder builder;
    const UUID first = builder.Add(RerouteNode{});
    const UUID second = builder.Add(RerouteNode{});

    CHECK_FALSE(CanConnect(builder.Graph, Pin(first, GraphPin::Out), Pin(second, GraphPin::In)).has_value());
}

TEST_CASE("A reroute carries exec, a Select does not", "[scripting][graph][wildcard]")
{
    GraphBuilder builder;
    const UUID event = builder.Add(EventNode{ GraphEvent::OnCreate });
    const UUID select = builder.Add(SelectNode{});
    const UUID reroute = builder.Add(RerouteNode{});

    CHECK_FALSE(CanConnect(builder.Graph, Pin(event, GraphPin::Then), Pin(select, GraphPin::IfTrue)).has_value());
    CHECK(CanConnect(builder.Graph, Pin(event, GraphPin::Then), Pin(reroute, GraphPin::In)).has_value());
}

TEST_CASE("A link to the Select condition does not give the Select a type", "[scripting][graph][wildcard]")
{
    GraphBuilder builder;
    const UUID flag = builder.Add(LiteralNode{ true });
    const UUID select = builder.Add(SelectNode{});

    builder.Link(flag, GraphPin::Value, select, GraphPin::Condition);
    CHECK_FALSE(StoredType<SelectNode>(builder.Graph, select).has_value());
}

TEST_CASE("A Select forgets its type and wildcard values once its last link is gone", "[scripting][graph][wildcard]")
{
    GraphBuilder builder;
    const UUID literal = builder.Add(LiteralNode{ 1 });
    const UUID select = builder.Add(SelectNode{});

    builder.Link(literal, GraphPin::Value, select, GraphPin::IfTrue);
    builder.SetDefault(select, GraphPin::IfFalse, 4);
    builder.SetDefault(select, GraphPin::Condition, true);

    builder.Graph.RemoveNode(literal);

    const GraphNode& node = *builder.Graph.FindNode(select);
    CHECK_FALSE(std::get<SelectNode>(node.Data).Type.has_value());
    CHECK_FALSE(node.Defaults.contains(GraphPin::IfFalse));
    CHECK(node.Defaults.contains(GraphPin::Condition));
}

TEST_CASE("Exec runs through a reroute", "[scripting][graph][wildcard]")
{
    GraphFixture fixture;
    GraphBuilder builder;
    const UUID count = builder.Variable("Count", 0);

    const UUID event = builder.Add(EventNode{ GraphEvent::OnCreate });
    const UUID reroute = builder.Add(RerouteNode{});
    builder.Link(event, GraphPin::Then, reroute, GraphPin::In);
    builder.AppendIncrement(count, reroute, GraphPin::Out);

    const Scope<ScriptInstance> instance = fixture.Instantiate(fixture.Import(builder.Graph, "ExecReroute.bfgraph"));
    instance->OnCreate();

    CHECK(Field<i32>(*instance, "Count") == 1);
}

TEST_CASE("Data runs through a reroute and is converted after it", "[scripting][graph][wildcard]")
{
    GraphFixture fixture;
    GraphBuilder builder;
    const UUID source = builder.Variable("Source", 5);
    const UUID target = builder.Variable("Target", 0.0f);

    const UUID event = builder.Add(EventNode{ GraphEvent::OnCreate });
    const UUID get = builder.Add(GetVariableNode{ source });
    const UUID reroute = builder.Add(RerouteNode{});
    const UUID set = builder.Add(SetVariableNode{ target });

    builder.Link(get, GraphPin::Value, reroute, GraphPin::In);
    builder.Link(reroute, GraphPin::Out, set, GraphPin::Value);
    builder.Link(event, GraphPin::Then, set, GraphPin::Exec);

    const Scope<ScriptInstance> instance = fixture.Instantiate(fixture.Import(builder.Graph, "DataReroute.bfgraph"));
    instance->OnCreate();

    CHECK(Field<f32>(*instance, "Target") == 5.0f);
}

TEST_CASE("A Select picks its input by the condition", "[scripting][graph][wildcard]")
{
    GraphFixture fixture;
    GraphBuilder builder;
    const UUID pick = builder.Variable("Pick", true);
    const UUID result = builder.Variable("Result", 0);

    const UUID event = builder.Add(EventNode{ GraphEvent::OnCreate });
    const UUID getPick = builder.Add(GetVariableNode{ pick });
    const UUID ten = builder.Add(LiteralNode{ 10 });
    const UUID select = builder.Add(SelectNode{});
    const UUID set = builder.Add(SetVariableNode{ result });

    builder.Link(getPick, GraphPin::Value, select, GraphPin::Condition);
    builder.Link(ten, GraphPin::Value, select, GraphPin::IfTrue);
    builder.SetDefault(select, GraphPin::IfFalse, 20);
    builder.Link(select, GraphPin::Value, set, GraphPin::Value);
    builder.Link(event, GraphPin::Then, set, GraphPin::Exec);

    REQUIRE_FALSE(HasError(builder.Graph));
    const UUID asset = fixture.Import(builder.Graph, "Select.bfgraph");

    const Scope<ScriptInstance> picked = fixture.Instantiate(asset);
    picked->OnCreate();
    CHECK(Field<i32>(*picked, "Result") == 10);

    const Scope<ScriptInstance> other = fixture.Instantiate(asset);
    REQUIRE(other->SetField("Pick", false));
    other->OnCreate();
    CHECK(Field<i32>(*other, "Result") == 20);
}

TEST_CASE("Wildcard types survive saving a graph", "[scripting][graph][wildcard][serializer]")
{
    GraphBuilder builder;
    const UUID event = builder.Add(EventNode{ GraphEvent::OnCreate });
    const UUID reroute = builder.Add(RerouteNode{});
    const UUID vector = builder.Add(LiteralNode{ glm::vec2(1.0f) });
    const UUID select = builder.Add(SelectNode{});
    const UUID untyped = builder.Add(SelectNode{});

    builder.Link(event, GraphPin::Then, reroute, GraphPin::In);
    builder.Link(vector, GraphPin::Value, select, GraphPin::IfTrue);

    const Result<ScriptGraph> loaded = DeserializeScriptGraph(SerializeScriptGraph(builder.Graph));
    RequireSuccess(loaded);

    CHECK(StoredType<RerouteNode>(*loaded, reroute) == PinType::Exec());
    CHECK(StoredType<SelectNode>(*loaded, select) == PinType::Of(ScriptFieldType::Vector2));
    CHECK_FALSE(StoredType<SelectNode>(*loaded, untyped).has_value());
}

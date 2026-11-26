#include "Scripting/GraphTestHelpers.h"
#include "TestUtilities.h"

#include <Engine/Scripting/API/ScriptAPI.h>
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
    PinType KeyType()
    {
        const ScriptEnum* key = ScriptAPI::Get().FindEnum("Key");
        REQUIRE(key != nullptr);
        return PinType::OfEnum(key->Name);
    }

    PinType OtherEnumType()
    {
        for (const ScriptEnum* scriptEnum : ScriptAPI::Get().GetEnums())
        {
            if (scriptEnum->Name != "Key")
                return PinType::OfEnum(scriptEnum->Name);
        }
        FAIL("The script API has only one enum");
        return {};
    }

    PinType ValueType(const ScriptGraph& graph, const UUID node, const PinDirection direction)
    {
        const NodeSignature signature = DescribeNode(graph, *graph.FindNode(node));
        const PinInfo* pin = FindPin(signature, GraphPin::Value, direction);
        REQUIRE(pin != nullptr);
        return pin->Type;
    }
}

TEST_CASE("The script API lists its enums, and they are literal types", "[scripting][graph][enums]")
{
    const std::vector<const ScriptEnum*> enums = ScriptAPI::Get().GetEnums();
    CHECK(std::ranges::any_of(enums, [](const ScriptEnum* scriptEnum) { return scriptEnum->Name == "Key"; }));

    const std::vector<PinType> literals = GetLiteralTypes();
    CHECK(std::ranges::find(literals, KeyType()) != literals.end());
}

TEST_CASE("A variable can hold an enum", "[scripting][graph][enums]")
{
    GraphBuilder builder;
    const UUID key = builder.Variable("Key", 0);
    RequireSuccess(builder.Graph.SetVariableType(key, KeyType()));

    const GraphVariable& variable = *builder.Graph.FindVariable(key);
    CHECK(TypeOf(variable) == KeyType());
    CHECK(variable.Default == DefaultValueFor(KeyType()));

    const UUID get = builder.Add(GetVariableNode{ key });
    CHECK(ValueType(builder.Graph, get, PinDirection::Output) == KeyType());
}

TEST_CASE("Another number keeps a variable's enum, a value of another kind ends it", "[scripting][graph][enums]")
{
    GraphBuilder builder;
    const UUID key = builder.Variable("Key", 0);
    RequireSuccess(builder.Graph.SetVariableType(key, KeyType()));

    RequireSuccess(builder.Graph.SetVariableDefault(key, 5));
    CHECK(TypeOf(*builder.Graph.FindVariable(key)) == KeyType());

    RequireSuccess(builder.Graph.SetVariableDefault(key, 1.0f));
    CHECK(TypeOf(*builder.Graph.FindVariable(key)) == PinType::Of(ScriptFieldType::Float));
    CHECK(builder.Graph.FindVariable(key)->Enum.empty());
}

TEST_CASE("Switching to another enum keeps links to Int inputs only", "[scripting][graph][enums]")
{
    GraphBuilder builder;
    const UUID source = builder.Variable("Source", 0);
    const UUID sameEnum = builder.Variable("SameEnum", 0);
    const UUID plain = builder.Variable("Plain", 0);
    RequireSuccess(builder.Graph.SetVariableType(source, KeyType()));
    RequireSuccess(builder.Graph.SetVariableType(sameEnum, KeyType()));

    const UUID get = builder.Add(GetVariableNode{ source });
    const UUID setSame = builder.Add(SetVariableNode{ sameEnum });
    const UUID setPlain = builder.Add(SetVariableNode{ plain });
    builder.Link(get, GraphPin::Value, setSame, GraphPin::Value);
    builder.Link(get, GraphPin::Value, setPlain, GraphPin::Value);

    RequireSuccess(builder.Graph.SetVariableType(source, OtherEnumType()));

    const auto linkedTo = [&](const UUID node)
    {
        return std::ranges::any_of(builder.Graph.GetLinks(), [&](const GraphLink& link) { return link.To.Node == node; });
    };
    CHECK_FALSE(linkedTo(setSame));
    CHECK(linkedTo(setPlain));
}

TEST_CASE("An enum literal outputs its enum", "[scripting][graph][enums]")
{
    GraphBuilder builder;
    const UUID literal = builder.Add(LiteralNode{ .Value = DefaultValueFor(KeyType()), .Enum = "Key" });

    CHECK(ValueType(builder.Graph, literal, PinDirection::Output) == KeyType());
    CHECK(DescribeNode(builder.Graph, *builder.Graph.FindNode(literal)).Title == "Key");
}

TEST_CASE("An enum the script API does not have is an error on the nodes that use it", "[scripting][graph][enums]")
{
    GraphBuilder builder;
    const UUID variable = builder.Variable("Gone", 0);
    builder.Add(GetVariableNode{ variable });

    ScriptGraph graph = builder.Graph;
    nlohmann::json json = SerializeScriptGraph(graph);
    json["variables"][0]["enum"] = "NoSuchEnum";

    const Result<ScriptGraph> loaded = DeserializeScriptGraph(json);
    RequireSuccess(loaded);
    CHECK(HasError(*loaded));
}

TEST_CASE("Enum types survive saving a graph", "[scripting][graph][enums][serializer]")
{
    GraphBuilder builder;
    const UUID variable = builder.Variable("Key", 0);
    RequireSuccess(builder.Graph.SetVariableType(variable, KeyType()));

    const UUID literal = builder.Add(LiteralNode{ .Value = DefaultValueFor(KeyType()), .Enum = "Key" });

    const Result<UUID> event = builder.Graph.AddEvent("Pressed");
    RequireSuccess(event);
    GraphCustomEvent declaration = *builder.Graph.FindEvent(*event);
    declaration.Parameters.push_back({ .Id = UUID(), .Name = "Which", .Default = DefaultValueFor(KeyType()),
                                       .Enum = "Key" });
    RequireSuccess(builder.Graph.UpdateEvent(declaration));

    const Result<ScriptGraph> loaded = DeserializeScriptGraph(SerializeScriptGraph(builder.Graph));
    RequireSuccess(loaded);

    CHECK(TypeOf(*loaded->FindVariable(variable)) == KeyType());
    CHECK(ValueType(*loaded, literal, PinDirection::Output) == KeyType());
    CHECK(TypeOf(loaded->FindEvent(*event)->Parameters.front()) == KeyType());
}

TEST_CASE("An exposed enum variable tells the Inspector its enum", "[scripting][graph][enums]")
{
    GraphFixture fixture;
    GraphBuilder builder;
    const UUID variable = builder.Variable("Key", 0);
    RequireSuccess(builder.Graph.SetVariableType(variable, KeyType()));

    const UUID asset = fixture.Import(builder.Graph, "EnumField.bfgraph");
    ScriptBackend* backend = fixture.Engine.FindBackendByName("Graph");
    REQUIRE(backend != nullptr);

    const auto fields = backend->GetFields(ScriptGraphClassName(asset));
    REQUIRE(fields.size() == 1);
    CHECK(fields.front().Enum == "Key");
}

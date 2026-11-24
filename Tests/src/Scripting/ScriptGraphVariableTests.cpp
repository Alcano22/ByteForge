#include "TestUtilities.h"

#include <Engine/Scripting/Visual/GraphSchema.h>
#include <Engine/Scripting/Visual/ScriptGraph.h>

#include <catch2/catch_test_macros.hpp>

#include <glm/glm.hpp>

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace ByteForge;

namespace
{
    UUID AddVariable(ScriptGraph& graph, std::string name, ScriptValue value)
    {
        const Result<UUID> id = graph.AddVariable(std::move(name), std::move(value));
        Tests::RequireSuccess(id);
        return *id;
    }

    void Link(ScriptGraph& graph, const UUID from, const std::string_view fromPin,
              const UUID to, const std::string_view toPin)
    {
        Tests::RequireSuccess(graph.Connect({ from, std::string(fromPin) }, { to, std::string(toPin) }));
    }

    bool HasLink(const ScriptGraph& graph, const UUID from, const UUID to)
    {
        return std::ranges::any_of(graph.GetLinks(), [&](const GraphLink& link)
        {
            return link.From.Node == from && link.To.Node == to;
        });
    }

    bool Contains(const std::vector<PinType>& types, const PinType& type)
    {
        return std::ranges::find(types, type) != types.end();
    }
}

TEST_CASE("Changing a variable's type removes only the links that no longer fit", "[scripting][graph][variables]")
{
    ScriptGraph graph;
    const UUID count = AddVariable(graph, "Count", 0);

    const UUID get = graph.AddNode(GetVariableNode{ count }, glm::vec2(0.0f));
    const UUID add = graph.AddNode(CallNode{ "Math.AddInt" }, glm::vec2(0.0f));
    const UUID literal = graph.AddNode(LiteralNode{ 3 }, glm::vec2(0.0f));
    const UUID set = graph.AddNode(SetVariableNode{ count }, glm::vec2(0.0f));
    const UUID event = graph.AddNode(EventNode{ GraphEvent::OnCreate }, glm::vec2(0.0f));

    Link(graph, get, GraphPin::Value, add, "A");
    Link(graph, literal, GraphPin::Value, set, GraphPin::Value);
    Link(graph, event, GraphPin::Then, set, GraphPin::Exec);

    Tests::RequireSuccess(graph.SetVariableDefault(count, 0.0f));

    CHECK_FALSE(HasLink(graph, get, add));
    CHECK(HasLink(graph, literal, set));
    CHECK(HasLink(graph, event, set));

    CHECK(ValidateGraph(graph).empty());
}

TEST_CASE("Changing a variable's type drops stored Set values that no longer fit", "[scripting][graph][variables]")
{
    ScriptGraph graph;
    const UUID count = AddVariable(graph, "Count", 0);
    const UUID set = graph.AddNode(SetVariableNode{ count }, glm::vec2(0.0f));
    graph.FindNode(set)->Defaults.insert_or_assign(std::string(GraphPin::Value), ScriptValue(5));

    SECTION("an Int value is kept for a Float variable")
    {
        Tests::RequireSuccess(graph.SetVariableDefault(count, 0.0f));
        CHECK(graph.FindNode(set)->Defaults.contains(GraphPin::Value));
    }

    SECTION("an Int value is dropped for a Bool variable")
    {
        Tests::RequireSuccess(graph.SetVariableDefault(count, false));
        CHECK_FALSE(graph.FindNode(set)->Defaults.contains(GraphPin::Value));
    }
}

TEST_CASE("A new default of the same type keeps every link", "[scripting][graph][variables]")
{
    ScriptGraph graph;
    const UUID count = AddVariable(graph, "Count", 0);
    const UUID get = graph.AddNode(GetVariableNode{ count }, glm::vec2(0.0f));
    const UUID add = graph.AddNode(CallNode{ "Math.AddInt" }, glm::vec2(0.0f));
    Link(graph, get, GraphPin::Value, add, "A");

    Tests::RequireSuccess(graph.SetVariableDefault(count, 42));

    CHECK(HasLink(graph, get, add));
    CHECK(graph.FindVariable(count)->Default == ScriptValue(42));
}

TEST_CASE("Variable setters reject unknown variables", "[scripting][graph][variables]")
{
    ScriptGraph graph;
    const UUID count = AddVariable(graph, "Count", 0);

    Tests::RequireSuccess(graph.SetVariableExposed(count, false));
    CHECK_FALSE(graph.FindVariable(count)->Exposed);

    CHECK_FALSE(graph.SetVariableExposed(UUID(12345), true).has_value());
    CHECK_FALSE(graph.SetVariableDefault(UUID(12345), 1.0f).has_value());
}

TEST_CASE("Literal and variable types follow the script API", "[scripting][graph][variables]")
{
    const std::vector<PinType> literals = GetLiteralTypes();
    const std::vector<PinType> variables = GetVariableTypes();

    CHECK(Contains(literals, PinType::Of(ScriptFieldType::Asset, AssetType::Prefab)));
    CHECK(Contains(literals, PinType::Of(ScriptFieldType::Asset, AssetType::Scene)));
    CHECK(Contains(literals, PinType::Of(ScriptFieldType::Float)));

    CHECK_FALSE(Contains(literals, PinType::Of(ScriptFieldType::Entity)));
    CHECK(Contains(variables, PinType::Of(ScriptFieldType::Entity)));

    CHECK(GetValueTypeName(PinType::Of(ScriptFieldType::Asset, AssetType::Prefab)) == "Prefab");
    CHECK(GetValueTypeName(PinType::Of(ScriptFieldType::Float)) == "Float");
}

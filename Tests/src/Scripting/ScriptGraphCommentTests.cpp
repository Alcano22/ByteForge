#include "Scripting/GraphTestHelpers.h"
#include "TestUtilities.h"

#include <Engine/Scripting/ScriptBackend.h>
#include <Engine/Scripting/Visual/GraphSchema.h>
#include <Engine/Scripting/Visual/ScriptGraph.h>
#include <Engine/Scripting/Visual/ScriptGraphSerializer.h>

#include <catch2/catch_test_macros.hpp>

#include <glm/glm.hpp>

#include <string>
#include <variant>

using namespace ByteForge;
using namespace ByteForge::Tests;

TEST_CASE("A comment keeps its text and size through JSON", "[scripting][graph][comment][serializer]")
{
    GraphBuilder builder;
    const UUID comment = builder.Add(CommentNode{ .Text = "Movement", .Size = glm::vec2(640.0f, 320.0f) });

    const Result<ScriptGraph> loaded = DeserializeScriptGraph(SerializeScriptGraph(builder.Graph));
    RequireSuccess(loaded);

    const GraphNode* node = loaded->FindNode(comment);
    REQUIRE(node != nullptr);
    REQUIRE(std::holds_alternative<CommentNode>(node->Data));

    const auto& data = std::get<CommentNode>(node->Data);
    CHECK(data.Text == "Movement");
    CHECK(data.Size == glm::vec2(640.0f, 320.0f));
}

TEST_CASE("A comment without a stored size gets the default frame", "[scripting][graph][comment][serializer]")
{
    nlohmann::json json = SerializeScriptGraph(GraphBuilder{}.Graph);
    json["nodes"].push_back({
        { "id",       42 },
        { "kind",     "Comment" },
        { "text",     "Old" },
        { "position", { 0.0f, 0.0f } }
    });

    const Result<ScriptGraph> loaded = DeserializeScriptGraph(json);
    RequireSuccess(loaded);

    const GraphNode* node = loaded->FindNode(UUID(42));
    REQUIRE(node != nullptr);
    CHECK(std::get<CommentNode>(node->Data).Size == CommentNode{}.Size);
}

TEST_CASE("A comment has no pins and does not change what the graph does", "[scripting][graph][comment]")
{
    GraphFixture fixture;
    GraphBuilder builder;

    const UUID counter = builder.Variable("Counter", 0);
    builder.AppendIncrement(counter, builder.Add(EventNode{ GraphEvent::OnCreate }), GraphPin::Then);
    const UUID comment = builder.Add(CommentNode{});

    CHECK(DescribeNode(builder.Graph, *builder.Graph.FindNode(comment)).Pins.empty());
    CHECK_FALSE(HasError(builder.Graph));

    const Scope<ScriptInstance> instance = fixture.Instantiate(fixture.Import(builder.Graph, "Commented.bfgraph"));
    instance->OnCreate();
    CHECK(Field<i32>(*instance, "Counter") == 1);
}

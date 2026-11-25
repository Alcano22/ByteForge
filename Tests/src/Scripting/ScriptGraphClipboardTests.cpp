#include "Scripting/GraphTestHelpers.h"
#include "TestUtilities.h"

#include <Engine/Scripting/ScriptBackend.h>
#include <Engine/Scripting/Visual/GraphClipboard.h>
#include <Engine/Scripting/Visual/GraphSchema.h>
#include <Engine/Scripting/Visual/ScriptGraph.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <glm/glm.hpp>

#include <algorithm>
#include <ranges>
#include <vector>

using namespace ByteForge;
using namespace ByteForge::Tests;
using Catch::Matchers::ContainsSubstring;

namespace
{
    std::vector<UUID> AllNodes(const ScriptGraph& graph)
    {
        std::vector<UUID> nodes;
        for (const GraphNode& node : graph.GetNodes())
            nodes.push_back(node.Id);
        return nodes;
    }

    const GraphNode* FindNodeOfKind(const ScriptGraph& graph, const std::vector<UUID>& among, auto predicate)
    {
        for (const UUID id : among)
        {
            const GraphNode* node = graph.FindNode(id);
            if (node != nullptr && predicate(*node))
                return node;
        }
        return nullptr;
    }

    struct IncrementChain
    {
        GraphBuilder Builder;
        UUID Counter{ 0 };
        UUID Get{ 0 };
        UUID Add{ 0 };
        UUID Set{ 0 };

        IncrementChain()
        {
            Counter = Builder.Variable("Counter", 0);
            Get = Builder.Add(GetVariableNode{ Counter });
            Add = Builder.Add(CallNode{ "Math.AddInt" });
            Set = Builder.Add(SetVariableNode{ Counter });

            Builder.SetDefault(Add, "B", 1);
            Builder.Link(Get, GraphPin::Value, Add, "A");
            Builder.Link(Add, "Result", Set, GraphPin::Value);

            Builder.Graph.FindNode(Get)->Position = glm::vec2(100.0f, 50.0f);
            Builder.Graph.FindNode(Add)->Position = glm::vec2(250.0f, 50.0f);
            Builder.Graph.FindNode(Set)->Position = glm::vec2(400.0f, 80.0f);
        }
    };
}

TEST_CASE("Pasted nodes get new ids and keep their links, values and layout", "[scripting][graph][clipboard]")
{
    IncrementChain chain;
    ScriptGraph& graph = chain.Builder.Graph;

    const std::string text = CopyGraphNodes(graph, std::vector{ chain.Get, chain.Add, chain.Set });
    const Result<PastedGraphNodes> pasted = PasteGraphNodes(graph, text, glm::vec2(1000.0f, 500.0f));
    RequireSuccess(pasted);

    REQUIRE(pasted->Nodes.size() == 3);
    CHECK(graph.GetNodes().size() == 6);
    CHECK(graph.GetLinks().size() == 4);
    for (const UUID id : pasted->Nodes)
        CHECK((id != chain.Get && id != chain.Add && id != chain.Set));

    CHECK(graph.GetVariables().size() == 1);

    const GraphNode* add = FindNodeOfKind(graph, pasted->Nodes, [](const GraphNode& node)
    {
        return std::holds_alternative<CallNode>(node.Data);
    });
    REQUIRE(add != nullptr);
    CHECK(add->Defaults.at("B") == ScriptValue(1));

    CHECK(add->Position == glm::vec2(1150.0f, 500.0f));

    const GraphNode* set = FindNodeOfKind(graph, pasted->Nodes, [](const GraphNode& node)
    {
        return std::holds_alternative<SetVariableNode>(node.Data);
    });
    REQUIRE(set != nullptr);
    CHECK(set->Position == glm::vec2(1300.0f, 530.0f));
    CHECK(std::get<SetVariableNode>(set->Data).Variable == chain.Counter);

    const bool linked = std::ranges::any_of(graph.GetLinks(), [&](const GraphLink& link)
    {
        return link.From.Node == add->Id && link.To.Node == set->Id;
    });
    CHECK(linked);
    CHECK_FALSE(HasError(graph));
}

TEST_CASE("Links to nodes outside the copied ones are left out", "[scripting][graph][clipboard]")
{
    IncrementChain chain;
    ScriptGraph& graph = chain.Builder.Graph;

    const std::string text = CopyGraphNodes(graph, std::vector{ chain.Add, chain.Set });
    const Result<PastedGraphNodes> pasted = PasteGraphNodes(graph, text, glm::vec2(0.0f));
    RequireSuccess(pasted);

    REQUIRE(pasted->Nodes.size() == 2);
    CHECK(graph.GetLinks().size() == 3);
}

TEST_CASE("Variables are matched or added when pasting into another graph", "[scripting][graph][clipboard]")
{
    IncrementChain chain;
    const std::string text = CopyGraphNodes(chain.Builder.Graph, AllNodes(chain.Builder.Graph));

    GraphBuilder other;

    SECTION("a missing variable is added with its name, type and default")
    {
        const Result<PastedGraphNodes> pasted = PasteGraphNodes(other.Graph, text, glm::vec2(0.0f));
        RequireSuccess(pasted);

        const GraphVariable* counter = other.Graph.FindVariable("Counter");
        REQUIRE(counter != nullptr);
        CHECK(counter->Default == ScriptValue(0));
        CHECK_FALSE(HasError(other.Graph));

        RequireSuccess(PasteGraphNodes(other.Graph, text, glm::vec2(0.0f)));
        CHECK(other.Graph.GetVariables().size() == 1);
    }

    SECTION("a variable of the same name but another type is not reused")
    {
        const UUID existing = other.Variable("Counter", std::string("not a number"));

        const Result<PastedGraphNodes> pasted = PasteGraphNodes(other.Graph, text, glm::vec2(0.0f));
        RequireSuccess(pasted);

        const GraphVariable* added = other.Graph.FindVariable("Counter 2");
        REQUIRE(added != nullptr);
        CHECK(added->Id != existing);
        CHECK(added->Default == ScriptValue(0));
        CHECK_FALSE(HasError(other.Graph));
    }
}

TEST_CASE("An event the graph already has is not pasted", "[scripting][graph][clipboard]")
{
    GraphBuilder builder;
    const UUID event = builder.Add(EventNode{ GraphEvent::OnCreate });
    const UUID branch = builder.Add(BranchNode{});
    builder.Link(event, GraphPin::Then, branch, GraphPin::Exec);

    const std::string text = CopyGraphNodes(builder.Graph, std::vector{ event, branch });
    const Result<PastedGraphNodes> pasted = PasteGraphNodes(builder.Graph, text, glm::vec2(0.0f));
    RequireSuccess(pasted);

    CHECK(pasted->SkippedEvents == 1);
    REQUIRE(pasted->Nodes.size() == 1);
    CHECK(std::holds_alternative<BranchNode>(builder.Graph.FindNode(pasted->Nodes.front())->Data));
    CHECK_FALSE(HasError(builder.Graph));
}

TEST_CASE("Text that was not copied from a graph is refused", "[scripting][graph][clipboard]")
{
    GraphBuilder builder;
    builder.Add(BranchNode{});

    for (const std::string_view text : { "", "hello", "{}", "[1, 2]", R"({"byteforgeGraphNodes": "x"})" })
    {
        const Result<PastedGraphNodes> pasted = PasteGraphNodes(builder.Graph, text, glm::vec2(0.0f));
        REQUIRE_FALSE(pasted.has_value());
        CHECK_THAT(pasted.error(), ContainsSubstring("no script graph nodes"));
    }

    CHECK(builder.Graph.GetNodes().size() == 1);
}

TEST_CASE("A chain pasted into an empty graph runs", "[scripting][graph][clipboard]")
{
    GraphFixture fixture;

    IncrementChain chain;
    const UUID event = chain.Builder.Add(EventNode{ GraphEvent::OnCreate });
    chain.Builder.Link(event, GraphPin::Then, chain.Set, GraphPin::Exec);

    GraphBuilder target;
    RequireSuccess(PasteGraphNodes(target.Graph, CopyGraphNodes(chain.Builder.Graph, AllNodes(chain.Builder.Graph)),
                                   glm::vec2(0.0f)));

    const Scope<ScriptInstance> instance = fixture.Instantiate(fixture.Import(target.Graph, "Pasted.bfgraph"));
    instance->OnCreate();
    CHECK(Field<i32>(*instance, "Counter") == 1);
}

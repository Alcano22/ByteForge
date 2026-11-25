#include "Scripting/GraphTestHelpers.h"
#include "TestUtilities.h"

#include <Engine/Scripting/ScriptBackend.h>
#include <Engine/Scripting/ScriptEngine.h>
#include <Engine/Scripting/API/ScriptCall.h>
#include <Engine/Scripting/Visual/GraphDebugger.h>
#include <Engine/Scripting/Visual/GraphSchema.h>
#include <Engine/Scripting/Visual/ScriptGraph.h>

#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <vector>

using namespace ByteForge;
using namespace ByteForge::Tests;

namespace
{
    GraphLink LinkFrom(const ScriptGraph& graph, const UUID node, const std::string_view pin)
    {
        for (const GraphLink& link : graph.GetLinks())
        {
            if (link.From.Node == node && link.From.Pin == pin)
                return link;
        }

        FAIL("No link from that pin");
        return {};
    }

    bool SameLinks(const std::vector<GraphLink>& actual, const std::vector<GraphLink>& expected)
    {
        if (actual.size() != expected.size())
            return false;

        for (usize i = 0; i < actual.size(); ++i)
        {
            if (actual[i].From != expected[i].From || actual[i].To != expected[i].To)
                return false;
        }
        return true;
    }

    struct BranchingGraph
    {
        GraphBuilder Builder;
        UUID Event{ 0 };
        UUID Branch{ 0 };
        UUID Sequence{ 0 };

        BranchingGraph()
        {
            const UUID flag = Builder.Variable("Flag", false);
            const UUID hits = Builder.Variable("Hits", 0);
            const UUID a = Builder.Variable("A", 0);
            const UUID b = Builder.Variable("B", 0);

            Event = Builder.Add(EventNode{ GraphEvent::OnCreate });
            Branch = Builder.Add(BranchNode{});
            Sequence = Builder.Add(SequenceNode{});

            Builder.Link(Event, GraphPin::Then, Branch, GraphPin::Exec);
            Builder.Link(Builder.Add(GetVariableNode{ flag }), GraphPin::Value, Branch, GraphPin::Condition);
            Builder.AppendIncrement(hits, Branch, GraphPin::True);
            Builder.Link(Branch, GraphPin::False, Sequence, GraphPin::Exec);
            Builder.AppendIncrement(a, Sequence, SequencePinName(0));
            Builder.AppendIncrement(b, Sequence, SequencePinName(1));
        }
    };
}

TEST_CASE("A watched graph reports the exec links it ran through, in order", "[scripting][graph][debug]")
{
    GraphFixture fixture;
    BranchingGraph graph;
    const ScriptGraph& built = graph.Builder.Graph;

    const UUID asset = fixture.Import(built, "Traced.bfgraph");
    const Scope<ScriptInstance> instance = fixture.Instantiate(asset);

    GraphDebugger::Watch(asset);
    instance->OnCreate();

    const std::vector<GraphLink> expected{
        LinkFrom(built, graph.Event, GraphPin::Then),
        LinkFrom(built, graph.Branch, GraphPin::False),
        LinkFrom(built, graph.Sequence, SequencePinName(0)),
        LinkFrom(built, graph.Sequence, SequencePinName(1))
    };
    CHECK(SameLinks(GraphDebugger::TakeLinks(), expected));
    CHECK(GraphDebugger::TakeLinks().empty());
}

TEST_CASE("Only the watched graph is traced", "[scripting][graph][debug]")
{
    GraphFixture fixture;
    BranchingGraph graph;

    const UUID asset = fixture.Import(graph.Builder.Graph, "Untraced.bfgraph");
    const Scope<ScriptInstance> instance = fixture.Instantiate(asset);

    SECTION("nothing is reported while no graph is watched")
    {
        instance->OnCreate();
        CHECK(GraphDebugger::TakeLinks().empty());
    }

    SECTION("watching another graph drops the links not yet taken")
    {
        GraphDebugger::Watch(asset);
        instance->OnCreate();

        GraphDebugger::Watch(UUID(0));
        CHECK(GraphDebugger::TakeLinks().empty());
    }
}

TEST_CASE("A link taken every frame is reported once until the links are taken", "[scripting][graph][debug]")
{
    GraphFixture fixture;
    GraphBuilder builder;

    const UUID counter = builder.Variable("Counter", 0);
    const UUID update = builder.Add(EventNode{ GraphEvent::OnUpdate });
    builder.AppendIncrement(counter, update, GraphPin::Then);

    const UUID asset = fixture.Import(builder.Graph, "EveryFrame.bfgraph");
    const Scope<ScriptInstance> instance = fixture.Instantiate(asset);
    GraphDebugger::Watch(asset);

    instance->OnUpdate(0.016f);
    instance->OnUpdate(0.016f);
    instance->OnUpdate(0.016f);
    CHECK(GraphDebugger::TakeLinks().size() == 1);

    instance->OnUpdate(0.016f);
    CHECK(GraphDebugger::TakeLinks().size() == 1);
    CHECK(Field<i32>(*instance, "Counter") == 4);
}

TEST_CASE("A runtime error names the node and entity it came from", "[scripting][graph][debug]")
{
    GraphFixture fixture;
    GraphBuilder builder;

    const UUID result = builder.Variable("Result", 0);

    const UUID divide = builder.Add(CallNode{ "Math.DivideInt" });
    builder.SetDefault(divide, "A", 1);
    builder.SetDefault(divide, "B", 0);

    const UUID set = builder.Add(SetVariableNode{ result });
    builder.Link(divide, "Result", set, GraphPin::Value);
    builder.Link(builder.Add(EventNode{ GraphEvent::OnUpdate }), GraphPin::Then, set, GraphPin::Exec);

    const UUID asset = fixture.Import(builder.Graph, "Failing.bfgraph");
    const Scope<ScriptInstance> instance = fixture.Instantiate(asset);

    CHECK_THROWS_AS(instance->OnUpdate(0.016f), ScriptError);

    const std::vector<GraphRuntimeError> errors = GraphDebugger::GetErrors(asset);
    REQUIRE(errors.size() == 1);
    CHECK(errors.front().Node == divide);
    CHECK(errors.front().Entity == fixture.Owner.GetUUID());
    CHECK_FALSE(errors.front().Message.empty());

    SECTION("a node failing again keeps one entry")
    {
        CHECK_THROWS_AS(instance->OnUpdate(0.016f), ScriptError);
        CHECK(GraphDebugger::GetErrors(asset).size() == 1);
    }

    SECTION("the errors are cleared when the game starts again")
    {
        fixture.Engine.OnRuntimeStart(fixture.World);
        CHECK(GraphDebugger::GetErrors(asset).empty());
    }
}

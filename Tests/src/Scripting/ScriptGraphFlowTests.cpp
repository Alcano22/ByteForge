#include "Scripting/GraphTestHelpers.h"
#include "TestUtilities.h"

#include <Engine/Scripting/ScriptBackend.h>
#include <Engine/Scripting/Visual/GraphSchema.h>
#include <Engine/Scripting/Visual/ScriptGraph.h>
#include <Engine/Scripting/Visual/ScriptGraphSerializer.h>

#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <string>
#include <variant>

using namespace ByteForge;
using namespace ByteForge::Tests;

namespace
{
    void AppendCopy(GraphBuilder& builder, const UUID source, const UUID target, const UUID after,
                    const std::string_view pin)
    {
        const UUID get = builder.Add(GetVariableNode{ source });
        const UUID set = builder.Add(SetVariableNode{ target });
        builder.Link(get, GraphPin::Value, set, GraphPin::Value);
        builder.Link(after, pin, set, GraphPin::Exec);
    }

    UUID AddEvent(GraphBuilder& builder, const GraphEvent event)
    {
        return builder.Add(EventNode{ event });
    }
}

TEST_CASE("A Sequence runs its outputs in order", "[scripting][graph][flow]")
{
    GraphFixture fixture;
    GraphBuilder builder;

    const UUID first = builder.Variable("First", 0);
    const UUID second = builder.Variable("Second", 0);
    const UUID third = builder.Variable("Third", 0);

    const UUID sequence = builder.Add(SequenceNode{ 3 });
    builder.Link(AddEvent(builder, GraphEvent::OnCreate), GraphPin::Then, sequence, GraphPin::Exec);

    builder.AppendIncrement(first, sequence, SequencePinName(0));
    AppendCopy(builder, first, second, sequence, SequencePinName(1));
    AppendCopy(builder, second, third, sequence, SequencePinName(2));

    const Scope<ScriptInstance> instance = fixture.Instantiate(fixture.Import(builder.Graph, "Sequence.bfgraph"));
    instance->OnCreate();

    CHECK(Field<i32>(*instance, "First") == 1);
    CHECK(Field<i32>(*instance, "Second") == 1);
    CHECK(Field<i32>(*instance, "Third") == 1);
}

TEST_CASE("A Delay in a Sequence output does not hold back the next output", "[scripting][graph][flow]")
{
    GraphFixture fixture;
    GraphBuilder builder;

    const UUID delayed = builder.Variable("Delayed", 0);
    const UUID direct = builder.Variable("Direct", 0);

    const UUID sequence = builder.Add(SequenceNode{});
    builder.Link(AddEvent(builder, GraphEvent::OnCreate), GraphPin::Then, sequence, GraphPin::Exec);

    const UUID delay = builder.Add(DelayNode{});
    builder.SetDefault(delay, GraphPin::Duration, 1.0f);
    builder.Link(sequence, SequencePinName(0), delay, GraphPin::Exec);
    builder.AppendIncrement(delayed, delay, GraphPin::Then);

    builder.AppendIncrement(direct, sequence, SequencePinName(1));

    const Scope<ScriptInstance> instance = fixture.Instantiate(fixture.Import(builder.Graph, "SequenceDelay.bfgraph"));
    instance->OnCreate();

    CHECK(Field<i32>(*instance, "Direct") == 1);
    CHECK(Field<i32>(*instance, "Delayed") == 0);

    instance->OnUpdate(1.5f);
    CHECK(Field<i32>(*instance, "Delayed") == 1);
    CHECK(Field<i32>(*instance, "Direct") == 1);
}

TEST_CASE("A For Loop runs its body for every index, then Completed", "[scripting][graph][flow]")
{
    GraphFixture fixture;
    GraphBuilder builder;

    const UUID sum = builder.Variable("Sum", 0);
    const UUID rounds = builder.Variable("Rounds", 0);
    const UUID finished = builder.Variable("Finished", -1);

    const UUID loop = builder.Add(ForLoopNode{});
    builder.Link(AddEvent(builder, GraphEvent::OnCreate), GraphPin::Then, loop, GraphPin::Exec);

    const UUID getSum = builder.Add(GetVariableNode{ sum });
    const UUID add = builder.Add(CallNode{ "Math.AddInt" });
    const UUID setSum = builder.Add(SetVariableNode{ sum });
    builder.Link(getSum, GraphPin::Value, add, "A");
    builder.Link(loop, GraphPin::Index, add, "B");
    builder.Link(add, "Result", setSum, GraphPin::Value);
    builder.Link(loop, GraphPin::LoopBody, setSum, GraphPin::Exec);
    builder.AppendIncrement(rounds, setSum, GraphPin::Then);

    AppendCopy(builder, sum, finished, loop, GraphPin::Completed);

    SECTION("from First to Last, both included")
    {
        builder.SetDefault(loop, GraphPin::First, 1);
        builder.SetDefault(loop, GraphPin::Last, 4);

        const Scope<ScriptInstance> instance = fixture.Instantiate(fixture.Import(builder.Graph, "Loop.bfgraph"));
        instance->OnCreate();

        CHECK(Field<i32>(*instance, "Sum") == 10);
        CHECK(Field<i32>(*instance, "Rounds") == 4);
        CHECK(Field<i32>(*instance, "Finished") == 10);
    }

    SECTION("not at all when First is past Last")
    {
        builder.SetDefault(loop, GraphPin::First, 5);
        builder.SetDefault(loop, GraphPin::Last, 4);

        const Scope<ScriptInstance> instance = fixture.Instantiate(fixture.Import(builder.Graph, "EmptyLoop.bfgraph"));
        instance->OnCreate();

        CHECK(Field<i32>(*instance, "Rounds") == 0);
        CHECK(Field<i32>(*instance, "Finished") == 0);
    }
}

TEST_CASE("A For Loop can run up to the largest Int without overflowing its index", "[scripting][graph][flow]")
{
    GraphFixture fixture;
    GraphBuilder builder;

    const UUID rounds = builder.Variable("Rounds", 0);

    const UUID loop = builder.Add(ForLoopNode{});
    builder.SetDefault(loop, GraphPin::First, std::numeric_limits<i32>::max() - 1);
    builder.SetDefault(loop, GraphPin::Last, std::numeric_limits<i32>::max());
    builder.Link(AddEvent(builder, GraphEvent::OnCreate), GraphPin::Then, loop, GraphPin::Exec);
    builder.AppendIncrement(rounds, loop, GraphPin::LoopBody);

    const Scope<ScriptInstance> instance = fixture.Instantiate(fixture.Import(builder.Graph, "MaxLoop.bfgraph"));
    instance->OnCreate();

    CHECK(Field<i32>(*instance, "Rounds") == 2);
}

TEST_CASE("Do Once lets one Exec through until it is reset", "[scripting][graph][flow]")
{
    GraphFixture fixture;
    GraphBuilder builder;

    const UUID counter = builder.Variable("Counter", 0);

    const UUID once = builder.Add(DoOnceNode{});
    builder.Link(AddEvent(builder, GraphEvent::OnUpdate), GraphPin::Then, once, GraphPin::Exec);
    builder.Link(AddEvent(builder, GraphEvent::OnClick), GraphPin::Then, once, GraphPin::Reset);
    builder.AppendIncrement(counter, once, GraphPin::Completed);

    const Scope<ScriptInstance> instance = fixture.Instantiate(fixture.Import(builder.Graph, "DoOnce.bfgraph"));

    instance->OnUpdate(0.016f);
    instance->OnUpdate(0.016f);
    instance->OnUpdate(0.016f);
    CHECK(Field<i32>(*instance, "Counter") == 1);

    instance->OnUIEvent(UIEvent::Click);
    instance->OnUpdate(0.016f);
    instance->OnUpdate(0.016f);
    CHECK(Field<i32>(*instance, "Counter") == 2);
}

TEST_CASE("Flip Flop alternates between A and B, starting with A", "[scripting][graph][flow]")
{
    GraphFixture fixture;
    GraphBuilder builder;

    const UUID countA = builder.Variable("CountA", 0);
    const UUID countB = builder.Variable("CountB", 0);
    const UUID lastWasA = builder.Variable("LastWasA", false);

    const UUID flipFlop = builder.Add(FlipFlopNode{});
    builder.Link(AddEvent(builder, GraphEvent::OnUpdate), GraphPin::Then, flipFlop, GraphPin::Exec);
    builder.AppendIncrement(countA, flipFlop, GraphPin::A);
    builder.AppendIncrement(countB, flipFlop, GraphPin::B);

    const UUID record = builder.Add(SetVariableNode{ lastWasA });
    builder.Link(flipFlop, GraphPin::IsA, record, GraphPin::Value);
    builder.Link(AddEvent(builder, GraphEvent::OnClick), GraphPin::Then, record, GraphPin::Exec);

    const Scope<ScriptInstance> instance = fixture.Instantiate(fixture.Import(builder.Graph, "FlipFlop.bfgraph"));

    instance->OnUpdate(0.016f);
    instance->OnUIEvent(UIEvent::Click);
    CHECK(Field<i32>(*instance, "CountA") == 1);
    CHECK(Field<i32>(*instance, "CountB") == 0);
    CHECK(Field<bool>(*instance, "LastWasA"));

    instance->OnUpdate(0.016f);
    instance->OnUIEvent(UIEvent::Click);
    CHECK(Field<i32>(*instance, "CountB") == 1);
    CHECK_FALSE(Field<bool>(*instance, "LastWasA"));

    instance->OnUpdate(0.016f);
    CHECK(Field<i32>(*instance, "CountA") == 2);
    CHECK(Field<i32>(*instance, "CountB") == 1);
}

TEST_CASE("A Gate passes Enter on only while it is open", "[scripting][graph][flow]")
{
    GraphFixture fixture;
    GraphBuilder builder;

    const UUID counter = builder.Variable("Counter", 0);

    const UUID gate = builder.Add(GateNode{});
    builder.Link(AddEvent(builder, GraphEvent::OnUpdate), GraphPin::Then, gate, GraphPin::Enter);
    builder.Link(AddEvent(builder, GraphEvent::OnClick), GraphPin::Then, gate, GraphPin::Toggle);
    builder.AppendIncrement(counter, gate, GraphPin::Exit);

    const Scope<ScriptInstance> instance = fixture.Instantiate(fixture.Import(builder.Graph, "Gate.bfgraph"));

    instance->OnUpdate(0.016f);
    CHECK(Field<i32>(*instance, "Counter") == 1);

    instance->OnUIEvent(UIEvent::Click);
    instance->OnUpdate(0.016f);
    CHECK(Field<i32>(*instance, "Counter") == 1);

    instance->OnUIEvent(UIEvent::Click);
    instance->OnUpdate(0.016f);
    CHECK(Field<i32>(*instance, "Counter") == 2);
}

TEST_CASE("Open and Close set a Gate regardless of its state", "[scripting][graph][flow]")
{
    GraphFixture fixture;
    GraphBuilder builder;

    const UUID counter = builder.Variable("Counter", 0);

    const UUID gate = builder.Add(GateNode{});
    const UUID sequence = builder.Add(SequenceNode{});
    builder.Link(AddEvent(builder, GraphEvent::OnCreate), GraphPin::Then, sequence, GraphPin::Exec);
    builder.Link(sequence, SequencePinName(0), gate, GraphPin::Close);
    builder.Link(sequence, SequencePinName(1), gate, GraphPin::Close);
    builder.Link(AddEvent(builder, GraphEvent::OnClick), GraphPin::Then, gate, GraphPin::Open);
    builder.Link(AddEvent(builder, GraphEvent::OnUpdate), GraphPin::Then, gate, GraphPin::Enter);
    builder.AppendIncrement(counter, gate, GraphPin::Exit);

    const Scope<ScriptInstance> instance = fixture.Instantiate(fixture.Import(builder.Graph, "GateOpenClose.bfgraph"));

    instance->OnCreate();
    instance->OnUpdate(0.016f);
    CHECK(Field<i32>(*instance, "Counter") == 0);

    instance->OnUIEvent(UIEvent::Click);
    instance->OnUIEvent(UIEvent::Click);
    instance->OnUpdate(0.016f);
    CHECK(Field<i32>(*instance, "Counter") == 1);
}

TEST_CASE("A Sequence keeps its output count through JSON", "[scripting][graph][flow][serializer]")
{
    GraphBuilder builder;
    const UUID sequence = builder.Add(SequenceNode{ 5 });

    const Result<ScriptGraph> loaded = DeserializeScriptGraph(SerializeScriptGraph(builder.Graph));
    RequireSuccess(loaded);

    const GraphNode* node = loaded->FindNode(sequence);
    REQUIRE(node != nullptr);
    REQUIRE(std::holds_alternative<SequenceNode>(node->Data));
    CHECK(std::get<SequenceNode>(node->Data).Outputs == 5);

    const NodeSignature signature = DescribeNode(*loaded, *node);
    CHECK(FindPin(signature, "Then 4", PinDirection::Output) != nullptr);
    CHECK(FindPin(signature, "Then 5", PinDirection::Output) == nullptr);
}

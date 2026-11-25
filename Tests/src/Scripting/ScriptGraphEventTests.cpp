#include "Scripting/GraphTestHelpers.h"
#include "TestUtilities.h"

#include <Engine/Scripting/ScriptBackend.h>
#include <Engine/Scripting/Visual/GraphClipboard.h>
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

    UUID DeclareEvent(GraphBuilder& builder, std::string name, std::vector<GraphParameter> parameters = {})
    {
        const Result<UUID> id = builder.Graph.AddEvent(std::move(name));
        RequireSuccess(id);

        GraphCustomEvent event = *builder.Graph.FindEvent(*id);
        event.Parameters = std::move(parameters);
        RequireSuccess(builder.Graph.UpdateEvent(std::move(event)));
        return *id;
    }

    bool HasLinkFrom(const ScriptGraph& graph, const UUID node, const std::string_view pin)
    {
        return std::ranges::any_of(graph.GetLinks(), [&](const GraphLink& link)
        {
            return link.From.Node == node && link.From.Pin == pin;
        });
    }

    struct GiveGraph
    {
        UUID Event{ 0 };
        UUID Entry{ 0 };
        UUID Call{ 0 };
    };

    GiveGraph BuildGiveGraph(GraphBuilder& builder)
    {
        const UUID total = builder.Variable("Total", 0);
        const UUID event = DeclareEvent(builder, "Give", { Parameter("Amount", 0) });

        const UUID entry = builder.Add(CustomEventNode{ event });
        const UUID set = builder.Add(SetVariableNode{ total });
        builder.Link(entry, "Amount", set, GraphPin::Value);
        builder.Link(entry, GraphPin::Then, set, GraphPin::Exec);

        const UUID start = builder.Add(EventNode{ GraphEvent::OnCreate });
        const UUID call = builder.Add(CallEventNode{ event });
        builder.SetDefault(call, "Amount", 7);
        builder.Link(start, GraphPin::Then, call, GraphPin::Exec);

        return { event, entry, call };
    }
}

TEST_CASE("Calling a custom event runs it before the call continues", "[scripting][graph][events]")
{
    GraphFixture fixture;
    GraphBuilder builder;
    const UUID ran = builder.Variable("Ran", 0);
    const UUID seen = builder.Variable("Seen", 0);

    const UUID event = DeclareEvent(builder, "Bump");
    const UUID entry = builder.Add(CustomEventNode{ event });
    builder.AppendIncrement(ran, entry, GraphPin::Then);

    const UUID start = builder.Add(EventNode{ GraphEvent::OnCreate });
    const UUID call = builder.Add(CallEventNode{ event });
    builder.Link(start, GraphPin::Then, call, GraphPin::Exec);

    // Seen copies Ran after the call, so it is only 1 if the event already ran
    const UUID get = builder.Add(GetVariableNode{ ran });
    const UUID set = builder.Add(SetVariableNode{ seen });
    builder.Link(get, GraphPin::Value, set, GraphPin::Value);
    builder.Link(call, GraphPin::Then, set, GraphPin::Exec);

    const Scope<ScriptInstance> instance = fixture.Instantiate(fixture.Import(builder.Graph, "CallEvent.bfgraph"));
    instance->OnCreate();

    CHECK(Field<i32>(*instance, "Ran") == 1);
    CHECK(Field<i32>(*instance, "Seen") == 1);
}

TEST_CASE("A Call node passes its arguments to the event", "[scripting][graph][events]")
{
    GraphFixture fixture;
    GraphBuilder builder;
    BuildGiveGraph(builder);

    const Scope<ScriptInstance> instance = fixture.Instantiate(fixture.Import(builder.Graph, "Give.bfgraph"));
    instance->OnCreate();

    CHECK(Field<i32>(*instance, "Total") == 7);
}

TEST_CASE("Renaming a parameter keeps its links and stored values", "[scripting][graph][events]")
{
    GraphFixture fixture;
    GraphBuilder builder;
    const GiveGraph give = BuildGiveGraph(builder);

    GraphCustomEvent changed = *builder.Graph.FindEvent(give.Event);
    changed.Parameters.front().Name = "Count";
    RequireSuccess(builder.Graph.UpdateEvent(std::move(changed)));

    CHECK(HasLinkFrom(builder.Graph, give.Entry, "Count"));
    CHECK_FALSE(HasLinkFrom(builder.Graph, give.Entry, "Amount"));
    CHECK(builder.Graph.FindNode(give.Call)->Defaults.contains("Count"));

    const Scope<ScriptInstance> instance = fixture.Instantiate(fixture.Import(builder.Graph, "Renamed.bfgraph"));
    instance->OnCreate();
    CHECK(Field<i32>(*instance, "Total") == 7);
}

TEST_CASE("Changing a parameter's type drops its links and stored values", "[scripting][graph][events]")
{
    GraphBuilder builder;
    const GiveGraph give = BuildGiveGraph(builder);

    GraphCustomEvent changed = *builder.Graph.FindEvent(give.Event);
    changed.Parameters.front().Default = std::string();
    RequireSuccess(builder.Graph.UpdateEvent(std::move(changed)));

    CHECK_FALSE(HasLinkFrom(builder.Graph, give.Entry, "Amount"));
    CHECK_FALSE(builder.Graph.FindNode(give.Call)->Defaults.contains("Amount"));
}

TEST_CASE("Parameter names must be unique and cannot take an exec pin's name", "[scripting][graph][events]")
{
    GraphBuilder builder;
    const UUID event = DeclareEvent(builder, "Hit");

    GraphCustomEvent twice = *builder.Graph.FindEvent(event);
    twice.Parameters = { Parameter("Damage", 0), Parameter("Damage", 0.0f) };
    CHECK_FALSE(builder.Graph.UpdateEvent(twice).has_value());

    GraphCustomEvent reserved = *builder.Graph.FindEvent(event);
    reserved.Parameters = { Parameter(std::string(GraphPin::Then), 0) };
    CHECK_FALSE(builder.Graph.UpdateEvent(reserved).has_value());

    CHECK(builder.Graph.FindEvent(event)->Parameters.empty());
}

TEST_CASE("Removing an event removes its nodes", "[scripting][graph][events]")
{
    GraphBuilder builder;
    const GiveGraph give = BuildGiveGraph(builder);

    builder.Graph.RemoveEvent(give.Event);

    CHECK(builder.Graph.FindEvent(give.Event) == nullptr);
    CHECK(builder.Graph.FindNode(give.Entry) == nullptr);
    CHECK(builder.Graph.FindNode(give.Call) == nullptr);
}

TEST_CASE("A Delay in a custom event hands control back to the caller", "[scripting][graph][events]")
{
    GraphFixture fixture;
    GraphBuilder builder;
    const UUID delayed = builder.Variable("Delayed", 0);
    const UUID direct = builder.Variable("Direct", 0);

    const UUID event = DeclareEvent(builder, "Later");
    const UUID entry = builder.Add(CustomEventNode{ event });
    const UUID delay = builder.Add(DelayNode{});
    builder.SetDefault(delay, GraphPin::Duration, 1.0f);
    builder.Link(entry, GraphPin::Then, delay, GraphPin::Exec);
    builder.AppendIncrement(delayed, delay, GraphPin::Then);

    const UUID start = builder.Add(EventNode{ GraphEvent::OnCreate });
    const UUID call = builder.Add(CallEventNode{ event });
    builder.Link(start, GraphPin::Then, call, GraphPin::Exec);
    builder.AppendIncrement(direct, call, GraphPin::Then);

    const Scope<ScriptInstance> instance = fixture.Instantiate(fixture.Import(builder.Graph, "LaterEvent.bfgraph"));
    instance->OnCreate();

    CHECK(Field<i32>(*instance, "Direct") == 1);
    CHECK(Field<i32>(*instance, "Delayed") == 0);

    instance->OnUpdate(1.5f);
    CHECK(Field<i32>(*instance, "Delayed") == 1);
}

TEST_CASE("A call without an event node warns, a second event node is an error", "[scripting][graph][events]")
{
    GraphBuilder builder;
    const UUID event = DeclareEvent(builder, "Lonely");
    const UUID call = builder.Add(CallEventNode{ event });

    const std::vector<GraphDiagnostic> diagnostics = ValidateGraph(builder.Graph);
    CHECK(std::ranges::any_of(diagnostics, [&](const GraphDiagnostic& diagnostic)
    {
        return diagnostic.Node == call && diagnostic.Severity == GraphSeverity::Warning;
    }));
    CHECK_FALSE(HasError(builder.Graph));

    builder.Add(CustomEventNode{ event });
    builder.Add(CustomEventNode{ event });
    CHECK(HasError(builder.Graph));
}

TEST_CASE("Custom events survive saving a graph", "[scripting][graph][events][serializer]")
{
    GraphBuilder builder;
    const GiveGraph give = BuildGiveGraph(builder);

    const Result<ScriptGraph> loaded = DeserializeScriptGraph(SerializeScriptGraph(builder.Graph));
    RequireSuccess(loaded);

    const GraphCustomEvent* event = loaded->FindEvent(give.Event);
    REQUIRE(event != nullptr);
    CHECK(event->Name == "Give");
    REQUIRE(event->Parameters.size() == 1);
    CHECK(event->Parameters.front().Name == "Amount");
    CHECK(std::holds_alternative<i32>(event->Parameters.front().Default));

    CHECK(std::holds_alternative<CustomEventNode>(loaded->FindNode(give.Entry)->Data));
    CHECK(std::holds_alternative<CallEventNode>(loaded->FindNode(give.Call)->Data));
}

TEST_CASE("Pasting an event node into another graph brings its declaration along", "[scripting][graph][events][clipboard]")
{
    GraphBuilder source;
    const GiveGraph give = BuildGiveGraph(source);

    const std::vector<UUID> copied{ give.Entry };
    const std::string text = CopyGraphNodes(source.Graph, copied);

    ScriptGraph target;
    const Result<PastedGraphNodes> pasted = PasteGraphNodes(target, text, glm::vec2(0.0f));
    RequireSuccess(pasted);
    REQUIRE(pasted->Nodes.size() == 1);

    const GraphCustomEvent* event = target.FindEvent(std::string_view("Give"));
    REQUIRE(event != nullptr);
    CHECK(event->Parameters.size() == 1);

    const Result<PastedGraphNodes> again = PasteGraphNodes(target, text, glm::vec2(0.0f));
    RequireSuccess(again);
    CHECK(again->Nodes.empty());
    CHECK(again->SkippedEvents == 1);
}

#include "TestUtilities.h"

#include <Engine/Assets/AssetRegistry.h>
#include <Engine/Assets/Pak.h>
#include <Engine/Core/Error.h>
#include <Engine/Scene/Entity.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Scripting/API/ScriptAPI.h>
#include <Engine/Scripting/ScriptBackend.h>
#include <Engine/Scripting/ScriptEngine.h>
#include <Engine/Scripting/Visual/GraphSchema.h>
#include <Engine/Scripting/Visual/ScriptGraph.h>
#include <Engine/Scripting/Visual/ScriptGraphSerializer.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <glm/glm.hpp>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

using namespace ByteForge;
using Catch::Matchers::ContainsSubstring;

namespace
{
    struct GraphBuilder
    {
        ScriptGraph Graph;

        UUID Add(NodeData data) { return Graph.AddNode(std::move(data), glm::vec2(0.0f)); }

        UUID Variable(std::string name, ScriptValue value)
        {
            const Result<UUID> id = Graph.AddVariable(std::move(name), std::move(value));
            Tests::RequireSuccess(id);
            return *id;
        }

        void Link(const UUID from, const std::string_view fromPin, const UUID to, const std::string_view toPin)
        {
            Tests::RequireSuccess(Graph.Connect({ from, std::string(fromPin) }, { to, std::string(toPin) }));
        }

        void SetDefault(const UUID node, const std::string_view pin, ScriptValue value)
        {
            GraphNode* found = Graph.FindNode(node);
            REQUIRE(found != nullptr);
            found->Defaults.insert_or_assign(std::string(pin), std::move(value));
        }

        void AppendIncrement(const UUID variable, const UUID after, const std::string_view pin)
        {
            const UUID get = Add(GetVariableNode{ variable });
            const UUID add = Add(CallNode{ "Math.AddInt" });
            const UUID set = Add(SetVariableNode{ variable });

            SetDefault(add, "B", 1);
            Link(get, GraphPin::Value, add, "A");
            Link(add, "Result", set, GraphPin::Value);
            Link(after, pin, set, GraphPin::Exec);
        }
    };

    struct GraphFixture
    {
        Tests::TemporaryDirectory Directory;
        ScriptEngine Engine;
        Scene World;
        Entity Owner = World.CreateEntity("Owner");

        GraphFixture()
        {
            fs::create_directories(Directory / "assets");
            AssetRegistry::Init(Directory / "assets");
        }

        ~GraphFixture() { AssetRegistry::Clear(); }

        GraphFixture(const GraphFixture&) = delete;
        GraphFixture& operator=(const GraphFixture&) = delete;

        UUID Import(const ScriptGraph& graph, const fs::path& relativePath)
        {
            Tests::RequireSuccess(SaveScriptGraph(graph, Directory / "assets" / relativePath));

            const UUID handle = AssetRegistry::Import(relativePath);
            REQUIRE(handle.IsSet());
            return handle;
        }

        Scope<ScriptInstance> Instantiate(const UUID graph)
        {
            ScriptBackend* backend = Engine.FindBackendByName("Graph");
            REQUIRE(backend != nullptr);
            return backend->CreateInstance(ScriptGraphClassName(graph), Owner);
        }
    };

    template<typename T>
    T Field(const ScriptInstance& instance, const std::string_view name)
    {
        const std::optional<ScriptValue> value = instance.GetField(name);
        REQUIRE(value.has_value());
        REQUIRE(std::holds_alternative<T>(*value));
        return std::get<T>(*value);
    }

    bool HasError(const ScriptGraph& graph)
    {
        return std::ranges::any_of(ValidateGraph(graph), [](const GraphDiagnostic& diagnostic)
        {
            return diagnostic.Severity == GraphSeverity::Error;
        });
    }
}

TEST_CASE("A graph runs its events and keeps its variables between them", "[scripting][graph]")
{
    GraphFixture fixture;
    GraphBuilder builder;

    const UUID counter = builder.Variable("Counter", 0);
    builder.AppendIncrement(counter, builder.Add(EventNode{ GraphEvent::OnCreate }), GraphPin::Then);
    builder.AppendIncrement(counter, builder.Add(EventNode{ GraphEvent::OnUpdate }), GraphPin::Then);

    const Scope<ScriptInstance> instance = fixture.Instantiate(fixture.Import(builder.Graph, "Counter.bfgraph"));

    instance->OnCreate();
    CHECK(Field<i32>(*instance, "Counter") == 1);

    instance->OnUpdate(0.016f);
    instance->OnUpdate(0.016f);
    CHECK(Field<i32>(*instance, "Counter") == 3);
}

TEST_CASE("A branch takes the path that matches its condition", "[scripting][graph]")
{
    GraphFixture fixture;
    GraphBuilder builder;

    const UUID flag = builder.Variable("Flag", false);
    const UUID hits = builder.Variable("Hits", 0);
    const UUID misses = builder.Variable("Misses", 0);

    const UUID branch = builder.Add(BranchNode{});
    builder.Link(builder.Add(EventNode{ GraphEvent::OnCreate }), GraphPin::Then, branch, GraphPin::Exec);
    builder.Link(builder.Add(GetVariableNode{ flag }), GraphPin::Value, branch, GraphPin::Condition);
    builder.AppendIncrement(hits, branch, GraphPin::True);
    builder.AppendIncrement(misses, branch, GraphPin::False);

    const UUID graph = fixture.Import(builder.Graph, "Branch.bfgraph");

    const Scope<ScriptInstance> taken = fixture.Instantiate(graph);
    REQUIRE(taken->SetField("Flag", true));
    taken->OnCreate();
    CHECK(Field<i32>(*taken, "Hits") == 1);
    CHECK(Field<i32>(*taken, "Misses") == 0);

    const Scope<ScriptInstance> skipped = fixture.Instantiate(graph);
    skipped->OnCreate();
    CHECK(Field<i32>(*skipped, "Hits") == 0);
    CHECK(Field<i32>(*skipped, "Misses") == 1);
}

TEST_CASE("Exec paths that meet run the shared node once per path", "[scripting][graph]")
{
    GraphFixture fixture;
    GraphBuilder builder;

    const UUID flag = builder.Variable("Flag", false);
    const UUID counter = builder.Variable("Counter", 0);

    const UUID branch = builder.Add(BranchNode{});
    builder.Link(builder.Add(EventNode{ GraphEvent::OnCreate }), GraphPin::Then, branch, GraphPin::Exec);
    builder.Link(builder.Add(GetVariableNode{ flag }), GraphPin::Value, branch, GraphPin::Condition);
    builder.AppendIncrement(counter, branch, GraphPin::True);

    const GraphLink* shared = nullptr;
    for (const GraphLink& link : builder.Graph.GetLinks())
    {
        if (link.From.Node == branch && link.From.Pin == GraphPin::True)
            shared = &link;
    }
    REQUIRE(shared != nullptr);
    const UUID set = shared->To.Node;
    builder.Link(branch, GraphPin::False, set, GraphPin::Exec);

    const UUID graph = fixture.Import(builder.Graph, "Shared.bfgraph");

    for (const bool value : { true, false })
    {
        const Scope<ScriptInstance> instance = fixture.Instantiate(graph);
        REQUIRE(instance->SetField("Flag", value));
        instance->OnCreate();
        CHECK(Field<i32>(*instance, "Counter") == 1);
    }
}

TEST_CASE("A Delay resumes its chain once its time has passed", "[scripting][graph]")
{
    GraphFixture fixture;
    GraphBuilder builder;

    const UUID counter = builder.Variable("Counter", 0);

    const UUID delay = builder.Add(DelayNode{});
    builder.Link(builder.Add(EventNode{ GraphEvent::OnCreate }), GraphPin::Then, delay, GraphPin::Exec);
    builder.AppendIncrement(counter, delay, GraphPin::Then);

    SECTION("with a Float duration")
    {
        builder.SetDefault(delay, GraphPin::Duration, 0.5f);
    }

    SECTION("with an Int duration that is converted to Float")
    {
        builder.Link(builder.Add(LiteralNode{ 1 }), GraphPin::Value, delay, GraphPin::Duration);
        builder.SetDefault(delay, GraphPin::Duration, 0.5f); // ignored, the link wins

        const Scope<ScriptInstance> instance = fixture.Instantiate(fixture.Import(builder.Graph, "IntDelay.bfgraph"));
        instance->OnCreate();

        instance->OnUpdate(0.6f);
        CHECK(Field<i32>(*instance, "Counter") == 0);

        instance->OnUpdate(0.6f);
        CHECK(Field<i32>(*instance, "Counter") == 1);
        return;
    }

    const Scope<ScriptInstance> instance = fixture.Instantiate(fixture.Import(builder.Graph, "Delay.bfgraph"));
    instance->OnCreate();

    instance->OnUpdate(0.3f);
    CHECK(Field<i32>(*instance, "Counter") == 0);

    instance->OnUpdate(0.3f);
    CHECK(Field<i32>(*instance, "Counter") == 1);
}

TEST_CASE("A delayed chain that fails does not block other delays", "[scripting][graph]")
{
    GraphFixture fixture;
    GraphBuilder builder;

    const UUID ignored = builder.Variable("Ignored", 0);
    const UUID counter = builder.Variable("Counter", 0);

    const UUID failing = builder.Add(DelayNode{});
    builder.SetDefault(failing, GraphPin::Duration, 0.1f);
    builder.Link(builder.Add(EventNode{ GraphEvent::OnCreate }), GraphPin::Then, failing, GraphPin::Exec);

    const UUID divide = builder.Add(CallNode{ "Math.DivideInt" });
    builder.SetDefault(divide, "A", 1);
    builder.SetDefault(divide, "B", 0);

    const UUID setIgnored = builder.Add(SetVariableNode{ ignored });
    builder.Link(divide, "Result", setIgnored, GraphPin::Value);
    builder.Link(failing, GraphPin::Then, setIgnored, GraphPin::Exec);

    const UUID working = builder.Add(DelayNode{});
    builder.SetDefault(working, GraphPin::Duration, 0.1f);
    builder.Link(builder.Add(EventNode{ GraphEvent::OnUpdate }), GraphPin::Then, working, GraphPin::Exec);
    builder.AppendIncrement(counter, working, GraphPin::Then);

    const Scope<ScriptInstance> instance = fixture.Instantiate(fixture.Import(builder.Graph, "Failing.bfgraph"));
    instance->OnCreate();

    CHECK_THROWS_AS(instance->OnUpdate(0.2f), ScriptError);
    CHECK(Field<i32>(*instance, "Counter") == 1);

    CHECK_NOTHROW(instance->OnUpdate(0.2f));
    CHECK(Field<i32>(*instance, "Counter") == 2);
}

TEST_CASE("Script graphs run from a packed archive", "[scripting][graph][pak]")
{
    GraphFixture fixture;
    GraphBuilder builder;

    const UUID counter = builder.Variable("Counter", 0);
    builder.AppendIncrement(counter, builder.Add(EventNode{ GraphEvent::OnCreate }), GraphPin::Then);

    const UUID handle = fixture.Import(builder.Graph, "Counter.bfgraph");

    const Result<std::vector<std::byte>> graphBytes = AssetRegistry::ReadFile("Counter.bfgraph");
    const Result<std::vector<std::byte>> metaBytes = AssetRegistry::ReadFile("Counter.bfgraph.meta");
    Tests::RequireSuccess(graphBytes);
    Tests::RequireSuccess(metaBytes);

    const fs::path archive = fixture.Directory / "game.bfpak";
    {
        Result<Scope<PakWriter>> writer = PakWriter::Create(archive);
        Tests::RequireSuccess(writer);
        Tests::RequireSuccess((*writer)->Add("Counter.bfgraph", *graphBytes));
        Tests::RequireSuccess((*writer)->Add("Counter.bfgraph.meta", *metaBytes));
        Tests::RequireSuccess((*writer)->Finish());
    }

    fs::remove_all(fixture.Directory / "assets");
    Tests::RequireSuccess(AssetRegistry::Mount(archive));
    REQUIRE(AssetRegistry::IsPacked());

    const Scope<ScriptInstance> instance = fixture.Instantiate(handle);
    instance->OnCreate();
    CHECK(Field<i32>(*instance, "Counter") == 1);
}

TEST_CASE("A graph with errors cannot be instantiated", "[scripting][graph]")
{
    GraphFixture fixture;
    GraphBuilder builder;

    builder.Add(CallNode{ "Missing.Function" });
    REQUIRE(HasError(builder.Graph));

    const UUID graph = fixture.Import(builder.Graph, "Broken.bfgraph");
    CHECK_THROWS_AS(fixture.Instantiate(graph), Error);
}

TEST_CASE("Connections are checked for type and loops", "[scripting][graph]")
{
    GraphBuilder builder;

    const UUID delay = builder.Add(DelayNode{});
    const UUID boolLiteral = builder.Add(LiteralNode{ true });
    const UUID intLiteral = builder.Add(LiteralNode{ 3 });

    SECTION("an Int may feed a Float input")
    {
        CHECK(CanConnect(builder.Graph, { intLiteral, "Value" }, { delay, "Duration" }).has_value());
    }

    SECTION("a Bool may not feed a Float input")
    {
        const Result<PinType> result = CanConnect(builder.Graph, { boolLiteral, "Value" }, { delay, "Duration" });
        REQUIRE_FALSE(result.has_value());
        CHECK_THAT(result.error(), ContainsSubstring("Cannot connect"));
    }

    SECTION("data connections may not form a loop")
    {
        const UUID first = builder.Add(CallNode{ "Math.AddInt" });
        const UUID second = builder.Add(CallNode{ "Math.AddInt" });
        builder.Link(first, "Result", second, "A");

        const Result<PinType> result = CanConnect(builder.Graph, { second, "Result" }, { first, "A" });
        REQUIRE_FALSE(result.has_value());
        CHECK_THAT(result.error(), ContainsSubstring("loop"));
    }
}

TEST_CASE("A graph survives the round trip through JSON", "[scripting][graph][serializer]")
{
    GraphBuilder builder;

    const UUID counter = builder.Variable("Counter", 7);
    const UUID delay = builder.Add(DelayNode{});
    builder.SetDefault(delay, GraphPin::Duration, 2.5f);
    builder.Link(builder.Add(EventNode{ GraphEvent::OnCreate }), GraphPin::Then, delay, GraphPin::Exec);
    builder.AppendIncrement(counter, delay, GraphPin::Then);

    const Result<ScriptGraph> loaded = DeserializeScriptGraph(SerializeScriptGraph(builder.Graph));
    Tests::RequireSuccess(loaded);

    CHECK(loaded->GetNodes().size() == builder.Graph.GetNodes().size());
    CHECK(loaded->GetLinks().size() == builder.Graph.GetLinks().size());

    const GraphVariable* variable = loaded->FindVariable("Counter");
    REQUIRE(variable != nullptr);
    CHECK(variable->Id == counter);
    CHECK(variable->Default == ScriptValue(7));

    const GraphNode* loadedDelay = loaded->FindNode(delay);
    REQUIRE(loadedDelay != nullptr);
    CHECK(std::holds_alternative<DelayNode>(loadedDelay->Data));
    CHECK(loadedDelay->Defaults.at("Duration") == ScriptValue(2.5f));

    CHECK_FALSE(HasError(*loaded));
}

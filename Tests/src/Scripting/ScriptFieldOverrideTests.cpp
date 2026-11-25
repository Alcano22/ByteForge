#include "Scripting/GraphTestHelpers.h"
#include "TestUtilities.h"

#include <Engine/Scene/Components/ScriptComponent.h>
#include <Engine/Scripting/ScriptBackend.h>
#include <Engine/Scripting/ScriptField.h>
#include <Engine/Scripting/Visual/ScriptGraph.h>

#include <catch2/catch_test_macros.hpp>

#include <nlohmann/json.hpp>

#include <string>

using namespace ByteForge;
using namespace ByteForge::Tests;

namespace
{
    ScriptFieldInfo MakeField(std::string name, const u64 id = 0)
    {
        return { .Name = std::move(name), .Type = ScriptFieldType::Int, .Default = 0, .Id = UUID(id) };
    }
}

TEST_CASE("An override follows a field that was renamed", "[scripting][fields]")
{
    ScriptEntry entry;
    entry.SetField(MakeField("Speed", 7), 5);

    const ScriptValue* value = entry.FindField(MakeField("Velocity", 7));
    REQUIRE(value != nullptr);
    CHECK(*value == ScriptValue(5));
}

TEST_CASE("Setting a renamed field drops the value under its old name", "[scripting][fields]")
{
    ScriptEntry entry;
    entry.SetField(MakeField("Speed", 7), 5);
    entry.SetField(MakeField("Velocity", 7), 8);

    CHECK(entry.Fields.size() == 1);
    CHECK_FALSE(entry.Fields.contains("Speed"));
    CHECK(*entry.FindField(MakeField("Velocity", 7)) == ScriptValue(8));
}

TEST_CASE("A new field with the name of a removed one does not inherit its override", "[scripting][fields]")
{
    ScriptEntry entry;
    entry.SetField(MakeField("Speed", 7), 5);

    CHECK(entry.FindField(MakeField("Speed", 9)) == nullptr);
}

TEST_CASE("Fields without an id are matched by name", "[scripting][fields]")
{
    ScriptEntry entry;
    entry.SetField(MakeField("Speed"), 5);

    CHECK(entry.FieldIds.empty());
    REQUIRE(entry.FindField(MakeField("Speed")) != nullptr);
    CHECK(entry.FindField(MakeField("Velocity")) == nullptr);
}

TEST_CASE("Resetting a field removes its value under any name", "[scripting][fields]")
{
    ScriptEntry entry;
    entry.SetField(MakeField("Speed", 7), 5);
    entry.ResetField(MakeField("Velocity", 7));

    CHECK(entry.Fields.empty());
    CHECK(entry.FieldIds.empty());
}

TEST_CASE("Field ids survive saving a script component", "[scripting][fields][serializer]")
{
    ScriptComponent component;
    component.Add("SomeClass").SetField(MakeField("Speed", 7), 5);

    nlohmann::json json;
    ScriptComponent::Write(component, json);

    ScriptComponent loaded;
    ScriptComponent::Read(loaded, json);

    REQUIRE(loaded.Scripts.size() == 1);
    const ScriptValue* value = loaded.Scripts.front().FindField(MakeField("Velocity", 7));
    REQUIRE(value != nullptr);
    CHECK(*value == ScriptValue(5));
}

TEST_CASE("Exposed graph variables report their id as field id", "[scripting][fields][graph]")
{
    GraphFixture fixture;
    GraphBuilder builder;
    const UUID speed = builder.Variable("Speed", 1.0f);

    const UUID asset = fixture.Import(builder.Graph, "Fields.bfgraph");
    ScriptBackend* backend = fixture.Engine.FindBackendByName("Graph");
    REQUIRE(backend != nullptr);

    const auto fields = backend->GetFields(ScriptGraphClassName(asset));
    REQUIRE(fields.size() == 1);
    CHECK(fields.front().Id == speed);
}

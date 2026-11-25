#pragma once

#include "TestUtilities.h"

#include <Engine/Assets/AssetRegistry.h>
#include <Engine/Scene/Entity.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Scripting/ScriptBackend.h>
#include <Engine/Scripting/ScriptEngine.h>
#include <Engine/Scripting/Visual/GraphDebugger.h>
#include <Engine/Scripting/Visual/GraphSchema.h>
#include <Engine/Scripting/Visual/ScriptGraph.h>
#include <Engine/Scripting/Visual/ScriptGraphSerializer.h>

#include <catch2/catch_test_macros.hpp>

#include <glm/glm.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace ByteForge::Tests
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

        ~GraphFixture()
        {
            GraphDebugger::Watch(UUID(0));
            GraphDebugger::ClearErrors();
            AssetRegistry::Clear();
        }

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

    inline bool HasError(const ScriptGraph& graph)
    {
        return std::ranges::any_of(ValidateGraph(graph), [](const GraphDiagnostic& diagnostic)
        {
            return diagnostic.Severity == GraphSeverity::Error;
        });
    }
}

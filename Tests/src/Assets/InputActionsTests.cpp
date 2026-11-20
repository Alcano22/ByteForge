#include <Engine/Assets/InputActions.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <nlohmann/json.hpp>

#include <string>

using namespace ByteForge;
using Catch::Matchers::ContainsSubstring;

namespace
{
    std::string ValidationError(const InputActionsData& data)
    {
        const Result<> result = ValidateInputActions(data);
        REQUIRE_FALSE(result.has_value());
        return result.error();
    }

    std::string ParseError(const char* json)
    {
        const Result<InputActionsData> result = ParseInputActions(nlohmann::json::parse(json));
        REQUIRE_FALSE(result.has_value());
        return result.error();
    }
}

TEST_CASE("The default input actions are valid", "[assets][input]")
{
    CHECK(ValidateInputActions(MakeDefaultInputActions()).has_value());
}

TEST_CASE("Input actions survive a round trip", "[assets][input]")
{
    InputActionsData data = MakeDefaultInputActions();
    data.Maps[0].Actions[0].Bindings[0].Processors.Scale = 2.5f;
    data.Maps[0].Actions[0].Bindings[1].Processors.InvertY = true;
    data.Maps[0].Actions[1].Bindings[0].Controls[0] = InputControl{};

    const Result<InputActionsData> parsed = ParseInputActions(SerializeInputActions(data));
    REQUIRE(parsed.has_value());
    CHECK(*parsed == data);
}

TEST_CASE("Default processors are left out of the file", "[assets][input]")
{
    const nlohmann::json json = SerializeInputActions(MakeDefaultInputActions());
    const nlohmann::json& actions = json.at("maps").at(0).at("actions");

    CHECK(actions.at(0).at("bindings").at(0).contains("processors"));
    CHECK_FALSE(actions.at(1).at("bindings").at(0).contains("processors"));
}

TEST_CASE("Actions are found by path and by id", "[assets][input]")
{
    const InputActionsAsset asset(UUID(1), MakeDefaultInputActions());

    const InputActionData* move = asset.FindAction("Gameplay/Move");
    REQUIRE(move != nullptr);
    CHECK(move->Type == InputActionType::Vector2);
    CHECK(asset.FindAction(move->Id) == move);

    CHECK(asset.FindMap("Gameplay") != nullptr);
    CHECK(asset.FindMap("UI") == nullptr);
    CHECK(asset.FindAction("Gameplay/Fly") == nullptr);
    CHECK(asset.FindAction("UI/Move") == nullptr);
    CHECK(asset.FindAction("Move") == nullptr);
    CHECK(asset.FindAction(UUID(12345)) == nullptr);
}

TEST_CASE("Validation rejects broken input actions", "[assets][input]")
{
    InputActionsData data = MakeDefaultInputActions();
    InputActionMapData& map = data.Maps[0];

    SECTION("Duplicate action names")
    {
        map.Actions[1].Name = "Move";
        CHECK_THAT(ValidationError(data), ContainsSubstring("two actions named 'Move'"));
    }

    SECTION("A slash in a name")
    {
        map.Name = "Game/play";
        CHECK_THAT(ValidationError(data), ContainsSubstring("must not contain '/'"));
    }

    SECTION("An empty name")
    {
        map.Actions[0].Name.clear();
        CHECK_THAT(ValidationError(data), ContainsSubstring("has no name"));
    }

    SECTION("The wrong number of controls")
    {
        map.Actions[0].Bindings[0].Controls.pop_back();
        CHECK_THAT(ValidationError(data), ContainsSubstring("needs 4 controls but has 3"));
    }

    SECTION("A missing id")
    {
        map.Actions[1].Id = UUID(0);
        CHECK_THAT(ValidationError(data), ContainsSubstring("missing or duplicate id"));
    }

    SECTION("A duplicate id")
    {
        map.Actions[1].Id = map.Actions[0].Id;
        CHECK_THAT(ValidationError(data), ContainsSubstring("missing or duplicate id"));
    }
}

TEST_CASE("Unknown controls load as unbound", "[assets][input]")
{
    const nlohmann::json json = nlohmann::json::parse(R"({
        "version": 1,
        "maps": [{
            "id": 1, "name": "Gameplay",
            "actions": [{
                "id": 2, "name": "Jump", "type": "Button",
                "bindings": [{ "kind": "Single", "controls": ["Key/NotAKey"] }]
            }]
        }]
    })");

    const Result<InputActionsData> parsed = ParseInputActions(json);
    REQUIRE(parsed.has_value());
    CHECK_FALSE(parsed->Maps[0].Actions[0].Bindings[0].Controls[0].IsSet());
}

TEST_CASE("Broken files are rejected with a reason", "[assets][input]")
{
    SECTION("An unknown action type")
    {
        CHECK_THAT(ParseError(R"({ "maps": [{ "id": 1, "name": "Gameplay",
                                    "actions": [{ "id": 2, "name": "Aim", "type": "Vector3" }] }] })"),
                   ContainsSubstring("unknown type 'Vector3'"));
    }

    SECTION("A newer format")
    {
        CHECK_THAT(ParseError(R"({ "version": 99, "maps": [] })"), ContainsSubstring("newer ByteForge version"));
    }

    SECTION("Maps that aren't an array")
    {
        CHECK_THAT(ParseError(R"({ "maps": {} })"), ContainsSubstring("'maps' must be an array"));
    }
}

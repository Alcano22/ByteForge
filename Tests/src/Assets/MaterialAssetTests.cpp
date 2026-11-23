#include <Engine/Assets/MaterialAsset.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <nlohmann/json.hpp>

#include <string>
#include <utility>
#include <variant>

using namespace ByteForge;
using Catch::Matchers::ContainsSubstring;

namespace
{
    std::string ParseError(const char* json)
    {
        const Result<MaterialData> result = ParseMaterial(nlohmann::json::parse(json));
        REQUIRE_FALSE(result.has_value());
        return result.error();
    }

    MaterialData Parse(const char* json)
    {
        Result<MaterialData> result = ParseMaterial(nlohmann::json::parse(json));
        if (!result)
            FAIL(result.error());
        return std::move(*result);
    }

    const MaterialParameterInfo& Tint()
    {
        const MaterialShaderInfo* shader = FindMaterialShader(SpriteLitShader);
        REQUIRE(shader != nullptr);

        const MaterialParameterInfo* tint = shader->FindParameter("Tint");
        REQUIRE(tint != nullptr);
        return *tint;
    }
}

TEST_CASE("Both built-in sprite shaders exist", "[assets][material]")
{
    CHECK(GetMaterialShaders().size() == 2);
    CHECK(FindMaterialShader(SpriteUnlitShader) != nullptr);
    CHECK(FindMaterialShader(SpriteLitShader) != nullptr);
    CHECK(FindMaterialShader("Sprite/Missing") == nullptr);
}

TEST_CASE("A new material is an unlit sprite material without stored values", "[assets][material]")
{
    const MaterialData data;
    CHECK(data.Shader == SpriteUnlitShader);
    CHECK(data.Parameters.empty());
}

TEST_CASE("Parameter types follow the value", "[assets][material]")
{
    CHECK(GetMaterialParameterType(MaterialValue(1.0f)) == MaterialParameterType::Float);
    CHECK(GetMaterialParameterType(MaterialValue(glm::vec4(1.0f))) == MaterialParameterType::Color);
    CHECK(GetMaterialParameterType(MaterialValue(MaterialTexture{ UUID(7) })) == MaterialParameterType::Texture);
}

TEST_CASE("A material survives a round trip", "[assets][material]")
{
    MaterialData data;
    data.Shader = SpriteLitShader;
    data.Parameters.emplace("Tint", glm::vec4(0.25f, 0.5f, 0.75f, 1.0f));
    data.Parameters.emplace("Strength", 2.5f);
    data.Parameters.emplace("Normal Map", MaterialTexture{ UUID(1234567890123ull) });

    const Result<MaterialData> parsed = ParseMaterial(SerializeMaterial(data));
    REQUIRE(parsed.has_value());
    CHECK(*parsed == data);
}

TEST_CASE("Materials are stored in a readable format", "[assets][material]")
{
    MaterialData data;
    data.Shader = SpriteLitShader;
    data.Parameters.emplace("Tint", glm::vec4(1.0f, 0.5f, 0.0f, 1.0f));
    data.Parameters.emplace("Normal Map", MaterialTexture{ UUID(42) });

    const nlohmann::json json = SerializeMaterial(data);

    CHECK(json.at("version") == 1);
    CHECK(json.at("shader") == "Sprite/Lit");
    CHECK(json.at("parameters").at("Tint").at("type") == "Color");
    CHECK(json.at("parameters").at("Tint").at("value") == nlohmann::json::array({ 1.0f, 0.5f, 0.0f, 1.0f }));
    CHECK(json.at("parameters").at("Normal Map").at("type") == "Texture");
    CHECK(json.at("parameters").at("Normal Map").at("value") == 42);
}

TEST_CASE("Missing fields use their defaults", "[assets][material]")
{
    CHECK(Parse("{}") == MaterialData{});
}

TEST_CASE("An unknown shader is kept instead of being replaced", "[assets][material]")
{
    const MaterialData data = Parse(R"({ "shader": "Custom/Water" })");
    CHECK(data.Shader == "Custom/Water");
    CHECK(FindMaterialShader(data.Shader) == nullptr);
}

TEST_CASE("Parameters resolve to their stored value or their default", "[assets][material]")
{
    const MaterialParameterInfo& tint = Tint();

    MaterialData data;
    CHECK(std::get<glm::vec4>(ResolveMaterialParameter(data, tint)) == glm::vec4(1.0f));

    data.Parameters.insert_or_assign("Tint", glm::vec4(0.5f));
    CHECK(std::get<glm::vec4>(ResolveMaterialParameter(data, tint)) == glm::vec4(0.5f));

    data.Parameters.insert_or_assign("Tint", 3.0f);
    CHECK(std::get<glm::vec4>(ResolveMaterialParameter(data, tint)) == glm::vec4(1.0f));
}

TEST_CASE("A new material asset starts at revision zero", "[assets][material]")
{
    const MaterialAsset asset(UUID(1), MaterialData{});
    CHECK(asset.GetHandle() == UUID(1));
    CHECK(asset.GetRevision() == 0);
}

TEST_CASE("Malformed materials are rejected with a reason", "[assets][material]")
{
    CHECK_THAT(ParseError("[]"), ContainsSubstring("JSON object"));
    CHECK_THAT(ParseError(R"({ "version": 2 })"), ContainsSubstring("newer ByteForge"));
    CHECK_THAT(ParseError(R"({ "version": "1" })"), ContainsSubstring("version"));
    CHECK_THAT(ParseError(R"({ "shader": 5 })"), ContainsSubstring("shader"));
    CHECK_THAT(ParseError(R"({ "shader": "" })"), ContainsSubstring("empty"));
    CHECK_THAT(ParseError(R"({ "parameters": [] })"), ContainsSubstring("parameters"));
}

TEST_CASE("Malformed parameters are rejected with their name", "[assets][material]")
{
    CHECK_THAT(ParseError(R"({ "parameters": { "Tint": 1 } })"), ContainsSubstring("'Tint'"));
    CHECK_THAT(ParseError(R"({ "parameters": { "Tint": { "value": 1 } } })"), ContainsSubstring("no type"));
    CHECK_THAT(ParseError(R"({ "parameters": { "Tint": { "type": "Color" } } })"), ContainsSubstring("no value"));
    CHECK_THAT(ParseError(R"({ "parameters": { "Tint": { "type": "Gradient", "value": 1 } } })"),
               ContainsSubstring("unknown type 'Gradient'"));
    CHECK_THAT(ParseError(R"({ "parameters": { "Tint": { "type": "Color", "value": [1, 1, 1] } } })"),
               ContainsSubstring("4 numbers"));
    CHECK_THAT(ParseError(R"({ "parameters": { "Tint": { "type": "Color", "value": [1, 1, "1", 1] } } })"),
               ContainsSubstring("4 numbers"));
    CHECK_THAT(ParseError(R"({ "parameters": { "Strength": { "type": "Float", "value": "2" } } })"),
               ContainsSubstring("number"));
    CHECK_THAT(ParseError(R"({ "parameters": { "Map": { "type": "Texture", "value": -1 } } })"),
               ContainsSubstring("asset handle"));
}

#include "Engine/Assets/MaterialAsset.h"

#include <magic_enum/magic_enum.hpp>
#include <nlohmann/json.hpp>

#include <array>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

namespace ByteForge
{
    namespace
    {
        constexpr i32 FormatVersion = 1;

        static_assert(std::is_same_v<std::variant_alternative_t<0, MaterialValue>, f32>);
        static_assert(std::is_same_v<std::variant_alternative_t<1, MaterialValue>, glm::vec4>);
        static_assert(std::is_same_v<std::variant_alternative_t<2, MaterialValue>, MaterialTexture>);

        const std::array<MaterialParameterInfo, 1>& SpriteParameters()
        {
            static constexpr std::array<MaterialParameterInfo, 1> parameters{{
                {
                    .Name    = "Tint",
                    .Type    = MaterialParameterType::Color,
                    .Default = glm::vec4(1.0f),
                    .Tooltip = "Multiplied with the sprite's texture and the SpriteRenderer's color"
                }
            }};
            return parameters;
        }

        const std::array<MaterialShaderInfo, 2>& Shaders()
        {
            static const std::array<MaterialShaderInfo, 2> shaders{{
                {
                    .Id          = SpriteUnlitShader,
                    .DisplayName = "Sprite Unlit",
                    .Description = "Draws the sprite as it is, lights have no effect on it",
                    .Parameters  = SpriteParameters()
                },
                {
                    .Id          = SpriteLitShader,
                    .DisplayName = "Sprite Lit",
                    .Description = "Lit by the scene's ambient color and its 2D lights",
                    .Parameters  = SpriteParameters()
                }
            }};
            return shaders;
        }

        Result<glm::vec4> ParseColor(const nlohmann::json& value)
        {
            if (!value.is_array() || value.size() != 4)
                return Unexpected("a Color must be an array of 4 numbers");

            glm::vec4 color{ 0.0f };
            for (usize i = 0; i < 4; ++i)
            {
                const nlohmann::json& channel = value[i];
                if (!channel.is_number())
                    return Unexpected("a Color must be an array of 4 numbers");

                color[static_cast<glm::length_t>(i)] = channel.get<f32>();
            }

            return color;
        }

        Result<MaterialValue> ParseValue(const std::string& name, const nlohmann::json& data)
        {
            if (!data.is_object())
                return Unexpected("parameter '{}' must be a JSON object", name);

            const auto type = data.find("type");
            if (type == data.end() || !type->is_string())
                return Unexpected("parameter '{}' has no type", name);

            const auto value = data.find("value");
            if (value == data.end())
                return Unexpected("parameter '{}' has no value", name);

            const std::string typeName = type->get<std::string>();
            const std::optional<MaterialParameterType> parsedType = magic_enum::enum_cast<MaterialParameterType>(typeName);
            if (!parsedType)
                return Unexpected("parameter '{}' has the unknown type '{}'", name, typeName);

            switch (*parsedType)
            {
                case MaterialParameterType::Float:
                    if (!value->is_number())
                        return Unexpected("parameter '{}' must be a number", name);
                    return MaterialValue(value->get<f32>());

                case MaterialParameterType::Color:
                {
                    const Result<glm::vec4> color = ParseColor(*value);
                    if (!color)
                        return Unexpected("parameter '{}': {}", name, color.error());
                    return MaterialValue(*color);
                }

                case MaterialParameterType::Texture:
                    if (!value->is_number_unsigned())
                        return Unexpected("parameter '{}' must be an asset handle", name);
                    return MaterialValue(MaterialTexture{ UUID(value->get<u64>()) });
            }

            return Unexpected("parameter '{}' has an invalid type", name);
        }

        nlohmann::json SerializeValue(const MaterialValue& value)
        {
            nlohmann::json json{
                { "type", std::string(magic_enum::enum_name(GetMaterialParameterType(value))) }
            };

            if (const auto* number = std::get_if<f32>(&value))
                json["value"] = *number;
            else if (const auto* color = std::get_if<glm::vec4>(&value))
                json["value"] = nlohmann::json::array({ color->r, color->g, color->b, color->a });
            else if (const auto* texture = std::get_if<MaterialTexture>(&value))
                json["value"] = static_cast<u64>(texture->Handle);

            return json;
        }
    }

    MaterialParameterType GetMaterialParameterType(const MaterialValue& value)
    {
        return static_cast<MaterialParameterType>(value.index());
    }

    std::span<const MaterialShaderInfo> GetMaterialShaders() { return Shaders(); }

    const MaterialShaderInfo* FindMaterialShader(const std::string_view id)
    {
        for (const MaterialShaderInfo& shader : Shaders())
        {
            if (shader.Id == id)
                return &shader;
        }
        return nullptr;
    }

    MaterialValue ResolveMaterialParameter(const MaterialData& data, const MaterialParameterInfo& parameter)
    {
        const auto it = data.Parameters.find(parameter.Name);
        if (it != data.Parameters.end() && GetMaterialParameterType(it->second) == parameter.Type)
            return it->second;

        return parameter.Default;
    }

    MaterialAsset::MaterialAsset(const UUID handle, MaterialData data)
        : m_Handle(handle), m_Data(std::move(data)) {}

    Result<MaterialData> ParseMaterial(const nlohmann::json& data)
    {
        if (!data.is_object())
            return Unexpected("expected a JSON object");

        if (const auto version = data.find("version"); version != data.end())
        {
            if (!version->is_number_integer())
                return Unexpected("'version' must be a whole number");

            if (version->get<i64>() > FormatVersion)
                return Unexpected("the file was made by a newer ByteForge version (format {})", version->get<i64>());
        }

        MaterialData material;

        if (const auto shader = data.find("shader"); shader != data.end())
        {
            if (!shader->is_string())
                return Unexpected("'shader' must be a string");

            material.Shader = shader->get<std::string>();
        }

        if (material.Shader.empty())
            return Unexpected("'shader' must not be empty");

        if (const auto parameters = data.find("parameters"); parameters != data.end())
        {
            if (!parameters->is_object())
                return Unexpected("'parameters' must be a JSON object");

            for (const auto& item : parameters->items())
            {
                Result<MaterialValue> value = ParseValue(item.key(), item.value());
                if (!value)
                    return Unexpected(value.error());

                material.Parameters.emplace(item.key(), std::move(*value));
            }
        }

        return material;
    }

    nlohmann::json SerializeMaterial(const MaterialData& data)
    {
        nlohmann::json parameters = nlohmann::json::object();
        for (const auto& [name, value] : data.Parameters)
            parameters[name] = SerializeValue(value);

        return nlohmann::json{
            { "version",    FormatVersion         },
            { "shader",     data.Shader           },
            { "parameters", std::move(parameters) }
        };
    }
}

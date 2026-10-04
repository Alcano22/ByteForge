#include "Engine/Scene/SceneSerializer.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/Entity.h"
#include "Engine/Scene/Components.h"
#include "Engine/Assets/AssetManager.h"

#include <format>
#include <fstream>
#include <stdexcept>
#include <string>
#include <optional>
#include <type_traits>
#include <variant>

namespace
{
    using namespace ByteForge;

    nlohmann::json ToJson(const glm::vec2& v) { return { v.x, v.y }; }
    nlohmann::json ToJson(const glm::vec3& v) { return { v.x, v.y, v.z }; }
    nlohmann::json ToJson(const glm::vec4& v) { return { v.x, v.y, v.z, v.w }; }

    glm::vec2 ToVec2(const nlohmann::json& j) { return { j.at(0).get<float>(), j.at(1).get<float>() }; }
    glm::vec3 ToVec3(const nlohmann::json& j) { return { j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>() }; }
    glm::vec4 ToVec4(const nlohmann::json& j) { return { j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>(), j.at(3).get<float>() }; }

    nlohmann::json ToJson(const ScriptValue& value)
    {
        return std::visit([]<typename T>(const T& v) -> nlohmann::json
        {
            if constexpr (std::is_arithmetic_v<T>)
                return v;
            else
                return ToJson(v);
        }, value);
    }

    std::optional<ScriptValue> ReadScriptValue(const nlohmann::json& fieldJson)
    {
        const std::optional<ScriptFieldType> type = ParseScriptFieldType(fieldJson.value("type", std::string()));
        if (!type || !fieldJson.contains("value"))
            return std::nullopt;

        const nlohmann::json& v = fieldJson.at("value");
        switch (*type)
        {
            case ScriptFieldType::Bool:    return v.get<bool>();
            case ScriptFieldType::Int:     return v.get<int32_t>();
            case ScriptFieldType::Float:   return v.get<float>();
            case ScriptFieldType::Double:  return v.get<double>();
            case ScriptFieldType::Vector2: return ToVec2(v);
            case ScriptFieldType::Vector3: return ToVec3(v);
            case ScriptFieldType::Vector4: return ToVec4(v);
        }
        return std::nullopt;
    }

    void WriteMaterial(nlohmann::json& json, const Ref<PhysicsMaterialAsset>& material)
    {
        if (material && material->IsFileBacked())
            json["material"] = static_cast<uint64_t>(material->GetHandle());
    }

    Ref<PhysicsMaterialAsset> ReadMaterial(const nlohmann::json& json)
    {
        if (!json.contains("material"))
            return nullptr;

        return AssetManager::LoadPhysicsMaterial(UUID(json.at("material").get<uint64_t>()));
    }

    const char* ToString(const Rigidbody2DComponent::BodyType type)
    {
        switch (type)
        {
            case Rigidbody2DComponent::BodyType::Static:    return "Static";
            case Rigidbody2DComponent::BodyType::Kinematic: return "Kinematic";
            case Rigidbody2DComponent::BodyType::Dynamic:   return "Dynamic";
        }
        return "Static";
    }

    Rigidbody2DComponent::BodyType ToBodyType(const std::string& value)
    {
        if (value == "Kinematic") return Rigidbody2DComponent::BodyType::Kinematic;
        if (value == "Dynamic")   return Rigidbody2DComponent::BodyType::Dynamic;
        return Rigidbody2DComponent::BodyType::Static;
    }

    template<typename T>
    void RemoveIfPresent(const Entity entity)
    {
        if (entity.HasComponent<T>())
            entity.RemoveComponent<T>();
    }

    void RemoveSerializableComponents(const Entity entity)
    {
        RemoveIfPresent<SpriteRendererComponent>(entity);
        RemoveIfPresent<Rigidbody2DComponent>(entity);
        RemoveIfPresent<BoxCollider2DComponent>(entity);
        RemoveIfPresent<CircleCollider2DComponent>(entity);
        RemoveIfPresent<AudioSourceComponent>(entity);
        RemoveIfPresent<ScriptComponent>(entity);
    }

    void ReadComponents(const Entity entity, const nlohmann::json& entityJson)
    {
        entity.GetComponent<TagComponent>().Tag = entityJson.value("tag", std::string("Entity"));

        if (entityJson.contains("transform"))
        {
            const auto& t = entityJson.at("transform");
            auto& transform = entity.GetComponent<TransformComponent>();
            transform.Position = ToVec3(t.at("position"));
            transform.Rotation = t.at("rotation").get<float>();
            transform.Scale = ToVec2(t.at("scale"));
        }

        if (entityJson.contains("spriteRenderer"))
        {
            const auto& s = entityJson.at("spriteRenderer");
            auto& sprite = entity.AddComponent<SpriteRendererComponent>();
            sprite.Color = ToVec4(s.at("color"));

            if (s.contains("sprite"))
            {
                const auto& sp = s.at("sprite");
                sprite.Sprite = MakeRef<Sprite>(AssetManager::LoadTexture2D(UUID(sp.at("texture").get<uint64_t>())),
                                                ToVec2(sp.at("uvMin")), ToVec2(sp.at("uvMax")));
            }
        }

        if (entityJson.contains("rigidbody2D"))
        {
            const auto& r = entityJson.at("rigidbody2D");
            auto& rb = entity.AddComponent<Rigidbody2DComponent>();
            rb.Type = ToBodyType(r.value("type", std::string("Static")));
            rb.FixedRotation = r.value("fixedRotation", false);
            rb.GravityScale = r.value("gravityScale", 1.0f);
            rb.Material = ReadMaterial(r);
        }

        if (entityJson.contains("boxCollider2D"))
        {
            const auto& c = entityJson.at("boxCollider2D");
            auto& collider = entity.AddComponent<BoxCollider2DComponent>();
            collider.Offset = ToVec2(c.at("offset"));
            collider.Size = ToVec2(c.at("size"));
            collider.Density = c.value("density", 1.0f);
            collider.IsSensor = c.value("isSensor", false);
            collider.Material = ReadMaterial(c);
        }

        if (entityJson.contains("circleCollider2D"))
        {
            const auto& c = entityJson.at("circleCollider2D");
            auto& collider = entity.AddComponent<CircleCollider2DComponent>();
            collider.Offset = ToVec2(c.at("offset"));
            collider.Radius = c.value("radius", 0.5f);
            collider.Density = c.value("density", 1.0f);
            collider.IsSensor = c.value("isSensor", false);
            collider.Material = ReadMaterial(c);
        }

        if (entityJson.contains("audioSource"))
        {
            const auto& a = entityJson.at("audioSource");
            auto& source = entity.AddComponent<AudioSourceComponent>();
            source.Clip = UUID(a.value("clip", uint64_t{ 0 }));
            source.Volume = a.value("volume", 1.0f);
            source.Pitch = a.value("pitch", 1.0f);
            source.Loop = a.value("loop", false);
            source.PlayOnStart = a.value("playOnStart", true);
        }

        if (entityJson.contains("script"))
        {
            const auto& scriptJson = entityJson.at("script");
            auto& script = entity.AddComponent<ScriptComponent>(scriptJson.value("class", std::string()));

            if (scriptJson.contains("fields"))
            {
                for (const auto& [name, fieldJson] : scriptJson.at("fields").items())
                {
                    if (std::optional<ScriptValue> value = ReadScriptValue(fieldJson))
                        script.Fields.insert_or_assign(name, std::move(*value));
                }
            }
        }
    }
}

namespace ByteForge
{
    nlohmann::json SceneSerializer::Serialize(Scene& scene)
    {
        nlohmann::json data;
        data["version"] = 1;
        data["entities"] = nlohmann::json::array();

        scene.Each<UUIDComponent>([&data](const Entity entity, const UUIDComponent&)
        {
            data["entities"].push_back(SerializeEntity(entity));
        });

        return data;
    }

    void SceneSerializer::Deserialize(Scene& scene, const nlohmann::json& data)
    {
        scene.Clear();

        if (!data.contains("entities")) return;

        for (const auto& entityJson : data.at("entities"))
            DeserializeEntity(scene, entityJson);
    }

    void SceneSerializer::SerializeToFile(Scene& scene, const std::string& filepath)
    {
        std::ofstream file(filepath);
        if (!file.is_open())
            throw std::runtime_error(std::format("SceneSerializer::SerializeToFile: Could not open '{}' for writing",
                                                 filepath));

        file << Serialize(scene).dump(4);
    }

    bool SceneSerializer::DeserializeFromFile(Scene& scene, const std::string& filepath)
    {
        std::ifstream file(filepath);
        if (!file.is_open())
            return false;

        nlohmann::json data;
        try
        {
            file >> data;
        } catch (const nlohmann::json::parse_error&)
        {
            return false;
        }

        Deserialize(scene, data);
        return true;
    }

    nlohmann::json SceneSerializer::SerializeEntity(const Entity entity)
    {
        nlohmann::json entityJson;
        entityJson["uuid"] = static_cast<uint64_t>(entity.GetUUID());
        entityJson["tag"] = entity.GetTag();

        const auto& transform = entity.GetComponent<TransformComponent>();
        entityJson["transform"] = {
            { "position", ToJson(transform.Position) },
            { "rotation", transform.Rotation         },
            { "scale",    ToJson(transform.Scale)    }
        };

        if (entity.HasComponent<SpriteRendererComponent>())
        {
            const auto& sprite = entity.GetComponent<SpriteRendererComponent>();

            nlohmann::json spriteJson;
            spriteJson["color"] = ToJson(sprite.Color);

            if (sprite.Sprite)
            {
                spriteJson["sprite"] = {
                    { "texture", static_cast<uint64_t>(sprite.Sprite->GetTextureAsset()->GetHandle()) },
                    { "uvMin",   ToJson(sprite.Sprite->GetUVMin())                                    },
                    { "uvMax",   ToJson(sprite.Sprite->GetUVMax())                                    }
                };
            }

            entityJson["spriteRenderer"] = std::move(spriteJson);
        }

        if (entity.HasComponent<Rigidbody2DComponent>())
        {
            const auto& rb = entity.GetComponent<Rigidbody2DComponent>();
            nlohmann::json rigidbodyJson{
                { "type",          ToString(rb.Type) },
                { "fixedRotation", rb.FixedRotation  },
                { "gravityScale",  rb.GravityScale   }
            };
            WriteMaterial(rigidbodyJson, rb.Material);
            entityJson["rigidbody2D"] = std::move(rigidbodyJson);
        }

        if (entity.HasComponent<BoxCollider2DComponent>())
        {
            const auto& collider = entity.GetComponent<BoxCollider2DComponent>();
            nlohmann::json colliderJson{
                { "offset",   ToJson(collider.Offset) },
                { "size",     ToJson(collider.Size)   },
                { "density",  collider.Density        },
                { "isSensor", collider.IsSensor       }
            };
            WriteMaterial(colliderJson, collider.Material);
            entityJson["boxCollider2D"] = std::move(colliderJson);
        }

        if (entity.HasComponent<CircleCollider2DComponent>())
        {
            const auto& collider = entity.GetComponent<CircleCollider2DComponent>();
            nlohmann::json colliderJson{
                { "offset",   ToJson(collider.Offset) },
                { "radius",   collider.Radius         },
                { "density",  collider.Density        },
                { "isSensor", collider.IsSensor       }
            };
            WriteMaterial(colliderJson, collider.Material);
            entityJson["circleCollider2D"] = std::move(colliderJson);
        }

        if (entity.HasComponent<AudioSourceComponent>())
        {
            const auto& source = entity.GetComponent<AudioSourceComponent>();
            nlohmann::json sourceJson{
                { "volume",      source.Volume      },
                { "pitch",       source.Pitch       },
                { "loop",        source.Loop        },
                { "playOnStart", source.PlayOnStart }
            };

            if (static_cast<uint64_t>(source.Clip) != 0)
                sourceJson["clip"] = static_cast<uint64_t>(source.Clip);

            entityJson["audioSource"] = std::move(sourceJson);
        }

        if (entity.HasComponent<ScriptComponent>())
        {
            const auto& script = entity.GetComponent<ScriptComponent>();

            nlohmann::json fields = nlohmann::json::object();
            for (const auto& [name, value] : script.Fields)
            {
                fields[name] = {
                    { "type",  std::string(ScriptFieldTypeName(GetFieldType(value))) },
                    { "value", ToJson(value)                                         }
                };
            }

            entityJson["script"] = { { "class", script.ClassName }, { "fields", std::move(fields) } };
        }

        return entityJson;
    }

    Entity SceneSerializer::DeserializeEntity(Scene& scene, const nlohmann::json& data)
    {
        const Entity entity = scene.CreateEntity(data.value("tag", std::string("Entity")));
        entity.GetComponent<UUIDComponent>().ID = UUID(data.at("uuid").get<uint64_t>());

        ReadComponents(entity, data);
        return entity;
    }

    void SceneSerializer::ApplyEntity(const Entity entity, const nlohmann::json& data)
    {
        RemoveSerializableComponents(entity);
        ReadComponents(entity, data);
    }
}

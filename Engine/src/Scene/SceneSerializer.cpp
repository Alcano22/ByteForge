#include "Engine/Scene/SceneSerializer.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/Entity.h"
#include "Engine/Scene/Components.h"
#include "Engine/Assets/AssetManager.h"

#include <format>
#include <fstream>
#include <stdexcept>
#include <string>

namespace
{
    using namespace ByteForge;

    nlohmann::json ToJson(const glm::vec2& v) { return { v.x, v.y }; }
    nlohmann::json ToJson(const glm::vec3& v) { return { v.x, v.y, v.z }; }
    nlohmann::json ToJson(const glm::vec4& v) { return { v.x, v.y, v.z, v.w }; }

    glm::vec2 ToVec2(const nlohmann::json& j) { return { j.at(0).get<float>(), j.at(1).get<float>() }; }
    glm::vec3 ToVec3(const nlohmann::json& j) { return { j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>() }; }
    glm::vec4 ToVec4(const nlohmann::json& j) { return { j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>(), j.at(3).get<float>() }; }

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
        }

        if (entityJson.contains("boxCollider2D"))
        {
            const auto& c = entityJson.at("boxCollider2D");
            auto& collider = entity.AddComponent<BoxCollider2DComponent>();
            collider.Offset = ToVec2(c.at("offset"));
            collider.Size = ToVec2(c.at("size"));
            collider.Density = c.value("density", 1.0f);
            collider.Friction = c.value("friction", 0.6f);
            collider.Restitution = c.value("restitution", 0.0f);
            collider.IsSensor = c.value("isSensor", false);
        }

        if (entityJson.contains("circleCollider2D"))
        {
            const auto& c = entityJson.at("circleCollider2D");
            auto& collider = entity.AddComponent<CircleCollider2DComponent>();
            collider.Offset = ToVec2(c.at("offset"));
            collider.Radius = c.value("radius", 0.5f);
            collider.Density = c.value("density", 1.0f);
            collider.Friction = c.value("friction", 0.6f);
            collider.Restitution = c.value("restitution", 0.0f);
            collider.IsSensor = c.value("isSensor", false);
        }
    }
}

namespace ByteForge
{
    nlohmann::json SceneSerializer::Serialize(Scene& scene)
    {
        nlohmann::json data;
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
            entityJson["rigidbody2D"] = {
                { "type",          ToString(rb.Type) },
                { "fixedRotation", rb.FixedRotation  },
                { "gravityScale",  rb.GravityScale   }
            };
        }

        if (entity.HasComponent<BoxCollider2DComponent>())
        {
            const auto& collider = entity.GetComponent<BoxCollider2DComponent>();
            entityJson["boxCollider2D"] = {
                { "offset",      ToJson(collider.Offset) },
                { "size",        ToJson(collider.Size)   },
                { "density",     collider.Density        },
                { "friction",    collider.Friction       },
                { "restitution", collider.Restitution    },
                { "isSensor",    collider.IsSensor       }
            };
        }

        if (entity.HasComponent<CircleCollider2DComponent>())
        {
            const auto& collider = entity.GetComponent<CircleCollider2DComponent>();
            entityJson["circleCollider2D"] = {
                { "offset",      ToJson(collider.Offset) },
                { "radius",      collider.Radius         },
                { "density",     collider.Density        },
                { "friction",    collider.Friction       },
                { "restitution", collider.Restitution    },
                { "isSensor",    collider.IsSensor       }
            };
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

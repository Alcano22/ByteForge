#include "Engine/Scene/SceneSerializer.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/Entity.h"
#include "Engine/Scene/Components.h"
#include "Engine/Core/FileSystem.h"
#include "Engine/Assets/AssetManager.h"
#include "Scripting/ScriptValueJson.h"

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
    using namespace ByteForge::JsonValue;

    void WriteMaterial(nlohmann::json& json, const Ref<PhysicsMaterialAsset>& material)
    {
        if (material && material->IsFileBacked())
            json["material"] = static_cast<u64>(material->GetHandle());
    }

    Ref<PhysicsMaterialAsset> ReadMaterial(const nlohmann::json& json)
    {
        if (!json.contains("material"))
            return nullptr;

        return AssetManager::LoadPhysicsMaterial(UUID(json.at("material").get<u64>()));
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
        RemoveIfPresent<CameraComponent>(entity);
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
            transform.Rotation = t.at("rotation").get<f32>();
            transform.Scale = ToVec2(t.at("scale"));
        }

        if (entityJson.contains("spriteRenderer"))
        {
            const auto& s = entityJson.at("spriteRenderer");
            auto& sprite = entity.AddComponent<SpriteRendererComponent>();
            sprite.Color = ToVec4(s.at("color"));
            sprite.SortingLayer = s.value("sortingLayer", 0);

            if (s.contains("sprite"))
            {
                const auto& sp = s.at("sprite");
                sprite.Sprite = MakeRef<Sprite>(AssetManager::LoadTexture2D(UUID(sp.at("texture").get<u64>())),
                                                ToVec2(sp.at("uvMin")), ToVec2(sp.at("uvMax")));
            }
        }

        if (entityJson.contains("camera"))
        {
            const auto& c = entityJson.at("camera");
            const CameraComponent defaults;
            auto& camera = entity.AddComponent<CameraComponent>();
            camera.Primary = c.value("primary", defaults.Primary);
            camera.OrthographicSize = c.value("orthographicSize", defaults.OrthographicSize);
            camera.Near = c.value("near", defaults.Near);
            camera.Far = c.value("far", defaults.Far);
            if (c.contains("clearColor"))
                camera.ClearColor = ToVec4(c.at("clearColor"));
            camera.UseFixedAspect = c.value("useFixedAspect", defaults.UseFixedAspect);
            camera.FixedAspect = c.value("fixedAspect", defaults.FixedAspect);
        }

        if (entityJson.contains("rigidbody2D"))
        {
            const auto& r = entityJson.at("rigidbody2D");
            auto& rb = entity.AddComponent<Rigidbody2DComponent>();
            rb.Type = ToBodyType(r.value("type", std::string("Static")));
            rb.FixedRotation = r.value("fixedRotation", false);
            rb.GravityScale = r.value("gravityScale", 1.0f);
            rb.Layer = std::min<u32>(r.value("layer", PhysicsLayers::DefaultLayer), PhysicsLayers::Count - 1);
            rb.CollisionMask = r.value("collisionMask", PhysicsLayers::AllLayers);
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
            source.Clip = UUID(a.value("clip", u64{ 0 }));
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

    Entity CreateFromJson(Scene& scene, const nlohmann::json& data)
    {
        const Entity entity = scene.CreateEntityWithUUID(UUID(data.at("uuid").get<u64>()),
                                                         data.value("tag", std::string("Entity")));

        ReadComponents(entity, data);
        return entity;
    }

    void LinkToParent(Scene& scene, const Entity entity, const nlohmann::json& data)
    {
        if (!data.contains("parent")) return;

        const Entity parent = scene.FindEntityByUUID(UUID(data.at("parent").get<u64>()));
        if (!parent.IsValid() || parent == entity || parent.IsDescendantOf(entity)) return;

        entity.SetParent(parent, false);
    }

    void SerializeHierarchy(const Entity entity, nlohmann::json& out)
    {
        out.push_back(SceneSerializer::SerializeEntity(entity));

        for (const Entity child : entity.GetChildren())
            SerializeHierarchy(child, out);
    }
}

namespace ByteForge
{
    nlohmann::json SceneSerializer::Serialize(Scene& scene)
    {
        nlohmann::json data;
        data["version"] = 1;
        data["entities"] = nlohmann::json::array();

        for (const Entity root : scene.GetRootEntities())
            SerializeHierarchy(root, data["entities"]);

        return data;
    }

    void SceneSerializer::Deserialize(Scene& scene, const nlohmann::json& data)
    {
        scene.Clear();

        if (!data.contains("entities")) return;

        for (const auto& entityJson : data.at("entities"))
            CreateFromJson(scene, entityJson);

        for (const auto& entityJson : data.at("entities"))
        {
            const Entity entity = scene.FindEntityByUUID(UUID(entityJson.at("uuid").get<u64>()));
            LinkToParent(scene, entity, entityJson);
        }
    }

    void SceneSerializer::SerializeToFile(Scene& scene, const std::string& filepath)
    {
        if (const auto written = WriteJsonFile(filepath, Serialize(scene)); !written)
            throw std::runtime_error(std::format("SceneSerializer::SerializeToFile: {}", written.error()));
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
        entityJson["uuid"] = static_cast<u64>(entity.GetUUID());
        entityJson["tag"] = entity.GetTag();

        if (const Entity parent = entity.GetParent(); parent.IsValid())
            entityJson["parent"] = static_cast<u64>(parent.GetUUID());

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
            spriteJson["sortingLayer"] = sprite.SortingLayer;

            if (sprite.Sprite)
            {
                spriteJson["sprite"] = {
                    { "texture", static_cast<u64>(sprite.Sprite->GetTextureAsset()->GetHandle()) },
                    { "uvMin",   ToJson(sprite.Sprite->GetUVMin())                                    },
                    { "uvMax",   ToJson(sprite.Sprite->GetUVMax())                                    }
                };
            }

            entityJson["spriteRenderer"] = std::move(spriteJson);
        }

        if (entity.HasComponent<CameraComponent>())
        {
            const auto& camera = entity.GetComponent<CameraComponent>();
            entityJson["camera"] = {
                { "primary",          camera.Primary          },
                { "orthographicSize", camera.OrthographicSize },
                { "near",             camera.Near             },
                { "far",              camera.Far              },
                { "clearColor",       ToJson(camera.ClearColor) },
                { "useFixedAspect",   camera.UseFixedAspect   },
                { "fixedAspect",      camera.FixedAspect      }
            };
        }

        if (entity.HasComponent<Rigidbody2DComponent>())
        {
            const auto& rb = entity.GetComponent<Rigidbody2DComponent>();
            nlohmann::json rigidbodyJson{
                { "type",          ToString(rb.Type) },
                { "fixedRotation", rb.FixedRotation  },
                { "gravityScale",  rb.GravityScale   },
                { "layer",         rb.Layer          },
                { "collisionMask", rb.CollisionMask  }
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

            if (static_cast<u64>(source.Clip) != 0)
                sourceJson["clip"] = static_cast<u64>(source.Clip);

            entityJson["audioSource"] = std::move(sourceJson);
        }

        if (entity.HasComponent<ScriptComponent>())
        {
            const auto& script = entity.GetComponent<ScriptComponent>();

            nlohmann::json fields = nlohmann::json::object();
            for (const auto& [name, value] : script.Fields)
                fields[name] = TypedValueToJson(value);

            entityJson["script"] = { { "class", script.ClassName }, { "fields", std::move(fields) } };
        }

        return entityJson;
    }

    Entity SceneSerializer::DeserializeEntity(Scene& scene, const nlohmann::json& data)
    {
        const Entity entity = CreateFromJson(scene, data);
        LinkToParent(scene, entity, data);
        return entity;
    }

    void SceneSerializer::ApplyEntity(const Entity entity, const nlohmann::json& data)
    {
        RemoveSerializableComponents(entity);
        ReadComponents(entity, data);
    }

    nlohmann::json SceneSerializer::SerializeSubtree(const Entity root)
    {
        nlohmann::json entities = nlohmann::json::array();
        SerializeHierarchy(root, entities);
        return entities;
    }

    Entity SceneSerializer::DeserializeSubtree(Scene& scene, const nlohmann::json& entities)
    {
        Entity root;

        for (const auto& entityJson : entities)
        {
            const Entity entity = DeserializeEntity(scene, entityJson);
            if (!root.IsValid())
                root = entity;
        }

        return root;
    }
}

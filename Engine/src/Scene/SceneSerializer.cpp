#include "Engine/Scene/SceneSerializer.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/Entity.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Core/FileSystem.h"

#include <format>
#include <fstream>
#include <stdexcept>
#include <string>

namespace ByteForge
{
    namespace
    {
        void ReadComponents(const Entity entity, const nlohmann::json& entityJson)
        {
            entity.GetComponent<TagComponent>().Tag = entityJson.value("tag", std::string("Entity"));

            ComponentRegistry::ReadAll(entity, entityJson);
        }

        Entity CreateFromJson(Scene& scene, const nlohmann::json& data)
        {
            const Entity entity = scene.CreateEntityWithUUID(UUID(data.at("uuid").get<u64>()),
                                                             data.value("tag", std::string("Entity")));

            ReadComponents(entity, data);
            return entity;
        }

        void LinkToParent(const Scene& scene, const Entity entity, const nlohmann::json& data)
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

    void SceneSerializer::SerializeToFile(Scene& scene, const fs::path& file)
    {
        if (const auto written = WriteJsonFile(file, Serialize(scene)); !written)
            throw Error("SceneSerializer::SerializeToFile: {}", written.error());
    }

    bool SceneSerializer::DeserializeFromFile(Scene& scene, const fs::path& file)
    {
        const Result<nlohmann::json> data = ReadJsonFile(file);
        if (!data)
            return false;

        Deserialize(scene, *data);
        return true;
    }

    nlohmann::json SceneSerializer::SerializeEntity(const Entity entity)
    {
        nlohmann::json entityJson;
        entityJson["uuid"] = static_cast<u64>(entity.GetUUID());
        entityJson["tag"] = entity.GetTag();

        if (const Entity parent = entity.GetParent(); parent.IsValid())
            entityJson["parent"] = static_cast<u64>(parent.GetUUID());

        ComponentRegistry::WriteAll(entity, entityJson);

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
        ComponentRegistry::RemoveAll(entity);
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

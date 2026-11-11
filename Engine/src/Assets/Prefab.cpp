#include "Engine/Assets/Prefab.h"
#include "Engine/Assets/AssetRegistry.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/SceneSerializer.h"
#include "Scripting/ScriptValueJson.h"

#include <nlohmann/json.hpp>

#include <exception>
#include <format>
#include <fstream>
#include <system_error>
#include <unordered_map>

namespace ByteForge
{
    using namespace ByteForge::JsonValue;

    namespace
    {
        constexpr i32 FormatVersion = 1;

        std::unordered_map<UUID, nlohmann::json> s_Cache;

        std::expected<const nlohmann::json*, std::string> LoadEntities(const UUID handle)
        {
            if (const auto it = s_Cache.find(handle); it != s_Cache.end())
                return &it->second;

            AssetMetadata metadata;
            if (!AssetRegistry::TryGetMetadata(handle, metadata) || metadata.Type != AssetType::Prefab)
                return std::unexpected(std::format("{} is not a known prefab", static_cast<u64>(handle)));

            const std::string name = metadata.Path.generic_string();

            std::ifstream file(AssetRegistry::GetAssetRoot() / metadata.Path);
            if (!file.is_open())
                return std::unexpected(std::format("cannot open '{}'", name));

            nlohmann::json data = nlohmann::json::parse(file, nullptr, false);
            if (!data.is_object() || !data.contains("entities") || !data.at("entities").is_array() ||
                data.at("entities").empty())
            {
                return std::unexpected(std::format("'{}' is not a valid prefab", name));
            }

            try
            {
                for (const auto& entityJson : data.at("entities"))
                    static_cast<void>(entityJson.at("uuid").get<u64>());
            } catch (const nlohmann::json::exception& e)
            {
                return std::unexpected(std::format("'{}' has invalid entities: {}", name, e.what()));
            }

            return &s_Cache.insert_or_assign(handle, std::move(data.at("entities"))).first->second;
        }
    }

    std::expected<UUID, std::string> Prefab::Create(const Entity root, const fs::path& relativePath)
    {
        if (!root.IsValid())
            return std::unexpected("the entity is not valid");

        const fs::path path = AssetRegistry::GetAssetRoot() / relativePath;

        std::error_code error;
        if (fs::exists(path, error))
            return std::unexpected(std::format("'{}' already exists", relativePath.generic_string()));

        nlohmann::json entities = SceneSerializer::SerializeSubtree(root);

        nlohmann::json& rootJson = entities.at(0);
        rootJson.erase("parent");

        const TransformComponent world = root.GetWorldTransform();
        rootJson["transform"] = {
            { "position", ToJson(world.Position) },
            { "rotation", world.Rotation         },
            { "scale",    ToJson(world.Scale)    }
        };

        {
            std::ofstream file(path);
            if (!file.is_open())
                return std::unexpected(std::format("cannot open '{}' for writing", relativePath.generic_string()));

            const nlohmann::json data{ { "version", FormatVersion }, { "entities", std::move(entities) } };
            file << data.dump(4);
        }

        return AssetRegistry::Import(relativePath);
    }

    std::expected<Entity, std::string> Prefab::Instantiate(Scene& scene, const UUID handle, const Entity parent,
                                                           const std::optional<glm::vec2> position)
    {
        const auto loaded = LoadEntities(handle);
        if (!loaded)
            return std::unexpected(loaded.error());

        nlohmann::json entities = **loaded;

        std::unordered_map<UUID, UUID> remap;
        for (const auto& entityJson : entities)
            remap.emplace(UUID(entityJson.at("uuid").get<u64>()), UUID());

        for (auto& entityJson : entities)
        {
            entityJson["uuid"] = static_cast<u64>(remap.at(UUID(entityJson.at("uuid").get<u64>())));

            if (!entityJson.contains("parent")) continue;

            if (const auto it = remap.find(UUID(entityJson.at("parent").get<u64>())); it != remap.end())
                entityJson["parent"] = static_cast<u64>(it->second);
            else
                entityJson.erase("parent");
        }

        Entity root;
        try
        {
            root = SceneSerializer::DeserializeSubtree(scene, entities);
        } catch (const std::exception& e)
        {
            for (const auto& entry : remap)
            {
                if (const Entity created = scene.FindEntityByUUID(entry.second); created.IsValid())
                    scene.DestroyEntity(created);
            }

            return std::unexpected(std::format("cannot instantiate the prefab: {}", e.what()));
        }

        for (const auto& entry : remap)
        {
            const Entity created = scene.FindEntityByUUID(entry.second);
            auto* script = created.TryGetComponent<ScriptComponent>();
            if (script == nullptr) continue;

            for (auto& [name, value] : script->Fields)
            {
                auto* reference = std::get_if<EntityRef>(&value);
                if (reference == nullptr) continue;

                if (const auto it = remap.find(reference->Id); it != remap.end())
                    reference->Id = it->second;
            }
        }

        if (position)
        {
            TransformComponent world = root.GetWorldTransform();
            world.Position.x = position->x;
            world.Position.y = position->y;
            root.SetWorldTransform(world);
        }

        if (parent.IsValid())
            root.SetParent(parent, true);

        return root;
    }

    void Prefab::Invalidate(const UUID handle) { s_Cache.erase(handle); }

    void Prefab::Clear() { s_Cache.clear(); }
}

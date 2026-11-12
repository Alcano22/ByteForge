#include "Engine/Assets/Prefab.h"
#include "Engine/Assets/AssetRegistry.h"
#include "Engine/Core/FileSystem.h"
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

        struct Prototype
        {
            Scope<Scene> Entities;
            Entity Root;
        };

        std::unordered_map<UUID, Prototype> s_Cache;

        Result<const Prototype*> LoadPrototype(const UUID handle)
        {
            if (const auto it = s_Cache.find(handle); it != s_Cache.end())
                return &it->second;

            AssetMetadata metadata;
            if (!AssetRegistry::TryGetMetadata(handle, metadata) || metadata.Type != AssetType::Prefab)
                return std::unexpected(std::format("{} is not a known prefab", static_cast<u64>(handle)));

            const std::string name = metadata.Path.generic_string();

            const Result<nlohmann::json> data = AssetRegistry::ReadJson(handle);
            if (!data)
                return std::unexpected(data.error());

            if (!data->is_object() || !data->contains("entities") || !data->at("entities").is_array() ||
                data->at("entities").empty())
            {
                return std::unexpected(std::format("'{}' is not a valid prefab", name));
            }

            Prototype prototype{ .Entities = MakeScope<Scene>() };
            try
            {
                prototype.Root = SceneSerializer::DeserializeSubtree(*prototype.Entities, data->at("entities"));
            } catch (const std::exception& e)
            {
                return std::unexpected(std::format("'{}' cannot be loaded: {}", name, e.what()));
            }

            return &s_Cache.insert_or_assign(handle, std::move(prototype)).first->second;
        }
    }

    Result<UUID> Prefab::Create(const Entity root, const fs::path& relativePath)
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

        const nlohmann::json data{ { "version", FormatVersion }, { "entities", std::move(entities) } };
        if (const auto written = WriteJsonFile(path, data); !written)
            return std::unexpected(written.error());

        return AssetRegistry::Import(relativePath);
    }

    Result<Entity> Prefab::Instantiate(Scene& scene, const UUID handle, const Entity parent,
                                       const std::optional<glm::vec2> position)
    {
        const Result<const Prototype*> prototype = LoadPrototype(handle);
        if (!prototype)
            return std::unexpected(prototype.error());

        const Entity root = scene.CopySubtree((*prototype)->Root);

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

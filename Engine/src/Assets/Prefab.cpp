#include "Engine/Assets/Prefab.h"
#include "Engine/Assets/AssetRegistry.h"
#include "Engine/Core/FileSystem.h"
#include "Engine/Scene/ComponentRegistry.h"
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
#include <utility>
#include <vector>

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

        const char* InstanceKey()
        {
            return ComponentRegistry::FindByType(ComponentTypeId<PrefabInstanceComponent>())->Key;
        }

        void LinkSubtree(const Entity entity, const UUID handle, const bool root)
        {
            const PrefabInstanceComponent link{ .Prefab = handle, .Source = entity.GetUUID(), .Root = root };

            if (auto* existing = entity.TryGetComponent<PrefabInstanceComponent>())
                *existing = link;
            else
                entity.AddComponent<PrefabInstanceComponent>(link);

            for (const Entity child : entity.GetChildren())
                LinkSubtree(child, handle, false);
        }

        void UnlinkSubtree(const Entity entity, const UUID handle, const bool first)
        {
            const auto* link = entity.TryGetComponent<PrefabInstanceComponent>();
            if (link == nullptr || link->Prefab != handle || (!first && link->Root)) return;

            entity.RemoveComponent<PrefabInstanceComponent>();

            for (const Entity child : entity.GetChildren())
                UnlinkSubtree(child, handle, false);
        }

        nlohmann::json MakePrefabData(const Entity root)
        {
            nlohmann::json entities = SceneSerializer::SerializeSubtree(root);

            for (nlohmann::json& entity : entities)
                entity.erase(InstanceKey());

            nlohmann::json& rootJson = entities.at(0);
            rootJson.erase("parent");

            const TransformComponent world = root.GetWorldTransform();
            rootJson["transform"] = {
                { "position", ToJson(world.Position) },
                { "rotation", world.Rotation         },
                { "scale",    ToJson(world.Scale)    }
            };

            return { { "version", FormatVersion }, { "entities", std::move(entities) } };
        }

        Result<nlohmann::json> ReadEntities(const UUID handle, std::string& name)
        {
            AssetMetadata metadata;
            if (!AssetRegistry::TryGetMetadata(handle, metadata) || metadata.Type != AssetType::Prefab)
                return std::unexpected(std::format("{} is not a known prefab", static_cast<u64>(handle)));

            name = metadata.Path.generic_string();

            Result<nlohmann::json> data = AssetRegistry::ReadJson(handle);
            if (!data)
                return std::unexpected(data.error());

            if (!data->is_object() || !data->contains("entities") ||
                !data->at("entities").is_array() || data->at("entities").empty())
            {
                return std::unexpected(std::format("'{}' is not a valid prefab", name));
            }

            return std::move(data->at("entities"));
        }

        Result<const Prototype*> LoadPrototype(const UUID handle)
        {
            if (const auto it = s_Cache.find(handle); it != s_Cache.end())
                return &it->second;

            std::string name;
            const Result<nlohmann::json> entities = ReadEntities(handle, name);
            if (!entities)
                return std::unexpected(entities.error());

            Prototype prototype{ .Entities = MakeScope<Scene>() };
            try
            {
                prototype.Root = SceneSerializer::DeserializeSubtree(*prototype.Entities, *entities);
            } catch (const std::exception& e)
            {
                return std::unexpected(std::format("'{}' cannot be loaded: {}", name, e.what()));
            }

            LinkSubtree(prototype.Root, handle, true);

            return &s_Cache.insert_or_assign(handle, std::move(prototype)).first->second;
        }

        void CollectMembers(const Entity entity, const UUID handle, const bool first,
                            std::unordered_map<UUID, Entity>& members)
        {
            const auto* link = entity.TryGetComponent<PrefabInstanceComponent>();
            if (link == nullptr || link->Prefab != handle || (!first && link->Root)) return;

            members.emplace(link->Source, entity);

            for (const Entity child : entity.GetChildren())
                CollectMembers(child, handle, false, members);
        }

        struct SyncPair
        {
            Entity Source;
            Entity Target;
            i32 SiblingIndex = 0;
        };

        void PairEntities(Scene& scene, const Entity source, const i32 siblingIndex,
                          const std::unordered_map<UUID, Entity>& members,
                          std::unordered_map<UUID, UUID>& remap, std::vector<SyncPair>& pairs)
        {
            const auto existing = members.find(source.GetUUID());
            const Entity target = existing != members.end() ? existing->second : scene.CreateEntity(source.GetTag());

            remap.emplace(source.GetUUID(), target.GetUUID());
            pairs.push_back({ .Source = source, .Target = target, .SiblingIndex = siblingIndex });

            i32 index = 0;
            for (const Entity child : source.GetChildren())
                PairEntities(scene, child, index++, members, remap, pairs);
        }

        void SyncInstance(Scene& scene, const Entity instanceRoot, const Prototype& prototype, const UUID handle)
        {
            std::unordered_map<UUID, Entity> members;
            CollectMembers(instanceRoot, handle, true, members);

            const TransformComponent rootTransform = instanceRoot.GetComponent<TransformComponent>();
            const std::string rootName = instanceRoot.GetTag();

            members.insert_or_assign(prototype.Root.GetUUID(), instanceRoot);

            std::unordered_map<UUID, UUID> remap;
            std::vector<SyncPair> pairs;
            PairEntities(scene, prototype.Root, 0, members, remap, pairs);

            for (const SyncPair& pair : pairs)
            {
                SceneSerializer::ApplyEntity(pair.Target, SceneSerializer::SerializeEntity(pair.Source));

                if (pair.Source == prototype.Root) continue;

                const Entity parent = scene.FindEntityByUUID(remap.at(pair.Source.GetParent().GetUUID()));
                pair.Target.SetParent(parent, false, pair.SiblingIndex);
            }

            instanceRoot.GetComponent<TransformComponent>() = rootTransform;
            instanceRoot.GetComponent<TagComponent>().Tag = rootName;

            for (const auto& [source, entity] : members)
            {
                if (!remap.contains(source) && entity != instanceRoot && entity.IsValid())
                    scene.DestroyEntity(entity);
            }

            scene.RemapEntityReferences(instanceRoot, remap);
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

        if (const auto written = WriteJsonFile(path, MakePrefabData(root)); !written)
            return std::unexpected(written.error());

        return AssetRegistry::Import(relativePath);
    }

    Result<void> Prefab::Save(const UUID handle, const Entity root)
    {
        if (!root.IsValid())
            return std::unexpected("the entity is not valid");
        if (!Exists(handle))
            return std::unexpected(std::format("{} is not a known prefab", static_cast<u64>(handle)));

        if (const auto written = WriteJsonFile(AssetRegistry::Resolve(handle), MakePrefabData(root)); !written)
            return std::unexpected(written.error());

        Invalidate(handle);
        return {};
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

    Result<Entity> Prefab::LoadForEditing(Scene& scene, const UUID handle)
    {
        std::string name;
        const Result<nlohmann::json> entities = ReadEntities(handle, name);
        if (!entities)
            return std::unexpected(entities.error());

        try
        {
            return SceneSerializer::DeserializeSubtree(scene, *entities);
        } catch (const std::exception& e)
        {
            return std::unexpected(std::format("'{}' cannot be loaded: {}", name, e.what()));
        }
    }

    usize Prefab::SyncInstances(Scene& scene, const UUID handle)
    {
        const Result<const Prototype*> prototype = LoadPrototype(handle);
        if (!prototype)
            return 0;

        std::vector<Entity> roots;
        scene.Each<PrefabInstanceComponent>([&](const Entity entity, const PrefabInstanceComponent& link)
        {
            if (link.Prefab == handle && link.Root)
                roots.push_back(entity);
        });

        for (const Entity root : roots)
            SyncInstance(scene, root, **prototype, handle);

        return roots.size();
    }

    void Prefab::Link(const Entity root, const UUID handle)
    {
        if (root.IsValid())
            LinkSubtree(root, handle, true);
    }

    void Prefab::Unlink(const Entity entity)
    {
        const auto* link = entity.TryGetComponent<PrefabInstanceComponent>();
        if (link == nullptr) return;

        const UUID handle = link->Prefab;
        UnlinkSubtree(entity, handle, true);
    }

    Entity Prefab::GetInstanceRoot(const Entity entity)
    {
        Entity current = entity;

        while (true)
        {
            const auto* link = current.TryGetComponent<PrefabInstanceComponent>();
            if (link == nullptr)
                return {};
            if (link->Root)
                return current;

            const Entity parent = current.GetParent();
            const auto* parentLink = parent.TryGetComponent<PrefabInstanceComponent>();
            if (parentLink == nullptr || parentLink->Prefab != link->Prefab)
                return {};

            current = parent;
        }
    }

    bool Prefab::Exists(const UUID handle)
    {
        AssetMetadata metadata;
        return handle.IsSet() && AssetRegistry::TryGetMetadata(handle, metadata) && metadata.Type == AssetType::Prefab;
    }

    void Prefab::Invalidate(const UUID handle) { s_Cache.erase(handle); }

    void Prefab::Clear() { s_Cache.clear(); }
}

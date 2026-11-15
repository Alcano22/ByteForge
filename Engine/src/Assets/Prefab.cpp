#include "Engine/Assets/Prefab.h"
#include "Engine/Assets/AssetRegistry.h"
#include "Engine/Core/FileSystem.h"
#include "Engine/Scene/ComponentRegistry.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/SceneSerializer.h"
#include "Engine/Scripting/ScriptField.h"
#include "Scripting/ScriptValueJson.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <exception>
#include <format>
#include <fstream>
#include <string>
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

        const char* ScriptKey()
        {
            return ComponentRegistry::FindByType(ComponentTypeId<ScriptComponent>())->Key;
        }

        void SetLink(const Entity entity, PrefabInstanceComponent link)
        {
            if (auto* existing = entity.TryGetComponent<PrefabInstanceComponent>())
                *existing = std::move(link);
            else
                entity.AddComponent<PrefabInstanceComponent>(std::move(link));
        }

        void LinkSubtree(const Entity entity, const UUID handle, const bool root)
        {
            SetLink(entity, { .Prefab = handle, .Source = entity.GetUUID(), .Root = root });

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

        std::unordered_map<UUID, UUID> MakeToSourceMap(const std::unordered_map<UUID, Entity>& members)
        {
            std::unordered_map<UUID, UUID> toSource;
            for (const auto& [source, entity] : members)
                toSource.emplace(entity.GetUUID(), source);
            return toSource;
        }

        void RemapScriptFields(nlohmann::json& script, const std::unordered_map<UUID, UUID>& remap)
        {
            const auto fields = script.find("fields");
            if (fields == script.end() || !fields->is_object()) return;

            const std::string entityType(ScriptFieldTypeName(ScriptFieldType::Entity));

            for (auto& [name, field] : fields->items())
            {
                if (!field.is_object() || field.value("type", std::string()) != entityType || !field.contains("value"))
                    continue;

                nlohmann::json& value = field.at("value");
                nlohmann::json& id = value.is_object() ? value["entity"] : value;
                if (!id.is_number_unsigned()) continue;

                if (const auto it = remap.find(UUID(id.get<u64>())); it != remap.end())
                    id = static_cast<u64>(it->second);
            }
        }

        nlohmann::json MakeComparable(nlohmann::json entity, const std::unordered_map<UUID, UUID>* toSource)
        {
            entity.erase("uuid");
            entity.erase("parent");
            entity.erase(InstanceKey());

            const auto scripts = entity.find(ScriptKey());
            if (scripts != entity.end() && scripts->is_array())
            {
                nlohmann::json byId = nlohmann::json::object();
                i64 index = 0;

                for (nlohmann::json& script : *scripts)
                {
                    if (!script.is_object()) continue;

                    const std::string id = std::to_string(script.value("id", u64{ 0 }));
                    script.erase("id");
                    script["index"] = index++;

                    if (toSource != nullptr)
                        RemapScriptFields(script, *toSource);

                    byId[id] = std::move(script);
                }

                *scripts = std::move(byId);
            }

            return entity;
        }

        nlohmann::json MakeEntityJson(nlohmann::json comparable)
        {
            const auto scripts = comparable.find(ScriptKey());
            if (scripts != comparable.end() && scripts->is_object())
            {
                std::vector<std::pair<i64, nlohmann::json>> ordered;

                for (auto& [id, script] : scripts->items())
                {
                    if (!script.is_object()) continue;

                    const i64 index = script.value("index", i64{ 0 });
                    script.erase("index");
                    script["id"] = std::stoull(id);
                    ordered.emplace_back(index, std::move(script));
                }

                std::ranges::stable_sort(ordered, {}, &std::pair<i64, nlohmann::json>::first);

                nlohmann::json array = nlohmann::json::array();
                for (auto& [index, script] : ordered)
                    array.push_back(std::move(script));

                *scripts = std::move(array);
            }

            return comparable;
        }

        nlohmann::json MakeMergePatch(const nlohmann::json& base, const nlohmann::json& target)
        {
            if (!base.is_object() || !target.is_object())
                return target;

            nlohmann::json patch = nlohmann::json::object();

            for (const auto& [key, value] : target.items())
            {
                const auto it = base.find(key);
                if (it == base.end())
                    patch[key] = value;
                else if (*it != value)
                    patch[key] = MakeMergePatch(*it, value);
            }

            for (const auto& [key, value] : base.items())
            {
                if (!target.contains(key))
                    patch[key] = nullptr;
            }

            return patch;
        }

        nlohmann::json ComputeOverrides(const Entity source, const Entity instance,
                                        const std::unordered_map<UUID, UUID>& toSource)
        {
            return MakeMergePatch(MakeComparable(SceneSerializer::SerializeEntity(source), nullptr),
                                  MakeComparable(SceneSerializer::SerializeEntity(instance), &toSource));
        }

        std::vector<Entity> FindInstanceRoots(Scene& scene, const std::optional<UUID> handle = std::nullopt)
        {
            std::vector<Entity> roots;
            scene.Each<PrefabInstanceComponent>([&](const Entity entity, const PrefabInstanceComponent& link)
            {
                if (link.Root && (!handle || link.Prefab == *handle))
                    roots.push_back(entity);
            });
            return roots;
        }

        struct SyncPair
        {
            Entity Source;
            Entity Target;
            i32 SiblingIndex = 0;
            nlohmann::json Overrides = nlohmann::json::object();
        };

        void PairEntities(Scene& scene, const Entity source, const i32 siblingIndex,
                          const std::unordered_map<UUID, Entity>& members,
                          std::unordered_map<UUID, UUID>& remap, std::vector<SyncPair>& pairs)
        {
            const auto existing = members.find(source.GetUUID());

            SyncPair pair{ .Source = source, .SiblingIndex = siblingIndex };
            if (existing != members.end())
            {
                pair.Target = existing->second;

                const auto* link = pair.Target.TryGetComponent<PrefabInstanceComponent>();
                if (link != nullptr && link->Overrides.is_object())
                    pair.Overrides = link->Overrides;
            } else
                pair.Target = scene.CreateEntity(source.GetTag());

            remap.emplace(source.GetUUID(), pair.Target.GetUUID());
            pairs.push_back(std::move(pair));

            i32 index = 0;
            for (const Entity child : source.GetChildren())
                PairEntities(scene, child, index++, members, remap, pairs);
        }

        void SyncInstance(Scene& scene, const Entity instanceRoot, const Prototype& prototype, const UUID handle)
        {
            std::unordered_map<UUID, Entity> members;
            CollectMembers(instanceRoot, handle, true, members);
            members.insert_or_assign(prototype.Root.GetUUID(), instanceRoot);

            const std::unordered_map<UUID, UUID> toSource = MakeToSourceMap(members);
            for (const auto& [source, entity] : members)
            {
                auto& link = entity.GetComponent<PrefabInstanceComponent>();
                if (!link.Overrides.is_null()) continue;

                const Entity sourceEntity = prototype.Entities->FindEntityByUUID(source);
                link.Overrides = sourceEntity.IsValid() ? ComputeOverrides(sourceEntity, entity, toSource)
                                                        : nlohmann::json::object();
            }

            std::unordered_map<UUID, UUID> remap;
            std::vector<SyncPair> pairs;
            PairEntities(scene, prototype.Root, 0, members, remap, pairs);

            for (const SyncPair& pair : pairs)
            {
                nlohmann::json merged = MakeComparable(SceneSerializer::SerializeEntity(pair.Source), nullptr);
                merged.merge_patch(pair.Overrides);

                try
                {
                    SceneSerializer::ApplyEntity(pair.Target, MakeEntityJson(std::move(merged)));
                } catch (const std::exception& e)
                {
                    CORE_WARN("Prefab instance '{}': overrides of '{}' could not be applied ({}), using the prefab's data",
                              instanceRoot.GetTag(), pair.Source.GetTag(), e.what());
                    SceneSerializer::ApplyEntity(pair.Target,
                                                 MakeEntityJson(MakeComparable(SceneSerializer::SerializeEntity(pair.Source), nullptr)));
                }

                SetLink(pair.Target, {
                    .Prefab    = handle,
                    .Source    = pair.Source.GetUUID(),
                    .Root      = pair.Source == prototype.Root,
                    .Overrides = pair.Overrides
                });

                if (pair.Source == prototype.Root) continue;

                const Entity parent = scene.FindEntityByUUID(remap.at(pair.Source.GetParent().GetUUID()));
                pair.Target.SetParent(parent, false, pair.SiblingIndex);
            }

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

        const std::vector<Entity> roots = FindInstanceRoots(scene, handle);
        for (const Entity root : roots)
            SyncInstance(scene, root, **prototype, handle);

        return roots.size();
    }

    usize Prefab::SyncAllInstances(Scene& scene)
    {
        std::vector<UUID> handles;
        for (const Entity root : FindInstanceRoots(scene))
        {
            const UUID handle = root.GetComponent<PrefabInstanceComponent>().Prefab;
            if (std::ranges::find(handles, handle) == handles.end())
                handles.push_back(handle);
        }

        usize updated = 0;
        for (const UUID handle : handles)
            updated += SyncInstances(scene, handle);
        return updated;
    }

    void Prefab::RecordOverrides(Scene& scene)
    {
        for (const Entity root : FindInstanceRoots(scene))
        {
            const UUID handle = root.GetComponent<PrefabInstanceComponent>().Prefab;

            const Result<const Prototype*> prototype = LoadPrototype(handle);
            if (!prototype) continue;

            std::unordered_map<UUID, Entity> members;
            CollectMembers(root, handle, true, members);
            const std::unordered_map<UUID, UUID> toSource = MakeToSourceMap(members);

            for (const auto& [source, entity] : members)
            {
                const Entity sourceEntity = (*prototype)->Entities->FindEntityByUUID(source);
                if (!sourceEntity.IsValid()) continue;

                entity.GetComponent<PrefabInstanceComponent>().Overrides =
                    ComputeOverrides(sourceEntity, entity, toSource);
            }
        }
    }

    nlohmann::json Prefab::GetOverrides(const Entity entity)
    {
        const auto* link = entity.TryGetComponent<PrefabInstanceComponent>();
        if (link == nullptr) return nullptr;

        const Entity root = GetInstanceRoot(entity);
        if (!root.IsValid()) return nullptr;

        const Result<const Prototype*> prototype = LoadPrototype(link->Prefab);
        if (!prototype) return nullptr;

        const Entity sourceEntity = (*prototype)->Entities->FindEntityByUUID(link->Source);
        if (!sourceEntity.IsValid()) return nullptr;

        std::unordered_map<UUID, Entity> members;
        CollectMembers(root, link->Prefab, true, members);

        return ComputeOverrides(sourceEntity, entity, MakeToSourceMap(members));
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

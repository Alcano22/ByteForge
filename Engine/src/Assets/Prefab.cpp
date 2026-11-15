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
#include <array>
#include <exception>
#include <format>
#include <fstream>
#include <ranges>
#include <span>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ByteForge
{
    using namespace ByteForge::JsonValue;

    namespace
    {
        constexpr i32 FormatVersion = 1;

        constexpr std::array<const char*, 2> RootPlacementKeys{ "transform", "tag" };

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

        Result<nlohmann::json> ReadPrefabFile(const UUID handle, std::string& name)
        {
            AssetMetadata metadata;
            if (!AssetRegistry::TryGetMetadata(handle, metadata) || metadata.Type != AssetType::Prefab)
                return std::unexpected(std::format("{} is not a known prefab", static_cast<u64>(handle)));

            name = metadata.Path.generic_string();

            Result<nlohmann::json> data = AssetRegistry::ReadJson(handle);
            if (!data)
                return std::unexpected(data.error());

            if (!data->is_object() || !data->contains("entities") || !data->at("entities").is_array() ||
                data->at("entities").empty())
            {
                return std::unexpected(std::format("'{}' is not a valid prefab", name));
            }

            return data;
        }

        Result<nlohmann::json> ReadEntities(const UUID handle, std::string& name)
        {
            Result<nlohmann::json> data = ReadPrefabFile(handle, name);
            if (!data)
                return std::unexpected(data.error());

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

        template<typename Fn>
        void ForEachEntityReference(nlohmann::json& script, Fn&& fn)
        {
            const auto fields = script.find("fields");
            if (fields == script.end() || !fields->is_object()) return;

            const std::string entityType(ScriptFieldTypeName(ScriptFieldType::Entity));

            for (auto& [name, field] : fields->items())
            {
                if (!field.is_object() || field.value("type", std::string()) != entityType) continue;

                const auto value = field.find("value");
                if (value == field.end()) continue;

                nlohmann::json* id = &*value;
                if (value->is_object())
                {
                    const auto entity = value->find("entity");
                    if (entity == value->end()) continue;
                    id = &*entity;
                }

                if (id->is_number_unsigned())
                    fn(name, *id);
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
                    {
                        ForEachEntityReference(script, [&](const std::string&, nlohmann::json& reference)
                        {
                            if (const auto it = toSource->find(UUID(reference.get<u64>())); it != toSource->end())
                                reference = static_cast<u64>(it->second);
                        });
                    }

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

        struct InstanceContext
        {
            Entity Root;
            UUID Handle;
            const Prototype* Source = nullptr;
            std::unordered_map<UUID, Entity> Members;
        };

        Result<InstanceContext> FindInstance(const Entity entity)
        {
            const auto* link = entity.TryGetComponent<PrefabInstanceComponent>();
            if (link == nullptr)
                return std::unexpected(std::format("'{}' is not part of a prefab instance", entity.GetTag()));

            InstanceContext instance{ .Root = Prefab::GetInstanceRoot(entity), .Handle = link->Prefab };
            if (!instance.Root.IsValid())
                return std::unexpected(std::format("'{}' is detached from its prefab instance", entity.GetTag()));

            const Result<const Prototype*> prototype = LoadPrototype(instance.Handle);
            if (!prototype)
                return std::unexpected(prototype.error());

            instance.Source = *prototype;
            CollectMembers(instance.Root, instance.Handle, true, instance.Members);
            return instance;
        }

        void RecordInstance(const InstanceContext& instance)
        {
            const std::unordered_map<UUID, UUID> toSource = MakeToSourceMap(instance.Members);

            for (const auto& [source, entity] : instance.Members)
            {
                const Entity sourceEntity = instance.Source->Entities->FindEntityByUUID(source);
                if (!sourceEntity.IsValid()) continue;

                entity.GetComponent<PrefabInstanceComponent>().Overrides =
                    ComputeOverrides(sourceEntity, entity, toSource);
            }
        }

        std::vector<std::string> SplitPath(const std::string_view path)
        {
            std::vector<std::string> tokens;
            for (const auto token : std::views::split(path, '/'))
            {
                if (!token.empty())
                    tokens.emplace_back(token.begin(), token.end());
            }
            return tokens;
        }

        const nlohmann::json* FindPath(const nlohmann::json& overrides, const std::vector<std::string>& tokens)
        {
            const nlohmann::json* current = &overrides;
            for (const std::string& token : tokens)
            {
                if (!current->is_object())
                    return nullptr;

                const auto it = current->find(token);
                if (it == current->end())
                    return nullptr;

                current = &*it;
            }
            return current;
        }

        nlohmann::json ExtractPath(const nlohmann::json& overrides, const std::vector<std::string>& tokens)
        {
            nlohmann::json patch = nlohmann::json::object();

            const nlohmann::json* value = FindPath(overrides, tokens);
            if (value == nullptr)
                return patch;

            nlohmann::json* slot = &patch;
            for (const std::string& token : tokens)
                slot = &(*slot)[token];
            *slot = *value;

            return patch;
        }

        bool EraseTokens(nlohmann::json& node, const std::span<const std::string> tokens)
        {
            if (tokens.empty() || !node.is_object()) return false;

            const auto it = node.find(tokens.front());
            if (it == node.end()) return false;

            if (tokens.size() == 1)
            {
                node.erase(it);
                return true;
            }

            if (!EraseTokens(*it, tokens.subspan(1))) return false;

            if (it->is_object() && it->empty())
                node.erase(it);
            return true;
        }

        nlohmann::json SelectOverrides(const nlohmann::json& overrides, const PrefabOverrideScope scope,
                                       const std::vector<std::string>& tokens, const bool root)
        {
            if (!overrides.is_object()) return nlohmann::json::object();
            if (scope == PrefabOverrideScope::Path) return ExtractPath(overrides, tokens);

            nlohmann::json selected = overrides;
            if (root)
            {
                for (const char* key : RootPlacementKeys)
                    selected.erase(key);
            }
            return selected;
        }

        void DropOverrides(nlohmann::json& overrides, const PrefabOverrideScope scope,
                           const std::vector<std::string>& tokens, const bool root)
        {
            if (!overrides.is_object())
            {
                overrides = nlohmann::json::object();
                return;
            }

            if (scope == PrefabOverrideScope::Path)
            {
                EraseTokens(overrides, tokens);
                return;
            }

            nlohmann::json kept = nlohmann::json::object();
            if (root)
            {
                for (const char* key : RootPlacementKeys)
                {
                    if (const auto it = overrides.find(key); it != overrides.end())
                        kept[key] = *it;
                }
            }
            overrides = std::move(kept);
        }

        std::vector<Entity> GetTargets(const InstanceContext& instance, const Entity entity,
                                       const PrefabOverrideScope scope)
        {
            if (scope != PrefabOverrideScope::Instance)
                return { entity };

            std::vector<Entity> targets;
            for (const auto& [source, member] : instance.Members)
                targets.push_back(member);
            return targets;
        }

        nlohmann::json PatchFileEntity(const nlohmann::json& fileEntity, const nlohmann::json& patch,
                                       const std::unordered_set<u64>& prefabIds, const std::string& prefabName)
        {
            nlohmann::json comparable = MakeComparable(fileEntity, nullptr);
            comparable.merge_patch(patch);

            if (const auto scripts = comparable.find(ScriptKey()); scripts != comparable.end() && scripts->is_object())
            {
                for (auto& [id, script] : scripts->items())
                {
                    ForEachEntityReference(script, [&](const std::string& field, nlohmann::json& reference)
                    {
                        const u64 value = reference.get<u64>();
                        if (value == 0 || prefabIds.contains(value)) return;

                        CORE_WARN("Prefab '{}': field '{}' references an entity outside the prefab and is cleared",
                                  prefabName, field);
                        reference = u64{ 0 };
                    });
                }
            }

            nlohmann::json result = MakeEntityJson(std::move(comparable));
            result["uuid"] = fileEntity.at("uuid");
            if (const auto parent = fileEntity.find("parent"); parent != fileEntity.end())
                result["parent"] = *parent;

            return result;
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

        return WriteData(handle, MakePrefabData(root));
    }

    Result<nlohmann::json> Prefab::ReadData(const UUID handle)
    {
        std::string name;
        return ReadPrefabFile(handle, name);
    }

    Result<void> Prefab::WriteData(const UUID handle, const nlohmann::json& data)
    {
        if (!Exists(handle))
            return std::unexpected(std::format("{} is not a known prefab", static_cast<u64>(handle)));

        if (const auto written = WriteJsonFile(AssetRegistry::Resolve(handle), data); !written)
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
            if (const Result<InstanceContext> instance = FindInstance(root))
                RecordInstance(*instance);
        }
    }

    nlohmann::json Prefab::GetOverrides(const Entity entity)
    {
        const Result<InstanceContext> instance = FindInstance(entity);
        if (!instance)
            return nullptr;

        const UUID source = entity.GetComponent<PrefabInstanceComponent>().Source;
        const Entity sourceEntity = instance->Source->Entities->FindEntityByUUID(source);
        if (!sourceEntity.IsValid())
            return nullptr;

        return ComputeOverrides(sourceEntity, entity, MakeToSourceMap(instance->Members));
    }

    std::string Prefab::ComponentOverridePath(const u32 componentTypeId)
    {
        const ComponentDescriptor* descriptor = ComponentRegistry::FindByType(componentTypeId);
        return std::format("/{}", descriptor != nullptr ? descriptor->Key : "?");
    }

    std::string Prefab::ScriptOverridePath(const u64 scriptId)
    {
        return std::format("/{}/{}", ScriptKey(), scriptId);
    }

    bool Prefab::IsOverridden(const nlohmann::json& overrides, const std::string_view path)
    {
        const std::vector<std::string> tokens = SplitPath(path);
        return !tokens.empty() && FindPath(overrides, tokens) != nullptr;
    }

    Result<void> Prefab::ApplyOverrides(const Entity entity, const PrefabOverrideScope scope,
                                        const std::string_view path)
    {
        const std::vector<std::string> tokens = SplitPath(path);
        if (scope == PrefabOverrideScope::Path && tokens.empty())
            return std::unexpected("no override path given");

        const Result<InstanceContext> instance = FindInstance(entity);
        if (!instance)
            return std::unexpected(instance.error());

        Scene& scene = entity.GetScene();
        RecordOverrides(scene);

        std::string name;
        Result<nlohmann::json> data = ReadPrefabFile(instance->Handle, name);
        if (!data)
            return std::unexpected(data.error());

        nlohmann::json& fileEntities = data->at("entities");

        std::unordered_set<u64> prefabIds;
        for (const nlohmann::json& fileEntity : fileEntities)
            prefabIds.insert(fileEntity.value("uuid", u64{ 0 }));

        const std::vector<Entity> targets = GetTargets(*instance, entity, scope);
        std::vector<Entity> applied;

        for (const Entity target : targets)
        {
            const auto& link = target.GetComponent<PrefabInstanceComponent>();

            const nlohmann::json patch = SelectOverrides(link.Overrides, scope, tokens, target == instance->Root);
            if (patch.empty()) continue;

            const auto fileEntity = std::ranges::find_if(fileEntities, [&](const nlohmann::json& candidate)
            {
                return candidate.value("uuid", u64{ 0 }) == static_cast<u64>(link.Source);
            });

            if (fileEntity == fileEntities.end())
            {
                CORE_WARN("Prefab '{}': '{}' is no longer part of the prefab and is skipped", name, target.GetTag());
                continue;
            }

            *fileEntity = PatchFileEntity(*fileEntity, patch, prefabIds, name);
            applied.push_back(target);
        }

        if (applied.empty())
            return {};

        if (const Result<void> written = WriteData(instance->Handle, *data); !written)
            return written;

        for (const Entity target : applied)
        {
            DropOverrides(target.GetComponent<PrefabInstanceComponent>().Overrides, scope, tokens,
                          target == instance->Root);
        }

        SyncInstances(scene, instance->Handle);
        return {};
    }

    Result<void> Prefab::RevertOverrides(const Entity entity, const PrefabOverrideScope scope,
                                         const std::string_view path)
    {
        const std::vector<std::string> tokens = SplitPath(path);
        if (scope == PrefabOverrideScope::Path && tokens.empty())
            return std::unexpected("no override path given");

        const Result<InstanceContext> instance = FindInstance(entity);
        if (!instance)
            return std::unexpected(instance.error());

        RecordInstance(*instance);

        for (const Entity target : GetTargets(*instance, entity, scope))
        {
            DropOverrides(target.GetComponent<PrefabInstanceComponent>().Overrides, scope, tokens,
                          target == instance->Root);
        }

        SyncInstance(entity.GetScene(), instance->Root, *instance->Source, instance->Handle);
        return {};
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

#include "Engine/Scene/Scene.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/Entity.h"
#include "Engine/Scene/Physics2DWorld.h"
#include "Engine/Scene/RaycastHit2D.h"
#include "Engine/Scene/SceneSerializer.h"
#include "Engine/Core/Log.h"
#include "Engine/Assets/AssetRegistry.h"
#include "Engine/Audio/AudioSystem.h"
#include "Engine/Animation/AnimationSystem.h"
#include "Scripting/ScriptRuntime.h"
#include "Engine/Scripting/ScriptField.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <compare>
#include <format>
#include <fstream>
#include <stdexcept>
#include <utility>
#include <unordered_map>
#include <variant>
#include <vector>

namespace ByteForge
{
    namespace
    {
        template<typename... Components>
        struct ComponentList {};

        using DuplicableComponents = ComponentList<TransformComponent, SpriteRendererComponent, CameraComponent,
                                                   Rigidbody2DComponent, BoxCollider2DComponent,
                                                   CircleCollider2DComponent, AudioSourceComponent,
                                                   AnimatorComponent, ScriptComponent>;

        template<typename... Components>
        void CopyComponents(ComponentList<Components...>, const entt::registry& from, const entt::entity source,
                            entt::registry& to, const entt::entity target)
        {
            ([&]
            {
                if (const auto* component = from.try_get<Components>(source))
                {
                    const Components copy = *component;
                    to.emplace_or_replace<Components>(target, copy);
                }
            }(), ...);
        }

        void RemapEntityReferences(const Scene& scene, const Entity root, const std::unordered_map<UUID, UUID>& remap)
        {
            if (auto* script = root.TryGetComponent<ScriptComponent>())
            {
                for (auto& [name, value] : script->Fields)
                {
                    auto* reference = std::get_if<EntityRef>(&value);
                    if (reference == nullptr) continue;

                    if (const auto it = remap.find(reference->Id); it != remap.end())
                        reference->Id = it->second;
                }
            }

            for (const Entity child : scene.GetChildren(root))
                RemapEntityReferences(scene, child, remap);
        }

        struct SpriteDrawItem
        {
            entt::entity Handle;
            i32 Layer;
            TransformComponent World;
        };

        constexpr f32 MinScaleMagnitude = 1e-6f;

        f32 SafeDivide(const f32 value, const f32 divisor)
        {
            return std::abs(divisor) < MinScaleMagnitude ? value : value / divisor;
        }

        TransformComponent ComposeTransforms(const TransformComponent& parentWorld, const TransformComponent& local)
        {
            const f32 cosine = std::cos(parentWorld.Rotation);
            const f32 sine = std::sin(parentWorld.Rotation);
            const glm::vec2 scaled = glm::vec2(local.Position) * parentWorld.Scale;

            TransformComponent world;
            world.Position = {
                parentWorld.Position.x + cosine * scaled.x - sine * scaled.y,
                parentWorld.Position.y + sine * scaled.x + cosine * scaled.y,
                parentWorld.Position.z + local.Position.z
            };
            world.Rotation = parentWorld.Rotation + local.Rotation;
            world.Scale = parentWorld.Scale * local.Scale;
            return world;
        }

        TransformComponent MakeLocalTransform(const TransformComponent& parentWorld, const TransformComponent& world)
        {
            const glm::vec2 offset = glm::vec2(world.Position) - glm::vec2(parentWorld.Position);
            const f32 cosine = std::cos(-parentWorld.Rotation);
            const f32 sine = std::sin(-parentWorld.Rotation);
            const glm::vec2 unrotated{ cosine * offset.x - sine * offset.y, sine * offset.x + cosine * offset.y };

            TransformComponent local;
            local.Position = {
                SafeDivide(unrotated.x, parentWorld.Scale.x),
                SafeDivide(unrotated.y, parentWorld.Scale.y),
                world.Position.z - parentWorld.Position.z
            };
            local.Rotation = world.Rotation - parentWorld.Rotation;
            local.Scale = {
                SafeDivide(world.Scale.x, parentWorld.Scale.x),
                SafeDivide(world.Scale.y, parentWorld.Scale.y)
            };
            return local;
        }
    }

    Scene::Scene() { ResetPhysicsWorld(); }

    Scene::~Scene()
    {
        OnRuntimeStop();

        m_Registry.on_destroy<Rigidbody2DComponent>()
                  .disconnect<&Physics2DWorld::OnRigidbodyDestroyed>(*m_PhysicsWorld);
    }

    Entity Scene::CreateEntity(const std::string& name) { return CreateEntityWithUUID(UUID(), name); }

    Entity Scene::CreateEntityWithUUID(const UUID uuid, const std::string& name)
    {
        if (m_EntityByUUID.contains(uuid))
        {
            throw std::runtime_error(std::format("Scene::CreateEntityWithUUID: an entity with UUID {} already exists",
                                                 static_cast<u64>(uuid)));
        }

        const Entity entity(m_Registry.create(), this);
        entity.AddComponent<UUIDComponent>(UUIDComponent{ uuid });
        entity.AddComponent<TransformComponent>();
        entity.AddComponent<RelationshipComponent>();
        entity.AddComponent<TagComponent>(name.empty() ? std::string("Entity") : name);

        m_EntityByUUID.emplace(uuid, entity.m_Handle);
        return entity;
    }

    void Scene::DestroyEntity(const Entity entity)
    {
        for (const Entity child : GetChildren(entity))
            DestroyEntity(child);

        if (const Entity parent = GetParent(entity); parent.IsValid())
            std::erase(m_Registry.get<RelationshipComponent>(parent.m_Handle).Children, entity.GetUUID());

        if (const auto* id = m_Registry.try_get<UUIDComponent>(entity.m_Handle))
            m_EntityByUUID.erase(id->ID);

        m_Registry.destroy(entity.m_Handle);
    }

    void Scene::QueueDestroyEntity(const UUID uuid) { m_PendingDestroy.push_back(uuid); }

    void Scene::FlushPendingDestroy()
    {
        const std::vector<UUID> pending = std::exchange(m_PendingDestroy, {});
        for (const UUID uuid : pending)
        {
            if (const Entity entity = FindEntityByUUID(uuid); entity.IsValid())
                DestroyEntity(entity);
        }
    }

    Entity Scene::DuplicateEntity(const Entity source)
    {
        if (!source.IsValid() || source.m_Scene != this)
            throw std::runtime_error("Scene::DuplicateEntity: the entity does not belong to this scene");

        const Entity copy = CopySubtree(source);
        if (const Entity parent = GetParent(source); parent.IsValid())
            SetParent(copy, parent, false);

        return copy;
    }

    Entity Scene::CopySubtree(const Entity source)
    {
        if (!source.IsValid())
            throw std::runtime_error("Scene::CopySubtree: the source entity is not valid");

        std::unordered_map<UUID, UUID> remap;
        const Entity copy = CopyEntityTree(source, remap);

        RemapEntityReferences(*this, copy, remap);
        return copy;
    }

    Entity Scene::CopyEntityTree(const Entity source, std::unordered_map<UUID, UUID>& remap)
    {
        const Scene& sourceScene = *source.m_Scene;

        const Entity copy = CreateEntity(source.GetTag());
        CopyComponents(DuplicableComponents{}, sourceScene.m_Registry, source.m_Handle, m_Registry, copy.m_Handle);
        remap.emplace(source.GetUUID(), copy.GetUUID());

        for (const Entity child : sourceScene.GetChildren(source))
            SetParent(CopyEntityTree(child, remap), copy, false);

        return copy;
    }

    Entity Scene::FindEntityByUUID(const UUID uuid) const
    {
        const auto it = m_EntityByUUID.find(uuid);
        return it != m_EntityByUUID.end() ? Entity(it->second, const_cast<Scene*>(this)) : Entity{};
    }

    Entity Scene::FindEntityByPickingId(const u32 pickingId)
    {
        if (pickingId == 0)
            return {};

        const auto handle = static_cast<entt::entity>(pickingId - 1);
        return m_Registry.valid(handle) ? Entity(handle, this) : Entity{};
    }

    Entity Scene::GetPrimaryCamera()
    {
        for (const auto handle : m_Registry.view<CameraComponent>())
        {
            if (m_Registry.get<CameraComponent>(handle).Primary)
                return Entity(handle, this);
        }

        return {};
    }

    void Scene::SetParent(const Entity child, const Entity parent, const bool keepWorldTransform, const i32 index)
    {
        if (!child.IsValid() || child.m_Scene != this)
            throw std::runtime_error("Scene::SetParent: the child does not belong to this scene");

        if (parent.IsValid())
        {
            if (parent.m_Scene != this)
                throw std::runtime_error("Scene::SetParent: the parent does not belong to this scene");

            if (parent == child || IsDescendantOf(parent, child))
                throw std::runtime_error("Scene::SetParent: the new parent is the entity itself or one of its descendants");
        }

        const TransformComponent world = GetWorldTransform(child);
        const UUID childId = child.GetUUID();

        if (const Entity oldParent = GetParent(child); oldParent.IsValid())
            std::erase(m_Registry.get<RelationshipComponent>(oldParent.m_Handle).Children, childId);

        m_Registry.get<RelationshipComponent>(child.m_Handle).Parent = parent.IsValid() ? parent.GetUUID() : UUID(0);

        if (parent.IsValid())
        {
            auto& siblings = m_Registry.get<RelationshipComponent>(parent.m_Handle).Children;
            const usize position = index < 0 ? siblings.size() : std::min(static_cast<usize>(index), siblings.size());
            siblings.insert(siblings.begin() + static_cast<std::ptrdiff_t>(position), childId);
        }

        if (keepWorldTransform)
            SetWorldTransform(child, world);
    }

    Entity Scene::GetParent(const Entity entity) const
    {
        const auto* relationship = m_Registry.try_get<RelationshipComponent>(entity.m_Handle);
        if (relationship == nullptr || static_cast<u64>(relationship->Parent) == 0)
            return {};

        return FindEntityByUUID(relationship->Parent);
    }

    std::vector<Entity> Scene::GetChildren(const Entity entity) const
    {
        std::vector<Entity> children;

        const auto* relationship = m_Registry.try_get<RelationshipComponent>(entity.m_Handle);
        if (relationship == nullptr)
            return children;

        children.reserve(relationship->Children.size());
        for (const UUID id : relationship->Children)
        {
            if (const Entity child = FindEntityByUUID(id); child.IsValid())
                children.push_back(child);
        }

        return children;
    }

    std::vector<Entity> Scene::GetRootEntities()
    {
        std::vector<Entity> roots;

        for (const auto handle : m_Registry.view<RelationshipComponent>())
        {
            if (static_cast<u64>(m_Registry.get<RelationshipComponent>(handle).Parent) == 0)
                roots.emplace_back(handle, this);
        }

        return roots;
    }

    bool Scene::IsDescendantOf(const Entity entity, const Entity ancestor) const
    {
        for (Entity current = GetParent(entity); current.IsValid(); current = GetParent(current))
        {
            if (current == ancestor)
                return true;
        }

        return false;
    }

    TransformComponent Scene::GetWorldTransform(const Entity entity) const
    {
        const auto& local = m_Registry.get<TransformComponent>(entity.m_Handle);

        const Entity parent = GetParent(entity);
        if (!parent.IsValid())
            return local;

        return ComposeTransforms(GetWorldTransform(parent), local);
    }

    void Scene::SetWorldTransform(const Entity entity, const TransformComponent& world)
    {
        auto& local = m_Registry.get<TransformComponent>(entity.m_Handle);

        const Entity parent = GetParent(entity);
        local = parent.IsValid() ? MakeLocalTransform(GetWorldTransform(parent), world) : world;
    }

    void Scene::ResetPhysicsWorld()
    {
        auto destroyed = m_Registry.on_destroy<Rigidbody2DComponent>();
        if (m_PhysicsWorld)
            destroyed.disconnect<&Physics2DWorld::OnRigidbodyDestroyed>(*m_PhysicsWorld);

        m_PhysicsWorld = MakeScope<Physics2DWorld>();
        destroyed.connect<&Physics2DWorld::OnRigidbodyDestroyed>(*m_PhysicsWorld);
    }

    u32 Scene::ToPickingId(const entt::entity handle)
    {
        return static_cast<u32>(entt::to_integral(handle)) + 1u;
    }

    void Scene::OnUpdateRuntime(const Timestep ts)
    {
        OnRuntimeStart();

        m_PhysicsWorld->EnsureBodiesCreated(*this);
        m_ScriptRuntime->Update(ts);
        AudioSystem::Update(*this);
        AnimationSystem::Update(*this, ts.GetSeconds());
        m_PhysicsWorld->Step(ts.GetSeconds(), *this);

        FlushPendingDestroy();
        ApplyPendingSceneLoad();
    }

    void Scene::OnStepRuntime()
    {
        OnUpdateRuntime(Timestep(Physics2DWorld::FixedTimestep));
    }

    void Scene::Render(Renderer2D& renderer, const Camera& camera)
    {
        renderer.BeginScene(camera);

        const auto view = m_Registry.view<TransformComponent, SpriteRendererComponent>();

        std::unordered_map<entt::entity, TransformComponent> worlds;
        worlds.reserve(view.size_hint());

        Scene& scene = *this;
        const auto worldOf = [&worlds, &scene](this const auto& self, const entt::entity handle) -> TransformComponent&
        {
            if (const auto it = worlds.find(handle); it != worlds.end())
                return it->second;

            const auto& local = scene.m_Registry.get<TransformComponent>(handle);
            const Entity parent = scene.GetParent(Entity(handle, &scene));

            const TransformComponent world = parent.IsValid() ? ComposeTransforms(self(parent.m_Handle), local)
                                                              : local;
            return worlds.emplace(handle, world).first->second;
        };

        std::vector<SpriteDrawItem> queue;
        queue.reserve(view.size_hint());
        for (const auto handle : view)
        {
            const auto& spriteRenderer = view.get<SpriteRendererComponent>(handle);
            queue.push_back({ handle, spriteRenderer.SortingLayer, worldOf(handle) });
        }

        std::ranges::stable_sort(queue, [](const SpriteDrawItem& a, const SpriteDrawItem& b)
        {
            if (a.Layer != b.Layer)
                return a.Layer < b.Layer;

            return std::strong_order(a.World.Position.z, b.World.Position.z) < 0;
        });

        for (const SpriteDrawItem& item : queue)
        {
            const TransformComponent& transform = item.World;
            const auto& spriteRenderer = view.get<SpriteRendererComponent>(item.Handle);
            const u32 pickingId = ToPickingId(item.Handle);

            if (spriteRenderer.Sprite)
            {
                const Sprite& sprite = *spriteRenderer.Sprite;
                renderer.DrawRotatedQuad(transform.Position, transform.Scale, transform.Rotation,
                                         sprite.GetTexture(), sprite.GetUVMin(), sprite.GetUVMax(),
                                         spriteRenderer.Color, pickingId);
            } else
            {
                renderer.DrawRotatedQuad(transform.Position, transform.Scale, transform.Rotation,
                                         spriteRenderer.Color, pickingId);
            }
        }

        renderer.EndScene();
    }

    void Scene::OnRuntimeStart()
    {
        if (m_ScriptRuntime) return;

        m_PhysicsWorld->EnsureBodiesCreated(*this);

        AudioSystem::Start(*this);
        AudioSystem::Update(*this);

        m_ScriptRuntime = MakeScope<ScriptRuntime>(*this);
        m_ScriptRuntime->Start();
    }

    void Scene::OnRuntimeStop()
    {
        if (!m_ScriptRuntime) return;

        m_ScriptRuntime->Stop();
        m_ScriptRuntime.reset();

        AudioSystem::Stop(*this);
    }

    void Scene::RequestSceneLoad(const UUID sceneAsset) { m_PendingSceneLoad = sceneAsset; }

    void Scene::ApplyPendingSceneLoad()
    {
        if (!m_PendingSceneLoad) return;

        const UUID handle = *m_PendingSceneLoad;
        m_PendingSceneLoad.reset();

        AssetMetadata metadata;
        if (!AssetRegistry::TryGetMetadata(handle, metadata) || metadata.Type != AssetType::Scene)
        {
            CORE_ERROR("Cannot load scene {}: it is not a scene asset", static_cast<u64>(handle));
            return;
        }

        const std::string name = metadata.Path.generic_string();

        const Result<nlohmann::json> data = AssetRegistry::ReadJson(handle);
        if (!data)
        {
            CORE_ERROR("Cannot load scene '{}': {}", name, data.error());
            return;
        }
        if (!data->is_object() || !data->contains("entities"))
        {
            CORE_ERROR("Cannot load scene '{}': the file is not a valid scene", name);
            return;
        }

        OnRuntimeStop();

        try
        {
            SceneSerializer::Deserialize(*this, *data);
        } catch (const std::exception& e)
        {
            CORE_ERROR("Scene '{}' could not be loaded completely: {}", name, e.what());
        }

        OnRuntimeStart();
        CORE_INFO("Loaded scene '{}'", name);
    }

    void Scene::EnsureRuntimeBody(const Entity entity)
    {
        if (m_PhysicsWorld)
            m_PhysicsWorld->EnsureBodyCreated(entity);
    }

    void Scene::SyncRuntimeBodies(const Entity root)
    {
        if (const auto* rigidbody = root.TryGetComponent<Rigidbody2DComponent>();
            rigidbody != nullptr && rigidbody->HasRuntimeBody())
        {
            const TransformComponent world = root.GetWorldTransform();
            rigidbody->Teleport({ world.Position.x, world.Position.y }, world.Rotation);
        }

        for (const Entity child : GetChildren(root))
            SyncRuntimeBodies(child);
    }

    void Scene::RebuildRuntimeBody(const Entity entity)
    {
        if (m_PhysicsWorld)
            m_PhysicsWorld->RebuildBody(entity);
    }

    ScriptInstance* Scene::FindScriptInstance(const Entity entity) const
    {
        return m_ScriptRuntime ? m_ScriptRuntime->FindInstance(entity.m_Handle) : nullptr;
    }

    void Scene::Clear()
    {
        OnRuntimeStop();

        m_PendingDestroy.clear();
        m_Registry.clear();
        m_EntityByUUID.clear();
        ResetPhysicsWorld();
    }

    void Scene::DispatchSensorEvent(const Entity self, const Entity other, const bool entered)
    {
        Scene& scene = self.GetScene();
        if (scene.m_ScriptRuntime)
        {
            scene.m_ScriptRuntime->DispatchContact(self.m_Handle,
                entered ? ContactEvent::SensorEnter : ContactEvent::SensorExit, other);
        }
    }

    void Scene::DispatchCollisionEvent(const Entity self, const Entity other, const bool entered)
    {
        Scene& scene = self.GetScene();
        if (scene.m_ScriptRuntime)
        {
            scene.m_ScriptRuntime->DispatchContact(self.m_Handle,
                entered ? ContactEvent::CollisionEnter : ContactEvent::CollisionExit, other);
        }
    }

    RaycastHit2D Scene::Raycast2D(const glm::vec2& origin, const glm::vec2& direction,
                                  const f32 maxDistance, const u32 layerMask) const
    {
        return m_PhysicsWorld->Raycast2D(origin, direction, maxDistance, *this, layerMask);
    }
}

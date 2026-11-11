#include "Scripting/API/CoreAPI.h"

#include "Engine/Assets/AssetRegistry.h"
#include "Engine/Audio/AudioEngine.h"
#include "Engine/Core/Log.h"
#include "Engine/Input/Input.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scene/RaycastHit2D.h"
#include "Engine/Scene/SceneCamera.h"
#include "Engine/Scripting/API/ScriptAPI.h"

#include <cstdint>
#include <format>
#include <limits>

namespace ByteForge
{
    namespace
    {
        constexpr CameraComponent DefaultCamera;
        constexpr TransformComponent DefaultTransform;

        f32 MathAddFloat(const ScriptCallContext&, const f32 a, const f32 b) { return a + b; }
        f32 MathSubtractFloat(const ScriptCallContext&, const f32 a, const f32 b) { return a - b; }
        f32 MathMultiplyFloat(const ScriptCallContext&, const f32 a, const f32 b) { return a * b; }
        f32 MathDivideFloat(const ScriptCallContext&, const f32 a, const f32 b) { return a / b; }
        bool MathGreaterFloat(const ScriptCallContext&, const f32 a, const f32 b) { return a > b; }
        bool MathLessFloat(const ScriptCallContext&, const f32 a, const f32 b) { return a < b; }

        glm::vec3 MathAddVector3(const ScriptCallContext&, const glm::vec3& a, const glm::vec3& b) { return a + b; }
        glm::vec3 MathScaleVector3(const ScriptCallContext&, const glm::vec3& v, const f32 s) { return v * s; }

        void DebugLog(const ScriptCallContext&, const std::string& message) { APP_INFO("{}", message); }

        Scene& RequireScene(const ScriptCallContext& context)
        {
            if (context.ActiveScene == nullptr)
                throw ScriptError("No scene is running");

            return *context.ActiveScene;
        }

        TransformComponent GetWorld(const ScriptCallContext& context, const EntityRef entity)
        {
            return context.Resolve(entity).GetWorldTransform();
        }

        void SetWorld(const ScriptCallContext& context, const EntityRef entity, const TransformComponent& world)
        {
            context.Resolve(entity).SetWorldTransform(world);
        }

        bool EntityIsValid(const ScriptCallContext& context, const EntityRef entity)
        {
            return context.ActiveScene != nullptr && entity.IsSet() &&
                   context.ActiveScene->FindEntityByUUID(entity.Id).IsValid();
        }

        std::string EntityGetName(const ScriptCallContext& context, const EntityRef entity)
        {
            return context.Require<TagComponent>(entity).Tag;
        }

        EntityRef EntityCreate(const ScriptCallContext& context, const std::string& name)
        {
            const Entity entity = RequireScene(context).CreateEntity(name);
            return EntityRef{ entity.GetUUID() };
        }

        EntityRef EntityInstantiate(const ScriptCallContext& context, const EntityRef source)
        {
            const Entity original = context.Resolve(source);
            const Entity copy = RequireScene(context).DuplicateEntity(original);
            return EntityRef{ copy.GetUUID() };
        }

        EntityRef EntityFindByName(const ScriptCallContext& context, const std::string& name)
        {
            EntityRef result;

            RequireScene(context).Each<TagComponent>([&](const Entity entity, const TagComponent& tag)
            {
                if (!result.IsSet() && tag.Tag == name)
                    result = EntityRef{ entity.GetUUID() };
            });

            return result;
        }

        void EntityDestroy(const ScriptCallContext& context, const EntityRef entity)
        {
            const Entity target = context.Resolve(entity);
            RequireScene(context).QueueDestroyEntity(target.GetUUID());
        }

        EntityRef EntityGetParent(const ScriptCallContext& context, const EntityRef entity)
        {
            const Entity parent = context.Resolve(entity).GetParent();
            return parent.IsValid() ? EntityRef{ parent.GetUUID() } : EntityRef{};
        }

        void EntitySetParent(const ScriptCallContext& context, const EntityRef entity, const EntityRef parent,
                             const bool keepWorldTransform)
        {
            const Entity child = context.Resolve(entity);
            const Entity newParent = parent.IsSet() ? context.Resolve(parent) : Entity{};

            if (newParent.IsValid() && (newParent == child || newParent.IsDescendantOf(child)))
                throw ScriptError("An entity cannot become a child of itself or of one of its own children");

            child.SetParent(newParent, keepWorldTransform);
        }

        i32 EntityGetChildCount(const ScriptCallContext& context, const EntityRef entity)
        {
            return static_cast<i32>(context.Resolve(entity).GetChildren().size());
        }

        EntityRef EntityGetChild(const ScriptCallContext& context, const EntityRef entity, const i32 index)
        {
            const std::vector<Entity> children = context.Resolve(entity).GetChildren();
            if (index < 0 || static_cast<usize>(index) >= children.size())
            {
                throw ScriptError(std::format("Child index {} is out of range (the entity has {} children)",
                                              index, children.size()));
            }

            return EntityRef{ children[static_cast<usize>(index)].GetUUID() };
        }

        void TeleportBodyToTransform(const ScriptCallContext& context, const EntityRef entity)
        {
            const Entity target = context.Resolve(entity);

            const auto* rigidbody = target.TryGetComponent<Rigidbody2DComponent>();
            if (rigidbody == nullptr) return;

            const TransformComponent world = target.GetWorldTransform();
            rigidbody->Teleport({ world.Position.x, world.Position.y }, world.Rotation);
        }

        glm::vec3 TransformGetPosition(const ScriptCallContext& context, const EntityRef entity)
        {
            return GetWorld(context, entity).Position;
        }

        void TransformSetPosition(const ScriptCallContext& context, const EntityRef entity, const glm::vec3& position)
        {
            TransformComponent world = GetWorld(context, entity);
            world.Position = position;
            SetWorld(context, entity, world);
            TeleportBodyToTransform(context, entity);
        }

        f32 TransformGetRotation(const ScriptCallContext& context, const EntityRef entity)
        {
            return GetWorld(context, entity).Rotation;
        }

        void TransformSetRotation(const ScriptCallContext& context, const EntityRef entity, const f32 rotation)
        {
            TransformComponent world = GetWorld(context, entity);
            world.Rotation = rotation;
            SetWorld(context, entity, world);
            TeleportBodyToTransform(context, entity);
        }

        glm::vec2 TransformGetScale(const ScriptCallContext& context, const EntityRef entity)
        {
            return GetWorld(context, entity).Scale;
        }

        void TransformSetScale(const ScriptCallContext& context, const EntityRef entity, const glm::vec2& scale)
        {
            TransformComponent world = GetWorld(context, entity);
            world.Scale = scale;
            SetWorld(context, entity, world);
        }

        glm::vec3 TransformGetLocalPosition(const ScriptCallContext& context, const EntityRef entity)
        {
            return context.Require<TransformComponent>(entity).Position;
        }

        void TransformSetLocalPosition(const ScriptCallContext& context, const EntityRef entity,
                                       const glm::vec3& position)
        {
            context.Require<TransformComponent>(entity).Position = position;
            TeleportBodyToTransform(context, entity);
        }

        f32 TransformGetLocalRotation(const ScriptCallContext& context, const EntityRef entity)
        {
            return context.Require<TransformComponent>(entity).Rotation;
        }

        void TransformSetLocalRotation(const ScriptCallContext& context, const EntityRef entity, const f32 rotation)
        {
            context.Require<TransformComponent>(entity).Rotation = rotation;
            TeleportBodyToTransform(context, entity);
        }

        glm::vec2 TransformGetLocalScale(const ScriptCallContext& context, const EntityRef entity)
        {
            return context.Require<TransformComponent>(entity).Scale;
        }

        void TransformSetLocalScale(const ScriptCallContext& context, const EntityRef entity, const glm::vec2& scale)
        {
            context.Require<TransformComponent>(entity).Scale = scale;
        }

        bool SpriteRendererExists(const ScriptCallContext& context, const EntityRef entity)
        {
            return context.Resolve(entity).HasComponent<SpriteRendererComponent>();
        }

        i32 SpriteRendererGetSortingLayer(const ScriptCallContext& context, const EntityRef entity)
        {
            return context.Require<SpriteRendererComponent>(entity).SortingLayer;
        }

        void SpriteRendererSetSortingLayer(const ScriptCallContext& context, const EntityRef entity, const i32 layer)
        {
            context.Require<SpriteRendererComponent>(entity).SortingLayer = layer;
        }

        bool Rigidbody2DExists(const ScriptCallContext& context, const EntityRef entity)
        {
            return context.Resolve(entity).HasComponent<Rigidbody2DComponent>();
        }

        glm::vec2 Rigidbody2DGetLinearVelocity(const ScriptCallContext& context, const EntityRef entity)
        {
            return context.Require<Rigidbody2DComponent>(entity).GetLinearVelocity();
        }

        void Rigidbody2DSetLinearVelocity(const ScriptCallContext& context, const EntityRef entity,
                                          const glm::vec2& velocity)
        {
            context.Require<Rigidbody2DComponent>(entity).SetLinearVelocity(velocity);
        }

        void Rigidbody2DApplyForce(const ScriptCallContext& context, const EntityRef entity, const glm::vec2& force)
        {
            context.Require<Rigidbody2DComponent>(entity).ApplyForceToCenter(force);
        }

        void Rigidbody2DApplyImpulse(const ScriptCallContext& context, const EntityRef entity,
                                     const glm::vec2& impulse)
        {
            context.Require<Rigidbody2DComponent>(entity).ApplyLinearImpulseToCenter(impulse);
        }

        RaycastHit2D CastRay(const ScriptCallContext& context, const glm::vec2& origin, const glm::vec2& direction,
                             const f32 maxDistance, const i32 layerMask)
        {
            return RequireScene(context).Raycast2D(origin, direction, maxDistance, static_cast<u32>(layerMask));
        }

        EntityRef PhysicsRaycast(const ScriptCallContext& context, const glm::vec2& origin,
                                 const glm::vec2& direction, const f32 maxDistance, const i32 layerMask)
        {
            const RaycastHit2D hit = CastRay(context, origin, direction, maxDistance, layerMask);
            if (!hit.Hit || !hit.HitEntity.IsValid())
                return {};

            return EntityRef{ hit.HitEntity.GetUUID() };
        }

        glm::vec2 PhysicsRaycastPoint(const ScriptCallContext& context, const glm::vec2& origin,
                                      const glm::vec2& direction, const f32 maxDistance, const i32 layerMask)
        {
            const RaycastHit2D hit = CastRay(context, origin, direction, maxDistance, layerMask);
            if (hit.Hit)
                return hit.Point;

            const f32 length = glm::length(direction);
            return length > 0.0f ? origin + direction / length * maxDistance : origin;
        }

        glm::vec2 PhysicsRaycastNormal(const ScriptCallContext& context, const glm::vec2& origin,
                                       const glm::vec2& direction, const f32 maxDistance, const i32 layerMask)
        {
            const RaycastHit2D hit = CastRay(context, origin, direction, maxDistance, layerMask);
            return hit.Hit ? hit.Normal : glm::vec2(0.0f);
        }

        f32 PhysicsRaycastDistance(const ScriptCallContext& context, const glm::vec2& origin,
                                     const glm::vec2& direction, const f32 maxDistance, const i32 layerMask)
        {
            const RaycastHit2D hit = CastRay(context, origin, direction, maxDistance, layerMask);
            return hit.Hit ? hit.Distance : maxDistance;
        }

        bool AudioSourceExists(const ScriptCallContext& context, const EntityRef entity)
        {
            return context.Resolve(entity).HasComponent<AudioSourceComponent>();
        }

        void AudioSourcePlay(const ScriptCallContext& context, const EntityRef entity)
        {
            context.Require<AudioSourceComponent>(entity).Play();
        }

        void AudioSourceStop(const ScriptCallContext& context, const EntityRef entity)
        {
            context.Require<AudioSourceComponent>(entity).Stop();
        }

        bool AudioSourceIsPlaying(const ScriptCallContext& context, const EntityRef entity)
        {
            return context.Require<AudioSourceComponent>(entity).IsPlaying();
        }

        void AudioPlayOneShot(const ScriptCallContext&, const AudioClipHandle clip)
        {
            AssetMetadata metadata;
            if (!clip.IsSet() || !AssetRegistry::TryGetMetadata(clip.Handle, metadata))
                throw ScriptError("Audio.PlayOneShot needs an existing audio clip");

            AudioEngine::Get().PlayOneShot(AssetRegistry::Resolve(clip.Handle), AudioBus::Game);
        }

        KeyCode ToKeyCode(const i32 key)
        {
            if (key < 0 || key > std::numeric_limits<u16>::max())
                throw ScriptError(std::format("{} is not a valid key code", key));

            return static_cast<KeyCode>(key);
        }

        MouseButton ToMouseButton(const i32 button)
        {
            if (button < 0 || button > static_cast<i32>(MouseButton::ButtonLast))
                throw ScriptError(std::format("{} is not a valid mouse button", button));

            return static_cast<MouseButton>(button);
        }

        bool InputIsKeyDown(const ScriptCallContext&, const i32 key) { return Input::IsKeyDown(ToKeyCode(key)); }
        bool InputIsKeyPressed(const ScriptCallContext&, const i32 key) { return Input::IsKeyPressed(ToKeyCode(key)); }
        bool InputIsKeyReleased(const ScriptCallContext&, const i32 key) { return Input::IsKeyReleased(ToKeyCode(key)); }

        bool InputIsMouseButtonDown(const ScriptCallContext&, const i32 button)
        {
            return Input::IsMouseButtonDown(ToMouseButton(button));
        }

        bool InputIsMouseButtonPressed(const ScriptCallContext&, const i32 button)
        {
            return Input::IsMouseButtonPressed(ToMouseButton(button));
        }

        bool InputIsMouseButtonReleased(const ScriptCallContext&, const i32 button)
        {
            return Input::IsMouseButtonReleased(ToMouseButton(button));
        }

        glm::vec2 InputGetMousePosition(const ScriptCallContext&) { return Input::GetViewportMousePosition(); }

        glm::vec2 InputGetMouseWorldPosition(const ScriptCallContext& context)
        {
            const Entity cameraEntity = RequireScene(context).GetPrimaryCamera();
            const bool hasCamera = cameraEntity.IsValid();

            const CameraComponent& settings = hasCamera ? cameraEntity.GetComponent<CameraComponent>()
                                                        : DefaultCamera;
            const TransformComponent transform = hasCamera ? cameraEntity.GetWorldTransform()
                                                           : DefaultTransform;

            const glm::vec2 viewportSize = Input::GetViewportSize();
            if (viewportSize.x < 1.0f || viewportSize.y < 1.0f)
                return { transform.Position.x, transform.Position.y };

            const OrthographicCamera camera = MakeSceneCamera(settings, transform, viewportSize.x / viewportSize.y);
            return ViewportPointToWorld(camera, Input::GetViewportMousePosition(), viewportSize);
        }

        glm::vec2 InputGetMouseDelta(const ScriptCallContext&) { return Input::GetMouseDelta(); }
        glm::vec2 InputGetScrollDelta(const ScriptCallContext&) { return Input::GetScrollDelta(); }
    }

    void RegisterCoreAPI(ScriptAPI& api)
    {
        using enum ScriptPurity;

        api.Register<&MathAddFloat>("Math.AddFloat", "Add", Pure, { "A", "B" });
        api.Register<&MathSubtractFloat>("Math.SubtractFloat", "Subtract", Pure, { "A", "B" });
        api.Register<&MathMultiplyFloat>("Math.MultiplyFloat", "Multiply", Pure, { "A", "B" });
        api.Register<&MathDivideFloat>("Math.DivideFloat", "Divide", Pure, { "A", "B" });
        api.Register<&MathGreaterFloat>("Math.GreaterFloat", "Greater", Pure, { "A", "B" });
        api.Register<&MathLessFloat>("Math.LessFloat", "Less", Pure, { "A", "B" });
        api.Register<&MathAddVector3>("Math.AddVector3", "Add (Vector3)", Pure, { "A", "B" });
        api.Register<&MathScaleVector3>("Math.ScaleVector3", "Scale (Vector3)", Pure, { "Vector", "Scale" });

        api.Register<&DebugLog>("Debug.Log", "Log", Impure, { "Message" });

        api.Register<&EntityIsValid>("Entity.IsValid", "Is Valid", Pure, { "Entity" });
        api.Register<&EntityGetName>("Entity.GetName", "Get Name", Pure, { "Entity" });
        api.Register<&EntityCreate>("Entity.Create", "Create Entity", Impure, { "Name" });
        api.Register<&EntityInstantiate>("Entity.Instantiate", "Instantiate", Impure, { "Source" });
        api.Register<&EntityFindByName>("Entity.FindByName", "Find By Name", Pure, { "Name" });
        api.Register<&EntityDestroy>("Entity.Destroy", "Destroy", Impure, { "Entity" });
        api.Register<&EntityGetParent>("Entity.GetParent", "Get Parent", Pure, { "Entity" });
        api.Register<&EntitySetParent>("Entity.SetParent", "Set Parent", Impure, { "Entity", "Parent", "Keep World Transform" });
        api.Register<&EntityGetChildCount>("Entity.GetChildCount", "Get Child Count", Pure, { "Entity" });
        api.Register<&EntityGetChild>("Entity.GetChild", "Get Child", Pure, { "Entity", "Index" });

        api.Register<&TransformGetPosition>("Transform.GetPosition", "Get Position", Pure, { "Entity" });
        api.Register<&TransformSetPosition>("Transform.SetPosition", "Set Position", Impure, { "Entity", "Position" });
        api.Register<&TransformGetRotation>("Transform.GetRotation", "Get Rotation", Pure, { "Entity" });
        api.Register<&TransformSetRotation>("Transform.SetRotation", "Set Rotation", Impure, { "Entity", "Rotation" });
        api.Register<&TransformGetScale>("Transform.GetScale", "Get Scale", Pure, { "Entity" });
        api.Register<&TransformSetScale>("Transform.SetScale", "Set Scale", Impure, { "Entity", "Scale" });
        api.Register<&TransformGetLocalPosition>("Transform.GetLocalPosition", "Get Local Position", Pure, { "Entity" });
        api.Register<&TransformSetLocalPosition>("Transform.SetLocalPosition", "Set Local Position", Impure, { "Entity", "Position" });
        api.Register<&TransformGetLocalRotation>("Transform.GetLocalRotation", "Get Local Rotation", Pure, { "Entity" });
        api.Register<&TransformSetLocalRotation>("Transform.SetLocalRotation", "Set Local Rotation", Impure, { "Entity", "Rotation" });
        api.Register<&TransformGetLocalScale>("Transform.GetLocalScale", "Get Local Scale", Pure, { "Entity" });
        api.Register<&TransformSetLocalScale>("Transform.SetLocalScale", "Set Local Scale", Impure, { "Entity", "Scale" });

        api.Register<&SpriteRendererExists>("SpriteRenderer.Exists", "Has Sprite Renderer", Pure, { "Entity" });
        api.Register<&SpriteRendererGetSortingLayer>("SpriteRenderer.GetSortingLayer", "Get Sorting Layer", Pure, { "Entity" });
        api.Register<&SpriteRendererSetSortingLayer>("SpriteRenderer.SetSortingLayer", "Set Sorting Layer", Impure, { "Entity", "Layer" });

        api.Register<&Rigidbody2DExists>("Rigidbody2D.Exists", "Has Rigidbody 2D", Pure, { "Entity" });
        api.Register<&Rigidbody2DGetLinearVelocity>("Rigidbody2D.GetLinearVelocity", "Get Linear Velocity", Pure, { "Entity" });
        api.Register<&Rigidbody2DSetLinearVelocity>("Rigidbody2D.SetLinearVelocity", "Set Linear Velocity", Impure, { "Entity", "Velocity" });
        api.Register<&Rigidbody2DApplyForce>("Rigidbody2D.ApplyForce", "Apply Force", Impure, { "Entity", "Force" });
        api.Register<&Rigidbody2DApplyImpulse>("Rigidbody2D.ApplyImpulse", "Apply Impulse", Impure, { "Entity", "Impulse" });

        api.Register<&PhysicsRaycast>("Physics.Raycast", "Raycast", Pure, { "Origin", "Direction", "Max Distance", "Layer Mask" });
        api.Register<&PhysicsRaycastPoint>("Physics.RaycastPoint", "Raycast Point", Pure, { "Origin", "Direction", "Max Distance", "Layer Mask" });
        api.Register<&PhysicsRaycastNormal>("Physics.RaycastNormal", "Raycast Normal", Pure, { "Origin", "Direction", "Max Distance", "Layer Mask" });
        api.Register<&PhysicsRaycastDistance>("Physics.RaycastDistance", "Raycast Distance", Pure, { "Origin", "Direction", "Max Distance", "Layer Mask" });

        api.Register<&AudioSourceExists>("AudioSource.Exists", "Has Audio Source", Pure, { "Entity" });
        api.Register<&AudioSourcePlay>("AudioSource.Play", "Play", Impure, { "Entity" });
        api.Register<&AudioSourceStop>("AudioSource.Stop", "Stop", Impure, { "Entity" });
        api.Register<&AudioSourceIsPlaying>("AudioSource.IsPlaying", "Is Playing", Pure, { "Entity" });

        api.Register<&AudioPlayOneShot>("Audio.PlayOneShot", "Play One Shot", Impure, { "Clip" });

        api.Register<&InputIsKeyDown>("Input.IsKeyDown", "Is Key Down", Pure, { "Key" });
        api.Register<&InputIsKeyPressed>("Input.IsKeyPressed", "Is Key Pressed", Pure, { "Key" });
        api.Register<&InputIsKeyReleased>("Input.IsKeyReleased", "Is Key Released", Pure, { "Key" });
        api.Register<&InputIsMouseButtonDown>("Input.IsMouseButtonDown", "Is Mouse Button Down", Pure, { "Button" });
        api.Register<&InputIsMouseButtonPressed>("Input.IsMouseButtonPressed", "Is Mouse Button Pressed", Pure, { "Button" });
        api.Register<&InputIsMouseButtonReleased>("Input.IsMouseButtonReleased", "Is Mouse Button Released", Pure, { "Button" });
        api.Register<&InputGetMousePosition>("Input.GetMousePosition", "Get Mouse Position", Pure);
        api.Register<&InputGetMouseWorldPosition>("Input.GetMouseWorldPosition", "Get Mouse World Position", Pure);
        api.Register<&InputGetMouseDelta>("Input.GetMouseDelta", "Get Mouse Delta", Pure);
        api.Register<&InputGetScrollDelta>("Input.GetScrollDelta", "Get Scroll Delta", Pure);
    }
}

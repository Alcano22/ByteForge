#include "Scripting/API/CoreAPI.h"

#include "Engine/Assets/AssetRegistry.h"
#include "Engine/Audio/AudioEngine.h"
#include "Engine/Core/Log.h"
#include "Engine/Input/Input.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scripting/API/ScriptAPI.h"

#include <cstdint>
#include <format>
#include <limits>

namespace ByteForge
{
    namespace
    {
        float MathAddFloat(const ScriptCallContext&, const float a, const float b) { return a + b; }
        float MathSubtractFloat(const ScriptCallContext&, const float a, const float b) { return a - b; }
        float MathMultiplyFloat(const ScriptCallContext&, const float a, const float b) { return a * b; }
        float MathDivideFloat(const ScriptCallContext&, const float a, const float b) { return a / b; }
        bool MathGreaterFloat(const ScriptCallContext&, const float a, const float b) { return a > b; }
        bool MathLessFloat(const ScriptCallContext&, const float a, const float b) { return a < b; }

        glm::vec3 MathAddVector3(const ScriptCallContext&, const glm::vec3& a, const glm::vec3& b) { return a + b; }
        glm::vec3 MathScaleVector3(const ScriptCallContext&, const glm::vec3& v, const float s) { return v * s; }

        void DebugLog(const ScriptCallContext&, const std::string& message) { APP_INFO("{}", message); }

        Scene& RequireScene(const ScriptCallContext& context)
        {
            if (context.ActiveScene == nullptr)
                throw ScriptError("No scene is running");

            return *context.ActiveScene;
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

        glm::vec3 TransformGetPosition(const ScriptCallContext& context, const EntityRef entity)
        {
            return context.Require<TransformComponent>(entity).Position;
        }

        void TransformSetPosition(const ScriptCallContext& context, const EntityRef entity, const glm::vec3& position)
        {
            context.Require<TransformComponent>(entity).Position = position;
        }

        float TransformGetRotation(const ScriptCallContext& context, const EntityRef entity)
        {
            return context.Require<TransformComponent>(entity).Rotation;
        }

        void TransformSetRotation(const ScriptCallContext& context, const EntityRef entity, const float rotation)
        {
            context.Require<TransformComponent>(entity).Rotation = rotation;
        }

        glm::vec2 TransformGetScale(const ScriptCallContext& context, const EntityRef entity)
        {
            return context.Require<TransformComponent>(entity).Scale;
        }

        void TransformSetScale(const ScriptCallContext& context, const EntityRef entity, const glm::vec2& scale)
        {
            context.Require<TransformComponent>(entity).Scale = scale;
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

        KeyCode ToKeyCode(const int key)
        {
            if (key < 0 || key > std::numeric_limits<uint16_t>::max())
                throw ScriptError(std::format("{} is not a valid key code", key));

            return static_cast<KeyCode>(key);
        }

        MouseButton ToMouseButton(const int button)
        {
            if (button < 0 || button > static_cast<int>(MouseButton::ButtonLast))
                throw ScriptError(std::format("{} is not a valid mouse button", button));

            return static_cast<MouseButton>(button);
        }

        bool InputIsKeyDown(const ScriptCallContext&, const int key) { return Input::IsKeyDown(ToKeyCode(key)); }
        bool InputIsKeyPressed(const ScriptCallContext&, const int key) { return Input::IsKeyPressed(ToKeyCode(key)); }
        bool InputIsKeyReleased(const ScriptCallContext&, const int key) { return Input::IsKeyReleased(ToKeyCode(key)); }

        bool InputIsMouseButtonDown(const ScriptCallContext&, const int button)
        {
            return Input::IsMouseButtonDown(ToMouseButton(button));
        }

        bool InputIsMouseButtonPressed(const ScriptCallContext&, const int button)
        {
            return Input::IsMouseButtonPressed(ToMouseButton(button));
        }

        bool InputIsMouseButtonReleased(const ScriptCallContext&, const int button)
        {
            return Input::IsMouseButtonReleased(ToMouseButton(button));
        }

        glm::vec2 InputGetMousePosition(const ScriptCallContext&) { return Input::GetMousePosition(); }
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

        api.Register<&TransformGetPosition>("Transform.GetPosition", "Get Position", Pure, { "Entity" });
        api.Register<&TransformSetPosition>("Transform.SetPosition", "Set Position", Impure, { "Entity", "Position" });
        api.Register<&TransformGetRotation>("Transform.GetRotation", "Get Rotation", Pure, { "Entity" });
        api.Register<&TransformSetRotation>("Transform.SetRotation", "Set Rotation", Impure, { "Entity", "Rotation" });
        api.Register<&TransformGetScale>("Transform.GetScale", "Get Scale", Pure, { "Entity" });
        api.Register<&TransformSetScale>("Transform.SetScale", "Set Scale", Impure, { "Entity", "Scale" });

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
        api.Register<&InputGetMouseDelta>("Input.GetMouseDelta", "Get Mouse Delta", Pure);
        api.Register<&InputGetScrollDelta>("Input.GetScrollDelta", "Get Scroll Delta", Pure);
    }
}

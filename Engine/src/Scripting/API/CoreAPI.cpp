#include "Scripting/API/CoreAPI.h"

#include "Engine/Assets/AssetRegistry.h"
#include "Engine/Audio/AudioEngine.h"
#include "Engine/Core/Log.h"
#include "Engine/Scene/Components.h"
#include "Engine/Scripting/API/ScriptAPI.h"

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

        bool EntityIsValid(const ScriptCallContext& context, const EntityRef entity)
        {
            return context.ActiveScene != nullptr && entity.IsSet() &&
                   context.ActiveScene->FindEntityByUUID(entity.Id).IsValid();
        }

        std::string EntityGetName(const ScriptCallContext& context, const EntityRef entity)
        {
            return context.Require<TagComponent>(entity).Tag;
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
    }
}

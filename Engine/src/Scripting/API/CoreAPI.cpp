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

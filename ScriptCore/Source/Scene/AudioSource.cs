using ByteForge.Interop;

namespace ByteForge;

public sealed class AudioSource : Component
{
    internal AudioSource(Entity entity)
    {
        Entity = entity;
    }

    public bool IsPlaying => ScriptAPI.AudioSource_IsPlaying(Entity.ID);

    public void Play() => ScriptAPI.AudioSource_Play(Entity.ID);
    public void Stop() => ScriptAPI.AudioSource_Stop(Entity.ID);
}

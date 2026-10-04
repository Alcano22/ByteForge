using ByteForge.Interop;

namespace ByteForge;

public sealed class AudioSource
{
    private readonly ulong _entity;

    internal AudioSource(ulong entity)
    {
        _entity = entity;
    }

    public bool IsPlaying => ScriptAPI.AudioSource_IsPlaying(_entity);

    public void Play() => ScriptAPI.AudioSource_Play(_entity);
    public void Stop() => ScriptAPI.AudioSource_Stop(_entity);
}

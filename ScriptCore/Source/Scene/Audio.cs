using System;
using ByteForge.Interop;

namespace ByteForge;

public static class Audio
{
    public static void PlayOneShot(AudioClip clip)
    {
        ArgumentNullException.ThrowIfNull(clip);
        ScriptAPI.Audio_PlayOneShot(clip);
    }
}

using System;
using ByteForge.Interop;

namespace ByteForge;

public sealed class Animator
{
    private readonly ulong _entity;

    internal Animator(ulong entity)
    {
        _entity = entity;
    }

    public bool IsPlaying => ScriptAPI.Animator_IsPlaying(_entity);

    /// <summary>Name of the active state, empty if none is active.</summary>
    public string CurrentState => ScriptAPI.Animator_GetState(_entity);

    /// <summary>Progress through the current pass of the clip in [0, 1].</summary>
    public float NormalizedTime => ScriptAPI.Animator_GetNormalizedTime(_entity);

    public float Speed
    {
        get => ScriptAPI.Animator_GetSpeed(_entity);
        set => ScriptAPI.Animator_SetSpeed(_entity, value);
    }

    /// <summary>
    /// Plays the state from the start. If it is already playing it keeps running, so this is safe to call
    /// every frame; pass restart to start it over anyway.
    /// </summary>
    public void Play(string state, bool restart = false)
    {
        ArgumentNullException.ThrowIfNull(state);
        ScriptAPI.Animator_Play(_entity, state, restart);
    }

    public void Stop() => ScriptAPI.Animator_Stop(_entity);
}

using System;
using ByteForge.Interop;

namespace ByteForge;

public sealed class Animator : Component
{
    internal Animator(Entity entity)
    {
        Entity = entity;
    }

    public bool IsPlaying => ScriptAPI.Animator_IsPlaying(Entity.ID);

    /// <summary>Name of the active state, empty if none is active.</summary>
    public string CurrentState => ScriptAPI.Animator_GetState(Entity.ID);

    /// <summary>Progress through the current pass of the clip in [0, 1].</summary>
    public float NormalizedTime => ScriptAPI.Animator_GetNormalizedTime(Entity.ID);

    public float Speed
    {
        get => ScriptAPI.Animator_GetSpeed(Entity.ID);
        set => ScriptAPI.Animator_SetSpeed(Entity.ID, value);
    }

    /// <summary>
    /// Plays the state from the start. If it is already playing it keeps running, so this is safe to call
    /// every frame; pass restart to start it over anyway.
    /// </summary>
    public void Play(string state, bool restart = false)
    {
        ArgumentNullException.ThrowIfNull(state);
        ScriptAPI.Animator_Play(Entity.ID, state, restart);
    }

    public void Stop() => ScriptAPI.Animator_Stop(Entity.ID);
}

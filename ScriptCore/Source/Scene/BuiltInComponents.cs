using System;
using System.Collections.Generic;
using ByteForge.Interop;

namespace ByteForge;

internal static class BuiltInComponents
{
    private sealed record Info(Func<ulong, bool> Exists, Func<Entity, Component> Create);

    private static readonly Dictionary<Type, Info> s_Types = new()
    {
        [typeof(Transform)]      = new(static _ => true,                static entity => new Transform(entity)),
        [typeof(SpriteRenderer)] = new(ScriptAPI.SpriteRenderer_Exists, static entity => new SpriteRenderer(entity)),
        [typeof(Rigidbody2D)]    = new(ScriptAPI.Rigidbody2D_Exists,    static entity => new Rigidbody2D(entity)),
        [typeof(AudioSource)]    = new(ScriptAPI.AudioSource_Exists,    static entity => new AudioSource(entity)),
        [typeof(Animator)]       = new(ScriptAPI.Animator_Exists,       static entity => new Animator(entity))
    };

    internal static bool Contains(Type type) => s_Types.ContainsKey(type);

    internal static bool Exists(Type type, ulong entity) => s_Types[type].Exists(entity);

    internal static Component Create(Type type, Entity entity) => s_Types[type].Create(entity);
}

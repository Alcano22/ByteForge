using System;
using System.Numerics;
using ByteForge.Interop;

namespace ByteForge;

public sealed class Transform
{
    private readonly ulong _entity;

    internal Transform(ulong entity)
    {
        _entity = entity;
    }

    public Vector3 Position
    {
        get => ScriptAPI.Transform_GetPosition(_entity);
        set => ScriptAPI.Transform_SetPosition(_entity, value);
    }

    public float Rotation
    {
        get => ScriptAPI.Transform_GetRotation(_entity);
        set => ScriptAPI.Transform_SetRotation(_entity, value);
    }

    public Vector2 Scale
    {
        get => ScriptAPI.Transform_GetScale(_entity);
        set => ScriptAPI.Transform_SetScale(_entity, value);
    }

    public Vector3 LocalPosition
    {
        get => ScriptAPI.Transform_GetLocalPosition(_entity);
        set => ScriptAPI.Transform_SetLocalPosition(_entity, value);
    }

    public float LocalRotation
    {
        get => ScriptAPI.Transform_GetLocalRotation(_entity);
        set => ScriptAPI.Transform_SetLocalRotation(_entity, value);
    }

    public Vector2 LocalScale
    {
        get => ScriptAPI.Transform_GetLocalScale(_entity);
        set => ScriptAPI.Transform_SetLocalScale(_entity, value);
    }
}

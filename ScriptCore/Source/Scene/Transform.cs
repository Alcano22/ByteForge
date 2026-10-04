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
}

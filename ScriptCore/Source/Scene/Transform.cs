using System;
using System.Numerics;
using ByteForge.Interop;

namespace ByteForge;

public sealed unsafe class Transform
{
    public Vector3 Position
    {
        get
        {
            Vector3 value;
            Check(Native.Api.Transform_GetPosition(_entity, &value));
            return value;
        }
        set => Check(Native.Api.Transform_SetPosition(_entity, &value));
    }

    public float Rotation
    {
        get
        {
            float value;
            Check(Native.Api.Transform_GetRotation(_entity, &value));
            return value;
        }
        set => Check(Native.Api.Transform_SetRotation(_entity, value));
    }

    public Vector2 Scale
    {
        get
        {
            Vector2 value;
            Check(Native.Api.Transform_GetScale(_entity, &value));
            return value;
        }
        set => Check(Native.Api.Transform_SetScale(_entity, &value));
    }

    private readonly ulong _entity;

    internal Transform(ulong entity)
    {
        _entity = entity;
    }

    private void Check(int result)
    {
        if (result == 0)
            throw new InvalidOperationException($"Entity {_entity} no longer exists");
    }
}

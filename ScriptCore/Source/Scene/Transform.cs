using ByteForge.Interop;

namespace ByteForge;

public sealed class Transform : Component
{
    internal Transform(Entity entity)
    {
        Entity = entity;
    }

    public Vector3 Position
    {
        get => ScriptAPI.Transform_GetPosition(Entity.ID);
        set => ScriptAPI.Transform_SetPosition(Entity.ID, value);
    }

    public float Rotation
    {
        get => ScriptAPI.Transform_GetRotation(Entity.ID);
        set => ScriptAPI.Transform_SetRotation(Entity.ID, value);
    }

    public Vector2 Scale
    {
        get => ScriptAPI.Transform_GetScale(Entity.ID);
        set => ScriptAPI.Transform_SetScale(Entity.ID, value);
    }

    public Vector3 LocalPosition
    {
        get => ScriptAPI.Transform_GetLocalPosition(Entity.ID);
        set => ScriptAPI.Transform_SetLocalPosition(Entity.ID, value);
    }

    public float LocalRotation
    {
        get => ScriptAPI.Transform_GetLocalRotation(Entity.ID);
        set => ScriptAPI.Transform_SetLocalRotation(Entity.ID, value);
    }

    public Vector2 LocalScale
    {
        get => ScriptAPI.Transform_GetLocalScale(Entity.ID);
        set => ScriptAPI.Transform_SetLocalScale(Entity.ID, value);
    }
}

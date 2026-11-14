using ByteForge.Interop;

namespace ByteForge;

public sealed class Rigidbody2D : Component
{
    internal Rigidbody2D(Entity entity)
    {
        Entity = entity;
    }

    public Vector2 LinearVelocity
    {
        get => ScriptAPI.Rigidbody2D_GetLinearVelocity(Entity.ID);
        set => ScriptAPI.Rigidbody2D_SetLinearVelocity(Entity.ID, value);
    }

    public void ApplyForce(Vector2 force) => ScriptAPI.Rigidbody2D_ApplyForce(Entity.ID, force);
    public void ApplyImpulse(Vector2 impulse) => ScriptAPI.Rigidbody2D_ApplyImpulse(Entity.ID, impulse);
}

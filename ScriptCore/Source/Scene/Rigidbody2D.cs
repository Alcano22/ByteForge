using ByteForge.Interop;

namespace ByteForge;

public sealed class Rigidbody2D
{
    private readonly ulong _entity;

    internal Rigidbody2D(ulong entity)
    {
        _entity = entity;
    }

    public Vector2 LinearVelocity
    {
        get => ScriptAPI.Rigidbody2D_GetLinearVelocity(_entity);
        set => ScriptAPI.Rigidbody2D_SetLinearVelocity(_entity, value);
    }

    public void ApplyForce(Vector2 force) => ScriptAPI.Rigidbody2D_ApplyForce(_entity, force);
    public void ApplyImpulse(Vector2 impulse) => ScriptAPI.Rigidbody2D_ApplyImpulse(_entity, impulse);
}

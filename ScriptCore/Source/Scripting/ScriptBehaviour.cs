namespace ByteForge;

public abstract class ScriptBehaviour
{
    public Entity Entity { get; internal set; } = null;

    public Transform Transform => Entity.Transform;

    protected virtual void OnCreate() {}
    protected virtual void OnDestroy() {}

    protected virtual void OnUpdate(float deltaTime) {}

    protected virtual void OnSensorEnter(Entity other) {}
    protected virtual void OnSensorExit(Entity other) {}
    protected virtual void OnCollisionEnter(Entity other) {}
    protected virtual void OnCollisionExit(Entity other) {}

    internal void InvokeCreate() => OnCreate();
    internal void InvokeUpdate(float deltaTime) => OnUpdate(deltaTime);
    internal void InvokeDestroy() => OnDestroy();

    internal void InvokeContact(int contact, Entity other)
    {
        switch (contact)
        {
            case 0: OnSensorEnter(other);    break;
            case 1: OnSensorExit(other);     break;
            case 2: OnCollisionEnter(other); break;
            case 3: OnCollisionExit(other);  break;
        }
    }
}

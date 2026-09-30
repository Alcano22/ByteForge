namespace ByteForge.Examples;

public sealed class Spinner : ScriptBehaviour
{
    public float Speed = 1.5f;

    protected override void OnCreate()
    {
        Log.Info($"Spinner started on {Entity}");
    }

    protected override void OnUpdate(float deltaTime)
    {
        Transform.Rotation += Speed * deltaTime;
    }
}

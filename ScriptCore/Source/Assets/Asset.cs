namespace ByteForge;

public enum AssetType : byte
{
    None = 0,
    Texture2D,
    Script,
    Scene,
    PhysicsMaterial2D,
    AudioClip,
    ScriptGraph
}

public abstract class Asset
{
    public ulong Handle { get; }
    public abstract AssetType Type { get; }

    private protected Asset(ulong handle)
    {
        Handle = handle;
    }

    public override string ToString() => $"{Type}({Handle})";

    internal static Asset? Create(AssetType type, ulong handle)
    {
        if (handle == 0)
            return null;

        return type switch
        {
            AssetType.AudioClip => new AudioClip(handle),
            _ => null
        };
    }
}

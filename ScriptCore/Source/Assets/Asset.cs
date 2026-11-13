namespace ByteForge;

public enum AssetType : byte
{
    None = 0,
    Texture2D,
    Script,
    Scene,
    PhysicsMaterial2D,
    AudioClip,
    ScriptGraph,
    Prefab,
    AnimationClip
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
            AssetType.Scene     => new SceneAsset(handle),
            AssetType.AudioClip => new AudioClip(handle),
            AssetType.Prefab    => new Prefab(handle),
            _ => null
        };
    }
}

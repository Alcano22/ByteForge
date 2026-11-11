namespace ByteForge;

public sealed class SceneAsset : Asset
{
    internal SceneAsset(ulong handle)
        : base(handle) {}

    public override AssetType Type => AssetType.Scene;
}

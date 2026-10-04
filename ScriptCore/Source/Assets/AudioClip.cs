namespace ByteForge;

public sealed class AudioClip : Asset
{
    internal AudioClip(ulong handle)
        : base(handle) {}

    public override AssetType Type => AssetType.AudioClip;
}

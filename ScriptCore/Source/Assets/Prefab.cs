using System;
using ByteForge.Interop;

namespace ByteForge;

public sealed class Prefab : Asset
{
    internal Prefab(ulong handle)
        : base(handle) {}

    public override AssetType Type => AssetType.Prefab;

    public Entity Instantiate() => new(ScriptAPI.Prefab_Instantiate(this));

    public Entity Instantiate(Vector2 position) => new(ScriptAPI.Prefab_InstantiateAt(this, position));

    public Entity Instantiate(Entity parent)
    {
        ArgumentNullException.ThrowIfNull(parent);
        return new Entity(ScriptAPI.Prefab_InstantiateAsChild(this, parent.ID));
    }
}

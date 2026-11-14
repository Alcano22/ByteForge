using ByteForge.Interop;

namespace ByteForge;

public sealed class SpriteRenderer : Component
{
    internal SpriteRenderer(Entity entity)
    {
        Entity = entity;
    }

    public int SortingLayer
    {
        get => ScriptAPI.SpriteRenderer_GetSortingLayer(Entity.ID);
        set => ScriptAPI.SpriteRenderer_SetSortingLayer(Entity.ID, value);
    }
}

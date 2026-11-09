using ByteForge.Interop;

namespace ByteForge;

public sealed class SpriteRenderer
{
    private readonly ulong _entity;

    internal SpriteRenderer(ulong entity)
    {
        _entity = entity;
    }

    public int SortingLayer
    {
        get => ScriptAPI.SpriteRenderer_GetSortingLayer(_entity);
        set => ScriptAPI.SpriteRenderer_SetSortingLayer(_entity, value);
    }
}

using ByteForge.Interop;

namespace ByteForge;

/// <summary>The text of a UI element.</summary>
public sealed class Text : Component
{
    internal Text(Entity entity)
    {
        Entity = entity;
    }

    /// <summary>What it says. Line breaks with '\n'.</summary>
    public string Value
    {
        get => ScriptAPI.Text_GetValue(Entity.ID);
        set => ScriptAPI.Text_SetValue(Entity.ID, value ?? string.Empty);
    }

    /// <summary>Red, green, blue and alpha from 0 to 1.</summary>
    public Vector4 Color
    {
        get => ScriptAPI.Text_GetColor(Entity.ID);
        set => ScriptAPI.Text_SetColor(Entity.ID, value);
    }

    /// <summary>In canvas units, at least 1.</summary>
    public float FontSize
    {
        get => ScriptAPI.Text_GetFontSize(Entity.ID);
        set => ScriptAPI.Text_SetFontSize(Entity.ID, value);
    }
}

using System;
using System.IO;
using System.Text;
using ByteForge.Interop;

namespace ByteForge;

/// <summary>
/// An action from the project's Input Actions asset. Get one with <see cref="InputActions.Find"/>,
/// ideally once in OnCreate, and poll it every frame. 
/// </summary>
public sealed class InputAction
{
    private readonly byte[] _path;

    /// <summary>The action's path, "Map/Action".</summary>
    public string Path { get; }

    internal InputAction(string path)
    {
        Path = path;
        _path = Encoding.UTF8.GetBytes(path);
    }

    /// <summary>Held this frame. Button actions count from half way, axis and vector actions from any non-zero value.</summary>
    public bool IsDown() => ScriptAPI.InputAction_IsDown(_path);

    /// <summary>Became down this frame.</summary>
    public bool WasPressedThisFrame() => ScriptAPI.InputAction_WasPressed(_path);

    /// <summary>Stopped being down this frame.</summary>
    public bool WasReleasedThisFrame() => ScriptAPI.InputAction_WasReleased(_path);

    /// <summary>Button actions give a value in [0, 1], Axis1D actions any float. Vector2 actions give their X.</summary>
    public float ReadFloat() => ScriptAPI.InputAction_ReadValue(_path).X;

    /// <summary>Vector2 actions give both axes, Button and Axis1D actions only fill X.</summary>
    public Vector2 ReadVector2() => ScriptAPI.InputAction_ReadValue(_path);

    public override string ToString() => Path;
}

/// <summary>The project-wide input actions picked in Project Settings.</summary>
public static class InputActions
{
    /// <summary>Looks an action up by "Map/Action". Null, with a warning, when the project has no such action.</summary>
    public static InputAction? Find(string path)
    {
        ArgumentNullException.ThrowIfNull(path);

        if (ScriptAPI.InputAction_Exists(path))
            return new InputAction(path);

        Log.Warning($"There is no input action '{path}' (paths look like 'Map/Action')");
        return null;
    }

    /// <summary>Maps start enabled. Throws when there is no map with this name.</summary>
    public static void EnableMap(string name)
    {
        ArgumentNullException.ThrowIfNull(name);
        ScriptAPI.InputActions_EnableMap(name);
    }

    /// <summary>Held actions of the map are released on the next frame. Throws when there is no map with this name.</summary>
    public static void DisableMap(string name)
    {
        ArgumentNullException.ThrowIfNull(name);
        ScriptAPI.InputActions_DisableMap(name);
    }

    public static bool IsMapEnabled(string name)
    {
        ArgumentNullException.ThrowIfNull(name);
        return ScriptAPI.InputActions_IsMapEnabled(name);
    }
}

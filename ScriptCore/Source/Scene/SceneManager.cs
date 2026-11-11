using System;
using ByteForge.Interop;

namespace ByteForge;

public static class SceneManager
{
    public static void Load(SceneAsset scene)
    {
        ArgumentNullException.ThrowIfNull(scene);
        ScriptAPI.Scene_Load(scene);
    }

    public static void Load(string name)
    {
        ArgumentNullException.ThrowIfNull(name);
        ScriptAPI.Scene_LoadByName(name);
    }
}

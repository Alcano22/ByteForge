using System.Reflection;
using System.Runtime.Loader;

namespace ByteForge.Interop;

internal sealed class GameAssemblyContext : AssemblyLoadContext
{
    public GameAssemblyContext()
        : base("GameScripts", isCollectible: true) {}

    protected override Assembly? Load(AssemblyName name)
    {
        Assembly scriptCore = typeof(ScriptBehaviour).Assembly;
        if (name.Name == scriptCore.GetName().Name)
            return scriptCore;

        return null;
    }
}

using System;
using System.Runtime.InteropServices;

namespace ByteForge.Interop;

internal static unsafe class Bootstrap
{
    [UnmanagedCallersOnly]
    public static int Initialize(NativeAPI* api)
    {
        try
        {
            if (api == null || api->Size != sizeof(NativeAPI))
                return 1;

            Native.Api = *api;

            if (!ScriptAPI.Bind())
                return 3;

            ScriptHost.DiscoverClasses([typeof(ScriptBehaviour).Assembly]);

            Log.Info($"ScriptCore initialized (.NET {Environment.Version}, {ScriptHost.ClassCount} script classes)");
            return 0;
        } catch
        {
            return 2;
        }
    }
}

using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace ByteForge.Interop;

internal static unsafe class ScriptHost
{
    private static readonly Dictionary<string, ScriptClass> s_Classes = new();

    private static GameAssemblyContext? s_GameContext;
    private static Assembly? s_GameAssembly;

    internal static int ClassCount => s_Classes.Count;

    internal static void DiscoverClasses(IEnumerable<Assembly> assemblies)
    {
        s_Classes.Clear();

        foreach (Assembly assembly in assemblies)
        {
            foreach (Type type in assembly.GetTypes())
            {
                if (type.IsAbstract || !type.IsSubclassOf(typeof(ScriptBehaviour))) continue;

                if (type.GetConstructor(Type.EmptyTypes) == null)
                {
                    Log.Warning($"Script class '{type.FullName}' has no parameterless constructor and is skipped");
                    continue;
                }

                s_Classes[type.FullName!] = new ScriptClass(type);
            }
        }
    }

    [UnmanagedCallersOnly]
    public static int LoadGameAssembly(byte* path)
    {
        try
        {
            string file = Marshal.PtrToStringUTF8((IntPtr)path) ?? string.Empty;

            UnloadGameAssembly();

            var context = new GameAssemblyContext();

            using FileStream assembly = File.OpenRead(file);
            string symbolsFile = Path.ChangeExtension(file, ".pdb");
            using FileStream? symbols = File.Exists(symbolsFile) ? File.OpenRead(symbolsFile) : null;

            s_GameAssembly = context.LoadFromStream(assembly, symbols);
            s_GameContext = context;

            DiscoverClasses([typeof(ScriptBehaviour).Assembly, s_GameAssembly]);
            Log.Info($"Loaded {s_GameAssembly.GetName().Name} ({ClassCount} script classes)");
            return 0;
        } catch (Exception e)
        {
            Native.ReportException(e);
            return 1;
        }
    }

    private static void UnloadGameAssembly()
    {
        if (s_GameContext == null) return;

        s_Classes.Clear();
        s_GameAssembly = null;

        s_GameContext.Unload();
        s_GameContext = null;
    }

    [UnmanagedCallersOnly]
    public static void GetClassNames(delegate* unmanaged<byte*, void*, void> sink, void* userData)
    {
        try
        {
            foreach (string name in s_Classes.Keys)
            {
                using var text = new Utf8String(name);
                sink(text.Pointer, userData);
            }
        } catch (Exception e)
        {
            Log.Error($"Could not list script classes: {e}");
        }
    }

    [UnmanagedCallersOnly]
    public static void GetClassFields(byte* className, delegate* unmanaged<byte*, int, void*, void*, void> sink, void* userData)
    {
        try
        {
            string name = Marshal.PtrToStringUTF8((IntPtr)className) ?? string.Empty;
            if (!s_Classes.TryGetValue(name, out ScriptClass? scriptClass) || scriptClass.Fields.Count == 0) return;

            object? defaults = scriptClass.CreateDefaults();
            byte* value = stackalloc byte[ScriptField.MaxValueSize];

            foreach (ScriptField field in scriptClass.Fields)
            {
                new Span<byte>(value, ScriptField.MaxValueSize).Clear();
                field.Read(defaults, value);

                using var text = new Utf8String(field.Name);
                sink(text.Pointer, (int)field.Type, value, userData);
            }
        } catch (Exception e)
        {
            Log.Error($"Could not list script fields: {e}");
        }
    }

    [UnmanagedCallersOnly]
    public static IntPtr CreateInstance(byte* className, ulong entity)
    {
        try
        {
            string name = Marshal.PtrToStringUTF8((IntPtr)className) ?? string.Empty;
            if (!s_Classes.TryGetValue(name, out ScriptClass? scriptClass))
                throw new InvalidOperationException($"C# script class '{name}' does not exist");

            var script = (ScriptBehaviour)Activator.CreateInstance(scriptClass.Type)!;
            script.Entity = new Entity(entity);

            return GCHandle.ToIntPtr(GCHandle.Alloc(script));
        } catch (Exception e)
        {
            Native.ReportException(e);
            return IntPtr.Zero;
        }
    }

    [UnmanagedCallersOnly]
    public static void DestroyInstance(IntPtr instance)
    {
        try
        {
            GCHandle.FromIntPtr(instance).Free();
        } catch (Exception e)
        {
            Log.Error($"Could not release a script instance: {e}");
        }
    }

    [UnmanagedCallersOnly]
    public static int GetField(IntPtr instance, byte* name, int* type, void* value)
    {
        try
        {
            ScriptBehaviour script = GetScript(instance);
            ScriptField? field = FindField(script, name);
            if (field == null)
                return 1;

            *type = (int)field.Type;
            field.Read(script, value);
            return 0;
        } catch (Exception e)
        {
            Log.Error($"Could not read a script field: {e}");
            return 1;
        }
    }

    [UnmanagedCallersOnly]
    public static int SetField(IntPtr instance, byte* name, int type, void* value)
    {
        try
        {
            ScriptBehaviour script = GetScript(instance);
            ScriptField? field = FindField(script, name);
            if (field == null || (int)field.Type != type)
                return 1;

            field.Write(script, value);
            return 0;
        } catch (Exception e)
        {
            Log.Error($"Could not write a script field: {e}");
            return 1;
        }
    }

    [UnmanagedCallersOnly]
    public static int OnCreate(IntPtr instance) =>
        Invoke(instance, 0, static (script, _) => script.InvokeCreate());

    [UnmanagedCallersOnly]
    public static int OnUpdate(IntPtr instance, float deltaTime) =>
        Invoke(instance, deltaTime, static (script, dt) => script.InvokeUpdate(dt));

    [UnmanagedCallersOnly]
    public static int OnDestroy(IntPtr instance) =>
        Invoke(instance, 0, static (script, _) => script.InvokeDestroy());

    [UnmanagedCallersOnly]
    public static int OnContact(IntPtr instance, int contact, ulong other) =>
        Invoke(instance, (contact, other), static (script, args) => script.InvokeContact(args.contact, new Entity(args.other)));

    private static ScriptBehaviour GetScript(IntPtr instance) =>
        (ScriptBehaviour)GCHandle.FromIntPtr(instance).Target!;

    private static ScriptField? FindField(ScriptBehaviour script, byte* name)
    {
        string fieldName = Marshal.PtrToStringUTF8((IntPtr)name) ?? string.Empty;
        return s_Classes.TryGetValue(script.GetType().FullName!, out ScriptClass? scriptClass)
             ? scriptClass.FindField(fieldName) : null;
    }

    private static int Invoke<TState>(IntPtr instance, TState state, Action<ScriptBehaviour, TState> call)
    {
        try
        {
            call(GetScript(instance), state);
            return 0;
        } catch (Exception e)
        {
            Native.ReportException(e);
            return 1;
        }
    }
}

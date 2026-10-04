using System;
using System.Numerics;
using System.Runtime.InteropServices;

namespace ByteForge.Interop;

internal static unsafe class ScriptAPI
{
    private const uint InvalidFunction = uint.MaxValue;

    private static uint s_EntityIsValid;
    private static uint s_EntityGetName;
    private static uint s_TransformGetPosition;
    private static uint s_TransformSetPosition;
    private static uint s_TransformGetRotation;
    private static uint s_TransformSetRotation;
    private static uint s_TransformGetScale;
    private static uint s_TransformSetScale;
    private static uint s_AudioSourceExists;
    private static uint s_AudioSourcePlay;
    private static uint s_AudioSourceStop;
    private static uint s_AudioSourceIsPlaying;
    private static uint s_AudioPlayOneShot;

    internal static bool Bind()
    {
        bool complete = true;

        s_EntityIsValid        = Resolve("Entity.IsValid",        "(Entity)->Bool",            ref complete);
        s_EntityGetName        = Resolve("Entity.GetName",        "(Entity)->String",          ref complete);
        s_TransformGetPosition = Resolve("Transform.GetPosition", "(Entity)->Vector3",         ref complete);
        s_TransformSetPosition = Resolve("Transform.SetPosition", "(Entity,Vector3)->Void",    ref complete);
        s_TransformGetRotation = Resolve("Transform.GetRotation", "(Entity)->Float",           ref complete);
        s_TransformSetRotation = Resolve("Transform.SetRotation", "(Entity,Float)->Void",      ref complete);
        s_TransformGetScale    = Resolve("Transform.GetScale",    "(Entity)->Vector2",         ref complete);
        s_TransformSetScale    = Resolve("Transform.SetScale",    "(Entity,Vector2)->Void",    ref complete);
        s_AudioSourceExists    = Resolve("AudioSource.Exists",    "(Entity)->Bool",            ref complete);
        s_AudioSourcePlay      = Resolve("AudioSource.Play",      "(Entity)->Void",            ref complete);
        s_AudioSourceStop      = Resolve("AudioSource.Stop",      "(Entity)->Void",            ref complete);
        s_AudioSourceIsPlaying = Resolve("AudioSource.IsPlaying", "(Entity)->Bool",            ref complete);
        s_AudioPlayOneShot     = Resolve("Audio.PlayOneShot",     "(Asset:AudioClip)->Void",   ref complete);

        return complete;
    }

    // Entity

    public static bool Entity_IsValid(ulong entity) =>
        Call(s_EntityIsValid, new APIValue { Handle = entity }).Bool != 0;

    public static string Entity_GetName(ulong entity) =>
        Call(s_EntityGetName, new APIValue { Handle = entity }).ReadText();

    // Transform

    public static Vector3 Transform_GetPosition(ulong entity) =>
        Call(s_TransformGetPosition, new APIValue { Handle = entity }).Vector3;

    public static void Transform_SetPosition(ulong entity, Vector3 position) =>
        Call(s_TransformSetPosition, new APIValue { Handle = entity }, new APIValue { Vector3 = position });

    public static float Transform_GetRotation(ulong entity) =>
        Call(s_TransformGetRotation, new APIValue { Handle = entity }).Float;

    public static void Transform_SetRotation(ulong entity, float rotation) =>
        Call(s_TransformSetRotation, new APIValue { Handle = entity }, new APIValue { Float = rotation });

    public static Vector2 Transform_GetScale(ulong entity) =>
        Call(s_TransformGetScale, new APIValue { Handle = entity }).Vector2;

    public static void Transform_SetScale(ulong entity, Vector2 scale) =>
        Call(s_TransformSetScale, new APIValue { Handle = entity }, new APIValue { Vector2 = scale });

    // Audio

    public static bool AudioSource_Exists(ulong entity) =>
        Call(s_AudioSourceExists, new APIValue { Handle = entity }).Bool != 0;

    public static void AudioSource_Play(ulong entity) =>
        Call(s_AudioSourcePlay, new APIValue { Handle = entity });

    public static void AudioSource_Stop(ulong entity) =>
        Call(s_AudioSourceStop, new APIValue { Handle = entity });

    public static bool AudioSource_IsPlaying(ulong entity) =>
        Call(s_AudioSourceIsPlaying, new APIValue { Handle = entity }).Bool != 0;

    public static void Audio_PlayOneShot(AudioClip? clip) =>
        Call(s_AudioPlayOneShot, new APIValue { Handle = clip?.Handle ?? 0, AssetType = (byte)AssetType.AudioClip });

    // Plumbing

    private static uint Resolve(string id, string signature, ref bool complete)
    {
        using var idText = new Utf8String(id);
        using var signatureText = new Utf8String(signature);

        uint index = Native.Api.Api_FindFunction(idText.Pointer, signatureText.Pointer);
        if (index == InvalidFunction)
        {
            Log.Error($"Script API function '{id}' {signature} is not provided by the engine");
            complete = false;
        }

        return index;
    }

    private static APIValue Call(uint function, APIValue argument)
    {
        APIValue* arguments = stackalloc APIValue[1];
        arguments[0] = argument;
        return Invoke(function, arguments, 1);
    }

    private static APIValue Call(uint function, APIValue first, APIValue second)
    {
        APIValue* arguments = stackalloc APIValue[2];
        arguments[0] = first;
        arguments[1] = second;
        return Invoke(function, arguments, 2);
    }

    private static APIValue Invoke(uint function, APIValue* arguments, int count)
    {
        APIValue result = default;
        if (Native.Api.Api_Invoke(function, arguments, count, &result) != 0)
            throw new ScriptException(Marshal.PtrToStringUTF8((IntPtr)Native.Api.Api_GetLastError()) ?? "Unknown engine error");

        return result;
    }
}

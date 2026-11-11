using System;
using System.Numerics;
using System.Runtime.InteropServices;
using System.Text;

namespace ByteForge.Interop;

internal static unsafe class ScriptAPI
{
    private const uint InvalidFunction = uint.MaxValue;

    private static uint s_EntityIsValid;
    private static uint s_EntityGetName;
    private static uint s_EntityCreate;
    private static uint s_EntityInstantiate;
    private static uint s_EntityFindByName;
    private static uint s_EntityDestroy;
    private static uint s_EntityGetParent;
    private static uint s_EntitySetParent;
    private static uint s_EntityGetChildCount;
    private static uint s_EntityGetChild;

    private static uint s_TransformGetPosition;
    private static uint s_TransformSetPosition;
    private static uint s_TransformGetRotation;
    private static uint s_TransformSetRotation;
    private static uint s_TransformGetScale;
    private static uint s_TransformSetScale;
    private static uint s_TransformGetLocalPosition;
    private static uint s_TransformSetLocalPosition;
    private static uint s_TransformGetLocalRotation;
    private static uint s_TransformSetLocalRotation;
    private static uint s_TransformGetLocalScale;
    private static uint s_TransformSetLocalScale;

    private static uint s_SpriteRendererExists;
    private static uint s_SpriteRendererGetSortingLayer;
    private static uint s_SpriteRendererSetSortingLayer;

    private static uint s_Rigidbody2DExists;
    private static uint s_Rigidbody2DGetLinearVelocity;
    private static uint s_Rigidbody2DSetLinearVelocity;
    private static uint s_Rigidbody2DApplyForce;
    private static uint s_Rigidbody2DApplyImpulse;

    private static uint s_PhysicsRaycast;
    private static uint s_PhysicsRaycastPoint;
    private static uint s_PhysicsRaycastNormal;
    private static uint s_PhysicsRaycastDistance;

    private static uint s_AudioSourceExists;
    private static uint s_AudioSourcePlay;
    private static uint s_AudioSourceStop;
    private static uint s_AudioSourceIsPlaying;
    private static uint s_AudioPlayOneShot;

    private static uint s_PrefabInstantiate;
    private static uint s_PrefabInstantiateAt;
    private static uint s_PrefabInstantiateAsChild;

    private static uint s_InputIsKeyDown;
    private static uint s_InputIsKeyPressed;
    private static uint s_InputIsKeyReleased;
    private static uint s_InputIsMouseButtonDown;
    private static uint s_InputIsMouseButtonPressed;
    private static uint s_InputIsMouseButtonReleased;
    private static uint s_InputGetMousePosition;
    private static uint s_InputGetMouseWorldPosition;
    private static uint s_InputGetMouseDelta;
    private static uint s_InputGetScrollDelta;

    internal static bool Bind()
    {
        bool complete = true;

        s_EntityIsValid                 = Resolve("Entity.IsValid",                 "(Entity)->Bool",                       ref complete);
        s_EntityGetName                 = Resolve("Entity.GetName",                 "(Entity)->String",                     ref complete);
        s_EntityCreate                  = Resolve("Entity.Create",                  "(String)->Entity",                     ref complete);
        s_EntityInstantiate             = Resolve("Entity.Instantiate",             "(Entity)->Entity",                     ref complete);
        s_EntityFindByName              = Resolve("Entity.FindByName",              "(String)->Entity",                     ref complete);
        s_EntityDestroy                 = Resolve("Entity.Destroy",                 "(Entity)->Void",                       ref complete);
        s_EntityGetParent               = Resolve("Entity.GetParent",               "(Entity)->Entity",                     ref complete);
        s_EntitySetParent               = Resolve("Entity.SetParent",               "(Entity,Entity,Bool)->Void",           ref complete);
        s_EntityGetChildCount           = Resolve("Entity.GetChildCount",           "(Entity)->Int",                        ref complete);
        s_EntityGetChild                = Resolve("Entity.GetChild",                "(Entity,Int)->Entity",                 ref complete);

        s_TransformGetPosition          = Resolve("Transform.GetPosition",          "(Entity)->Vector3",                    ref complete);
        s_TransformSetPosition          = Resolve("Transform.SetPosition",          "(Entity,Vector3)->Void",               ref complete);
        s_TransformGetRotation          = Resolve("Transform.GetRotation",          "(Entity)->Float",                      ref complete);
        s_TransformSetRotation          = Resolve("Transform.SetRotation",          "(Entity,Float)->Void",                 ref complete);
        s_TransformGetScale             = Resolve("Transform.GetScale",             "(Entity)->Vector2",                    ref complete);
        s_TransformSetScale             = Resolve("Transform.SetScale",             "(Entity,Vector2)->Void",               ref complete);
        s_TransformGetLocalPosition     = Resolve("Transform.GetLocalPosition",     "(Entity)->Vector3",                    ref complete);
        s_TransformSetLocalPosition     = Resolve("Transform.SetLocalPosition",     "(Entity,Vector3)->Void",               ref complete);
        s_TransformGetLocalRotation     = Resolve("Transform.GetLocalRotation",     "(Entity)->Float",                      ref complete);
        s_TransformSetLocalRotation     = Resolve("Transform.SetLocalRotation",     "(Entity,Float)->Void",                 ref complete);
        s_TransformGetLocalScale        = Resolve("Transform.GetLocalScale",        "(Entity)->Vector2",                    ref complete);
        s_TransformSetLocalScale        = Resolve("Transform.SetLocalScale",        "(Entity,Vector2)->Void",               ref complete);

        s_SpriteRendererExists          = Resolve("SpriteRenderer.Exists",          "(Entity)->Bool",                       ref complete);
        s_SpriteRendererGetSortingLayer = Resolve("SpriteRenderer.GetSortingLayer", "(Entity)->Int",                        ref complete);
        s_SpriteRendererSetSortingLayer = Resolve("SpriteRenderer.SetSortingLayer", "(Entity,Int)->Void",                   ref complete);

        s_Rigidbody2DExists             = Resolve("Rigidbody2D.Exists",             "(Entity)->Bool",                       ref complete);
        s_Rigidbody2DGetLinearVelocity  = Resolve("Rigidbody2D.GetLinearVelocity",  "(Entity)->Vector2",                    ref complete);
        s_Rigidbody2DSetLinearVelocity  = Resolve("Rigidbody2D.SetLinearVelocity",  "(Entity,Vector2)->Void",               ref complete);
        s_Rigidbody2DApplyForce         = Resolve("Rigidbody2D.ApplyForce",         "(Entity,Vector2)->Void",               ref complete);
        s_Rigidbody2DApplyImpulse       = Resolve("Rigidbody2D.ApplyImpulse",       "(Entity,Vector2)->Void",               ref complete);

        s_PhysicsRaycast                = Resolve("Physics.Raycast",                "(Vector2,Vector2,Float,Int)->Entity",  ref complete);
        s_PhysicsRaycastPoint           = Resolve("Physics.RaycastPoint",           "(Vector2,Vector2,Float,Int)->Vector2", ref complete);
        s_PhysicsRaycastNormal          = Resolve("Physics.RaycastNormal",          "(Vector2,Vector2,Float,Int)->Vector2", ref complete);
        s_PhysicsRaycastDistance        = Resolve("Physics.RaycastDistance",        "(Vector2,Vector2,Float,Int)->Float",   ref complete);

        s_AudioSourceExists             = Resolve("AudioSource.Exists",             "(Entity)->Bool",                       ref complete);
        s_AudioSourcePlay               = Resolve("AudioSource.Play",               "(Entity)->Void",                       ref complete);
        s_AudioSourceStop               = Resolve("AudioSource.Stop",               "(Entity)->Void",                       ref complete);
        s_AudioSourceIsPlaying          = Resolve("AudioSource.IsPlaying",          "(Entity)->Bool",                       ref complete);
        s_AudioPlayOneShot              = Resolve("Audio.PlayOneShot",              "(Asset:AudioClip)->Void",              ref complete);

        s_PrefabInstantiate             = Resolve("Prefab.Instantiate",             "(Asset:Prefab)->Entity",               ref complete);
        s_PrefabInstantiateAt           = Resolve("Prefab.InstantiateAt",           "(Asset:Prefab,Vector2)->Entity",       ref complete);
        s_PrefabInstantiateAsChild      = Resolve("Prefab.InstantiateAsChild",      "(Asset:Prefab,Entity)->Entity",        ref complete);

        s_InputIsKeyDown                = Resolve("Input.IsKeyDown",                "(Int)->Bool",                          ref complete);
        s_InputIsKeyPressed             = Resolve("Input.IsKeyPressed",             "(Int)->Bool",                          ref complete);
        s_InputIsKeyReleased            = Resolve("Input.IsKeyReleased",            "(Int)->Bool",                          ref complete);
        s_InputIsMouseButtonDown        = Resolve("Input.IsMouseButtonDown",        "(Int)->Bool",                          ref complete);
        s_InputIsMouseButtonPressed     = Resolve("Input.IsMouseButtonPressed",     "(Int)->Bool",                          ref complete);
        s_InputIsMouseButtonReleased    = Resolve("Input.IsMouseButtonReleased",    "(Int)->Bool",                          ref complete);
        s_InputGetMousePosition         = Resolve("Input.GetMousePosition",         "()->Vector2",                          ref complete);
        s_InputGetMouseWorldPosition    = Resolve("Input.GetMouseWorldPosition",    "()->Vector2",                          ref complete);
        s_InputGetMouseDelta            = Resolve("Input.GetMouseDelta",            "()->Vector2",                          ref complete);
        s_InputGetScrollDelta           = Resolve("Input.GetScrollDelta",           "()->Vector2",                          ref complete);

        return complete;
    }

    // Entity

    public static bool Entity_IsValid(ulong entity) =>
        Call(s_EntityIsValid, new APIValue { Handle = entity }).Bool != 0;

    public static string Entity_GetName(ulong entity) =>
        Call(s_EntityGetName, new APIValue { Handle = entity }).ReadText();

    public static ulong Entity_Create(string name) =>
        CallWithText(s_EntityCreate, name).Handle;

    public static ulong Entity_Instantiate(ulong source) =>
        Call(s_EntityInstantiate, new APIValue { Handle = source }).Handle;

    public static ulong Entity_FindByName(string name) =>
        CallWithText(s_EntityFindByName, name).Handle;

    public static void Entity_Destroy(ulong entity) =>
        Call(s_EntityDestroy, new APIValue { Handle = entity });

    public static ulong Entity_GetParent(ulong entity) =>
        Call(s_EntityGetParent, new APIValue { Handle = entity }).Handle;

    public static void Entity_SetParent(ulong entity, ulong parent, bool keepWorldTransform) =>
        Call(s_EntitySetParent, new APIValue { Handle = entity }, new APIValue { Handle = parent },
             new APIValue { Bool = (byte)(keepWorldTransform ? 1 : 0) });

    public static int Entity_GetChildCount(ulong entity) =>
        Call(s_EntityGetChildCount, new APIValue { Handle = entity }).Int;

    public static ulong Entity_GetChild(ulong entity, int index) =>
        Call(s_EntityGetChild, new APIValue { Handle = entity }, new APIValue { Int = index }).Handle;

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

    public static Vector3 Transform_GetLocalPosition(ulong entity) =>
        Call(s_TransformGetLocalPosition, new APIValue { Handle = entity }).Vector3;

    public static void Transform_SetLocalPosition(ulong entity, Vector3 position) =>
        Call(s_TransformSetLocalPosition, new APIValue { Handle = entity }, new APIValue { Vector3 = position });

    public static float Transform_GetLocalRotation(ulong entity) =>
        Call(s_TransformGetLocalRotation, new APIValue { Handle = entity }).Float;

    public static void Transform_SetLocalRotation(ulong entity, float rotation) =>
        Call(s_TransformSetLocalRotation, new APIValue { Handle = entity }, new APIValue { Float = rotation });

    public static Vector2 Transform_GetLocalScale(ulong entity) =>
        Call(s_TransformGetLocalScale, new APIValue { Handle = entity }).Vector2;

    public static void Transform_SetLocalScale(ulong entity, Vector2 scale) =>
        Call(s_TransformSetLocalScale, new APIValue { Handle = entity }, new APIValue { Vector2 = scale });

    // SpriteRenderer

    public static bool SpriteRenderer_Exists(ulong entity) =>
        Call(s_SpriteRendererExists, new APIValue { Handle = entity }).Bool != 0;

    public static int SpriteRenderer_GetSortingLayer(ulong entity) =>
        Call(s_SpriteRendererGetSortingLayer, new APIValue { Handle = entity }).Int;

    public static void SpriteRenderer_SetSortingLayer(ulong entity, int layer) =>
        Call(s_SpriteRendererSetSortingLayer, new APIValue { Handle = entity }, new APIValue { Int = layer });

    // Rigidbody2D

    public static bool Rigidbody2D_Exists(ulong entity) =>
        Call(s_Rigidbody2DExists, new APIValue { Handle = entity }).Bool != 0;

    public static Vector2 Rigidbody2D_GetLinearVelocity(ulong entity) =>
        Call(s_Rigidbody2DGetLinearVelocity, new APIValue { Handle = entity }).Vector2;

    public static void Rigidbody2D_SetLinearVelocity(ulong entity, Vector2 velocity) =>
        Call(s_Rigidbody2DSetLinearVelocity, new APIValue { Handle = entity }, new APIValue { Vector2 = velocity });

    public static void Rigidbody2D_ApplyForce(ulong entity, Vector2 force) =>
        Call(s_Rigidbody2DApplyForce, new APIValue { Handle = entity }, new APIValue { Vector2 = force });

    public static void Rigidbody2D_ApplyImpulse(ulong entity, Vector2 impulse) =>
        Call(s_Rigidbody2DApplyImpulse, new APIValue { Handle = entity }, new APIValue { Vector2 = impulse });

    // Physics

    public static ulong Physics_Raycast(Vector2 origin, Vector2 direction, float maxDistance, int layerMask) =>
        Call(s_PhysicsRaycast, new APIValue { Vector2 = origin }, new APIValue { Vector2 = direction },
             new APIValue { Float = maxDistance }, new APIValue { Int = layerMask }).Handle;

    public static Vector2 Physics_RaycastPoint(Vector2 origin, Vector2 direction, float maxDistance, int layerMask) =>
        Call(s_PhysicsRaycastPoint, new APIValue { Vector2 = origin }, new APIValue { Vector2 = direction },
             new APIValue { Float = maxDistance }, new APIValue { Int = layerMask }).Vector2;

    public static Vector2 Physics_RaycastNormal(Vector2 origin, Vector2 direction, float maxDistance, int layerMask) =>
        Call(s_PhysicsRaycastNormal, new APIValue { Vector2 = origin }, new APIValue { Vector2 = direction },
             new APIValue { Float = maxDistance }, new APIValue { Int = layerMask }).Vector2;

    public static float Physics_RaycastDistance(Vector2 origin, Vector2 direction, float maxDistance, int layerMask) =>
        Call(s_PhysicsRaycastDistance, new APIValue { Vector2 = origin }, new APIValue { Vector2 = direction },
             new APIValue { Float = maxDistance }, new APIValue { Int = layerMask }).Float;

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

    // Prefab

    public static ulong Prefab_Instantiate(Prefab prefab) =>
        Call(s_PrefabInstantiate, PrefabValue(prefab)).Handle;

    public static ulong Prefab_InstantiateAt(Prefab prefab, Vector2 position) =>
        Call(s_PrefabInstantiateAt, PrefabValue(prefab), new APIValue { Vector2 = position }).Handle;

    public static ulong Prefab_InstantiateAsChild(Prefab prefab, ulong parent) =>
        Call(s_PrefabInstantiateAsChild, PrefabValue(prefab), new APIValue { Handle = parent }).Handle;

    private static APIValue PrefabValue(Prefab prefab) =>
        new() { Handle = prefab.Handle, AssetType = (byte)AssetType.Prefab };

    // Input

    public static bool Input_IsKeyDown(int key) =>
        Call(s_InputIsKeyDown, new APIValue { Int = key }).Bool != 0;

    public static bool Input_IsKeyPressed(int key) =>
        Call(s_InputIsKeyPressed, new APIValue { Int = key }).Bool != 0;

    public static bool Input_IsKeyReleased(int key) =>
        Call(s_InputIsKeyReleased, new APIValue { Int = key }).Bool != 0;

    public static bool Input_IsMouseButtonDown(int button) =>
        Call(s_InputIsMouseButtonDown, new APIValue { Int = button }).Bool != 0;

    public static bool Input_IsMouseButtonPressed(int button) =>
        Call(s_InputIsMouseButtonPressed, new APIValue { Int = button }).Bool != 0;

    public static bool Input_IsMouseButtonReleased(int button) =>
        Call(s_InputIsMouseButtonReleased, new APIValue { Int = button }).Bool != 0;

    public static Vector2 Input_GetMousePosition() => Call(s_InputGetMousePosition).Vector2;
    public static Vector2 Input_GetMouseWorldPosition() => Call(s_InputGetMouseWorldPosition).Vector2;
    public static Vector2 Input_GetMouseDelta() => Call(s_InputGetMouseDelta).Vector2;
    public static Vector2 Input_GetScrollDelta() => Call(s_InputGetScrollDelta).Vector2;

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

    private static APIValue Call(uint function, params ReadOnlySpan<APIValue> arguments)
    {
        APIValue result = default;

        fixed (APIValue* data = arguments)
        {
            if (Native.Api.Api_Invoke(function, data, arguments.Length, &result) != 0)
                throw new ScriptException(Marshal.PtrToStringUTF8((IntPtr)Native.Api.Api_GetLastError()) ?? "Unknown engine error");
        }

        return result;
    }

    private static APIValue CallWithText(uint function, string text)
    {
        byte[] bytes = Encoding.UTF8.GetBytes(text);
        fixed (byte* data = bytes)
        {
            return Call(function, new APIValue { Text = data, TextLength = bytes.Length });
        }
    }
}

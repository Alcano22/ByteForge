using System;
using System.Linq;
using System.Collections.Generic;
using System.Numerics;
using System.Reflection;

namespace ByteForge.Interop;

internal enum ScriptFieldType
{
    Bool,
    Int,
    Float,
    Double,
    Vector2,
    Vector3,
    Vector4,
    Entity,
    Asset
}

internal sealed unsafe class ScriptField(FieldInfo info, ScriptFieldType type, AssetType assetType = AssetType.None)
{
    public const int MaxValueSize = 16;
    private const int AssetTypeOffset = sizeof(ulong);

    private static readonly Dictionary<Type, ScriptFieldType> s_FieldTypes = new()
    {
        [typeof(bool)]    = ScriptFieldType.Bool,
        [typeof(int)]     = ScriptFieldType.Int,
        [typeof(float)]   = ScriptFieldType.Float,
        [typeof(double)]  = ScriptFieldType.Double,
        [typeof(Vector2)] = ScriptFieldType.Vector2,
        [typeof(Vector3)] = ScriptFieldType.Vector3,
        [typeof(Vector4)] = ScriptFieldType.Vector4,
        [typeof(Entity)]  = ScriptFieldType.Entity
    };

    private static readonly Dictionary<Type, AssetType> s_AssetTypes = new()
    {
        [typeof(AudioClip)] = AssetType.AudioClip
    };

    public FieldInfo Info { get; } = info;
    public ScriptFieldType Type { get; } = type;
    public AssetType AssetType { get; } = assetType;
    public string Name => Info.Name;

    public static ScriptField? TryCreate(FieldInfo info)
    {
        if (s_FieldTypes.TryGetValue(info.FieldType, out ScriptFieldType type))
            return new ScriptField(info, type);

        if (s_AssetTypes.TryGetValue(info.FieldType, out AssetType assetType))
            return new ScriptField(info, ScriptFieldType.Asset, assetType);

        return null;
    }

    public void Read(object? target, void* destination)
    {
        object? value = target != null ? Info.GetValue(target) : null;

        switch (Type)
        {
            case ScriptFieldType.Bool:    *(byte*)destination = value is true ? (byte)1 : (byte)0; break;
            case ScriptFieldType.Int:     *(int*)destination = value is int i ? i : 0; break;
            case ScriptFieldType.Float:   *(float*)destination = value is float f ? f : 0.0f; break;
            case ScriptFieldType.Double:  *(double*)destination = value is double d ? d : 0.0; break;
            case ScriptFieldType.Vector2: *(Vector2*)destination = value is Vector2 v2 ? v2 : default; break;
            case ScriptFieldType.Vector3: *(Vector3*)destination = value is Vector3 v3 ? v3 : default; break;
            case ScriptFieldType.Vector4: *(Vector4*)destination = value is Vector4 v4 ? v4 : default; break;
            case ScriptFieldType.Entity:  *(ulong*)destination = (value as Entity)?.ID ?? 0; break;
            case ScriptFieldType.Asset:
                *(ulong*)destination = (value as Asset)?.Handle ?? 0;
                *((byte*)destination + AssetTypeOffset) = (byte)AssetType;
                break;
        }
    }

    public void Write(object target, void* source)
    {
        object? value = Type switch
        {
            ScriptFieldType.Bool    => *(byte*)source != 0,
            ScriptFieldType.Int     => *(int*)source,
            ScriptFieldType.Float   => *(float*)source,
            ScriptFieldType.Double  => *(double*)source,
            ScriptFieldType.Vector2 => *(Vector2*)source,
            ScriptFieldType.Vector3 => *(Vector3*)source,
            ScriptFieldType.Vector4 => *(Vector4*)source,
            ScriptFieldType.Entity  => *(ulong*)source is var id and not 0 ? new Entity(id) : null,
            ScriptFieldType.Asset   => Asset.Create(AssetType, *(ulong*)source),
            _ => throw new InvalidOperationException($"Unknown field type {Type}")
        };

        Info.SetValue(target, value);
    }
}

internal sealed class ScriptClass
{
    public Type Type { get; }
    public IReadOnlyList<ScriptField> Fields { get; }

    private readonly Dictionary<string, ScriptField> _fieldsByName;

    public ScriptClass(Type type)
    {
        Type = type;

        Fields = type.GetFields(BindingFlags.Public | BindingFlags.Instance)
                     .Where(field => !field.IsInitOnly)
                     .OrderBy(field => field.MetadataToken)
                     .DistinctBy(field => field.Name)
                     .Select(ScriptField.TryCreate)
                     .OfType<ScriptField>()
                     .ToArray();

        _fieldsByName = Fields.ToDictionary(field => field.Name);
    }

    public ScriptField? FindField(string name) => _fieldsByName.GetValueOrDefault(name);

    public object? CreateDefaults()
    {
        try
        {
            return Activator.CreateInstance(Type);
        } catch (Exception e)
        {
            Log.Warning($"Could not read the field defaults of '{Type.FullName}': {e.InnerException?.Message ?? e.Message}");
            return null;
        }
    }
}

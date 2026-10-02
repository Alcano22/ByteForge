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
    Vector4
}

internal sealed unsafe class ScriptField(FieldInfo info, ScriptFieldType type)
{
    public const int MaxValueSize = 16;

    public FieldInfo Info { get; } = info;
    public ScriptFieldType Type { get; } = type;
    public string Name => Info.Name;

    public static ScriptFieldType? TypeOf(Type type)
    {
        if (type == typeof(bool))    return ScriptFieldType.Bool;
        if (type == typeof(int))     return ScriptFieldType.Int;
        if (type == typeof(float))   return ScriptFieldType.Float;
        if (type == typeof(double))  return ScriptFieldType.Double;
        if (type == typeof(Vector2)) return ScriptFieldType.Vector2;
        if (type == typeof(Vector3)) return ScriptFieldType.Vector3;
        if (type == typeof(Vector4)) return ScriptFieldType.Vector4;
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
        }
    }

    public void Write(object target, void* source)
    {
        object value = Type switch
        {
            ScriptFieldType.Bool    => *(byte*)source != 0,
            ScriptFieldType.Int     => *(int*)source,
            ScriptFieldType.Float   => *(float*)source,
            ScriptFieldType.Double  => *(double*)source,
            ScriptFieldType.Vector2 => *(Vector2*)source,
            ScriptFieldType.Vector3 => *(Vector3*)source,
            ScriptFieldType.Vector4 => *(Vector4*)source,
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
                     .Select(field => ScriptField.TypeOf(field.FieldType) is { } fieldType ? new ScriptField(field, fieldType) : null)
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

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using ByteForge.Interop;

namespace ByteForge;

public sealed class Entity : IEquatable<Entity>
{
    public ulong ID { get; }

    private Dictionary<Type, Component>? _components;

    internal Entity(ulong id)
    {
        ID = id;
    }

    public static Entity Create(string name)
    {
        ArgumentNullException.ThrowIfNull(name);
        return new Entity(ScriptAPI.Entity_Create(name));
    }

    public static Entity Instantiate(Entity source)
    {
        ArgumentNullException.ThrowIfNull(source);
        return new Entity(ScriptAPI.Entity_Instantiate(source.ID));
    }

    public static Entity? Find(string name)
    {
        ArgumentNullException.ThrowIfNull(name);

        ulong id = ScriptAPI.Entity_FindByName(name);
        return id != 0 ? new Entity(id) : null;
    }

    public bool IsValid => ScriptAPI.Entity_IsValid(ID);
    public string Name => ScriptAPI.Entity_GetName(ID);

    public Entity? Parent
    {
        get
        {
            ulong id = ScriptAPI.Entity_GetParent(ID);
            return id != 0 ? new Entity(id) : null;
        }
        set => SetParent(value);
    }

    public int ChildCount => ScriptAPI.Entity_GetChildCount(ID);

    public Entity GetChild(int index) => new(ScriptAPI.Entity_GetChild(ID, index));

    public IReadOnlyList<Entity> Children
    {
        get
        {
            int count = ChildCount;
            var children = new List<Entity>(count);
            for (int i = 0; i < count; i++)
                children.Add(GetChild(i));
            return children;
        }
    }

    public void SetParent(Entity? parent, bool keepWorldTransform = true) =>
        ScriptAPI.Entity_SetParent(ID, parent?.ID ?? 0, keepWorldTransform);

    public Transform Transform => GetComponent<Transform>()!;

    public T? GetComponent<T>() where T : Component
    {
        if (BuiltInComponents.Contains(typeof(T)))
            return (T?)GetBuiltIn(typeof(T));

        return ScriptHost.FindScript<T>(ID);
    }

    public bool TryGetComponent<T>([NotNullWhen(true)] out T? component) where T : Component
    {
        component = GetComponent<T>();
        return component is not null;
    }

    public bool HasComponent<T>() where T : Component => GetComponent<T>() is not null;

    /// <summary>All components of type T, in inspector order. Several scripts of one type are all returned.</summary>
    public IReadOnlyList<T> GetComponents<T>() where T : Component
    {
        if (BuiltInComponents.Contains(typeof(T)))
            return GetComponent<T>() is { } single ? [single] : [];

        return ScriptHost.FindScripts<T>(ID);
    }

    private Component? GetBuiltIn(Type type)
    {
        if (!BuiltInComponents.Exists(type, ID))
            return null;

        _components ??= new Dictionary<Type, Component>();
        if (!_components.TryGetValue(type, out Component? component))
            _components[type] = component = BuiltInComponents.Create(type, this);
        
        return component;
    }

    public void Destroy() => ScriptAPI.Entity_Destroy(ID);

    public bool Equals(Entity? other) => other is not null && other.ID == ID;
    public override bool Equals(object? obj) => obj is Entity other && Equals(other);
    public override int GetHashCode() => ID.GetHashCode();
    public override string ToString() => $"Entity({ID})";

    public static bool operator ==(Entity? left, Entity? right) => left?.Equals(right) ?? right is null;
    public static bool operator !=(Entity? left, Entity? right) => !(left == right);
}

using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;

namespace ByteForge;

/// <summary>
/// Base of everything that lives on an entity: the built-in components and every ScriptBehaviour.
/// </summary>
public abstract class Component
{
    public Entity Entity { get; internal set; } = null!;

    public Transform Transform => Entity.Transform;

    private protected Component() {}

    /// <summary>The first component of type T on the same entity, or null.</summary>
    public T? GetComponent<T>() where T : Component => Entity.GetComponent<T>();

    public bool TryGetComponent<T>([NotNullWhen(true)] out T? component) where T : Component =>
        Entity.TryGetComponent(out component);

    public bool HasComponent<T>() where T : Component => Entity.HasComponent<T>();

    /// <summary>All components of type T on the same entity (several scripts can share a type).</summary>
    public IReadOnlyList<T> GetComponents<T>() where T : Component => Entity.GetComponents<T>();
}

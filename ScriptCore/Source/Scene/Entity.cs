using System;
using ByteForge.Interop;

namespace ByteForge;

public sealed class Entity : IEquatable<Entity>
{
    public ulong ID { get; }

    private Transform? _transform;
    private AudioSource? _audioSource;

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

    public Transform Transform => _transform ??= new Transform(ID);

    // Null when the entity has no Audio Source component
    public AudioSource? AudioSource => ScriptAPI.AudioSource_Exists(ID) ? (_audioSource ??= new AudioSource(ID)) : null;

    public void Destroy() => ScriptAPI.Entity_Destroy(ID);

    public bool Equals(Entity? other) => other is not null && other.ID == ID;
    public override bool Equals(object? obj) => obj is Entity other && Equals(other);
    public override int GetHashCode() => ID.GetHashCode();
    public override string ToString() => $"Entity({ID})";

    public static bool operator ==(Entity? left, Entity? right) => left?.Equals(right) ?? right is null;
    public static bool operator !=(Entity? left, Entity? right) => !(left == right);
}

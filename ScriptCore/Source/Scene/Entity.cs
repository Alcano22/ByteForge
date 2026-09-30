using System;
using ByteForge.Interop;

namespace ByteForge;

public sealed unsafe class Entity : IEquatable<Entity>
{
    public ulong ID { get; }

    private Transform? _transform;

    internal Entity(ulong id)
    {
        ID = id;
    }

    public bool IsValid => Native.Api.Entity_IsValid(ID) != 0;

    public Transform Transform => _transform ??= new Transform(ID);

    public bool Equals(Entity? other) => other is not null && other.ID == ID;
    public override bool Equals(object? obj) => obj is Entity other && Equals(other);
    public override int GetHashCode() => ID.GetHashCode();
    public override string ToString() => $"Entity({ID})";

    public static bool operator ==(Entity? left, Entity? right) => left?.Equals(right) ?? right is null;
    public static bool operator !=(Entity? left, Entity? right) => !(left == right);
}

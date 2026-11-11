using System;
using System.Collections.Generic;
using ByteForge.Interop;

namespace ByteForge;

public sealed class Entity : IEquatable<Entity>
{
    public ulong ID { get; }

    private Transform? _transform;
    private AudioSource? _audioSource;
    private Rigidbody2D? _rigidbody2D;
    private SpriteRenderer? _spriteRenderer;

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

    public Transform Transform => _transform ??= new Transform(ID);
    public SpriteRenderer? SpriteRenderer => ScriptAPI.SpriteRenderer_Exists(ID) ? (_spriteRenderer ??= new SpriteRenderer(ID)) : null;
    public AudioSource? AudioSource => ScriptAPI.AudioSource_Exists(ID) ? (_audioSource ??= new AudioSource(ID)) : null;
    public Rigidbody2D? Rigidbody2D => ScriptAPI.Rigidbody2D_Exists(ID) ? (_rigidbody2D ??= new Rigidbody2D(ID)) : null;

    public void Destroy() => ScriptAPI.Entity_Destroy(ID);

    public bool Equals(Entity? other) => other is not null && other.ID == ID;
    public override bool Equals(object? obj) => obj is Entity other && Equals(other);
    public override int GetHashCode() => ID.GetHashCode();
    public override string ToString() => $"Entity({ID})";

    public static bool operator ==(Entity? left, Entity? right) => left?.Equals(right) ?? right is null;
    public static bool operator !=(Entity? left, Entity? right) => !(left == right);
}

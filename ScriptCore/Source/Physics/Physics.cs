using System;
using System.Numerics;
using ByteForge.Interop;

namespace ByteForge;

public readonly struct RaycastHit
{
    public Entity Entity { get; }
    public Vector2 Point { get; }
    public Vector2 Normal { get; }
    public float Distance { get; }

    internal RaycastHit(Entity entity, Vector2 point, Vector2 normal, float distance)
    {
        Entity = entity;
        Point = point;
        Normal = normal;
        Distance = distance;
    }
}

public static class Physics
{
    public const int AllLayers = -1;

    public static int LayerMask(params int[] layers)
    {
        int mask = 0;
        foreach (int layer in layers)
        {
            if (layer is < 0 or > 31)
                throw new ArgumentOutOfRangeException(nameof(layers), "Physics layers range from 0 to 31.");
            mask |= 1 << layer;
        }
        return mask;
    }

    public static bool Raycast(Vector2 origin, Vector2 direction, float maxDistance, 
                               out RaycastHit hit, int layerMask = AllLayers)
    {
        ulong id = ScriptAPI.Physics_Raycast(origin, direction, maxDistance, layerMask);
        if (id == 0)
        {
            hit = default;
            return false;
        }

        hit = new RaycastHit(new Entity(id),
                             ScriptAPI.Physics_RaycastPoint(origin, direction, maxDistance, layerMask),
                             ScriptAPI.Physics_RaycastNormal(origin, direction, maxDistance, layerMask),
                             ScriptAPI.Physics_RaycastDistance(origin, direction, maxDistance, layerMask));
        return true;
    }
}

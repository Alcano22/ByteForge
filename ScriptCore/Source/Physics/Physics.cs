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
    public static bool Raycast(Vector2 origin, Vector2 direction, float maxDistance, out RaycastHit hit)
    {
        ulong id = ScriptAPI.Physics_Raycast(origin, direction, maxDistance);
        if (id == 0)
        {
            hit = default;
            return false;
        }

        hit = new RaycastHit(new Entity(id),
                             ScriptAPI.Physics_RaycastPoint(origin, direction, maxDistance),
                             ScriptAPI.Physics_RaycastNormal(origin, direction, maxDistance),
                             ScriptAPI.Physics_RaycastDistance(origin, direction, maxDistance));
        return true;
    }
}

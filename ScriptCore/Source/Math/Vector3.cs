using System;
using System.Runtime.InteropServices;

namespace ByteForge;

/// <summary>3D vector. The layout matches glm::vec3 on the native side, do not add fields.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct Vector3 : IEquatable<Vector3>
{
    public static Vector3 Zero  => new( 0f,  0f, 0f);
    public static Vector3 One   => new( 1f,  1f, 1f);
    public static Vector3 Up    => new( 0f,  1f, 0f);
    public static Vector3 Down  => new( 0f, -1f, 0f);
    public static Vector3 Left  => new(-1f,  0f, 0f);
    public static Vector3 Right => new( 1f,  0f, 0f);

    public float X;
    public float Y;
    public float Z;

    public readonly float SqrMagnitude => X * X + Y * Y + Z * Z;
    public readonly float Magnitude => Mathf.Sqrt(SqrMagnitude);

    /// <summary>Unit-length copy, or zero if the vector is (almost) zero.</summary>
    public readonly Vector3 Normalized
    {
        get
        {
            float magnitude = Magnitude;
            return magnitude > Mathf.NormalizeEpsilon ? this / magnitude : Zero;
        }
    }

    public Vector3(float x, float y, float z)
    {
        X = x;
        Y = y;
        Z = z;
    }

    public Vector3(float x, float y)
        : this(x, y, 0f) {}

    public Vector3(float scalar)
        : this(scalar, scalar, scalar) {}

    public void Normalize() => this = Normalized;

    public static float Dot(Vector3 a, Vector3 b) => a.X * b.X + a.Y * b.Y + a.Z * b.Z;

    public static float Distance(Vector3 a, Vector3 b) => (a - b).Magnitude;

    /// <summary>Component-wise product.</summary>
    public static Vector3 Scale(Vector3 a, Vector3 b) => new(a.X * b.X, a.Y * b.Y, a.Z * b.Z);

    public static Vector3 Min(Vector3 a, Vector3 b) =>
        new(Mathf.Min(a.X, b.X), Mathf.Min(a.Y, b.Y), Mathf.Min(a.Z, b.Z));

    public static Vector3 Max(Vector3 a, Vector3 b) =>
        new(Mathf.Max(a.X, b.X), Mathf.Max(a.Y, b.Y), Mathf.Max(a.Z, b.Z));

    public static Vector3 LerpUnclamped(Vector3 a, Vector3 b, float t) =>
        new(a.X + (b.X - a.X) * t, a.Y + (b.Y - a.Y) * t, a.Z + (b.Z - a.Z) * t);

    /// <summary>Interpolates between a and b, t is clamped to [0, 1].</summary>
    public static Vector3 Lerp(Vector3 a, Vector3 b, float t) => LerpUnclamped(a, b, Mathf.Clamp01(t));

    public static Vector3 MoveTowards(Vector3 current, Vector3 target, float maxDistanceDelta)
    {
        Vector3 delta = target - current;
        float sqrDistance = delta.SqrMagnitude;
        if (sqrDistance == 0f || (maxDistanceDelta >= 0f && sqrDistance <= maxDistanceDelta * maxDistanceDelta))
            return target;

        return current + delta / Mathf.Sqrt(sqrDistance) * maxDistanceDelta;
    }

    public static Vector3 ClampMagnitude(Vector3 vector, float maxLength)
    {
        float sqrMagnitude = vector.SqrMagnitude;
        if (sqrMagnitude <= maxLength * maxLength)
            return vector;

        return vector / Mathf.Sqrt(sqrMagnitude) * maxLength;
    }

    public static Vector3 operator +(Vector3 a, Vector3 b) => new(a.X + b.X, a.Y + b.Y, a.Z + b.Z);
    public static Vector3 operator -(Vector3 a, Vector3 b) => new(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
    public static Vector3 operator -(Vector3 v) => new(-v.X, -v.Y, -v.Z);
    public static Vector3 operator *(Vector3 v, float scalar) => new(v.X * scalar, v.Y * scalar, v.Z * scalar);
    public static Vector3 operator *(float scalar, Vector3 v) => v * scalar;
    public static Vector3 operator /(Vector3 v, float scalar) => new(v.X / scalar, v.Y / scalar, v.Z / scalar);

    public static bool operator ==(Vector3 left, Vector3 right) => left.Equals(right);
    public static bool operator !=(Vector3 left, Vector3 right) => !left.Equals(right);

    /// <summary>Adds Z = 0. Implicit because nothing gets lost.</summary>
    public static implicit operator Vector3(Vector2 v) => new(v.X, v.Y, 0f);

    public readonly bool Equals(Vector3 other) => X == other.X && Y == other.Y && Z == other.Z;
    public override readonly bool Equals(object? obj) => obj is Vector3 other && Equals(other);
    public override readonly int GetHashCode() => HashCode.Combine(X, Y, Z);
    public override readonly string ToString() => FormattableString.Invariant($"({X}, {Y}, {Z})");
}

using System;
using System.Diagnostics.CodeAnalysis;
using System.Numerics;
using System.Runtime.InteropServices;

namespace ByteForge;

/// <summary>2D vector. The layout matches glm::vec2 on the native side, do not add fields.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct Vector2 : IEquatable<Vector2>
{
    public static Vector2 Zero  => new( 0f,  0f);
    public static Vector2 One   => new( 1f,  1f);
    public static Vector2 Up    => new( 0f,  1f);
    public static Vector2 Down  => new( 0f, -1f);
    public static Vector2 Left  => new(-1f,  0f);
    public static Vector2 Right => new( 1f,  0f);

    public float X;
    public float Y;

    public readonly float SqrMagnitude => X * X + Y * Y;
    public readonly float Magnitude => Mathf.Sqrt(SqrMagnitude);

    public readonly Vector2 Normalized
    {
        get
        {
            float magnitude = Magnitude;
            return magnitude > Mathf.NormalizeEpsilon ? this / magnitude : Zero;
        }
    }

    public Vector2(float x, float y)
    {
        X = x;
        Y = y;
    }

    public Vector2(float scalar)
        : this(scalar, scalar) {}

    public void Normalize() => this = Normalized;

    public static float Dot(Vector2 a, Vector2 b) => a.X * b.X + a.Y * b.Y;

    public static float Distance(Vector2 a, Vector2 b) => (a - b).Magnitude;

    /// <summary>Component-wise product.</summary>
    public static Vector2 Scale(Vector2 a, Vector2 b) => new(a.X * b.X, a.Y * b.Y);

    public static Vector2 Min(Vector2 a, Vector2 b) => new(Mathf.Min(a.X, b.X), Mathf.Min(a.Y, b.Y));
    public static Vector2 Max(Vector2 a, Vector2 b) => new(Mathf.Max(a.X, b.X), Mathf.Max(a.Y, b.Y));

    public static Vector2 LerpUnclamped(Vector2 a, Vector2 b, float t) => new(a.X + (b.X - a.X) * t, a.Y + (b.Y - a.Y) * t);

    /// <summary>Interpolates between a and b, t is clamped to [0, 1].</summary>
    public static Vector2 Lerp(Vector2 a, Vector2 b, float t) => LerpUnclamped(a, b, Mathf.Clamp01(t));

    public static Vector2 MoveTowards(Vector2 current, Vector2 target, float maxDistanceDelta)
    {
        Vector2 delta = target - current;
        float sqrDistance = delta.SqrMagnitude;
        if (sqrDistance == 0f || (maxDistanceDelta >= 0f && sqrDistance <= maxDistanceDelta * maxDistanceDelta))
            return target;

        return current + delta / Mathf.Sqrt(sqrDistance) * maxDistanceDelta;
    }

    public static Vector2 ClampMagnitude(Vector2 vector, float maxLength)
    {
        float sqrMagnitude = vector.SqrMagnitude;
        if (sqrMagnitude <= maxLength * maxLength)
            return vector;

        return vector / Mathf.Sqrt(sqrMagnitude) * maxLength;
    }

    /// <summary>Reflects a direction at a surface. The normal must be normalized.</summary>
    public static Vector2 Reflect(Vector2 direction, Vector2 normal) => direction - 2f * Dot(direction, normal) * normal;

    /// <summary>The vector rotated by 90 degrees counter-clockwise.</summary>
    public static Vector2 Perpendicular(Vector2 vector) => new(-vector.Y, vector.X);

    /// <summary>Unsigned angle between two vectors in degrees, in [0, 180].</summary>
    public static float Angle(Vector2 from, Vector2 to)
    {
        float denom = Mathf.Sqrt(from.SqrMagnitude * to.SqrMagnitude);
        if (denom < 1e-15f)
            return 0f;

        float cosine = Mathf.Clamp(Dot(from, to) / denom, -1f, 1f);
        return Mathf.Acos(cosine) * Mathf.Rad2Deg;
    }

    /// <summary>Angle from one vector to another in degrees, counter-clockwise is positive.</summary>
    public static float SignedAngle(Vector2 from, Vector2 to) =>
        Angle(from, to) * Mathf.Sign(from.X * to.Y - from.Y * to.X);

    public static Vector2 operator +(Vector2 a, Vector2 b) => new(a.X + b.X, a.Y + b.Y);
    public static Vector2 operator -(Vector2 a, Vector2 b) => new(a.X - b.X, a.Y - b.Y);
    public static Vector2 operator -(Vector2 v) => new(-v.X, -v.Y);
    public static Vector2 operator *(Vector2 v, float scalar) => new(v.X * scalar, v.Y * scalar);
    public static Vector2 operator *(float scalar, Vector2 v) => v * scalar;
    public static Vector2 operator /(Vector2 v, float scalar) => new(v.X / scalar, v.Y / scalar);

    public static bool operator ==(Vector2 left, Vector2 right) => left.Equals(right);
    public static bool operator !=(Vector2 left, Vector2 right) => !left.Equals(right);

    /// <summary>Drops Z. Explicit because information gets lost.</summary>
    public static explicit operator Vector2(Vector3 v) => new(v.X, v.Y);

    public readonly bool Equals(Vector2 other) => X == other.X && Y == other.Y;
    public override readonly bool Equals(object? obj) => obj is Vector2 other && Equals(other);
    public override readonly int GetHashCode() => HashCode.Combine(X, Y);
    public override readonly string ToString() => FormattableString.Invariant($"({X}, {Y})");
}

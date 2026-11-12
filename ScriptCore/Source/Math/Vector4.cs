using System;
using System.Runtime.InteropServices;

namespace ByteForge;

/// <summary>
/// 4D vector. The layout matches glm::vec4 on the native side, do not add fields.
/// Used for inspector fields such as colors until a dedicated Color type exists.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct Vector4 : IEquatable<Vector4>
{
    public float X;
    public float Y;
    public float Z;
    public float W;

    public static Vector4 Zero => new(0f, 0f, 0f, 0f);
    public static Vector4 One  => new(1f, 1f, 1f, 1f);

    public Vector4(float x, float y, float z, float w)
    {
        X = x;
        Y = y;
        Z = z;
        W = w;
    }

    public Vector4(float x, float y, float z)
        : this(x, y, z, 0f) {}

    public Vector4(float x, float y)
        : this(x, y, 0f, 0f) {}

    public Vector4(float scalar)
        : this(scalar, scalar, scalar, scalar) {}

    public static Vector4 operator +(Vector4 a, Vector4 b) => new(a.X + b.X, a.Y + b.Y, a.Z + b.Z, a.W + b.W);
    public static Vector4 operator -(Vector4 a, Vector4 b) => new(a.X - b.X, a.Y - b.Y, a.Z - b.Z, a.W - b.W);
    public static Vector4 operator -(Vector4 v) => new(-v.X, -v.Y, -v.Z, -v.W);
    public static Vector4 operator *(Vector4 v, float scalar) => new(v.X * scalar, v.Y * scalar, v.Z * scalar, v.W * scalar);
    public static Vector4 operator *(float scalar, Vector4 v) => v * scalar;
    public static Vector4 operator /(Vector4 v, float scalar) => new(v.X / scalar, v.Y / scalar, v.Z / scalar, v.W / scalar);

    public static bool operator ==(Vector4 left, Vector4 right) => left.Equals(right);
    public static bool operator !=(Vector4 left, Vector4 right) => !left.Equals(right);

    public readonly bool Equals(Vector4 other) => X == other.X && Y == other.Y && Z == other.Z && W == other.W;
    public override readonly bool Equals(object? obj) => obj is Vector4 other && Equals(other);
    public override readonly int GetHashCode() => HashCode.Combine(X, Y, Z, W);
    public override readonly string ToString() => FormattableString.Invariant($"({X}, {Y}, {Z}, {W})");
}

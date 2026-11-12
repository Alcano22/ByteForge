using System;

namespace ByteForge;

public static class Mathf
{
    public const float PI = MathF.PI;
    public const float Deg2Rad = PI / 180f;
    public const float Rad2Deg = 180f / PI;
    public const float Infinity = float.PositiveInfinity;

    internal const float NormalizeEpsilon = 1e-5f;

    public static float Abs(float value) => MathF.Abs(value);
    public static int Abs(int value) => Math.Abs(value);

    public static float Sign(float value) => value >= 0f ? 1f : -1f;

    public static float Min(float a, float b) => MathF.Min(a, b);
    public static int Min(int a, int b) => Math.Min(a, b);
    public static float Max(float a, float b) => MathF.Max(a, b);
    public static int Max(int a, int b) => Math.Max(a, b);

    public static float Clamp(float value, float min, float max) => value < min ? min : value > max ? max : value;
    public static int Clamp(int value, int min, int max) => value < min ? min : value > max ? max : value;
    public static float Clamp01(float value) => Clamp(value, 0f, 1f);

    public static float Sqrt(float value) => MathF.Sqrt(value);
    public static float Pow(float value, float power) => MathF.Pow(value, power);

    public static float Sin(float radians) => MathF.Sin(radians);
    public static float Cos(float radians) => MathF.Cos(radians);
    public static float Acos(float radians) => MathF.Acos(radians);
    public static float Tan(float radians) => MathF.Tan(radians);
    public static float Atan2(float y, float x) => MathF.Atan2(y, x);

    public static float Floor(float value) => MathF.Floor(value);
    public static float Ceil(float value) => MathF.Ceiling(value);
    public static float Round(float value) => MathF.Round(value);
    public static int FloorToInt(float value) => (int)Floor(value);
    public static int CeilToInt(float value) => (int)Ceil(value);
    public static int RoundToInt(float value) => (int)Round(value);

    public static float LerpUnclamped(float a, float b, float t) => a + (b - a) * t;

    /// <summary>Interpolates between a and b, t is clamped to [0, 1].</summary>
    public static float Lerp(float a, float b, float t) => LerpUnclamped(a, b, Clamp01(t));

    /// <summary>Returns where value lies between a and b as a fraction in [0, 1].</summary>
    public static float InverseLerp(float a, float b, float value) =>
        a != b ? Clamp01((value - a) / (b - a)) : 0f;

    public static float MoveTowards(float current, float target, float maxDelta)
    {
        if (MathF.Abs(target - current) <= maxDelta)
            return target;

        return current + Sign(target - current) * maxDelta;
    }

    /// <summary>Wraps t into [0, length).</summary>
    public static float Repeat(float t, float length) => Clamp(t - Floor(t / length) * length, 0f, length);

    /// <summary>Shortest difference between two angles in degrees, in [-180, 180].</summary>
    public static float DeltaAngle(float current, float target)
    {
        float delta = Repeat(target - current, 360f);
        if (delta > 180f)
            delta -= 360f;
        
        return delta;
    }

    public static float LerpAngle(float a, float b, float t) => a + DeltaAngle(a, b) * Clamp01(t);

    public static bool Approximately(float a, float b) =>
        Abs(b - a) < Max(1e-6f * Max(Abs(a), Abs(b)), float.Epsilon * 8f);

    /// <summary>Critically damped spring towards the target. Keep the velocity variable between calls.</summary>
    public static float SmoothDamp(float current, float target, ref float velocity, float smoothTime,
                                   float deltaTime, float maxSpeed = Infinity)
    {
        smoothTime = Max(0.0001f, smoothTime);

        float omega = 2.0f / smoothTime;
        float x = omega * deltaTime;
        float damping = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);

        float change = current - target;
        float originalTarget = target;

        float maxChange = maxSpeed * smoothTime;
        change = Clamp(change, -maxChange, maxChange);
        target = current - change;

        float temp = (velocity + omega * change) * deltaTime;
        velocity = (velocity - omega * temp) * damping;
        float output = target + (change + temp) * damping;

        // Prevent overshooting
        if (originalTarget - current > 0.0f == output > originalTarget)
        {
            output = originalTarget;
            velocity = (output - originalTarget) / deltaTime;
        }

        return output;
    }
}

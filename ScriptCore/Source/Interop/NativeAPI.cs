using System.Numerics;

namespace ByteForge.Interop;

#pragma warning disable CS0649

internal unsafe struct NativeAPI
{
    public int Size;

    public delegate* unmanaged<int, byte*, void> Log;
    public delegate* unmanaged<byte*, void> ReportException;

    public delegate* unmanaged<ulong, int> Entity_IsValid;

    public delegate* unmanaged<ulong, Vector3*, int> Transform_GetPosition;
    public delegate* unmanaged<ulong, Vector3*, int> Transform_SetPosition;
    public delegate* unmanaged<ulong, float*, int> Transform_GetRotation;
    public delegate* unmanaged<ulong, float, int> Transform_SetRotation;
    public delegate* unmanaged<ulong, Vector2*, int> Transform_GetScale;
    public delegate* unmanaged<ulong, Vector2*, int> Transform_SetScale;
}

#pragma warning restore CS0649

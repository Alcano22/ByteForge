using System.Numerics;

namespace ByteForge.Interop;

#pragma warning disable CS0649

internal unsafe struct NativeAPI
{
    public int Size;

    public delegate* unmanaged<int, byte*, void> Log;
    public delegate* unmanaged<byte*, void> ReportException;

    public delegate* unmanaged<byte*, byte*, uint> Api_FindFunction;
    public delegate* unmanaged<uint, APIValue*, int, APIValue*, int> Api_Invoke;
    public delegate* unmanaged<byte*> Api_GetLastError;
}

#pragma warning restore CS0649

using System;
using System.Runtime.InteropServices;

namespace ByteForge.Interop;

internal readonly unsafe ref struct Utf8String
{
    public readonly byte* Pointer;

    public Utf8String(string text)
    {
        Pointer = (byte*)Marshal.StringToCoTaskMemUTF8(text);
    }

    public void Dispose() => Marshal.FreeCoTaskMem((IntPtr)Pointer);
}

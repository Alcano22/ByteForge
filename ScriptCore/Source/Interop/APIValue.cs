using System.Numerics;
using System.Runtime.InteropServices;
using System.Text;

namespace ByteForge.Interop;

[StructLayout(LayoutKind.Explicit, Size = 16)]
internal unsafe struct APIValue
{
    [FieldOffset(0)] public byte Bool;
    [FieldOffset(0)] public int Int;
    [FieldOffset(0)] public float Float;
    [FieldOffset(0)] public double Double;
    [FieldOffset(0)] public Vector2 Vector2;
    [FieldOffset(0)] public Vector3 Vector3;
    [FieldOffset(0)] public Vector4 Vector4;
    [FieldOffset(0)] public ulong Handle; // Entity UUID or asset handle
    [FieldOffset(0)] public byte* Text;
    [FieldOffset(8)] public byte AssetType;
    [FieldOffset(8)] public int TextLength;

    public readonly string ReadText() =>
        Text == null || TextLength <= 0 ? string.Empty : Encoding.UTF8.GetString(Text, TextLength);
}

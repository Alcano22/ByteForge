using System;

namespace ByteForge.Interop;

internal static unsafe class Native
{
    internal static NativeAPI Api;

    internal static void ReportException(Exception exception)
    {
        using var text = new Utf8String(exception.ToString());
        Api.ReportException(text.Pointer);
    }
}

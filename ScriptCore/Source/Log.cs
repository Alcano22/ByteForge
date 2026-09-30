using ByteForge.Interop;

namespace ByteForge;

public enum LogLevel
{
    Trace   = 0,
    Info    = 1,
    Warning = 2,
    Error   = 3
}

public static class Log
{
    public static void Trace(string message) => Write(LogLevel.Trace, message);
    public static void Info(string message) => Write(LogLevel.Info, message);
    public static void Warning(string message) => Write(LogLevel.Warning, message);
    public static void Error(string message) => Write(LogLevel.Error, message);

    private static unsafe void Write(LogLevel level, string message)
    {
        using var text = new Utf8String(message);
        Native.Api.Log((int)level, text.Pointer);
    }
}

using System;

namespace ByteForge;

public sealed class ScriptException(string message) : Exception(message);

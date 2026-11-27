using ByteForge.Interop;

namespace ByteForge;

/// <summary>The kind of controller, as far as the platform can tell.</summary>
public enum GamepadType : byte
{
    Unknown,
    Standard,
    Xbox360,
    XboxOne,
    PlayStation3,
    PlayStation4,
    PlayStation5,
    SwitchPro,
    SwitchJoyConLeft,
    SwitchJoyConRight,
    SwitchJoyConPair
}

public enum GamepadTrigger : byte
{
    Left, Right
}

/// <summary>What a connected gamepad can do. Everything is false for a missing gamepad.</summary>
public readonly record struct GamepadFeatures(bool Rumble, bool TriggerRumble, bool Led, bool AdaptiveTriggers);

/// <summary>
/// Direct access to gamepads by slot (0 to <see cref="MaxGamepads"/> - 1). Output functions return false
/// when the gamepad is missing or cannot do it, check <see cref="GetFeatures"/> if the difference matters. 
/// </summary>
public static class Gamepad
{
    public const int MaxGamepads = 4;

    public static bool IsConnected(int gamepad = 0) => ScriptAPI.Gamepad_IsConnected(gamepad);

    public static string GetName(int gamepad = 0) => ScriptAPI.Gamepad_GetName(gamepad);

    public static GamepadType GetDeviceType(int gamepad = 0) => (GamepadType)ScriptAPI.Gamepad_GetType(gamepad);

    public static GamepadFeatures GetFeatures(int gamepad = 0) => new(
        ScriptAPI.Gamepad_SupportsRumble(gamepad),
        ScriptAPI.Gamepad_SupportsTriggerRumble(gamepad),
        ScriptAPI.Gamepad_SupportsLed(gamepad),
        ScriptAPI.Gamepad_SupportsAdaptiveTriggers(gamepad)
    );

    /// <summary>Intensities in [0, 1], duration in seconds. A new call replaces the running rumble, 0 stops it.</summary>
    public static bool Rumble(float lowFrequency, float highFrequency, float duration, int gamepad = 0) =>
        ScriptAPI.Gamepad_Rumble(gamepad, lowFrequency, highFrequency, duration);

    /// <summary>The motors in the triggers of Xbox One and Series controllers. Intensities in [0, 1].</summary>
    public static bool RumbleTriggers(float left, float right, float duration, int gamepad = 0) =>
        ScriptAPI.Gamepad_RumbleTriggers(gamepad, left, right, duration);

    /// <summary>Light bar color, each channel in [0, 1].</summary>
    public static bool SetLed(Vector3 color, int gamepad = 0) => ScriptAPI.Gamepad_SetLed(gamepad, color);

    /// <summary>Removes the effect, the trigger moves freely again.</summary>
    public static bool SetTriggerOff(GamepadTrigger trigger, int gamepad = 0) =>
        ScriptAPI.Gamepad_SetTriggerOff(gamepad, (int)trigger);

    /// <summary>Constant resistance from position (0-9) on. Strength 1-8, 0 turns it off.</summary>
    public static bool SetTriggerFeedback(GamepadTrigger trigger, int position, int strength, int gamepad = 0) =>
        ScriptAPI.Gamepad_SetTriggerFeedback(gamepad, (int)trigger, position, strength);

    /// <summary>Resistance between start (2-7) and end (start + 1 to 8) that gives way at end, like a gun trigger.</summary>
    public static bool SetTriggerWeapon(GamepadTrigger trigger, int start, int end, int strength, int gamepad = 0) =>
        ScriptAPI.Gamepad_SetTriggerWeapon(gamepad, (int)trigger, start, end, strength);

    /// <summary>Vibration from position (0-9) on. Amplitude 1-8, frequency 1-255 Hz.</summary>
    public static bool SetTriggerVibration(GamepadTrigger trigger, int position, int amplitude, int frequency,
                                           int gamepad = 0) =>
        ScriptAPI.Gamepad_SetTriggerVibration(gamepad, (int)trigger, position, amplitude, frequency);
}

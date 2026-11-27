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

public enum GamepadButton : ushort
{
    South = 0, East = 1, West = 2, North = 3,
    LeftBumper = 4, RightBumper = 5,
    Back = 6, Start = 7, Guide = 8,
    LeftThumb = 9, RightThumb = 10,
    DPadUp = 11, DPadRight = 12, DPadDown = 13, DPadLeft = 14,
    Touchpad = 15, Misc = 16
}

/// <summary>Sticks are in [-1, 1] with Y pointing up, triggers in [0, 1].</summary>
public enum GamepadAxis : ushort
{
    LeftX = 0, LeftY = 1,
    RightX = 2, RightY = 3,
    LeftTrigger = 4, RightTrigger = 5
}

public enum GamepadTrigger : byte
{
    Left, Right
}

/// <summary>What a connected gamepad can do. Everything is false for a missing gamepad.</summary>
public readonly record struct GamepadFeatures(bool Rumble, bool TriggerRumble, bool Led, bool AdaptiveTriggers,
                                              bool Touchpad, bool Gyro, bool Accelerometer);

/// <summary>
/// Direct access to gamepads by slot (0 to <see cref="MaxGamepads"/> - 1). Output functions return false
/// when the gamepad is missing or cannot do it, check <see cref="GetFeatures"/> if the difference matters. 
/// </summary>
public static class Gamepad
{
    public const int MaxGamepads = 4;
    public const int MaxTouchFingers = 2;

    public static bool IsConnected(int gamepad = 0) => ScriptAPI.Gamepad_IsConnected(gamepad);

    public static string GetName(int gamepad = 0) => ScriptAPI.Gamepad_GetName(gamepad);

    public static GamepadType GetDeviceType(int gamepad = 0) => (GamepadType)ScriptAPI.Gamepad_GetType(gamepad);

    public static GamepadFeatures GetFeatures(int gamepad = 0) => new(
        ScriptAPI.Gamepad_SupportsRumble(gamepad),
        ScriptAPI.Gamepad_SupportsTriggerRumble(gamepad),
        ScriptAPI.Gamepad_SupportsLed(gamepad),
        ScriptAPI.Gamepad_SupportsAdaptiveTriggers(gamepad),
        ScriptAPI.Gamepad_SupportsTouchpad(gamepad),
        ScriptAPI.Gamepad_SupportsGyro(gamepad),
        ScriptAPI.Gamepad_SupportsAccelerometer(gamepad)
    );

    public static bool IsButtonDown(GamepadButton button, int gamepad = 0) =>
        ScriptAPI.Gamepad_IsButtonDown(gamepad, (int)button);

    public static bool IsButtonPressed(GamepadButton button, int gamepad = 0) =>
        ScriptAPI.Gamepad_IsButtonPressed(gamepad, (int)button);

    public static bool IsButtonReleased(GamepadButton button, int gamepad = 0) =>
        ScriptAPI.Gamepad_IsButtonReleased(gamepad, (int)button);

    public static float GetAxis(GamepadAxis axis, int gamepad = 0) =>
        ScriptAPI.Gamepad_GetAxis(gamepad, (int)axis);

    public static bool IsTouchDown(int finger = 0, int gamepad = 0) => ScriptAPI.Gamepad_IsTouchDown(gamepad, finger);

    public static bool IsTouchPressed(int finger = 0, int gamepad = 0) =>
        ScriptAPI.Gamepad_IsTouchPressed(gamepad, finger);

    public static bool IsTouchReleased(int finger = 0, int gamepad = 0) =>
        ScriptAPI.Gamepad_IsTouchReleased(gamepad, finger);

    public static Vector2 GetTouchPosition(int finger = 0, int gamepad = 0) =>
        ScriptAPI.Gamepad_GetTouchPosition(gamepad, finger);

    /// <summary>Movement since the last frame, zero on the frame the finger lands.</summary>
    public static Vector2 GetTouchDelta(int finger = 0, int gamepad = 0) =>
        ScriptAPI.Gamepad_GetTouchDelta(gamepad, finger);

    public static float GetTouchPressure(int finger = 0, int gamepad = 0) =>
        ScriptAPI.Gamepad_GetTouchPressure(gamepad, finger);

    /// <summary>Latest gyro reading in rad/s: X pitch, Y yaw, Z roll.</summary>
    public static Vector3 GetAngularVelocity(int gamepad = 0) => ScriptAPI.Gamepad_GetAngularVelocity(gamepad);

    /// <summary>Radians turned since the last frame, from every sample. Use this for gyro aiming.</summary>
    public static Vector3 GetRotationDelta(int gamepad = 0) => ScriptAPI.Gamepad_GetRotationDelta(gamepad);

    /// <summary>m/s² including gravity, about (0, 9.81, 0) while lying flat.</summary>
    public static Vector3 GetAcceleration(int gamepad = 0) => ScriptAPI.Gamepad_GetAcceleration(gamepad);

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

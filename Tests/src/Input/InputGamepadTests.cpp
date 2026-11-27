#include <Engine/Event/GamepadEvent.h>
#include <Engine/Input/Input.h>
#include <Engine/Input/GamepadBackend.h>

#include <catch2/catch_test_macros.hpp>

using namespace ByteForge;

namespace
{
    class GamepadFixture
    {
    public:
        GamepadFixture() { Reset(); }
        ~GamepadFixture() { Reset(); }

        GamepadFixture(const GamepadFixture&) = delete;
        GamepadFixture& operator=(const GamepadFixture&) = delete;

    protected:
        static void Connect(const u32 slot = 0)
        {
            GamepadConnectedEvent event(slot, "Test Gamepad");
            Input::OnEvent(event);
        }

        static void Disconnect(const u32 slot = 0)
        {
            GamepadDisconnectedEvent event(slot);
            Input::OnEvent(event);
        }

        static void Press(const GamepadButton button, const u32 slot = 0)
        {
            GamepadButtonPressedEvent event(slot, button);
            Input::OnEvent(event);
        }

        static void Release(const GamepadButton button, const u32 slot = 0)
        {
            GamepadButtonReleasedEvent event(slot, button);
            Input::OnEvent(event);
        }

        static void MoveAxis(const GamepadAxis axis, const f32 value, const u32 slot = 0)
        {
            GamepadAxisMovedEvent event(slot, axis, value);
            Input::OnEvent(event);
        }

    private:
        static void Reset()
        {
            Input::SetGamepadBlocked(false);

            for (u32 slot = 0; slot < MaxGamepads; ++slot)
                Disconnect(slot);

            Input::EndFrame();
        }
    };

    class FakeGamepadBackend final : public GamepadBackend
    {
    public:
        u32 LastGamepad = MaxGamepads;
        f32 LastLow = 0.0f;
        f32 LastHigh = 0.0f;
        f32 LastDuration = 0.0f;

        [[nodiscard]] GamepadType GetType(u32) const override { return GamepadType::PlayStation5; }
        [[nodiscard]] GamepadFeatures GetFeatures(u32) const override { return { .Rumble = true }; }

        bool Rumble(const u32 gamepad, const f32 low, const f32 high, const f32 duration) override
        {
            LastGamepad = gamepad;
            LastLow = low;
            LastHigh = high;
            LastDuration = duration;
            return true;
        }

        bool RumbleTriggers(u32, f32, f32, f32) override { return false; }
        bool SetLed(u32, const glm::vec3&) override { return false; }
        bool SetTriggerEffect(u32, GamepadTrigger, const TriggerEffect&) override { return false; }
    };

    class BackendGuard
    {
    public:
        explicit BackendGuard(GamepadBackend& backend) { Input::SetGamepadBackend(&backend); }
        ~BackendGuard() { Input::SetGamepadBackend(nullptr); }

        BackendGuard(const BackendGuard&) = delete;
        BackendGuard& operator=(const BackendGuard&) = delete;
    };
}

TEST_CASE_METHOD(GamepadFixture, "A connected gamepad reports its slot and name", "[input][gamepad]")
{
    Connect(1);

    CHECK(Input::IsGamepadConnected(1));
    CHECK(Input::GetGamepadName(1) == "Test Gamepad");
    CHECK_FALSE(Input::IsGamepadConnected(0));
}

TEST_CASE_METHOD(GamepadFixture, "A button is pressed for exactly one frame", "[input][gamepad]")
{
    Connect();
    Press(GamepadButton::South);

    CHECK(Input::IsGamepadButtonDown(GamepadButton::South));
    CHECK(Input::IsGamepadButtonPressed(GamepadButton::South));
    CHECK_FALSE(Input::IsGamepadButtonReleased(GamepadButton::South));

    Input::EndFrame();

    CHECK(Input::IsGamepadButtonDown(GamepadButton::South));
    CHECK_FALSE(Input::IsGamepadButtonPressed(GamepadButton::South));
}

TEST_CASE_METHOD(GamepadFixture, "A button is released for exactly one frame", "[input][gamepad]")
{
    Connect();
    Press(GamepadButton::East);
    Input::EndFrame();

    Release(GamepadButton::East);

    CHECK_FALSE(Input::IsGamepadButtonDown(GamepadButton::East));
    CHECK(Input::IsGamepadButtonReleased(GamepadButton::East));

    Input::EndFrame();

    CHECK_FALSE(Input::IsGamepadButtonReleased(GamepadButton::East));
}

TEST_CASE_METHOD(GamepadFixture, "Axis values are kept until they change", "[input][gamepad]")
{
    Connect();
    MoveAxis(GamepadAxis::LeftY, 0.5f);
    Input::EndFrame();

    CHECK(Input::GetGamepadAxis(GamepadAxis::LeftY) == 0.5f);
    CHECK(Input::GetGamepadAxis(GamepadAxis::LeftX) == 0.0f);
}

TEST_CASE_METHOD(GamepadFixture, "Unplugging a gamepad releases its buttons and zeroes its axes", "[input][gamepad]")
{
    Connect();
    Press(GamepadButton::North);
    MoveAxis(GamepadAxis::RightTrigger, 1.0f);
    Input::EndFrame();

    Disconnect();

    CHECK_FALSE(Input::IsGamepadConnected());
    CHECK(Input::GetGamepadName().empty());
    CHECK_FALSE(Input::IsGamepadButtonDown(GamepadButton::North));
    CHECK(Input::IsGamepadButtonReleased(GamepadButton::North));
    CHECK(Input::GetGamepadAxis(GamepadAxis::RightTrigger) == 0.0f);
}

TEST_CASE_METHOD(GamepadFixture, "A blocked gamepad reports nothing but keeps its state", "[input][gamepad]")
{
    Connect();
    Press(GamepadButton::Start);
    MoveAxis(GamepadAxis::LeftX, -1.0f);

    Input::SetGamepadBlocked(true);

    CHECK_FALSE(Input::IsGamepadButtonDown(GamepadButton::Start));
    CHECK(Input::GetGamepadAxis(GamepadAxis::LeftX) == 0.0f);
    CHECK(Input::IsGamepadConnected());

    Input::SetGamepadBlocked(false);

    CHECK(Input::IsGamepadButtonDown(GamepadButton::Start));
    CHECK(Input::GetGamepadAxis(GamepadAxis::LeftX) == -1.0f);
}

TEST_CASE_METHOD(GamepadFixture, "Events for slots out of range are ignored", "[input][gamepad]")
{
    Connect(MaxGamepads);
    Press(GamepadButton::South, MaxGamepads);

    CHECK_FALSE(Input::IsGamepadConnected(MaxGamepads));
    CHECK_FALSE(Input::IsGamepadButtonDown(GamepadButton::South, MaxGamepads));
    CHECK(Input::GetGamepadName(MaxGamepads).empty());
}

TEST_CASE_METHOD(GamepadFixture, "Gamepad output is forwarded to the backend for connected gamepads", "[Input][Gamepad]")
{
    FakeGamepadBackend backend;
    const BackendGuard guard(backend);

    SECTION("Nothing is forwarded while the gamepad is disconnected")
    {
        CHECK_FALSE(Input::RumbleGamepad(0.5f, 1.0f, 0.2f));
        CHECK(Input::GetGamepadType() == GamepadType::Unknown);
        CHECK_FALSE(Input::GetGamepadFeatures().Rumble);
        CHECK(backend.LastGamepad == MaxGamepads);
    }

    SECTION("A connected gamepad reaches the backend")
    {
        Connect(1);

        CHECK(Input::RumbleGamepad(0.5f, 1.0f, 0.2f, 1));
        CHECK(backend.LastGamepad == 1);
        CHECK(backend.LastLow == 0.5f);
        CHECK(backend.LastHigh == 1.0f);
        CHECK(backend.LastDuration == 0.2f);

        CHECK(Input::GetGamepadType(1) == GamepadType::PlayStation5);
        CHECK(Input::GetGamepadFeatures(1).Rumble);
        CHECK_FALSE(Input::SetGamepadLed({ 1.0f, 0.0f, 0.0f }, 1));

        Disconnect(1);
    }
}

TEST_CASE_METHOD(GamepadFixture, "Gamepad output without a backend does nothing", "[Input][Gamepad]")
{
    Connect();

    CHECK_FALSE(Input::RumbleGamepad(1.0f, 1.0f, 1.0f));
    CHECK(Input::GetGamepadType() == GamepadType::Unknown);

    Disconnect();
}

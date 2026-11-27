#include "Scripting/ScriptTestHelpers.h"

#include <Engine/Event/KeyEvent.h>
#include <Engine/Event/GamepadEvent.h>
#include <Engine/Input/Input.h>
#include <Engine/Input/InputActionSystem.h>
#include <Engine/Input/GamepadFeatures.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Scripting/API/ScriptAPI.h>

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>

using namespace ByteForge;
using namespace ByteForge::Tests;

namespace
{
    bool CallBool(const ScriptCallContext& context, const std::string_view id, const std::string& argument)
    {
        return CallScriptAs<bool>(context, id, { Text(argument) });
    }

    class InputAPIFixture
    {
    public:
        InputAPIFixture()
        {
            Input::SetKeyboardBlocked(false);

            m_Actions.SetActions(MakeRef<InputActionsAsset>(UUID(1), MakeDefaultInputActions()));
            m_Scene.SetInputActions(&m_Actions);
        }

        ~InputAPIFixture()
        {
            m_Scene.SetInputActions(nullptr);

            Input::OnEvent(KeyReleasedEvent(KeyCode::Space));
            Input::EndFrame();
        }

        InputAPIFixture(const InputAPIFixture&) = delete;
        InputAPIFixture& operator=(const InputAPIFixture&) = delete;

    protected:
        void PressSpace()
        {
            Input::OnEvent(KeyPressedEvent(KeyCode::Space));
            m_Actions.Update(RawInputSource{});
        }

        [[nodiscard]] ScriptCallContext Context() { return ScriptCallContext{ .ActiveScene = &m_Scene }; }

        InputActionSystem m_Actions;
        Scene m_Scene;
    };
}

TEST_CASE_METHOD(InputAPIFixture, "Script functions read input actions through the scene", "[scripting][input]")
{
    const ScriptCallContext context = Context();

    CHECK(CallBool(context, "InputAction.Exists", "Gameplay/Jump"));
    CHECK_FALSE(CallBool(context, "InputAction.IsDown", "Gameplay/Jump"));

    PressSpace();

    CHECK(CallBool(context, "InputAction.IsDown", "Gameplay/Jump"));
    CHECK(CallBool(context, "InputAction.WasPressed", "Gameplay/Jump"));
    CHECK_FALSE(CallBool(context, "InputAction.WasReleased", "Gameplay/Jump"));

    CHECK(CallScriptAs<glm::vec2>(context, "InputAction.ReadValue", { Text("Gameplay/Jump") }) == glm::vec2(1.0f, 0.0f));
}

TEST_CASE_METHOD(InputAPIFixture, "Unknown input actions are reported", "[scripting][input]")
{
    const ScriptCallContext context = Context();

    CHECK_FALSE(CallBool(context, "InputAction.Exists", "Gameplay/Fly"));
    CHECK_THROWS_AS(CallScript(context, "InputAction.IsDown", { Text("Gameplay/Fly") }), ScriptError);
    CHECK_THROWS_AS(CallScript(context, "InputAction.IsDown", { Text("Jump") }), ScriptError);
}

TEST_CASE_METHOD(InputAPIFixture, "Action maps can be switched from scripts", "[scripting][input]")
{
    const ScriptCallContext context = Context();

    CallScript(context, "InputActions.DisableMap", { Text("Gameplay") });
    CHECK_FALSE(CallBool(context, "InputActions.IsMapEnabled", "Gameplay"));

    CallScript(context, "InputActions.EnableMap", { Text("Gameplay") });
    CHECK(CallBool(context, "InputActions.IsMapEnabled", "Gameplay"));

    CHECK_THROWS_AS(CallScript(context, "InputActions.DisableMap", { Text("UI") }), ScriptError);
}

TEST_CASE("Without input actions the script functions explain why", "[scripting][input]")
{
    Scene scene;
    const ScriptCallContext context{ .ActiveScene = &scene };

    CHECK_FALSE(CallBool(context, "InputAction.Exists", "Gameplay/Jump"));
    CHECK_THROWS_AS(CallScript(context, "InputAction.IsDown", { Text("Gameplay/Jump") }), ScriptError);
    CHECK_THROWS_AS(CallScript(context, "InputActions.EnableMap", { Text("Gameplay") }), ScriptError);
}

TEST_CASE("Gamepad script functions validate the slot", "[scripting][input][gamepad]")
{
    const ScriptCallContext context{};

    CHECK_THROWS_AS(CallScript(context, "Gamepad.IsConnected", { -1 }), ScriptError);
    CHECK_THROWS_AS(CallScript(context, "Gamepad.IsConnected", { static_cast<i32>(MaxGamepads) }), ScriptError);
    CHECK_THROWS_AS(CallScript(context, "Gamepad.Rumble", { 7, 1.0f, 1.0f, 0.5f }), ScriptError);

    CHECK_FALSE(CallScriptAs<bool>(context, "Gamepad.IsConnected", { static_cast<i32>(MaxGamepads) - 1 }));
}

TEST_CASE("Gamepad output from scripts does nothing without a connected gamepad", "[scripting][input][gamepad]")
{
    const ScriptCallContext context{};

    CHECK_FALSE(CallScriptAs<bool>(context, "Gamepad.Rumble", { 0, 1.0f, 1.0f, 0.5f }));
    CHECK_FALSE(CallScriptAs<bool>(context, "Gamepad.SetLed", { 0, glm::vec3(1.0f) }));
    CHECK_FALSE(CallScriptAs<bool>(context, "Gamepad.SupportsAdaptiveTriggers", { 0 }));
    CHECK(CallScriptAs<i32>(context, "Gamepad.GetType", { 0 }) == static_cast<i32>(GamepadType::Unknown));
}

TEST_CASE("Gamepad buttons and axes reach scripts", "[scripting][input][gamepad]")
{
    Input::SetGamepadBlocked(false);
    Input::OnEvent(GamepadConnectedEvent(0, "Test Gamepad"));
    Input::OnEvent(GamepadButtonPressedEvent(0, GamepadButton::South));
    Input::OnEvent(GamepadAxisMovedEvent(0, GamepadAxis::RightTrigger, 0.75f));

    const ScriptCallContext context{};
    const i32 south = static_cast<i32>(GamepadButton::South);

    CHECK(CallScriptAs<bool>(context, "Gamepad.IsConnected", { 0 }));
    CHECK(CallScriptAs<std::string>(context, "Gamepad.GetName", { 0 }) == "Test Gamepad");
    CHECK(CallScriptAs<bool>(context, "Gamepad.IsButtonDown", { 0, south }));
    CHECK(CallScriptAs<bool>(context, "Gamepad.IsButtonPressed", { 0, south }));
    CHECK(CallScriptAs<f32>(context, "Gamepad.GetAxis", { 0, static_cast<i32>(GamepadAxis::RightTrigger) }) == 0.75f);
    CHECK_THROWS_AS(CallScript(context, "Gamepad.IsButtonDown", { 0, 99 }), ScriptError);

    Input::OnEvent(GamepadDisconnectedEvent(0));
    Input::EndFrame();
}

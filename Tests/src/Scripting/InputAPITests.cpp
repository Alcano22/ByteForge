#include <Engine/Event/KeyEvent.h>
#include <Engine/Input/Input.h>
#include <Engine/Input/InputActionSystem.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Scripting/API/ScriptAPI.h>

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace ByteForge;

namespace
{
    ScriptValue Text(std::string text)
    {
        return ScriptValue(std::in_place_type<std::string>, std::move(text));
    }

    std::optional<ScriptValue> Call(const ScriptCallContext& context, const std::string_view id,
                                    const std::vector<ScriptValue>& arguments)
    {
        const ScriptFunction* function = ScriptAPI::Get().Find(id);
        REQUIRE(function != nullptr);

        ValidateArguments(*function, arguments);
        return function->Invoke(context, arguments);
    }

    bool CallBool(const ScriptCallContext& context, const std::string_view id, const std::string& argument)
    {
        const std::optional<ScriptValue> result = Call(context, id, { Text(argument) });
        REQUIRE(result.has_value());
        return std::get<bool>(*result);
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

    const std::optional<ScriptValue> value = Call(context, "InputAction.ReadValue", { Text("Gameplay/Jump") });
    REQUIRE(value.has_value());
    CHECK(std::get<glm::vec2>(*value) == glm::vec2(1.0f, 0.0f));
}

TEST_CASE_METHOD(InputAPIFixture, "Unknown input actions are reported", "[scripting][input]")
{
    const ScriptCallContext context = Context();

    CHECK_FALSE(CallBool(context, "InputAction.Exists", "Gameplay/Fly"));
    CHECK_THROWS_AS(Call(context, "InputAction.IsDown", { Text("Gameplay/Fly") }), ScriptError);
    CHECK_THROWS_AS(Call(context, "InputAction.IsDown", { Text("Jump") }), ScriptError);
}

TEST_CASE_METHOD(InputAPIFixture, "Action maps can be switched from scripts", "[scripting][input]")
{
    const ScriptCallContext context = Context();

    Call(context, "InputActions.DisableMap", { Text("Gameplay") });
    CHECK_FALSE(CallBool(context, "InputActions.IsMapEnabled", "Gameplay"));

    Call(context, "InputActions.EnableMap", { Text("Gameplay") });
    CHECK(CallBool(context, "InputActions.IsMapEnabled", "Gameplay"));

    CHECK_THROWS_AS(Call(context, "InputActions.DisableMap", { Text("UI") }), ScriptError);
}

TEST_CASE("Without input actions the script functions explain why", "[scripting][input]")
{
    Scene scene;
    const ScriptCallContext context{ .ActiveScene = &scene };

    CHECK_FALSE(CallBool(context, "InputAction.Exists", "Gameplay/Jump"));
    CHECK_THROWS_AS(Call(context, "InputAction.IsDown", { Text("Gameplay/Jump") }), ScriptError);
    CHECK_THROWS_AS(Call(context, "InputActions.EnableMap", { Text("Gameplay") }), ScriptError);
}

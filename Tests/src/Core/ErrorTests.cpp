#include <Engine/Core/Error.h>

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <string_view>
#include <type_traits>

using namespace ByteForge;

TEST_CASE("Error formats its message like std::format", "[core][error]")
{
    const Error error("cannot open '{}': code {}", "level.scene", 42);
    CHECK(std::string_view(error.what()) == "cannot open 'level.scene': code 42");
}

TEST_CASE("Error takes a message without arguments as is", "[core][error]")
{
    const Error error("expected {x, y}");
    CHECK(std::string_view(error.what()) == "expected {x, y}");
}

TEST_CASE("Error and LogicError derive from the matching standard exceptions", "[core][error]")
{
    STATIC_CHECK(std::is_base_of_v<std::runtime_error, Error>);
    STATIC_CHECK(std::is_base_of_v<std::logic_error, LogicError>);

    CHECK_THROWS_AS(throw Error("failure"), std::runtime_error);
    CHECK_THROWS_AS(throw LogicError("bug {}", 1), std::logic_error);
}

TEST_CASE("Unexpected makes a failed Result with a formatted message", "[core][error]")
{
    const Result<int> result = Unexpected("value {} is out of range", 7);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == "value 7 is out of range");
}

TEST_CASE("Unexpected takes a message without arguments as is", "[core][error]")
{
    const Result<> result = Unexpected("expected {x, y}");

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == "expected {x, y}");
}

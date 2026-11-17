#include <Engine/Scene/UUID.h>

#include <catch2/catch_test_macros.hpp>

#include <functional>

using namespace ByteForge;

TEST_CASE("A default-constructed UUID is random and set", "[scene][uuid]")
{
    const UUID first;
    const UUID second;

    CHECK(first.IsSet());
    CHECK(second.IsSet());
    CHECK(first != second);
}

TEST_CASE("A UUID keeps the value it was created with", "[scene][uuid]")
{
    const UUID uuid(0x1234'5678'9ABC'DEF0);

    CHECK(static_cast<u64>(uuid) == 0x1234'5678'9ABC'DEF0);
    CHECK(uuid == UUID(0x1234'5678'9ABC'DEF0));
}

TEST_CASE("UUID 0 means 'not set'", "[scene][uuid]")
{
    CHECK_FALSE(UUID(0).IsSet());
}

TEST_CASE("A UUID hashes like its value", "[scene][uuid]")
{
    CHECK(std::hash<UUID>{}(UUID(42)) == std::hash<u64>{}(42));
}

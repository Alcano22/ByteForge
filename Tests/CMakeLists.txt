find_package(Catch2 3 CONFIG REQUIRED)

add_executable(ByteForgeTests
        src/Core/ErrorTests.cpp
        src/Core/Utf8Tests.cpp
        src/Scene/UUIDTests.cpp
)

target_link_libraries(ByteForgeTests PRIVATE
        Engine
        Catch2::Catch2WithMain
        ByteForgeWarnings
)

set_target_properties(ByteForgeTests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BYTEFORGE_TESTS_DIR})

byteforge_deploy_runtime(ByteForgeTests)

include(Catch)
catch_discover_tests(ByteForgeTests DISCOVERY_MODE PRE_TEST)

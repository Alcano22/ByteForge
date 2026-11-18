find_package(Catch2 3 CONFIG REQUIRED)

add_executable(ByteForgeTests
        src/Main.cpp
        src/TestUtilities.cpp
        src/Core/ErrorTests.cpp
        src/Core/SemanticVersionTests.cpp
        src/Core/Utf8Tests.cpp
        src/Scene/UUIDTests.cpp
        src/Scene/SceneTests.cpp
        src/Assets/PakTests.cpp
        src/Assets/PrefabTests.cpp
        src/Project/ProjectTests.cpp
        src/Project/RecentProjectsTests.cpp
        src/Hub/EditorInstallsTests.cpp
)

target_include_directories(ByteForgeTests PRIVATE src)

target_link_libraries(ByteForgeTests PRIVATE
        Engine
        HubCore
        Catch2::Catch2
        ByteForgeWarnings
)

set_target_properties(ByteForgeTests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BYTEFORGE_TESTS_DIR})

byteforge_deploy_runtime(ByteForgeTests)

include(Catch)
catch_discover_tests(ByteForgeTests DISCOVERY_MODE PRE_TEST)

find_package(Catch2 3 CONFIG REQUIRED)
find_package(LibArchive REQUIRED)

add_executable(ByteForgeTests
        src/Main.cpp
        src/TestUtilities.cpp
        src/Core/ErrorTests.cpp
        src/Core/SemanticVersionTests.cpp
        src/Core/Utf8Tests.cpp
        src/Scene/UUIDTests.cpp
        src/Scene/SceneTests.cpp
        src/Input/InputGamepadTests.cpp
        src/UI/GameUITests.cpp
        src/Text/FontAtlasTests.cpp
        src/Text/TextLayoutTests.cpp
        src/Text/TextLayoutCacheTests.cpp
        src/Text/RichTextTests.cpp
        src/Assets/PakTests.cpp
        src/Assets/PrefabTests.cpp
        src/Project/ProjectTests.cpp
        src/Project/RecentProjectsTests.cpp
        src/Hub/TestArchives.cpp
        src/Hub/ArchiveTests.cpp
        src/Hub/EditorChoiceTests.cpp
        src/Hub/EditorInstallsTests.cpp
        src/Hub/EditorManifestTests.cpp
        src/Hub/EditorReleasesTests.cpp
)

target_include_directories(ByteForgeTests PRIVATE src)

target_compile_definitions(ByteForgeTests PRIVATE
        BYTEFORGE_TEST_FONT="${CMAKE_SOURCE_DIR}/Engine/resources/fonts/inter_regular.ttf"
)

target_link_libraries(ByteForgeTests PRIVATE
        Engine
        HubCore
        LibArchive::LibArchive
        Catch2::Catch2
        ByteForgeWarnings
)

set_target_properties(ByteForgeTests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BYTEFORGE_TESTS_DIR})

byteforge_deploy_runtime(ByteForgeTests)

include(Catch)
catch_discover_tests(ByteForgeTests DISCOVERY_MODE PRE_TEST)

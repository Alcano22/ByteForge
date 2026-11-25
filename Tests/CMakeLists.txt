find_package(Catch2 3 CONFIG REQUIRED)
find_package(LibArchive REQUIRED)

add_executable(ByteForgeTests
        src/Main.cpp
        src/TestUtilities.cpp
        src/Core/ErrorTests.cpp
        src/Core/SemanticVersionTests.cpp
        src/Core/Utf8Tests.cpp
        src/Core/RandomTests.cpp
        src/Core/DotNetLocatorTests.cpp
        src/Core/StringUtilsTests.cpp
        src/Scene/UUIDTests.cpp
        src/Scene/SceneTests.cpp
        src/Scene/SpriteRendererTests.cpp
        src/Scene/Light2DTests.cpp
        src/Renderer/Light2DPreparationTests.cpp
        src/Scripting/InputAPITests.cpp
        src/Scripting/MathAPITests.cpp
        src/Scripting/LogicAPITests.cpp
        src/Scripting/VectorAPITests.cpp
        src/Scripting/StringAPITests.cpp
        src/Scripting/TimeAPITests.cpp
        src/Scripting/RandomAPITests.cpp
        src/Scripting/ScriptAPICompatibilityTests.cpp
        src/Scripting/ScriptGraphTests.cpp
        src/Scripting/ScriptGraphFlowTests.cpp
        src/Scripting/ScriptGraphVariableTests.cpp
        src/Scripting/ScriptGraphDebugTests.cpp
        src/Scripting/ScriptGraphClipboardTests.cpp
        src/Scripting/ScriptEnumTests.cpp
        src/Input/InputGamepadTests.cpp
        src/Input/InputControlTests.cpp
        src/Input/InputActionSystemTests.cpp
        src/UI/GameUITests.cpp
        src/Text/FontAtlasTests.cpp
        src/Text/TextLayoutTests.cpp
        src/Text/TextLayoutCacheTests.cpp
        src/Text/RichTextTests.cpp
        src/Assets/PakTests.cpp
        src/Assets/PrefabTests.cpp
        src/Assets/InputActionsTests.cpp
        src/Assets/MaterialAssetTests.cpp
        src/Project/ProjectTests.cpp
        src/Project/ProjectSettingsTests.cpp
        src/Project/RecentProjectsTests.cpp
        src/Hub/TestArchives.cpp
        src/Hub/ArchiveTests.cpp
        src/Hub/EditorChoiceTests.cpp
        src/Hub/EditorInstallsTests.cpp
        src/Hub/EditorManifestTests.cpp
        src/Hub/EditorReleasesTests.cpp
        src/Hub/DotNetPackageTests.cpp
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

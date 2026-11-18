cmake_minimum_required(VERSION 4.1)

project(ByteForge VERSION 0.3.0 LANGUAGES CXX)

set(BYTEFORGE_VERSION_STRING ${PROJECT_VERSION}-dev)
if(DEFINED ENV{BYTEFORGE_RELEASE_VERSION})
    set(release_version "$ENV{BYTEFORGE_RELEASE_VERSION}")
    string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" release_number "${release_version}")

    if(NOT release_number STREQUAL PROJECT_VERSION)
        message(FATAL_ERROR "The release is tagged ${release_version}, but CMakeLists.txt declares version "
                            "${PROJECT_VERSION}. Update project(ByteForge VERSION ...) before tagging.")
    endif()

    set(BYTEFORGE_VERSION_STRING ${release_version})
endif()
message(STATUS "ByteForge ${BYTEFORGE_VERSION_STRING}")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(ByteForgeWarnings INTERFACE)
target_compile_options(ByteForgeWarnings INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-missing-field-initializers>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
target_compile_definitions(ByteForgeWarnings INTERFACE GLM_ENABLE_EXPERIMENTAL)

option(BYTEFORGE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(BYTEFORGE_BUILD_HUB "Build ByteForge Hub (needs Qt 6.5 or newer)" ON)
option(BYTEFORGE_BUILD_TESTS "Build the unit tests" ON)
if(BYTEFORGE_WARNINGS_AS_ERRORS)
    target_compile_options(ByteForgeWarnings INTERFACE
            $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Werror>
            $<$<CXX_COMPILER_ID:MSVC>:/WX>
    )
endif()

set(BYTEFORGE_STAGE_DIR ${CMAKE_BINARY_DIR}/stage)
set(BYTEFORGE_BIN_DIR ${BYTEFORGE_STAGE_DIR}/bin)
set(BYTEFORGE_LIB_DIR ${BYTEFORGE_STAGE_DIR}/lib)
set(BYTEFORGE_SHARE_DIR ${BYTEFORGE_STAGE_DIR}/share)
set(BYTEFORGE_PLAYER_DIR ${BYTEFORGE_STAGE_DIR}/player)
set(BYTEFORGE_TESTS_DIR ${BYTEFORGE_STAGE_DIR}/tests)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BYTEFORGE_BIN_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${BYTEFORGE_LIB_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${BYTEFORGE_LIB_DIR})

set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")

include(cmake/CopyDirectory.cmake)
include(cmake/RuntimeDeploy.cmake)

add_subdirectory(Engine)
add_subdirectory(ScriptCore)
add_subdirectory(Editor)
add_subdirectory(Player)
add_subdirectory(Hub/Core)

if(BYTEFORGE_BUILD_HUB)
    add_subdirectory(Hub)
endif()

if(BYTEFORGE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()

include(cmake/Install.cmake)

cmake_minimum_required(VERSION 4.1)
project(ByteForge VERSION 0.1.0 LANGUAGES CXX)

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

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BYTEFORGE_BIN_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${BYTEFORGE_LIB_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${BYTEFORGE_LIB_DIR})

include(cmake/CopyDirectory.cmake)
include(cmake/RuntimeDeploy.cmake)

add_subdirectory(Engine)
add_subdirectory(ScriptCore)
add_subdirectory(Editor)
add_subdirectory(Player)

if(BYTEFORGE_BUILD_HUB)
    add_subdirectory(Hub)
endif()

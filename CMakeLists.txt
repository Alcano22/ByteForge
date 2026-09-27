cmake_minimum_required(VERSION 4.1)
project(ByteForge LANGUAGES CXX)

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
if (BYTEFORGE_WARNINGS_AS_ERRORS)
    target_compile_options(ByteForgeWarnings INTERFACE
            $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Werror>
            $<$<CXX_COMPILER_ID:MSVC>:/WX>
    )
endif()

add_subdirectory(Engine)
add_subdirectory(Editor)
add_subdirectory(Sandbox)

cmake_minimum_required(VERSION 4.1)
project(ByteForge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(Engine)
add_subdirectory(Editor)
add_subdirectory(Sandbox)

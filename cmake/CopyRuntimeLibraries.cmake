# Copies the shared libraries a built game needs next to its executable:
# the engine, its core and the shader compiler (DXC), which the engine loads at runtime.
# Usage: cmake -DSOURCE_DIR=<dir> -DDESTINATION=<dir> -P CopyRuntimeLibraries.cmake

file(GLOB libraries
        "${SOURCE_DIR}/libEngine*"
        "${SOURCE_DIR}/libByteForgeCore*"
        "${SOURCE_DIR}/libdxcompiler*"
        "${SOURCE_DIR}/libdxil*"
        "${SOURCE_DIR}/Engine.dll"
        "${SOURCE_DIR}/ByteForgeCore.dll"
        "${SOURCE_DIR}/dxcompiler.dll"
        "${SOURCE_DIR}/dxil.dll")

if(NOT libraries)
    message(WARNING "CopyRuntimeLibraries: no libraries found in '${SOURCE_DIR}'")
    return()
endif()

file(MAKE_DIRECTORY "${DESTINATION}")
file(COPY ${libraries} DESTINATION "${DESTINATION}" FOLLOW_SYMLINK_CHAIN)

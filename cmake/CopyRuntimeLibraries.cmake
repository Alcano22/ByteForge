# Copies the shared libraries a built game needs next to its executable
# Usage: cmake -DSOURCE_DIR=<dir> -DDESTINATION=<dir> -P CopyRuntimeLibraries.cmake
file (GLOB libraries
        "${SOURCE_DIR}/libEngine*"
        "${SOURCE_DIR}/libdxcompiler*"
        "${SOURCE_DIR}/libdxil*")

if(NOT libraries)
    message(WARNING "CopyRuntimeLibraries: no libraries found in '${SOURCE_DIR}'")
    return()
endif()

file(MAKE_DIRECTORY "${DESTINATION}")
file(COPY ${libraries} DESTINATION "${DESTINATION}" FOLLOW_SYMLINK_CHAIN)

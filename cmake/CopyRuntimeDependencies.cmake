# Resolves every DLL an executable needs, transitively, and copies them next to it.
# Reads the import tables with dumpbin, so indirect dependencies (Engine.dll -> freetype.dll -> libpng16.dll)
# are found too; $<TARGET_RUNTIME_DLLS> only knows the DLLs of targets that are linked directly.
#
# Arguments: EXECUTABLE, DESTINATION, SEARCH_DIR_ENGINE, SEARCH_DIR_PACKAGES

foreach(argument EXECUTABLE DESTINATION SEARCH_DIR_ENGINE SEARCH_DIR_PACKAGES)
    if(NOT DEFINED ${argument})
        message(FATAL_ERROR "CopyRuntimeDependencies.cmake needs ${argument}")
    endif()
endforeach()

set(CMAKE_GET_RUNTIME_DEPENDENCIES_PLATFORM "windows+pe")
set(CMAKE_GET_RUNTIME_DEPENDENCIES_TOOL "dumpbin")

file(GET_RUNTIME_DEPENDENCIES
        EXECUTABLES "${EXECUTABLE}"
        RESOLVED_DEPENDENCIES_VAR resolved
        UNRESOLVED_DEPENDENCIES_VAR unresolved
        DIRECTORIES "${SEARCH_DIR_ENGINE}" "${SEARCH_DIR_PACKAGES}"
        PRE_EXCLUDE_REGEXES "^api-ms-.*" "^ext-ms-.*"
        POST_EXCLUDE_REGEXES "^[A-Za-z]:[/\\\\][Ww][Ii][Nn][Dd][Oo][Ww][Ss][/\\\\]"
)

foreach(dll IN LISTS resolved)
    file(COPY "${dll}" DESTINATION "${DESTINATION}")
endforeach()

foreach(dll IN LISTS unresolved)
    message(STATUS "Not bundled (expected on the target system): ${dll}")
endforeach()

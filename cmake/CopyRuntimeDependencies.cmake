# Resolves every DLL an executable needs, transitively, and copies them next to it.
# Reads the import tables with dumpbin, so indirect dependencies (Engine.dll -> freetype.dll -> libpng16.dll)
# are found too; $<TARGET_RUNTIME_DLLS> only knows the DLLs of targets that are linked directly.
#
# Arguments: EXECUTABLE, DESTINATION, SEARCH_DIR_ENGINE, SEARCH_DIR_PACKAGES

if(POLICY CMP0207)
    cmake_policy(SET CMP0207 NEW)
endif()

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
        CONFLICTING_DEPENDENCIES_PREFIX conflicting
        DIRECTORIES "${SEARCH_DIR_ENGINE}" "${SEARCH_DIR_PACKAGES}"
        PRE_EXCLUDE_REGEXES "^api-ms-.*" "^ext-ms-.*"
        POST_EXCLUDE_REGEXES "^[A-Za-z]:[/\\\\][Ww][Ii][Nn][Dd][Oo][Ww][Ss][/\\\\]"
)

cmake_path(NORMAL_PATH SEARCH_DIR_PACKAGES OUTPUT_VARIABLE packages_dir)

foreach(name IN LISTS conflicting_FILENAMES)
    set(chosen "")
    foreach(candidate IN LISTS conflicting_${name})
        cmake_path(GET candidate PARENT_PATH candidate_dir)
        cmake_path(NORMAL_PATH candidate_dir)
        if(candidate_dir STREQUAL packages_dir)
            set(chosen "${candidate}")
            break()
        endif()
    endforeach()

    if(NOT chosen)
        list(GET conflicting_${name} 0 chosen)
    endif()

    list(APPEND resolved "${chosen}")
endforeach()

foreach(dll IN LISTS resolved)
    cmake_path(GET dll PARENT_PATH dll_dir)
    cmake_path(COMPARE "${dll_dir}" EQUAL "${DESTINATION}" already_there)
    if(NOT already_there)
        file(COPY "${dll}" DESTINATION "${DESTINATION}")
    endif()
endforeach()

foreach(dll IN LISTS unresolved)
    message(STATUS "Not bundled (expected on the target system): ${dll}")
endforeach()

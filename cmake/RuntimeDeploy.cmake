# Windows has no rpath: an executable only finds the DLLs next to it.
# Copies Engine.dll, the vcpkg DLLs the target needs (directly and indirectly) and the shader compiler
# (DXC, which the engine loads at runtime) next to the executable. Does nothing on Linux and macOS,
# where the rpath handles this.

function(byteforge_deploy_runtime target)
    if(NOT WIN32)
        return()
    endif()

    set(packages_bin "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin")
    set(packages_debug_bin "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/bin")

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:${target}>
            $<TARGET_FILE_DIR:${target}>
        COMMAND ${CMAKE_COMMAND}
            -DSOURCE_DIR=$<TARGET_FILE_DIR:Engine>
            -DDESTINATION=$<TARGET_FILE_DIR:${target}>
            -P ${CMAKE_SOURCE_DIR}/cmake/CopyRuntimeLibraries.cmake
        COMMAND ${CMAKE_COMMAND}
            -DEXECUTABLE=$<TARGET_FILE:${target}>
            -DDESTINATION=$<TARGET_FILE_DIR:${target}>
            -DSEARCH_DIR_ENGINE=$<TARGET_FILE_DIR:Engine>
            -DSEARCH_DIR_PACKAGES=$<IF:$<CONFIG:Debug>,${packages_debug_bin},${packages_bin}>
            -P ${CMAKE_SOURCE_DIR}/cmake/CopyRuntimeDependencies.cmake
        COMMAND_EXPAND_LISTS
        COMMENT "Copying the runtime DLLs next to ${target}"
        VERBATIM
    )
endfunction()

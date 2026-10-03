function(byteforge_copy_directory target source destination)
    get_filename_component(name "${source}" NAME)
    set(stamp "${CMAKE_CURRENT_BINARY_DIR}/${target}_${name}.stamp")

    file(GLOB_RECURSE entries LIST_DIRECTORIES true CONFIGURE_DEPENDS "${source}/*")

    add_custom_command(
            OUTPUT "${stamp}"
            COMMAND ${CMAKE_COMMAND} -E remove_directory "${destination}"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${source}" "${destination}"
            COMMAND ${CMAKE_COMMAND} -E touch "${stamp}"
            DEPENDS "${source}" ${entries}
            COMMENT "Copying ${name} for ${target}"
            VERBATIM
    )

    add_custom_target(${target}_${name} DEPENDS "${stamp}")
    add_dependencies(${target} ${target}_${name})
endfunction()

function(byteforge_copy_directory target source destination)
    cmake_parse_arguments(PARSE_ARGV 3 ARG "" "" "DEPENDS")

    get_filename_component(name "${destination}" NAME)
    set(stamp "${CMAKE_CURRENT_BINARY_DIR}/${target}_${name}.stamp")

    if (ARG_DEPENDS)
        set(dependencies ${ARG_DEPENDS})
    else()
        file(GLOB_RECURSE dependencies LIST_DIRECTORIES true CONFIGURE_DEPENDS "${source}/*")
        list(APPEND dependencies "${source}")
    endif()

    add_custom_command(
            OUTPUT "${stamp}"
            COMMAND ${CMAKE_COMMAND} -E remove_directory "${destination}"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${source}" "${destination}"
            COMMAND ${CMAKE_COMMAND} -E touch "${stamp}"
            DEPENDS ${dependencies}
            COMMENT "Copying ${name} for ${target}"
            VERBATIM
    )

    add_custom_target(${target}_${name} DEPENDS "${stamp}")
    add_dependencies(${target} ${target}_${name})
endfunction()

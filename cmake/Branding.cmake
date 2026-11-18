set(BYTEFORGE_BRANDING_DIR ${CMAKE_SOURCE_DIR}/Branding)

set(BYTEFORGE_ICON_SIZES 16 24 32 48 128 256)

function(byteforge_add_app_icon target)
    if(NOT WIN32)
        return()
    endif()

    set(BYTEFORGE_ICON_FILE "${BYTEFORGE_BRANDING_DIR}/byteforge.ico")
    set(resource "${CMAKE_CURRENT_BINARY_DIR}/${target}Icon.rc")

    configure_file("${BYTEFORGE_BRANDING_DIR}/byteforge.rc.in" "${resource}" @ONLY)
    target_sources(${target} PRIVATE "${resource}")
endfunction()

function(byteforge_copy_window_icons target)
    byteforge_copy_directory(${target}
            ${BYTEFORGE_BRANDING_DIR}/icons
            ${BYTEFORGE_SHARE_DIR}/icons
    )
endfunction()

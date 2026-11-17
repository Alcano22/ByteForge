# The release: cmake --install <build folder>   (into the preset's installDir, or --prefix <folder>)
#
# The stage folder is already laid out like an installation, and the post-build steps put exactly the libraries
# each executable needs into it. Installing copies its folders and leaves out what only matters while developing.

set(BYTEFORGE_INSTALL_EXCLUDES
        PATTERN "*.pdb" EXCLUDE
        PATTERN "*.ilk" EXCLUDE
        PATTERN "*.exp" EXCLUDE
        PATTERN "*.lib" EXCLUDE
        PATTERN "*.a"   EXCLUDE
)

install(DIRECTORY ${BYTEFORGE_BIN_DIR}/
        DESTINATION bin
        USE_SOURCE_PERMISSIONS
        ${BYTEFORGE_INSTALL_EXCLUDES}
)

if(WIN32)
    if(BYTEFORGE_BUILD_HUB)
        install(DIRECTORY ${BYTEFORGE_LIB_DIR}/qt/
                DESTINATION lib/qt
                ${BYTEFORGE_INSTALL_EXCLUDES}
        )
    endif()
else()
    install(DIRECTORY ${BYTEFORGE_LIB_DIR}/
            DESTINATION lib
            USE_SOURCE_PERMISSIONS
            ${BYTEFORGE_INSTALL_EXCLUDES}
    )
endif()

install(DIRECTORY ${BYTEFORGE_SHARE_DIR}/
        DESTINATION share
        ${BYTEFORGE_INSTALL_EXCLUDES}
)

install(DIRECTORY ${BYTEFORGE_PLAYER_DIR}/
        DESTINATION player
        USE_SOURCE_PERMISSIONS
        ${BYTEFORGE_INSTALL_EXCLUDES}
        PATTERN "ScriptCore" EXCLUDE
        PATTERN "shaders" EXCLUDE
)

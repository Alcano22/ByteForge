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

set(BYTEFORGE_HUB_FILES
        PATTERN "ByteForgeHub*"
        PATTERN "Qt6*"
        PATTERN "qt.conf"
)

set(BYTEFORGE_HUB_EXCLUDES
        PATTERN "ByteForgeHub*" EXCLUDE
        PATTERN "Qt6*" EXCLUDE
        PATTERN "qt.conf" EXCLUDE
)

install(DIRECTORY ${BYTEFORGE_BIN_DIR}/
        DESTINATION bin
        COMPONENT Editor
        USE_SOURCE_PERMISSIONS
        ${BYTEFORGE_INSTALL_EXCLUDES}
        ${BYTEFORGE_HUB_EXCLUDES}
)

if(BYTEFORGE_BUILD_HUB)
    install(DIRECTORY ${BYTEFORGE_BIN_DIR}/
            DESTINATION bin
            COMPONENT Hub
            USE_SOURCE_PERMISSIONS
            FILES_MATCHING
            ${BYTEFORGE_HUB_FILES}
            ${BYTEFORGE_INSTALL_EXCLUDES}
    )
endif()

if(WIN32)
    if(BYTEFORGE_BUILD_HUB)
        install(DIRECTORY ${BYTEFORGE_LIB_DIR}/qt/
                DESTINATION lib/qt
                COMPONENT Hub
                ${BYTEFORGE_INSTALL_EXCLUDES}
        )
    endif()
else()
    install(DIRECTORY ${BYTEFORGE_LIB_DIR}/
            DESTINATION lib
            COMPONENT Editor
            USE_SOURCE_PERMISSIONS
            ${BYTEFORGE_INSTALL_EXCLUDES}
    )
endif()

install(DIRECTORY ${BYTEFORGE_SHARE_DIR}/
        DESTINATION share
        COMPONENT Editor
        ${BYTEFORGE_INSTALL_EXCLUDES}
)

install(DIRECTORY ${BYTEFORGE_PLAYER_DIR}/
        DESTINATION player
        COMPONENT Editor
        USE_SOURCE_PERMISSIONS
        ${BYTEFORGE_INSTALL_EXCLUDES}
        PATTERN "ScriptCore" EXCLUDE
        PATTERN "shaders" EXCLUDE
)

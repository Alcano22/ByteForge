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

if(BYTEFORGE_BUILD_HUB)
    if(WIN32)
        install(TARGETS Hub
                RUNTIME_DEPENDENCY_SET ByteForgeHubLibraries
                RUNTIME DESTINATION bin COMPONENT Hub
        )

        install(RUNTIME_DEPENDENCY_SET ByteForgeHubLibraries
                DESTINATION bin
                COMPONENT Hub
                DIRECTORIES ${BYTEFORGE_BIN_DIR}
                PRE_EXCLUDE_REGEXES "^api-ms-.*" "^ext-ms-.*"
                POST_EXCLUDE_REGEXES "^[A-Za-z]:[/\\\\][Ww][Ii][Nn][Dd][Oo][Ww][Ss][/\\\\]"
        )

        install(DIRECTORY ${BYTEFORGE_BIN_DIR}/
                DESTINATION bin
                COMPONENT Hub
                FILES_MATCHING
                PATTERN "Qt6*.dll"
                PATTERN "qt.conf"
        )
    else()
        install(TARGETS Hub
                RUNTIME DESTINATION bin COMPONENT Hub
        )

        install(DIRECTORY ${BYTEFORGE_LIB_DIR}/
                DESTINATION lib
                COMPONENT Hub
                USE_SOURCE_PERMISSIONS
                FILES_MATCHING
                PATTERN "libByteForgeCore.so*"
        )
    endif()
endif()
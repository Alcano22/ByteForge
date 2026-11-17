add_executable(Player
        src/PlayerApp.cpp
)

target_link_libraries(Player PRIVATE Engine ByteForgeWarnings)

target_precompile_headers(Player PRIVATE ${BYTEFORGE_PCH_HEADER})

set_target_properties(Player PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BYTEFORGE_PLAYER_DIR})

byteforge_copy_directory(Player
        ${BYTEFORGE_SHARE_DIR}/ScriptCore
        ${BYTEFORGE_PLAYER_DIR}/ScriptCore
        DEPENDS ScriptCore ${BYTEFORGE_SHARE_DIR}/ScriptCore/ScriptCore.dll
)

byteforge_add_engine_shaders(Player ${BYTEFORGE_PLAYER_DIR})

if(UNIX AND NOT APPLE)
    set_target_properties(Player PROPERTIES BUILD_RPATH "$ORIGIN")
endif()

if(NOT WIN32)
    add_custom_command(TARGET Player POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DSOURCE_DIR=$<TARGET_FILE_DIR:Engine>
            -DDESTINATION=$<TARGET_FILE_DIR:Player>
            -P ${CMAKE_SOURCE_DIR}/cmake/CopyRuntimeLibraries.cmake
        COMMENT "Copying the engine next to the player"
        VERBATIM
    )
endif()

byteforge_deploy_runtime(Player)

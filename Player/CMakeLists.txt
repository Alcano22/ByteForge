add_executable(Player
        src/PlayerApp.cpp
)

target_link_libraries(Player PRIVATE Engine ByteForgeWarnings)

byteforge_copy_directory(Player
        ${CMAKE_BINARY_DIR}/ScriptCore
        ${CMAKE_CURRENT_BINARY_DIR}/ScriptCore
        DEPENDS ScriptCore ${CMAKE_BINARY_DIR}/ScriptCore/ScriptCore.dll
)

byteforge_add_engine_resources(Player)

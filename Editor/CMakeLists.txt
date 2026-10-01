add_executable(Editor
        src/EditorApp.cpp
        src/Editor/EditorContext.cpp
        src/Editor/EditorWidgets.cpp
        src/Editor/EditorIcons.cpp
        src/Editor/EditorFonts.cpp
        src/Editor/ScriptProject.cpp
        src/Editor/Panels/InspectorPanel.cpp
        src/Editor/Panels/SceneHierarchyPanel.cpp
        src/Editor/Panels/ViewportPanel.cpp
        src/Editor/Panels/AssetsPanel.cpp
        src/Editor/Panels/ConsolePanel.cpp
        src/Editor/Commands/CommandHistory.cpp
        src/Editor/Commands/EntityCommands.cpp
        src/Editor/Commands/EntityEditTracker.cpp
)

target_include_directories(Editor PRIVATE src)

target_link_libraries(Editor PRIVATE Engine ByteForgeWarnings)

add_custom_command(TARGET Editor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
                ${CMAKE_CURRENT_SOURCE_DIR}/resources
                $<TARGET_FILE_DIR:Editor>/resources
        COMMENT "Copying editor resources"
)

add_dependencies(Editor ScriptCore)

add_custom_command(TARGET Editor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
                ${CMAKE_BINARY_DIR}/ScriptCore
                $<TARGET_FILE_DIR:Editor>/ScriptCore
        COMMENT "Copying ScriptCore"
)

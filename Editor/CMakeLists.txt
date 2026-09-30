add_executable(Editor
        src/EditorApp.cpp
        src/Editor/EditorContext.cpp
        src/Editor/EditorWidgets.cpp
        src/Editor/Panels/InspectorPanel.cpp
        src/Editor/Panels/SceneHierarchyPanel.cpp
        src/Editor/Panels/ViewportPanel.cpp
        src/Editor/Panels/AssetsPanel.cpp
)

target_include_directories(Editor PRIVATE src)

target_link_libraries(Editor PRIVATE Engine ByteForgeWarnings)

add_custom_command(TARGET Editor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
                ${CMAKE_CURRENT_SOURCE_DIR}/resources
                $<TARGET_FILE_DIR:Editor>/resources
        COMMENT "Copying editor resources"
)

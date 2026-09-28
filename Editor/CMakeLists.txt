add_executable(Editor
        src/EditorApp.cpp
        src/Editor/EditorContext.cpp
        src/Editor/Panels/InspectorPanel.cpp
        src/Editor/Panels/SceneHierarchyPanel.cpp
        src/Editor/Panels/ViewportPanel.cpp
        src/Editor/Panels/AssetsPanel.cpp
)

target_include_directories(Editor PRIVATE src)

target_link_libraries(Editor PRIVATE Engine ByteForgeWarnings)

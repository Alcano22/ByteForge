find_package(nfd CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)
find_package(lunasvg CONFIG REQUIRED)

set(NODE_EDITOR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui-node-editor)

add_library(imgui_node_editor STATIC
        ${NODE_EDITOR_DIR}/imgui_node_editor.cpp
        ${NODE_EDITOR_DIR}/imgui_node_editor_api.cpp
        ${NODE_EDITOR_DIR}/imgui_canvas.cpp
        ${NODE_EDITOR_DIR}/crude_json.cpp
)
target_include_directories(imgui_node_editor SYSTEM PUBLIC ${NODE_EDITOR_DIR})
target_link_libraries(imgui_node_editor PUBLIC imgui::imgui)
target_compile_options(imgui_node_editor PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wno-deprecated-declarations>
)

add_executable(Editor
        src/EditorApp.cpp
        src/Editor/EditorContext.cpp
        src/Editor/EditorWidgets.cpp
        src/Editor/EditorIcons.cpp
        src/Editor/EditorFonts.cpp
        src/Editor/ScriptProject.cpp
        src/Editor/EditorLayer.cpp
        src/Editor/EditorActions.cpp
        src/Editor/SceneDocument.cpp
        src/Editor/SceneDialogs.cpp
        src/Editor/FileDialogs.cpp
        src/Editor/SvgIconFont.cpp
        src/Editor/Shell/EditorShell.cpp
        src/Editor/Shell/MainMenuBar.cpp
        src/Editor/Shell/Toolbar.cpp
        src/Editor/Panels/InspectorPanel.cpp
        src/Editor/Panels/SceneHierarchyPanel.cpp
        src/Editor/Panels/AssetsPanel.cpp
        src/Editor/Panels/ConsolePanel.cpp
        src/Editor/Panels/GamePanel.cpp
        src/Editor/Panels/ScenePanel.cpp
        src/Editor/Panels/GraphEditorPanel.cpp
        src/Editor/Commands/CommandHistory.cpp
        src/Editor/Commands/EntityCommands.cpp
        src/Editor/Commands/EntityEditTracker.cpp
        src/Editor/Inspectors/ComponentInspectors.cpp
        src/Editor/Inspectors/ScriptInspector.cpp
        src/Editor/Inspectors/AssetInspector.cpp
        src/Editor/Inspectors/TextureSettingsEditor.cpp
        src/Editor/Inspectors/LogEntryInspector.cpp
        src/Editor/Viewport/ViewportCanvas.cpp
        src/Editor/Viewport/EditorCamera2D.cpp
        src/Editor/Viewport/SceneGizmos.cpp
        src/Editor/Viewport/TransformGizmo.cpp
        src/Editor/Graph/GraphTheme.cpp
        src/Editor/Graph/GraphIds.cpp
        src/Editor/Graph/GraphDocument.cpp
        src/Editor/Graph/GraphNodeRenderer.cpp
        src/Editor/Graph/GraphNodePalette.cpp
)

target_include_directories(Editor PRIVATE src)

target_link_libraries(Editor PRIVATE
        Engine
        ByteForgeWarnings
        nfd::nfd
        lunasvg::lunasvg
        imguizmo::imguizmo
        imgui::imgui
        imgui_node_editor
)

byteforge_copy_directory(Editor
        ${CMAKE_CURRENT_SOURCE_DIR}/resources
        ${CMAKE_CURRENT_BINARY_DIR}/resources
)

byteforge_copy_directory(Editor
        ${CMAKE_BINARY_DIR}/ScriptCore
        ${CMAKE_CURRENT_BINARY_DIR}/ScriptCore
        DEPENDS ScriptCore ${CMAKE_BINARY_DIR}/ScriptCore/ScriptCore.dll
)

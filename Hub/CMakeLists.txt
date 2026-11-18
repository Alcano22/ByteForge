find_package(Qt6 6.5 REQUIRED COMPONENTS Quick QuickControls2)

qt_standard_project_setup(REQUIRES 6.5)

if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

qt_add_executable(Hub
        src/main.cpp
)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(Hub
    URI ByteForge.Hub
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/ProjectsPage.qml
        qml/InstallsPage.qml
        qml/NewProjectDialog.qml
        qml/ConfirmDialog.qml
        qml/ProjectDelegate.qml
        qml/EditorDelegate.qml
        qml/Theme.qml
        qml/HubButton.qml
        qml/HubTextField.qml
        qml/HubMenuItem.qml
        qml/HubTab.qml
    SOURCES
        src/QtConversions.h
        src/ProjectListModel.h
        src/ProjectListModel.cpp
        src/EditorListModel.h
        src/EditorListModel.cpp
        src/HubController.h
        src/HubController.cpp
)

target_include_directories(Hub PRIVATE src)

target_compile_definitions(Hub PRIVATE QT_NO_KEYWORDS)

target_link_libraries(Hub PRIVATE
        Engine
        HubCore
        Qt6::Quick
        Qt6::QuickControls2
        ByteForgeWarnings
)

set_target_properties(Hub PROPERTIES
        OUTPUT_NAME ByteForgeHub
        WIN32_EXECUTABLE ON
)

add_dependencies(Hub Editor)

if(WIN32)
    set(BYTEFORGE_QT_DIR ${BYTEFORGE_LIB_DIR}/qt)

    add_custom_command(TARGET Hub POST_BUILD
        COMMAND Qt6::windeployqt
            --no-translations
            --no-compiler-runtime
            --no-opengl-sw
            --no-system-d3d-compiler
            --skip-plugin-types qmltooling,generic,networkinformation,tls,iconengines,imageformats
            --plugindir "${BYTEFORGE_QT_DIR}/plugins"
            --qml-deploy-dir "${BYTEFORGE_QT_DIR}/qml"
            --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml"
            "$<TARGET_FILE:Hub>"
        COMMAND ${CMAKE_COMMAND}
            -DBIN_DIR=${BYTEFORGE_BIN_DIR}
            -DQML_DIR=${BYTEFORGE_QT_DIR}/qml
            -P ${CMAKE_SOURCE_DIR}/cmake/PruneQtDeployment.cmake
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/deploy/qt.conf"
            "${BYTEFORGE_BIN_DIR}/qt.conf"
        COMMENT "Deploying Qt for ByteForge Hub"
        VERBATIM
    )
endif()

byteforge_deploy_runtime(Hub)

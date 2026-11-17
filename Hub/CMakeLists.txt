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
        qml/NewProjectDialog.qml
        qml/ProjectDelegate.qml
        qml/Theme.qml
        qml/HubButton.qml
        qml/HubTextField.qml
        qml/HubMenuItem.qml
    SOURCES
        src/QtConversions.h
        src/ProjectListModel.h
        src/ProjectListModel.cpp
        src/HubController.h
        src/HubController.cpp
)

target_include_directories(Hub PRIVATE src)

target_compile_definitions(Hub PRIVATE QT_NO_KEYWORDS)

target_link_libraries(Hub PRIVATE
        Engine
        Qt6::Quick
        Qt6::QuickControls2
        ByteForgeWarnings
)

set_target_properties(Hub PROPERTIES
        OUTPUT_NAME ByteForgeHub
        RUNTIME_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:Editor>"
        WIN32_EXECUTABLE ON
)

add_dependencies(Hub Editor)

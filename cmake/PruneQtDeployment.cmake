# Removes the Qt Quick Controls styles the hub does not use from what windeployqt deployed.
#
# windeployqt copies every style because Qt Quick Dialogs imports the generic QtQuick.Controls, which may load any
# of them at runtime. The hub fixes its style to Basic (QQuickStyle::setStyle in main.cpp), so the others are never
# loaded. Keep that call: without it Qt would look for the Windows style that this removes.
#
# Arguments: BIN_DIR (the Qt DLLs), QML_DIR (the deployed QML modules)

foreach(argument BIN_DIR QML_DIR)
    if(NOT DEFINED ${argument})
        message(FATAL_ERROR "PruneQtDeployment.cmake needs ${argument}")
    endif()
endforeach()

set(unused_styles Fusion FluentWinUI3 Imagine Material Universal Windows)

foreach(style IN LISTS unused_styles)
    file(REMOVE_RECURSE "${QML_DIR}/QtQuick/Controls/${style}")

    file(GLOB libraries "${BIN_DIR}/Qt6QuickControls2${style}*.dll")
    if(libraries)
        file(REMOVE ${libraries})
    endif()

    file(REMOVE_RECURSE "${QML_DIR}/QtQuick/Dialogs/quickimpl/qml/+${style}")
endforeach()

file(REMOVE_RECURSE "${QML_DIR}/QtQuick/NativeStyle")

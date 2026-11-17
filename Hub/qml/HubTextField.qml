import QtQuick
import QtQuick.Controls

import ByteForge.Hub

TextField {
    id: control

    implicitHeight: Theme.controlHeight
    leftPadding: 10
    rightPadding: 10
    font.pixelSize: 13
    verticalAlignment: TextInput.AlignVCenter

    color: control.enabled ? Theme.text : Theme.textDisabled
    placeholderTextColor: Theme.textDim
    selectionColor: Theme.accent
    selectedTextColor: "white"

    background: Rectangle {
        radius: Theme.radius
        color: Theme.background
        border.width: 1
        border.color: control.activeFocus ? Theme.accent : Theme.border
    }
}

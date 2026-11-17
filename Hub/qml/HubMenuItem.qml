import QtQuick
import QtQuick.Controls

import ByteForge.Hub

MenuItem {
    id: control

    implicitHeight: 30
    leftPadding: 12
    rightPadding: 12
    font.pixelSize: 13

    contentItem: Label {
        text: control.text
        font: control.font
        verticalAlignment: Text.AlignVCenter
        color: !control.enabled ? Theme.textDisabled
            : control.highlighted ? "white"
                : Theme.text
    }

    background: Rectangle {
        implicitWidth: 180
        radius: 4
        color: control.highlighted && control.enabled ? Theme.accent : "transparent"
    }
}

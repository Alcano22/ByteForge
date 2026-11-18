import QtQuick
import QtQuick.Controls

import ByteForge.Hub

CheckBox {
    id: control

    font.pixelSize: 13
    spacing: 8

    indicator: Rectangle {
        implicitWidth: 16
        implicitHeight: 16
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        radius: 3
        color: control.checked ? Theme.accent : Theme.background
        border.color: control.checked || control.visualFocus ? Theme.accent : Theme.border

        Label {
            anchors.centerIn: parent
            visible: control.checked
            text: "\u2713"
            font.pixelSize: 12
            color: "white"
        }
    }

    contentItem: Label {
        text: control.text
        font: control.font
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + control.spacing
        color: control.enabled ? Theme.text : Theme.textDisabled
    }
}

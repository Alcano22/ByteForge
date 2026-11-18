import QtQuick
import QtQuick.Controls

import ByteForge.Hub

TabButton {
    id: control

    leftPadding: 12
    rightPadding: 12
    font.pixelSize: 14

    contentItem: Label {
        text: control.text
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        color: control.checked || control.hovered ? Theme.text : Theme.textDim
    }

    background: Item {
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 2
            color: Theme.accent
            visible: control.checked
        }
    }
}

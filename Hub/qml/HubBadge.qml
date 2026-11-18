import QtQuick
import QtQuick.Controls

import ByteForge.Hub

Rectangle {
    id: root

    property alias text: label.text

    implicitWidth: label.implicitWidth + 12
    implicitHeight: label.implicitHeight + 4
    radius: 4
    color: Theme.surfaceRaised
    border.color: Theme.border

    Label {
        id: label

        anchors.centerIn: parent
        font.pixelSize: 11
        color: Theme.textDim
    }
}

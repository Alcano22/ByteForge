import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import ByteForge.Hub

ItemDelegate {
    id: root

    required property string version
    required property bool preRelease
    required property string pageUrl

    height: 56
    hoverEnabled: true
    leftPadding: 16
    rightPadding: 16

    background: Rectangle {
        color: root.hovered ? Theme.surfaceRaised : "transparent"

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: Theme.border
        }
    }

    contentItem: RowLayout {
        spacing: 8

        Label {
            text: "ByteForge " + root.version
            font.pixelSize: 15
            font.bold: true
            color: Theme.text
        }

        HubBadge {
            visible: root.preRelease
            text: "Pre-release"
        }

        Item {
            Layout.fillWidth: true
        }

        HubButton {
            visible: root.pageUrl !== ""
            text: "Release Notes"
            onClicked: Qt.openUrlExternally(root.pageUrl)
        }
    }
}

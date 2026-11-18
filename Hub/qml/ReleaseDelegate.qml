import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import ByteForge.Hub

ItemDelegate {
    id: root

    required property string version
    required property bool preRelease
    required property string pageUrl

    readonly property bool installing: HubController.installingVersion === root.version
    readonly property bool busy: HubController.installingVersion !== ""

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

        ColumnLayout {
            Layout.preferredWidth: 240
            visible: root.installing
            spacing: 4

            Label {
                Layout.fillWidth: true
                text: HubController.installStatus
                font.pixelSize: 12
                color: Theme.textDim
                elide: Text.ElideRight
            }

            HubProgressBar {
                Layout.fillWidth: true
                indeterminate: HubController.installProgress < 0
                value: Math.max(0, HubController.installProgress)
            }
        }

        HubButton {
            visible: root.installing
            text: "Cancel"
            enabled: HubController.canCancelInstall
            onClicked: HubController.cancelInstall()
        }

        HubButton {
            visible: !root.installing && root.pageUrl !== ""
            text: "Release Notes"
            onClicked: Qt.openUrlExternally(root.pageUrl)
        }

        HubButton {
            visible: !root.installing
            text: "Install"
            accent: true
            enabled: !root.busy
            onClicked: HubController.installEditor(root.version)
        }
    }
}

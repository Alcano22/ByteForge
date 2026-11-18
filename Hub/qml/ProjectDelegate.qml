import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import ByteForge.Hub

ItemDelegate {
    id: root

    required property string name
    required property string path
    required property date lastOpened
    required property bool missing
    required property string engineVersion

    signal openWithRequested()

    height: 64
    hoverEnabled: true
    leftPadding: 16
    rightPadding: 16

    background: Rectangle {
        color: root.down ? Theme.pressed : root.hovered ? Theme.surfaceRaised : "transparent"

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: Theme.border
        }
    }

    contentItem: RowLayout {
        spacing: 16

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Label {
                Layout.fillWidth: true
                text: root.name
                font.pixelSize: 15
                font.bold: true
                color: root.missing ? Theme.textDim : Theme.text
                elide: Text.ElideRight
            }

            Label {
                Layout.fillWidth: true
                text: root.missing ? "Not found: " + root.path : root.path
                color: root.missing ? Theme.error : Theme.textDim
                elide: Text.ElideMiddle
            }
        }

        Label {
            text: root.engineVersion !== "" ? "v" + root.engineVersion : ""
            color: Theme.textDim
        }

        Label {
            Layout.preferredWidth: 120
            horizontalAlignment: Text.AlignRight
            text: Theme.relativeTime(root.lastOpened)
            color: Theme.textDim
        }
    }

    onDoubleClicked: {
        if (!root.missing)
            HubController.openProject(root.path)
    }

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: contextMenu.popup()
    }

    Menu {
        id: contextMenu

        padding: 4

        background: Rectangle {
            implicitWidth: 188
            color: Theme.surfaceRaised
            border.color: Theme.border
            radius: Theme.radius
        }

        HubMenuItem {
            text: "Open"
            enabled: !root.missing
            onTriggered: HubController.openProject(root.path)
        }

        HubMenuItem {
            text: "Open With..."
            enabled: !root.missing
            onTriggered: root.openWithRequested()
        }

        HubMenuItem {
            text: "Show in Folder"
            enabled: !root.missing
            onTriggered: HubController.showInFolder(root.path)
        }

        MenuSeparator {
            topPadding: 4
            bottomPadding: 4

            contentItem: Rectangle {
                implicitHeight: 1
                color: Theme.border
            }
        }

        HubMenuItem {
            text: "Remove from List"
            onTriggered: HubController.removeFromList(root.path)
        }
    }
}

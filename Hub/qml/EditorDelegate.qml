import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import ByteForge.Hub

ItemDelegate {
    id: root

    required property string version
    required property string path
    required property bool development
    required property bool preRelease

    signal removeRequested()

    component Badge: Rectangle {
        property alias text: badgeText.text

        implicitWidth: badgeText.implicitWidth + 12
        implicitHeight: badgeText.implicitHeight + 4
        radius: 4
        color: Theme.surfaceRaised
        border.color: Theme.border

        Label {
            id: badgeText

            anchors.centerIn: parent
            font.pixelSize: 11
            color: Theme.textDim
        }
    }

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

            RowLayout {
                spacing: 8

                Label {
                    text: "ByteForge " + root.version
                    font.pixelSize: 15
                    font.bold: true
                    color: Theme.text
                }

                Badge {
                    visible: root.development
                    text: "Development build"
                }

                Badge {
                    visible: root.preRelease
                    text: "Pre-release"
                }
            }

            Label {
                Layout.fillWidth: true
                text: root.path
                color: Theme.textDim
                elide: Text.ElideMiddle
            }
        }
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
            text: "Show in Folder"
            onTriggered: HubController.showInFolder(root.path)
        }

        HubMenuItem {
            text: "Remove"
            enabled: !root.development
            onTriggered: root.removeRequested()
        }
    }
}

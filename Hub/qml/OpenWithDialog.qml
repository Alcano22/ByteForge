import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import ByteForge.Hub

Dialog {
    id: root

    property string projectPath: ""
    property string projectName: ""
    property string projectVersion: ""

    signal editorChosen(string editorPath, string editorVersion, string change)

    function openFor(path, name, version) {
        projectPath = path
        projectName = name
        projectVersion = version
        open()
    }

    title: "Open " + projectName + " With"
    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: Overlay.overlay
    width: 480
    leftPadding: 20
    rightPadding: 20
    topPadding: 12
    bottomPadding: 12

    background: Rectangle {
        color: Theme.surface
        border.color: Theme.border
        radius: 8
    }

    Overlay.modal: Rectangle {
        color: Qt.rgba(0, 0, 0, 0.5)
    }

    header: Label {
        text: root.title
        font.pixelSize: 16
        font.bold: true
        color: Theme.text
        elide: Text.ElideRight
        leftPadding: 20
        rightPadding: 20
        topPadding: 20
    }

    contentItem: ColumnLayout {
        spacing: 8

        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Theme.textDim
            text: root.projectVersion !== ""
                ? "The project belongs to ByteForge " + root.projectVersion + "."
                : "The project names no ByteForge version, any editor can open it."
        }

        ListView {
            id: editorList

            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, 260)
            clip: true
            spacing: 4
            model: HubController.editors

            delegate: ItemDelegate {
                id: editorItem

                required property string version
                required property string path
                required property bool development
                required property bool preRelease

                readonly property string change: HubController.versionChange(root.projectVersion, version)

                width: ListView.view.width
                enabled: change !== "downgrade"
                hoverEnabled: true
                padding: 10

                background: Rectangle {
                    radius: Theme.radius
                    color: editorItem.hovered && editorItem.enabled ? Theme.surfaceRaised : "transparent"
                    border.color: editorItem.change === "same" ? Theme.accent : Theme.border
                }

                contentItem: RowLayout {
                    spacing: 8

                    Label {
                        text: "ByteForge " + editorItem.version
                        font.bold: true
                        color: editorItem.enabled ? Theme.text : Theme.textDisabled
                    }

                    HubBadge {
                        visible: editorItem.development
                        text: "Development"
                    }

                    HubBadge {
                        visible: editorItem.preRelease
                        text: "Pre-release"
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Label {
                        color: editorItem.change === "upgrade" ? Theme.accent : Theme.textDim
                        text: editorItem.change === "same" ? "Project's version"
                            : editorItem.change === "upgrade" ? "Upgrades the project"
                                : editorItem.change === "downgrade" ? "Too old for this project"
                                    : ""
                    }
                }

                onClicked: {
                    root.close()
                    root.editorChosen(editorItem.path, editorItem.version, editorItem.change)
                }
            }

            Label {
                anchors.centerIn: parent
                visible: editorList.count === 0
                color: Theme.textDim
                text: "No editor is installed"
            }
        }
    }

    footer: Control {
        leftPadding: 20
        rightPadding: 20
        topPadding: 8
        bottomPadding: 20

        background: null

        contentItem: RowLayout {
            Item {
                Layout.fillWidth: true
            }

            HubButton {
                text: "Cancel"
                onClicked: root.close()
            }
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import ByteForge.Hub

Dialog {
    id: root

    property string projectPath: ""
    property string projectName: ""
    property string version: ""

    property bool published: false

    signal openWithRequested()

    function openFor(path, name, projectVersion) {
        projectPath = path
        projectName = name
        version = projectVersion
        published = HubController.isPublished(projectVersion)
        open()
    }

    title: "ByteForge " + version + " is not installed"
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

    contentItem: Label {
        wrapMode: Text.Wrap
        color: Theme.textDim
        text: "'" + root.projectName + "' belongs to ByteForge " + root.version + ". "
            + (root.published
                ? "Install it to open the project, or open it with a newer version, which upgrades it."
                : "It is not available on GitHub (or GitHub cannot be reached right now). "
                + "You can open the project with a newer version, which upgrades it.")
    }

    footer: Control {
        leftPadding: 20
        rightPadding: 20
        topPadding: 8
        bottomPadding: 20

        background: null

        contentItem: RowLayout {
            spacing: 8

            Item {
                Layout.fillWidth: true
            }

            HubButton {
                text: "Cancel"
                onClicked: root.close()
            }

            HubButton {
                text: "Open With..."
                onClicked: {
                    root.close()
                    root.openWithRequested()
                }
            }

            HubButton {
                visible: root.published
                text: "Install " + root.version
                accent: true
                enabled: HubController.installingVersion === ""
                onClicked: {
                    root.close()
                    HubController.installAndOpen(root.version, root.projectPath)
                }
            }
        }
    }
}

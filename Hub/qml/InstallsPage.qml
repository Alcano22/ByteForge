import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import ByteForge.Hub

Item {
    id: root

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 12
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            spacing: 8

            Label {
                Layout.fillWidth: true
                text: "Editors are installed to " + HubController.installsDirectory
                color: Theme.textDim
                elide: Text.ElideMiddle
            }

            HubButton {
                text: "Open Folder"
                onClicked: HubController.openInstallsFolder()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: Theme.border
        }

        ListView {
            id: editorList

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: HubController.editors

            delegate: EditorDelegate {
                width: ListView.view.width

                onRemoveRequested: {
                    removeDialog.path = path
                    removeDialog.title = "Remove ByteForge " + version + "?"
                    removeDialog.open()
                }
            }

            ScrollBar.vertical: ScrollBar {}

            Label {
                anchors.centerIn: parent
                visible: editorList.count === 0
                color: Theme.textDim
                text: "No editors installed yet."
            }
        }
    }

    ConfirmDialog {
        id: removeDialog

        property string path: ""

        message: "This deletes " + path + ". Your projects are not touched."
        confirmText: "Remove"
        onConfirmed: HubController.removeEditor(path)
    }
}

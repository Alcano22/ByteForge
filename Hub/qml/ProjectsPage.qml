import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
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

            HubTextField {
                Layout.preferredWidth: 280
                placeholderText: "Search projects"
                onTextChanged: HubController.search = text
            }

            Item {
                Layout.fillWidth: true
            }

            HubButton {
                text: "Add..."
                onClicked: addDialog.open()
            }

            HubButton {
                text: "New Project"
                accent: true
                onClicked: newProjectDialog.openFresh()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: Theme.border
        }

        ListView {
            id: projectList

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: HubController.projects

            delegate: ProjectDelegate {
                width: ListView.view.width
            }

            ScrollBar.vertical: ScrollBar {}

            Label {
                anchors.centerIn: parent
                visible: projectList.count === 0
                color: Theme.textDim
                text: HubController.search !== ""
                    ? "No project matches \"" + HubController.search + "\""
                    : "No projects yet. Add an existing one or create a new project."
            }
        }
    }

    FolderDialog {
        id: addDialog

        title: "Add an existing project"
        onAccepted: HubController.addExistingProject(selectedFolder)
    }

    NewProjectDialog {
        id: newProjectDialog
    }
}

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

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.bottomMargin: 10
            visible: HubController.installingVersion !== ""
            spacing: 12

            Label {
                Layout.fillWidth: true
                text: "Installing ByteForge " + HubController.installingVersion + ": " + HubController.installStatus
                color: Theme.textDim
                elide: Text.ElideRight
            }

            HubProgressBar {
                Layout.preferredWidth: 200
                indeterminate: HubController.installProgress < 0
                value: Math.max(0, HubController.installProgress)
            }

            HubButton {
                text: "Cancel"
                enabled: HubController.canCancelInstall
                onClicked: HubController.cancelInstall()
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
                id: projectItem

                width: ListView.view.width
                onOpenWithRequested: openWithDialog.openFor(projectItem.path, projectItem.name,
                    projectItem.engineVersion)
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

    OpenWithDialog {
        id: openWithDialog

        onEditorChosen: (editorPath, editorVersion, change) => {
            if (change === "upgrade") {
                upgradeDialog.editorPath = editorPath
                upgradeDialog.title = "Upgrade " + projectName + " to ByteForge " + editorVersion + "?"
                upgradeDialog.message = "The project belongs to ByteForge " + projectVersion + ". ByteForge "
                    + editorVersion + " takes it over: afterwards it opens with " + editorVersion
                    + " or newer, not with " + projectVersion + " anymore. Commit or back up the project first."
                upgradeDialog.open()
            } else {
                HubController.openProjectWith(projectPath, editorPath)
            }
        }
    }

    ConfirmDialog {
        id: upgradeDialog

        property string editorPath: ""

        confirmText: "Upgrade and Open"
        onConfirmed: HubController.openProjectWith(openWithDialog.projectPath, editorPath)
    }

    MissingEditorDialog {
        id: missingDialog

        onOpenWithRequested: openWithDialog.openFor(projectPath, projectName, version)
    }

    Connections {
        target: HubController

        function onEditorMissing(projectPath, projectName, version) {
            missingDialog.openFor(projectPath, projectName, version)
        }
    }
}

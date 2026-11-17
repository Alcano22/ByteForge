import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

import ByteForge.Hub

Dialog {
    id: root

    property string templateId: ""
    property string createError: ""

    readonly property string problem: HubController.validateNewProject(nameField.text, locationField.text)
    readonly property bool canCreate: problem === "" && templateId !== ""

    function openFresh() {
        locationField.text = HubController.suggestLocation()
        nameField.text = HubController.suggestProjectName(locationField.text)
        templateId = HubController.templates.length > 0 ? HubController.templates[0].id : ""
        createError = ""

        open()
        nameField.forceActiveFocus()
        nameField.selectAll()
    }

    function create() {
        if (!canCreate)
            return

        createError = HubController.createProject(nameField.text, locationField.text, templateId)
        if (createError === "")
            close()
    }

    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: Overlay.overlay
    width: 560
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
        text: "New Project"
        font.pixelSize: 18
        font.bold: true
        color: Theme.text
        leftPadding: 20
        rightPadding: 20
        topPadding: 20
    }

    contentItem: ColumnLayout {
        spacing: 6

        Label {
            text: "Name"
            color: Theme.textDim
        }

        HubTextField {
            id: nameField

            Layout.fillWidth: true
            onTextChanged: root.createError = ""
            onAccepted: root.create()
        }

        Label {
            Layout.topMargin: 8
            text: "Location"
            color: Theme.textDim
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            HubTextField {
                id: locationField

                Layout.fillWidth: true
                onTextChanged: root.createError = ""
                onAccepted: root.create()
            }

            HubButton {
                text: "Browse..."
                onClicked: folderDialog.open()
            }
        }

        Label {
            Layout.topMargin: 8
            text: "Template"
            color: Theme.textDim
        }

        ListView {
            id: templateList

            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, 220)
            clip: true
            spacing: 6
            model: HubController.templates

            delegate: ItemDelegate {
                id: templateItem

                required property var modelData
                readonly property bool selected: modelData.id === root.templateId

                width: ListView.view.width
                hoverEnabled: true
                padding: 12

                background: Rectangle {
                    radius: Theme.radius
                    color: templateItem.selected ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18)
                        : templateItem.hovered ? Theme.surfaceRaised : "transparent"
                    border.color: templateItem.selected ? Theme.accent : Theme.border
                }

                contentItem: ColumnLayout {
                    spacing: 2

                    Label {
                        Layout.fillWidth: true
                        text: templateItem.modelData.name
                        font.bold: true
                        color: Theme.text
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: text !== ""
                        text: templateItem.modelData.description
                        color: Theme.textDim
                        wrapMode: Text.Wrap
                    }
                }

                onClicked: root.templateId = modelData.id

                onDoubleClicked: {
                    root.templateId = modelData.id
                    root.create()
                }
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.topMargin: 8
            wrapMode: Text.Wrap
            color: root.createError !== "" || root.problem !== "" ? Theme.error : Theme.textDim
            text: root.createError !== "" ? root.createError
                : root.problem !== "" ? root.problem
                    : "Creates " + HubController.projectPath(locationField.text, nameField.text)
        }
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
                text: "Create"
                accent: true
                enabled: root.canCreate
                onClicked: root.create()
            }
        }
    }

    FolderDialog {
        id: folderDialog

        title: "Choose where to create the project"
        currentFolder: HubController.pathToUrl(locationField.text)
        onAccepted: locationField.text = HubController.urlToPath(selectedFolder)
    }
}

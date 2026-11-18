import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import ByteForge.Hub

Item {
    id: root

    component SectionTitle: Label {
        Layout.leftMargin: 16
        Layout.topMargin: 16
        Layout.bottomMargin: 6
        font.pixelSize: 12
        font.bold: true
        font.letterSpacing: 1
        color: Theme.textDim
    }

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

        ScrollView {
            id: scroll

            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true

            ColumnLayout {
                width: scroll.availableWidth
                spacing: 0

                SectionTitle {
                    text: "INSTALLED"
                }

                ListView {
                    id: editorList

                    Layout.fillWidth: true
                    Layout.preferredHeight: contentHeight
                    interactive: false
                    model: HubController.editors

                    delegate: EditorDelegate {
                        width: ListView.view.width

                        onRemoveRequested: {
                            removeDialog.path = path
                            removeDialog.title = "Remove ByteForge " + version + "?"
                            removeDialog.open()
                        }
                    }
                }

                Label {
                    Layout.leftMargin: 16
                    visible: editorList.count === 0
                    text: "No editors installed yet."
                    color: Theme.textDim
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.rightMargin: 16
                    spacing: 12

                    SectionTitle {
                        text: "AVAILABLE ON GITHUB"
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    HubCheckBox {
                        Layout.topMargin: 10
                        text: "Show pre-releases"
                        checked: HubController.showPreReleases
                        onToggled: HubController.showPreReleases = checked
                    }

                    HubButton {
                        Layout.topMargin: 10
                        text: HubController.checkingReleases ? "Checking..." : "Check Again"
                        enabled: !HubController.checkingReleases
                        onClicked: HubController.checkForReleases()
                    }
                }

                ListView {
                    id: releaseList

                    Layout.fillWidth: true
                    Layout.preferredHeight: contentHeight
                    interactive: false
                    model: HubController.releases

                    delegate: ReleaseDelegate {
                        width: ListView.view.width
                    }
                }

                Label {
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    Layout.bottomMargin: 16
                    wrapMode: Text.Wrap
                    visible: text !== ""
                    color: HubController.releaseError !== "" ? Theme.error : Theme.textDim
                    text: {
                        if (HubController.releaseError !== "")
                            return HubController.releaseError
                        if (HubController.checkingReleases && releaseList.count === 0)
                            return "Asking GitHub for editor versions..."
                        if (HubController.releasesChecked && releaseList.count === 0)
                            return "Every published version is installed."
                        return ""
                    }
                }
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

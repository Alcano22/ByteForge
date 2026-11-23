import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import ByteForge.Hub

Item {
    id: root

    readonly property string dotnetName: ".NET " + PackageController.dotnetMajorVersion + " SDK"

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
                text: "Packages are installed to " + PackageController.packagesDirectory
                color: Theme.textDim
                elide: Text.ElideMiddle
            }

            HubButton {
                text: "Open Folder"
                onClicked: PackageController.openPackagesFolder()
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
                    text: "PACKAGES"
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: dotnetRow.implicitHeight + 24
                    color: "transparent"

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 1
                        color: Theme.border
                    }

                    RowLayout {
                        id: dotnetRow

                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        spacing: 16

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            RowLayout {
                                spacing: 8

                                Label {
                                    text: root.dotnetName
                                    font.pixelSize: 15
                                    font.bold: true
                                    color: Theme.text
                                }

                                HubBadge {
                                    visible: PackageController.dotnetSource === "hub"
                                    text: "Installed by the Hub"
                                }

                                HubBadge {
                                    visible: PackageController.dotnetSource === "system"
                                    text: "Found on this system"
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                color: Theme.textDim
                                text: "Builds and runs C# scripts in the editor. Editors that are already open "
                                    + "need a restart after it is installed."
                            }

                            Label {
                                Layout.fillWidth: true
                                visible: !PackageController.busy
                                elide: Text.ElideMiddle
                                color: PackageController.dotnetSource === "" ? Theme.error : Theme.text
                                text: PackageController.dotnetSource === ""
                                    ? "Not installed, C# scripts can't be built"
                                    : PackageController.dotnetVersion + " in " + PackageController.dotnetPath
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.topMargin: 4
                                visible: PackageController.busy
                                spacing: 6

                                Label {
                                    Layout.fillWidth: true
                                    text: PackageController.status
                                    color: Theme.text
                                    elide: Text.ElideRight
                                }

                                HubProgressBar {
                                    Layout.fillWidth: true
                                    indeterminate: PackageController.progress < 0
                                    value: Math.max(0, PackageController.progress)
                                }
                            }
                        }

                        HubButton {
                            visible: PackageController.busy
                            enabled: PackageController.canCancel
                            text: "Cancel"
                            onClicked: PackageController.cancel()
                        }

                        HubButton {
                            visible: !PackageController.busy && PackageController.dotnetSource === ""
                            accent: true
                            text: "Install"
                            onClicked: PackageController.installDotNet()
                        }

                        HubButton {
                            visible: !PackageController.busy && PackageController.dotnetSource === "system"
                            text: "Install Separately"
                            onClicked: PackageController.installDotNet()

                            ToolTip.visible: hovered
                            ToolTip.delay: 500
                            ToolTip.text: "Installs a copy the Hub manages, the editor then prefers it"
                        }

                        HubButton {
                            visible: !PackageController.busy && PackageController.dotnetSource === "hub"
                            text: "Update"
                            onClicked: PackageController.installDotNet()

                            ToolTip.visible: hovered
                            ToolTip.delay: 500
                            ToolTip.text: "Installs the newest " + root.dotnetName
                        }

                        HubButton {
                            visible: !PackageController.busy && PackageController.dotnetSource === "hub"
                            text: "Remove"
                            onClicked: removeDialog.open()
                        }
                    }
                }
            }
        }
    }

    ConfirmDialog {
        id: removeDialog

        title: "Remove the " + root.dotnetName + "?"
        message: "This deletes " + PackageController.dotnetPath + ". C# scripts can't be built afterwards, "
            + "unless .NET is installed some other way."
        confirmText: "Remove"
        onConfirmed: PackageController.removeDotNet()
    }
}

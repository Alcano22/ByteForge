import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import ByteForge.Hub

ApplicationWindow {
    id: window

    width: 960
    height: 600
    minimumWidth: 640
    minimumHeight: 400
    visible: true
    title: "ByteForge Hub"
    color: Theme.background

    palette {
        window: Theme.background
        windowText: Theme.text
        base: Theme.surface
        alternateBase: Theme.surfaceRaised
        text: Theme.text
        button: Theme.surfaceRaised
        buttonText: Theme.text
        brightText: "white"
        highlight: Theme.accent
        highlightedText: "white"
        placeholderText: Theme.textDim
        toolTipBase: Theme.surfaceRaised
        toolTipText: Theme.text
        mid: Theme.border
        dark: Theme.border
        light: Theme.surfaceRaised
    }

    onActiveChanged: {
        if (active) {
            HubController.refresh()
            PackageController.refresh()
        }
    }

    header: ToolBar {
        implicitHeight: 56

        background: Rectangle {
            color: Theme.surface

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: Theme.border
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 10

            Label {
                text: "ByteForge Hub"
                font.pixelSize: 18
                font.bold: true
                color: Theme.text
            }

            Label {
                text: "v" + HubController.engineVersion
                color: Theme.textDim
            }

            TabBar {
                id: tabs

                Layout.leftMargin: 24
                Layout.fillHeight: true
                spacing: 4
                background: null

                HubTab {
                    text: "Projects"
                }

                HubTab {
                    text: "Installs"
                }

                HubTab {
                    text: "Packages"
                }
            }

            Item {
                Layout.fillWidth: true
            }
        }
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: tabs.currentIndex

        ProjectsPage {}

        InstallsPage {}

        PackagesPage {}
    }

    Rectangle {
        id: errorBanner

        function show(message) {
            errorText.text = message
            visible = true
            hideTimer.restart()
        }

        visible: false
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 12
        height: errorText.implicitHeight + 20
        radius: Theme.radius
        color: Theme.error

        Label {
            id: errorText

            anchors.fill: parent
            anchors.margins: 10
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.Wrap
            color: "white"
        }

        Timer {
            id: hideTimer

            interval: 6000
            onTriggered: errorBanner.visible = false
        }

        TapHandler {
            onTapped: errorBanner.visible = false
        }
    }

    Connections {
        target: HubController

        function onErrorOccurred(message) {
            errorBanner.show(message)
        }
    }

    Connections {
        target: PackageController

        function onErrorOccurred(message) {
            errorBanner.show(message)
        }
    }
}

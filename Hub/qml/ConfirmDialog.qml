import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import ByteForge.Hub

Dialog {
    id: root

    property string message: ""
    property string confirmText: "OK"

    signal confirmed()

    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: Overlay.overlay
    width: 440
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
        text: root.message
        wrapMode: Text.Wrap
        color: Theme.textDim
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
                text: root.confirmText
                accent: true
                onClicked: {
                    root.close()
                    root.confirmed()
                }
            }
        }
    }
}

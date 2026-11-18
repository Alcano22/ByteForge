import QtQuick
import QtQuick.Controls

import ByteForge.Hub

ProgressBar {
    id: control

    implicitHeight: 6

    background: Rectangle {
        implicitWidth: 200
        implicitHeight: 6
        radius: 3
        color: Theme.background
        border.color: Theme.border
    }

    contentItem: Item {
        implicitHeight: 6
        clip: true

        Rectangle {
            visible: !control.indeterminate
            width: control.visualPosition * parent.width
            height: parent.height
            radius: 3
            color: Theme.accent
        }

        Rectangle {
            id: runner

            visible: control.indeterminate
            width: parent.width * 0.3
            height: parent.height
            radius: 3
            color: Theme.accent

            NumberAnimation on x {
                running: control.indeterminate && control.visible
                from: -runner.width
                to: runner.parent.width
                duration: 1200
                loops: Animation.Infinite
            }
        }
    }
}

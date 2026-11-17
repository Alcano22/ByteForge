import QtQuick
import QtQuick.Controls

import ByteForge.Hub

Button {
    id: control

    property bool accent: false

    implicitHeight: Theme.controlHeight
    leftPadding: 14
    rightPadding: 14
    font.pixelSize: 13

    contentItem: Label {
        text: control.text
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        color: !control.enabled ? Theme.textDisabled
            : control.accent ? "white"
                : Theme.text
    }

    background: Rectangle {
        radius: Theme.radius
        border.width: control.accent ? 0 : 1
        border.color: control.visualFocus ? Theme.accent : Theme.border
        opacity: control.enabled ? 1.0 : 0.6
        color: {
            if (control.accent)
                return control.down ? Theme.accentPressed : control.hovered ? Theme.accentHover : Theme.accent
            return control.down ? Theme.pressed : control.hovered ? Theme.hover : Theme.surfaceRaised
        }
    }
}

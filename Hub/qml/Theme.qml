pragma Singleton

import QtQuick

QtObject {
    readonly property color background: "#18181c"
    readonly property color surface: "#202026"
    readonly property color surfaceRaised: "#2a2a31"
    readonly property color border: "#34343c"
    readonly property color text: "#e6e6ea"
    readonly property color textDim: "#8c8c96"
    readonly property color accent: "#4f8cff"
    readonly property color error: "#d9534f"

    readonly property color hover: "#33333b"
    readonly property color pressed: "#3d3d46"
    readonly property color accentHover: "#6a9eff"
    readonly property color accentPressed: "#3f74d9"
    readonly property color textDisabled: "#5c5c66"

    readonly property int controlHeight: 32
    readonly property int radius: 6

    function plural(count, unit) {
        return count + " " + unit + (count === 1 ? "" : "s") + " ago"
    }

    function relativeTime(date) {
        if (date.getTime() <= 0)
            return "Never"

        const seconds = Math.max(0, (Date.now() - date.getTime()) / 1000)
        const minutes = Math.floor(seconds / 60)
        const hours = Math.floor(minutes / 60)
        const days = Math.floor(hours / 24)

        if (minutes < 1)
            return "Just now"
        if (hours < 1)
            return plural(minutes, "minute")
        if (days < 1)
            return plural(hours, "hour")
        if (days < 30)
            return plural(days, "day")
        return Qt.formatDate(date, "d MMM yyyy")
    }
}

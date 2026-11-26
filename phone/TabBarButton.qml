import QtQuick
import QtQuick.Controls
import LioraPhone

// one entry of the bottom tab bar: icon above a short label, accent colour when active
TabButton {
    id: button

    readonly property color tint: checked ? Backend.theme.accent : Backend.theme.textMuted

    display: AbstractButton.TextUnderIcon
    icon.width: 24
    icon.height: 24
    icon.color: tint
    palette.windowText: tint                         // The Basic style's label colour when checked
    palette.brightText: tint                         // … and when not
    font.pixelSize: 11
    background: Item {}
}

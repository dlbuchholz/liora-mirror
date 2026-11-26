import QtQuick
import LioraPhone

// category name on its palette colour desktop tags. An empty name is "No category".
Rectangle {
    id: tag

    property string name
    property string colorName

    readonly property var swatch: {
        Backend.theme;                               // Re-read when the theme changes
        return Backend.swatch(colorName);
    }

    color: swatch.background
    radius: height / 2
    implicitWidth: label.implicitWidth + 16
    implicitHeight: label.implicitHeight + 4

    Text {
        id: label
        anchors.centerIn: parent
        text: tag.name.length > 0 ? tag.name : qsTr("No category")
        color: tag.swatch.text
        font.pixelSize: 11
    }
}

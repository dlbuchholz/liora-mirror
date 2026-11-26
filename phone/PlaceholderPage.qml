import QtQuick
import QtQuick.Layouts
import LioraPhone

// A placeholder type not implemented yet...
Item {
    id: page

    property string title
    property string note
    property real topInset

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: page.topInset + 16
        anchors.leftMargin: 16
        anchors.rightMargin: 16

        Text {
            text: page.title
            color: Backend.theme.textPrimary
            font.pixelSize: 28
            font.bold: true
        }
        Text {
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: page.note
            color: Backend.theme.textMuted
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }
}

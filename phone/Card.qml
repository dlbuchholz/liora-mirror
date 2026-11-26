import QtQuick
import QtQuick.Layouts
import LioraPhone

// rounded panel like desktop dashboard cards. Children are stacked in a column.
Rectangle {
    id: card

    default property alias content: column.data
    property int padding: 14

    color: Backend.theme.card
    radius: 14
    implicitHeight: column.implicitHeight + 2 * padding

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: card.padding
        spacing: 6
    }
}

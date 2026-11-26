import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LioraPhone

// All transactions, newest first, with a header per day. Tap a day to fold it.
Item {
    id: page

    property real topInset

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: page.topInset + 16
        spacing: 8

        RowLayout {
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Text {
                text: qsTr("Transactions")
                color: Backend.theme.textPrimary
                font.pixelSize: 28
                font.bold: true
                Layout.fillWidth: true
            }
            ToolButton {
                icon.source: "qrc:/icons/plus.svg"
                icon.color: Backend.theme.accent
                icon.width: 24
                icon.height: 24
                background: Item {}
                onClicked: notYet.open()
                ToolTip {
                    id: notYet
                    text: qsTr("Adding comes later")
                    timeout: 2000
                }
            }
        }

        TextField {
            id: search
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            placeholderText: qsTr("Search")
            placeholderTextColor: Backend.theme.textMuted
            color: Backend.theme.textPrimary
            leftPadding: 12
            background: Rectangle {
                color: Backend.theme.card
                radius: 10
                implicitHeight: 36
            }
            onTextChanged: Backend.setSearch(text)
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: Backend.transactions

            delegate: Item {
                id: row

                required property int index
                required property var model
                readonly property bool isHeader: model.kind === 0     // TransactionListModel::Kind::DayHeader

                width: ListView.view.width
                height: isHeader ? 36 : 54

                // Day header: date and the day's net amount
                RowLayout {
                    visible: row.isHeader
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    anchors.topMargin: 12

                    Text {
                        text: (row.model.collapsed ? "▸ " : "") + Backend.dayLabel(row.index)
                        color: Backend.theme.textMuted
                        font.pixelSize: 12
                        Layout.fillWidth: true
                    }
                    Text {
                        text: row.isHeader ? Backend.money(row.model.dayNet, true) : ""
                        color: row.isHeader && row.model.dayNet > 0 ? Backend.theme.positive : Backend.theme.textMuted
                        font.pixelSize: 12
                    }
                }

                // Transaction: payee over its category tag, amount on the right
                RowLayout {
                    visible: !row.isHeader
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3
                        Text {
                            text: row.isHeader ? "" : (row.model.payee || row.model.description)
                            color: Backend.theme.textPrimary
                            font.pixelSize: 15
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        CategoryTag {
                            visible: !row.isHeader
                            name: row.isHeader ? "" : row.model.category
                            colorName: row.isHeader ? "" : row.model.categoryColor
                        }
                    }
                    Text {
                        text: row.isHeader ? "" : Backend.money(row.model.signedAmount, true)
                        color: !row.isHeader && row.model.signedAmount > 0 ? Backend.theme.positive : Backend.theme.textPrimary
                        font.pixelSize: 15
                    }
                }

                Rectangle {
                    visible: !row.isHeader
                    anchors.bottom: parent.bottom
                    x: 16
                    width: parent.width - 32
                    height: 1
                    color: Backend.theme.separator
                }

                TapHandler {
                    enabled: row.isHeader
                    onTapped: Backend.toggleDay(row.index)
                }
            }
        }
    }
}

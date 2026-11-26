import QtQuick
import QtQuick.Layouts
import LioraPhone

// The dashboard as one column of cards: balance, this month, spending by category, budgets
Flickable {
    id: page

    property real topInset
    readonly property var dash: Backend.dashboard

    contentHeight: column.implicitHeight + topInset + 32
    clip: true

    ColumnLayout {
        id: column
        x: 12
        y: page.topInset + 16
        width: page.width - 24
        spacing: 10

        ColumnLayout {
            Layout.leftMargin: 4
            spacing: 0
            Text {
                text: page.dash.greeting
                color: Backend.theme.textMuted
                font.pixelSize: 13
            }
            Text {
                text: qsTr("Overview")
                color: Backend.theme.textPrimary
                font.pixelSize: 28
                font.bold: true
            }
        }

        Card {
            Layout.fillWidth: true
            Text {
                text: qsTr("Total balance")
                color: Backend.theme.textMuted
                font.pixelSize: 12
            }
            Text {
                text: page.dash.balance
                color: Backend.theme.textPrimary
                font.pixelSize: 30
                font.bold: true
            }
            Text {
                text: page.dash.accounts
                color: Backend.theme.textMuted
                font.pixelSize: 12
            }
        }

        RowLayout {
            spacing: 10
            Layout.fillWidth: true

            Card {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Text {
                    text: qsTr("Income · %1").arg(page.dash.month)
                    color: Backend.theme.textMuted
                    font.pixelSize: 12
                }
                Text {
                    text: page.dash.income
                    color: Backend.theme.positive
                    font.pixelSize: 18
                }
            }
            Card {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Text {
                    text: qsTr("Spent · %1").arg(page.dash.month)
                    color: Backend.theme.textMuted
                    font.pixelSize: 12
                }
                Text {
                    text: page.dash.spent
                    color: Backend.theme.textPrimary
                    font.pixelSize: 18
                }
            }
        }

        Card {
            Layout.fillWidth: true
            visible: page.dash.categories.length > 0

            Text {
                text: qsTr("Spending by category")
                color: Backend.theme.textMuted
                font.pixelSize: 12
                Layout.bottomMargin: 4
            }

            // One bar split by the categories' shares
            Item {
                Layout.fillWidth: true
                Layout.bottomMargin: 6
                implicitHeight: 8

                Rectangle {
                    id: barShape
                    anchors.fill: parent
                    radius: 4
                    color: Backend.theme.track
                    clip: true

                    Row {
                        anchors.fill: parent
                        Repeater {
                            model: page.dash.categories
                            Rectangle {
                                required property var modelData
                                width: barShape.width * modelData.share
                                height: barShape.height
                                color: {
                                    Backend.theme;
                                    return Backend.swatch(modelData.color).solid;
                                }
                            }
                        }
                    }
                }
            }

            Repeater {
                model: page.dash.categories
                RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    CategoryTag {
                        name: modelData.name
                        colorName: modelData.color
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    Text {
                        text: modelData.amount
                        color: Backend.theme.textPrimary
                        font.pixelSize: 13
                    }
                }
            }
        }

        Repeater {
            model: page.dash.budgets

            Card {
                required property var modelData
                Layout.fillWidth: true

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: qsTr("Budget")
                        color: Backend.theme.textMuted
                        font.pixelSize: 12
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    CategoryTag {
                        name: modelData.name
                        colorName: modelData.color
                    }
                }
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 6
                    radius: 3
                    color: Backend.theme.track

                    Rectangle {
                        width: parent.width * modelData.progress
                        height: parent.height
                        radius: 3
                        color: {
                            Backend.theme;
                            return modelData.over ? Backend.theme.danger : Backend.swatch(modelData.color).solid;
                        }
                    }
                }
                Text {
                    text: qsTr("%1 of %2 · %3").arg(modelData.spent).arg(modelData.limit).arg(page.dash.daysLeft)
                    color: modelData.over ? Backend.theme.danger : Backend.theme.textMuted
                    font.pixelSize: 12
                }
            }
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LioraPhone

// One page per tab, with the tab bar at the bottom where the thumb reaches it
ApplicationWindow {
    id: window

    width: 390                                       // An iPhone 15/16 screen in points
    height: 844
    visible: true
    title: Qt.application.displayName
    color: Backend.theme.window

    property int startTab: 0                         // --tab

    // Room for the status bar and the home indicator on iOS; 0 on the desktop
    readonly property real topInset: contentItem.SafeArea.margins.top
    readonly property real bottomInset: contentItem.SafeArea.margins.bottom

    StackLayout {
        anchors.fill: parent
        currentIndex: tabBar.currentIndex

        OverviewPage {
            topInset: window.topInset
        }
        TransactionsPage {
            topInset: window.topInset
        }
        PlaceholderPage {
            topInset: window.topInset
            title: qsTr("Budget")
            note: qsTr("Not implemented yet. The overview shows this month's budgets.")
        }
        PlaceholderPage {
            topInset: window.topInset
            title: qsTr("Settings")
            note: qsTr("Not implemented yet.")
        }
    }

    footer: Rectangle {
        color: Backend.theme.window
        implicitHeight: tabBar.implicitHeight + window.bottomInset + 1

        Rectangle {
            width: parent.width
            height: 1
            color: Backend.theme.separator
        }

        TabBar {
            id: tabBar
            y: 1
            width: parent.width
            background: Item {}
            currentIndex: window.startTab

            TabBarButton {
                text: qsTr("Overview")
                icon.source: "qrc:/icons/dashboard.svg"
            }
            TabBarButton {
                text: qsTr("Transactions")
                icon.source: "qrc:/icons/transactions.svg"
            }
            TabBarButton {
                text: qsTr("Budget")
                icon.source: "qrc:/icons/budget.svg"
            }
            TabBarButton {
                text: qsTr("Settings")
                icon.source: "qrc:/icons/settings.svg"
            }
        }
    }
}

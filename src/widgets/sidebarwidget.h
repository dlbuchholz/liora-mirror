#ifndef SIDEBARWIDGET_H
#define SIDEBARWIDGET_H

#include <QWidget>
#include <QButtonGroup>
#include <QVariantAnimation>

class QLabel;
class QScrollArea;
class QVBoxLayout;
class SidebarItem;

// Left-hand navigation: page links, collapsible account and budget sections,
// and settings / user / collapse controls pinned to the bottom.
class SidebarWidget : public QWidget {
    Q_OBJECT

public:
    enum class Page {
        Dashboard,
        Transactions,
        Budget,
        Reports,
        Settings
    };
    Q_ENUM(Page)

    static constexpr int ALL_ACCOUNTS_ID = -1;

    explicit SidebarWidget(QWidget* parent = nullptr);

    void setUserId(int userId);
    void refresh();                                  // Reload user, accounts and budgets
    void setCurrentPage(Page page);

    bool isCollapsed() const;
    void setCollapsed(bool collapsed, bool animate = true);

signals:
    void pageRequested(SidebarWidget::Page page);
    void accountRequested(int accountId);            // ALL_ACCOUNTS_ID for all accounts
    void addAccountRequested();
    void correctBalanceRequested(int accountId);     // From an account's right-click menu
    void editAccountRequested(int accountId);        // The same
    void deleteAccountRequested(int accountId);      // The same
    void switchUserRequested();
    void budgetSortChanged();                        // Chosen from the Budget header's right-click menu

protected:
    void paintEvent(QPaintEvent* event) override;
    void changeEvent(QEvent* event) override;        // The scroll bar and the wordmark follow the theme
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void updateWordmark();                           // In the brand's colours, softened for the dark theme
    void showBrand();                                // The wordmark, or the logo when collapsed

    struct Section {
        SidebarItem* header = nullptr;
        QWidget* body = nullptr;
        QVBoxLayout* bodyLayout = nullptr;
        QString key;
        QString summary;                             // Shown in the header while the section is folded
        bool summaryWhenExpanded = false;            // ... and while it's open
        bool expanded = true;
    };

    SidebarItem* addNavItem(QVBoxLayout* layout, const QString& icon, const QString& text, Page page);
    Section createSection(QVBoxLayout* layout, const QString& key, const QString& title);
    void setSectionExpanded(Section& section, bool expanded);
    void clearSection(Section& section);
    void styleScrollBar();                           // Thin, without arrows, in the theme's colours

    void loadUser();
    void loadAccounts();
    void loadBudgets();
    void fitBudgets();
    void showUserMenu();
    void showBudgetSortMenu(const QPoint& position);

    int userId = -1;
    bool collapsed = false;

    QButtonGroup navGroup;
    QScrollArea* sectionsArea = nullptr;
    Section accountsSection;
    Section budgetSection;
    QList<SidebarItem*> budgetItems;                 // In the chosen order
    SidebarItem* moreBudgetsItem = nullptr;          // None when all are wanted
    SidebarItem* userItem = nullptr;
    SidebarItem* collapseItem = nullptr;
    QWidget* brandRow = nullptr;                     // The wordmark or logo, with the divider under it
    QLabel* wordmark = nullptr;                      // At the top; hidden in the collapsed rail
    QLabel* brandLogo = nullptr;                     // Only in the collapsed rail, which has no room for the wordmark
    QList<SidebarItem*> labelledItems;               // Items whose label hides in the collapsed rail
    QVariantAnimation widthAnimation;
};

#endif // SIDEBARWIDGET_H

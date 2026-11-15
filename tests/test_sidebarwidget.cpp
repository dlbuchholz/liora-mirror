#include <QtTest>
#include "appname.h"
#include <QScrollArea>
#include <QScrollBar>
#include <QAbstractButton>
#include <QLabel>
#include <QMenu>
#include <QSignalSpy>
#include <QTimer>

#include "account.h"
#include "budget.h"
#include "category.h"
#include "settings.h"
#include "theme.h"
#include "version.h"
#include "tests/test_db_fixture.h"
#include "widgets/sidebarwidget.h"

class TestSidebarWidget : public QObject {
    Q_OBJECT

    TestDbFixture db;
    QMap<QString, QVariant> originalSettings;
    int giroId = -1;
    int creditId = -1;

    // Sidebar entries are buttons; their right-hand value is the accessible description
    static QAbstractButton* item(SidebarWidget& sidebar, const QString& text) {
        for (QAbstractButton* button : sidebar.findChildren<QAbstractButton*>()) {
            if (button->text() == text) return button;
        }
        return nullptr;
    }
    static QString valueOf(SidebarWidget& sidebar, const QString& text) {
        QAbstractButton* button = item(sidebar, text);
        return button ? button->accessibleDescription() : QString("<missing %1>").arg(text);
    }
    static int categoryId(const char* name) {
        QSqlQuery query(DatabaseManager::instance().database());
        query.prepare("SELECT id FROM categories WHERE name = :name");
        query.bindValue(":name", name);
        return query.exec() && query.next() ? query.value(0).toInt() : 0;
    }

    static QString budgetHeaderTitle() {
        return QString("Budget · %1").arg(QDate::currentDate().toString("MMMM"));
    }

private slots:
    void initTestCase() {
        db.init();
        QSettings& settings = Settings::instance().getQSettings();
        for (const QString& key : settings.allKeys()) originalSettings.insert(key, settings.value(key));
    }
    void cleanupTestCase() {
        QSettings& settings = Settings::instance().getQSettings();
        settings.clear();
        for (auto it = originalSettings.cbegin(); it != originalSettings.cend(); ++it) settings.setValue(it.key(), it.value());
        settings.sync();
    }
    void init() {
        db.yankDb();
        db.seedDefaultUser(1, "alex", "Alex Example");
        Settings& s = Settings::instance();
        s.setCurrencySymbol(Settings::CurrencySymbol::EUR);
        s.setDecimalSeparator(Settings::DecimalSeparator::Comma);
        s.setThousandsSeparator(Settings::ThousandsSeparator::Point);
        s.setCurrencySymbolPrefix(false);
        s.setSidebarBudgetSort(Settings::BudgetSort::Percentage);
        s.getQSettings().remove("ui");                   // Expanded sidebar, open sections

        giroId = AccountDAO::createAccount(1, "Girokonto", "Checking", 210000);
        creditId = AccountDAO::createAccount(1, "Kreditkarte", "Checking", -24590);
        QVERIFY(giroId > 0 && creditId > 0);
    }
    void cleanup() { db.close(); }

    void withoutAUserThereIsNothingToRefresh() {
        SidebarWidget sidebar;
        const qsizetype before = sidebar.findChildren<QWidget*>().size();
        sidebar.refresh();
        QCOMPARE(sidebar.findChildren<QWidget*>().size(), before);
    }

    void showsAccountsWithBalances() {
        SidebarWidget sidebar;
        sidebar.setUserId(1);
        QCOMPARE(valueOf(sidebar, "All accounts"), QString("1.854,10 €"));
        QCOMPARE(valueOf(sidebar, "Girokonto"), QString("2.100,00 €"));
        QCOMPARE(valueOf(sidebar, "Kreditkarte"), QString("-245,90 €"));

        // New accounts appear after a refresh
        QVERIFY(AccountDAO::createAccount(1, "Tagesgeld", "Savings", 1000) > 0);
        sidebar.refresh();
        QCOMPARE(valueOf(sidebar, "Tagesgeld"), QString("10,00 €"));

        // The currency settings apply here too
        Settings::instance().setCurrencySymbol(Settings::CurrencySymbol::USD);
        Settings::instance().setCurrencySymbolPrefix(true);
        sidebar.refresh();
        QCOMPARE(valueOf(sidebar, "Kreditkarte"), QString("-$245,90"));
    }

    void clicksRequestAccountsPagesAndActions() {
        SidebarWidget sidebar;
        sidebar.setUserId(1);
        QSignalSpy accounts(&sidebar, &SidebarWidget::accountRequested);
        QSignalSpy addAccount(&sidebar, &SidebarWidget::addAccountRequested);
        QSignalSpy pages(&sidebar, &SidebarWidget::pageRequested);

        item(sidebar, "Kreditkarte")->click();
        item(sidebar, "All accounts")->click();
        QCOMPARE(accounts.count(), 2);
        QCOMPARE(accounts[0].first().toInt(), creditId);
        QCOMPARE(accounts[1].first().toInt(), int(SidebarWidget::ALL_ACCOUNTS_ID));

        item(sidebar, "Add account")->click();
        QCOMPARE(addAccount.count(), 1);

        item(sidebar, "Reports")->click();
        QCOMPARE(pages.count(), 1);
        QCOMPARE(pages.first().first().value<SidebarWidget::Page>(), SidebarWidget::Page::Reports);

        sidebar.setCurrentPage(SidebarWidget::Page::Settings);
        QVERIFY(item(sidebar, "Settings")->isChecked());
        QVERIFY(!item(sidebar, "Reports")->isChecked());
    }

    void showsTheUser() {
        SidebarWidget sidebar;
        sidebar.setUserId(1);
        QCOMPARE(sidebar.findChild<QAbstractButton*>("userItem")->text(), QString("Alex Example"));
    }

    void budgetsShowThisMonthsSpending() {
        const QDate today = QDate::currentDate();
        const QDate monthStart(today.year(), today.month(), 1);
        BudgetDAO budgets;
        QVERIFY(budgets.setMonthlyLimit(1, categoryId("Groceries"), monthStart, 40000));
        QVERIFY(budgets.setLimitForMonth(1, categoryId("Rent"), monthStart.addMonths(-1), 90000));  // Last month
        Transaction shopping(-1, giroId, "Rewe", 31200, today, "", categoryId("Groceries"), "expense");
        QVERIFY(TransactionDAO().addTransaction(shopping) > 0);

        SidebarWidget sidebar;
        sidebar.setUserId(1);
        QCOMPARE(sidebar.findChild<QAbstractButton*>("budgetHeader")->text(), budgetHeaderTitle());
        QCOMPARE(valueOf(sidebar, "Groceries"), QString("312 € / 400 €"));
        QVERIFY(!item(sidebar, "Rent"));                 // Only budgets covering today
    }

    void showsTheMostUsedBudgetsUnlessAllAreWanted() {
        const QDate today = QDate::currentDate();
        const QDate monthStart(today.year(), today.month(), 1);
        CategoryDAO categories(1);
        for (const char* name : {"Transport", "Travel", "Gifts"}) {
            QVERIFY(categories.addCategory(Category(-1, name, "expense")) > 0);
        }
        // Each 100 € a month; spent: Rent 95, Travel 120 (over), Gifts 10, Groceries 60, Utilities 30,
        // Entertainment 0, Transport 80
        const QList<QPair<const char*, Cents>> spending = {{"Rent", 9500}, {"Travel", 12000}, {"Gifts", 1000},
                                                          {"Groceries", 6000}, {"Utilities", 3000},
                                                          {"Entertainment", 0}, {"Transport", 8000}};
        for (const auto& [name, cents] : spending) {
            QVERIFY(BudgetDAO().setMonthlyLimit(1, categoryId(name), monthStart, 10000));
            if (cents > 0) {
                QVERIFY(TransactionDAO().addTransaction(Transaction(-1, giroId, name, cents, today, "", categoryId(name), "expense")) > 0);
            }
        }
        const auto shown = [](SidebarWidget& sidebar) {
            QStringList names;
            for (QAbstractButton* budget : sidebar.findChildren<QAbstractButton*>("budgetItem")) {
                if (!budget->isHidden()) names << budget->text();
            }
            return names;
        };
        const QStringList fullest = {"Travel", "Rent", "Transport", "Groceries", "Utilities", "Gifts", "Entertainment"};

        SidebarWidget sidebar;
        sidebar.setUserId(1);
        sidebar.setAttribute(Qt::WA_DontShowOnScreen);
        sidebar.resize(sidebar.width(), 1400);
        sidebar.show();
        QCoreApplication::processEvents();
        auto* more = sidebar.findChild<QAbstractButton*>("moreBudgetsItem");
        QVERIFY(more);
        auto* scrollBar = sidebar.findChild<QScrollArea*>()->verticalScrollBar();

        // Tall enough: every budget, fullest first, nothing more to link to
        QTRY_COMPARE(shown(sidebar), fullest);
        QVERIFY(more->isHidden());

        // Shorter by three budgets: as many as fit, without scrolling, and the rest behind the link
        auto* area = sidebar.findChild<QScrollArea*>();
        const int overhead = sidebar.height() - area->viewport()->height();
        const int content = area->widget()->sizeHint().height();
        const int row = sidebar.findChild<QAbstractButton*>("budgetItem")->sizeHint().height();
        const int shorter = overhead + content - 3 * row;
        sidebar.resize(sidebar.width(), shorter);
        QTRY_VERIFY(more->isVisible());
        const QStringList some = shown(sidebar);
        QVERIFY2(!some.isEmpty() && some.size() < fullest.size(), qPrintable(some.join(", ")));
        QCOMPARE(some, fullest.mid(0, some.size()));
        QCOMPARE(more->text(), QString("+ %1 more on the Budget page").arg(fullest.size() - some.size()));
        QTRY_COMPARE(scrollBar->maximum(), 0);

        // Folding the accounts makes room for more
        sidebar.findChild<QAbstractButton*>("accountsHeader")->click();
        QTRY_VERIFY(shown(sidebar).size() > some.size());
        QTRY_COMPARE(scrollBar->maximum(), 0);
        sidebar.findChild<QAbstractButton*>("accountsHeader")->click();
        QTRY_COMPARE(shown(sidebar), some);

        // No room at all: only the link
        sidebar.resize(sidebar.width(), 300);
        QTRY_COMPARE(shown(sidebar), QStringList());
        QCOMPARE(more->text(), QString("+ 7 more on the Budget page"));

        // Maximised again: all of them
        sidebar.resize(sidebar.width(), 1400);
        QTRY_COMPARE(shown(sidebar), fullest);
        QVERIFY(more->isHidden());
        sidebar.resize(sidebar.width(), shorter);
        QTRY_VERIFY(more->isVisible());
        QSignalSpy pages(&sidebar, &SidebarWidget::pageRequested);
        more->click();
        QCOMPARE(pages.count(), 1);
        QCOMPARE(pages.first().first().value<SidebarWidget::Page>(), SidebarWidget::Page::Budget);
        // The month's total counts every budget, also while the section is open: 395 of 700 €
        QCOMPARE(sidebar.findChild<QAbstractButton*>("budgetHeader")->accessibleDescription(), QString("56%"));

        // All wanted: every budget, scrolling when they don't fit
        Settings::instance().setSidebarShowingAllBudgets(true);
        sidebar.refresh();
        QTRY_COMPARE(shown(sidebar).size(), 7);
        QVERIFY(!sidebar.findChild<QAbstractButton*>("moreBudgetsItem"));
        QTRY_VERIFY(scrollBar->maximum() > 0);
        Settings::instance().setSidebarShowingAllBudgets(false);
    }

    // Menus block until closed, so choosing the entry is queued first
    static void chooseFromMenu(const QString& text) {
        QTimer::singleShot(0, qApp, [text]() {
            auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            for (QAction* action : menu->actions()) {
                if (action->text() == text) action->trigger();
            }
            menu->close();
        });
    }

    void accountMenusAskForTheRightThing() {
        SidebarWidget sidebar;
        sidebar.setUserId(1);
        sidebar.setAttribute(Qt::WA_DontShowOnScreen);
        sidebar.show();
        QCoreApplication::processEvents();
        QSignalSpy show(&sidebar, &SidebarWidget::accountRequested);
        QSignalSpy correct(&sidebar, &SidebarWidget::correctBalanceRequested);
        QSignalSpy edit(&sidebar, &SidebarWidget::editAccountRequested);
        QSignalSpy remove(&sidebar, &SidebarWidget::deleteAccountRequested);
        const int account = AccountDAO().getAllAccountsForUser(1).first().getAccountId();
        QAbstractButton* row = item(sidebar, AccountDAO().getAccountById(account).getName());
        QVERIFY(row);

        for (const char* entry : {"Show transactions", "Correct balance…", "Edit account…", "Delete account…"}) {
            chooseFromMenu(QString::fromUtf8(entry));
            emit row->customContextMenuRequested(QPoint(2, 2));
        }
        QCOMPARE(show.count(), 1);
        QCOMPARE(show[0][0].toInt(), account);
        QCOMPARE(correct.count(), 1);
        QCOMPARE(edit.count(), 1);
        QCOMPARE(remove.count(), 1);
        QCOMPARE(remove[0][0].toInt(), account);
    }

    void theUserMenuSwitchesUser() {
        SidebarWidget sidebar;
        sidebar.setUserId(1);
        sidebar.setAttribute(Qt::WA_DontShowOnScreen);
        sidebar.show();
        QCoreApplication::processEvents();
        QSignalSpy switchUser(&sidebar, &SidebarWidget::switchUserRequested);
        chooseFromMenu(QString::fromUtf8("Switch user…"));
        sidebar.findChild<QAbstractButton*>("userItem")->click();
        QCOMPARE(switchUser.count(), 1);
    }

    void collapsingAnimatesToTheNarrowWidthAndBack() {
        SidebarWidget sidebar;
        sidebar.setUserId(1);
        sidebar.setAttribute(Qt::WA_DontShowOnScreen);
        sidebar.setCollapsed(false, false);
        sidebar.show();
        QCoreApplication::processEvents();
        const int wide = sidebar.width();
        auto* wordmark = sidebar.findChild<QWidget*>("sidebarWordmark");
        auto* logo = sidebar.findChild<QWidget*>("sidebarLogo");

        sidebar.setCollapsed(true, true);
        QTRY_VERIFY(sidebar.width() < wide);
        QVERIFY(wordmark->isHidden());
        QVERIFY(!logo->isHidden());

        sidebar.setCollapsed(false, true);
        QTRY_COMPARE(sidebar.width(), wide);
        QTRY_VERIFY(!wordmark->isHidden());              // Back only once it's wide again
        QVERIFY(logo->isHidden());
    }

    void budgetsFollowTheChosenSort() {
        const QDate today = QDate::currentDate();
        const QDate monthStart(today.year(), today.month(), 1);
        // Limit and spent: Rent 900/450 (50%), Groceries 400/380 (95%), Utilities 200/50 (25%),
        // Entertainment 50/60 (120%)
        const QList<std::tuple<const char*, Cents, Cents>> budgets = {{"Rent", 90000, 45000},
                                                                      {"Groceries", 40000, 38000},
                                                                      {"Utilities", 20000, 5000},
                                                                      {"Entertainment", 5000, 6000}};
        for (const auto& [name, limit, spent] : budgets) {
            QVERIFY(BudgetDAO().setMonthlyLimit(1, categoryId(name), monthStart, limit));
            QVERIFY(TransactionDAO().addTransaction(Transaction(-1, giroId, name, spent, today, "", categoryId(name), "expense")) > 0);
        }
        const auto shown = [](SidebarWidget& sidebar) {
            QStringList names;
            for (QAbstractButton* budget : sidebar.findChildren<QAbstractButton*>("budgetItem")) names << budget->text();
            return names;
        };
        Settings& settings = Settings::instance();
        SidebarWidget sidebar;
        sidebar.setUserId(1);
        QCOMPARE(shown(sidebar), (QStringList{"Entertainment", "Groceries", "Rent", "Utilities"}));   // Percentage

        settings.setSidebarBudgetSort(Settings::BudgetSort::Spent);
        sidebar.refresh();
        QCOMPARE(shown(sidebar), (QStringList{"Rent", "Groceries", "Entertainment", "Utilities"}));
        settings.setSidebarBudgetSort(Settings::BudgetSort::Budgeted);
        sidebar.refresh();
        QCOMPARE(shown(sidebar), (QStringList{"Rent", "Groceries", "Utilities", "Entertainment"}));

        // The Budget header's right-click menu changes it too, and the sort holds when all are shown
        settings.setSidebarShowingAllBudgets(true);
        QSignalSpy changed(&sidebar, &SidebarWidget::budgetSortChanged);
        QTimer::singleShot(0, &sidebar, [&sidebar]() {
            QMenu* menu = sidebar.findChild<QMenu*>();
            QVERIFY(menu);
            QList<QAction*> actions = menu->actions();
            QCOMPARE(actions.size(), 4);
            QVERIFY(actions[2]->isChecked());                // Budgeted amount, the current one
            actions[3]->trigger();                           // Sort by name
            menu->close();
        });
        auto* header = sidebar.findChild<QAbstractButton*>("budgetHeader");
        emit header->customContextMenuRequested(QPoint(4, 4));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(settings.getSidebarBudgetSort(), Settings::BudgetSort::Name);
        QCOMPARE(shown(sidebar), (QStringList{"Entertainment", "Groceries", "Rent", "Utilities"}));

        settings.setSidebarShowingAllBudgets(false);
        settings.setSidebarBudgetSort(Settings::BudgetSort::Percentage);
    }

    void showsWhenThereAreNoBudgets() {
        SidebarWidget sidebar;
        sidebar.setUserId(1);
        QVERIFY(item(sidebar, "No budgets this month"));
    }

    void foldedSectionsShowASummaryAndAreRemembered() {
        const QDate today = QDate::currentDate();
        const QDate monthStart(today.year(), today.month(), 1);
        QVERIFY(BudgetDAO().setLimitForMonth(1, categoryId("Rent"), monthStart, 100000));
        Transaction rent(-1, giroId, "Landlord", 25000, today, "", categoryId("Rent"), "expense");
        QVERIFY(TransactionDAO().addTransaction(rent) > 0);

        {
            SidebarWidget sidebar;
            sidebar.setUserId(1);
            auto* accountsHeader = sidebar.findChild<QAbstractButton*>("accountsHeader");
            auto* budgetHeader = sidebar.findChild<QAbstractButton*>("budgetHeader");
            QCOMPARE(accountsHeader->accessibleDescription(), QString());   // Open: no summary
            accountsHeader->click();
            budgetHeader->click();
            QCOMPARE(accountsHeader->accessibleDescription(), QString("1.604,10 €"));
            QCOMPARE(budgetHeader->accessibleDescription(), QString("25%"));
            QCOMPARE(Settings::instance().isSidebarSectionExpanded("accounts"), false);
        }

        // A new sidebar (next start) opens folded
        SidebarWidget sidebar;
        sidebar.setUserId(1);
        QCOMPARE(sidebar.findChild<QAbstractButton*>("budgetHeader")->accessibleDescription(), QString("25%"));
    }

    // The wordmark when wide, the logo in the collapsed rail, which has no room for the wordmark
    void showsTheWordmarkAndTheLogoWhenCollapsed() {
        SidebarWidget sidebar;
        sidebar.setUserId(1);
        sidebar.setCollapsed(false, false);
        auto* logo = sidebar.findChild<QLabel*>("sidebarLogo");
        auto* wordmark = sidebar.findChild<QLabel*>("sidebarWordmark");
        QVERIFY(!logo->pixmap().isNull());
        QVERIFY(!wordmark->pixmap().isNull());
        QCOMPARE(wordmark->accessibleName(), QString(APP_NAME));
        QVERIFY(!wordmark->isHidden());
        QVERIFY(logo->isHidden());

        sidebar.setCollapsed(true, false);
        QVERIFY(wordmark->isHidden());
        QVERIFY(!logo->isHidden());
        sidebar.setCollapsed(false, false);
        QVERIFY(!wordmark->isHidden());
        QVERIFY(logo->isHidden());
    }

    // The wordmark starts where the icons of the items below it do (ICON_X into each item)
    void wordmarkLinesUpWithTheIcons() {
        SidebarWidget sidebar;
        sidebar.setUserId(1);
        sidebar.setAttribute(Qt::WA_DontShowOnScreen);
        sidebar.setCollapsed(false, false);
        sidebar.show();
        QCoreApplication::processEvents();
        auto* wordmark = sidebar.findChild<QLabel*>("sidebarWordmark");
        auto* firstItem = sidebar.findChild<QAbstractButton*>("collapseItem");
        const int wordmarkLeft = wordmark->mapTo(&sidebar, wordmark->contentsRect().topLeft()).x();
        QCOMPARE(wordmarkLeft, firstItem->mapTo(&sidebar, QPoint(0, 0)).x() + 9);
    }

    // The brand's own green-grey ink and green leaf in the light theme; a soft off-white and a
    // brighter leaf on the dark sidebar
    void wordmarkFollowsTheTheme() {
        SidebarWidget sidebar;
        sidebar.setUserId(1);
        auto* wordmark = sidebar.findChild<QLabel*>("sidebarWordmark");
        const auto hasColour = [wordmark](const QColor& colour) {
            const QImage image = wordmark->pixmap().toImage();
            for (int y = 0; y < image.height(); ++y)
                for (int x = 0; x < image.width(); ++x)
                    if (image.pixelColor(x, y) == colour) return true;
            return false;
        };
        Theme::applyTheme(Theme::Type::Dark);
        QCoreApplication::processEvents();
        QVERIFY(hasColour(Theme::colors().textSecondary));
        QVERIFY(hasColour(QColor(0x5F, 0xB5, 0x80)));
        Theme::applyTheme(Theme::Type::Light);
        QCoreApplication::processEvents();
        QVERIFY(hasColour(QColor(0x1F, 0x2A, 0x24)));
        QVERIFY(hasColour(QColor(0x3E, 0x8E, 0x5E)));
    }

    // Smaller than the logo, so it doesn't outweigh the page, and centred in the header
    void wordmarkIsSmallerThanTheLogoAndCentred() {
        SidebarWidget sidebar;
        sidebar.setUserId(1);
        sidebar.setAttribute(Qt::WA_DontShowOnScreen);
        sidebar.setCollapsed(false, false);
        sidebar.show();
        QCoreApplication::processEvents();
        auto* wordmark = sidebar.findChild<QLabel*>("sidebarWordmark");
        auto* logo = sidebar.findChild<QLabel*>("sidebarLogo");
        QCOMPARE(wordmark->pixmap().deviceIndependentSize().height(), 24.0);
        QVERIFY(wordmark->height() < logo->sizeHint().height());
        const QWidget* row = wordmark->parentWidget();
        const int above = wordmark->y();
        const int below = row->height() - row->layout()->contentsMargins().bottom() - wordmark->geometry().bottom() - 1;
        QVERIFY2(qAbs(above - below - row->layout()->contentsMargins().top()) <= 1,
                 qPrintable(QString("%1 %2").arg(above).arg(below)));
    }

    // A Beta tag until version 1.0
    void isBetaBeforeVersionOne() {
        QVERIFY(SidebarWidget::isBeta(0));
        QVERIFY(!SidebarWidget::isBeta(1));
        QVERIFY(!SidebarWidget::isBeta(2));
    }

    // Beside the wordmark while this version is a beta, with the full version on hover; gone in
    // the collapsed rail along with the wordmark
    void betaTagBesideTheWordmark() {
        SidebarWidget sidebar;
        sidebar.setUserId(1);
        sidebar.setCollapsed(false, false);
        auto* beta = sidebar.findChild<QLabel*>("sidebarBeta");
        QVERIFY(beta);
        QCOMPARE(beta->text(), QString("Beta"));
        QVERIFY(beta->toolTip().contains(APP_VERSION));
        QCOMPARE(!beta->isHidden(), SidebarWidget::isBeta(APP_VERSION_MAJOR));

        sidebar.setCollapsed(true, false);
        QVERIFY(beta->isHidden());
        sidebar.setCollapsed(false, false);
        QCOMPARE(!beta->isHidden(), SidebarWidget::isBeta(APP_VERSION_MAJOR));
    }

    // Reaches a little below the wordmark's baseline, like a tag beside text, rather than floating
    // above it
    void betaTagReachesTheBaseline() {
        if (!SidebarWidget::isBeta(APP_VERSION_MAJOR)) QSKIP("No Beta tag from version 1.0");
        Theme::applyTheme(Theme::Type::Light);
        SidebarWidget sidebar;
        sidebar.setUserId(1);
        sidebar.setAttribute(Qt::WA_DontShowOnScreen);
        sidebar.setCollapsed(false, false);
        sidebar.show();
        QCoreApplication::processEvents();
        auto* wordmark = sidebar.findChild<QLabel*>("sidebarWordmark");
        auto* beta = sidebar.findChild<QLabel*>("sidebarBeta");
        const QImage image = sidebar.grab().toImage();
        const qreal scale = image.devicePixelRatio();
        const QColor background = Theme::colors().sidebarBackground;
        // The lowest row, in the sidebar's pixels, where something is drawn in the widget's columns
        const auto lowest = [&](QWidget* widget) {
            const QRect area(widget->mapTo(&sidebar, QPoint(0, 0)), widget->size());
            for (int y = int((area.bottom() + 1) * scale) - 1; y >= int(area.top() * scale); --y)
                for (int x = int(area.left() * scale); x < int((area.right() + 1) * scale); ++x)
                    if (image.pixelColor(x, y) != background) return y / scale;
            return -1.0;
        };
        const qreal baseline = lowest(wordmark), tagBottom = lowest(beta);
        QVERIFY2(tagBottom >= baseline && tagBottom <= baseline + 3,
                 qPrintable(QString("baseline %1, tag %2").arg(baseline).arg(tagBottom)));
    }

    // In the leaf's green: lighter on the dark sidebar, darker on the light one
    void betaTagFollowsTheTheme() {
        SidebarWidget sidebar;
        sidebar.setUserId(1);
        auto* beta = sidebar.findChild<QLabel*>("sidebarBeta");
        Theme::applyTheme(Theme::Type::Dark);
        QCoreApplication::processEvents();
        QVERIFY2(beta->styleSheet().contains("#7fd1a0"), qPrintable(beta->styleSheet()));
        Theme::applyTheme(Theme::Type::Light);
        QCoreApplication::processEvents();
        QVERIFY2(beta->styleSheet().contains("#2a6f48"), qPrintable(beta->styleSheet()));
    }

    // Swapping the wordmark for the logo doesn't move the items below
    void collapsingKeepsTheItemsInPlace() {
        SidebarWidget sidebar;
        sidebar.setUserId(1);
        sidebar.setAttribute(Qt::WA_DontShowOnScreen);
        sidebar.setCollapsed(false, false);
        sidebar.show();
        QCoreApplication::processEvents();
        QAbstractButton* dashboard = nullptr;
        for (auto* button : sidebar.findChildren<QAbstractButton*>())
            if (button->text() == "Dashboard") dashboard = button;
        QVERIFY(dashboard);
        const int wideY = dashboard->y();
        sidebar.setCollapsed(true, false);
        QCoreApplication::processEvents();
        QCOMPARE(dashboard->y(), wideY);
    }

    // A thin line between the wordmark and the first item, in the border colour
    void dividerSeparatesTheWordmark() {
        Theme::applyTheme(Theme::Type::Light);
        SidebarWidget sidebar;
        sidebar.setUserId(1);
        sidebar.setAttribute(Qt::WA_DontShowOnScreen);
        sidebar.setCollapsed(false, false);
        sidebar.show();
        QCoreApplication::processEvents();
        const QImage image = sidebar.grab().toImage();
        const qreal scale = image.devicePixelRatio();
        auto* wordmark = sidebar.findChild<QLabel*>("sidebarWordmark");
        const int from = wordmark->mapTo(&sidebar, QPoint(0, wordmark->height())).y();
        const int to = sidebar.findChild<QWidget*>("sidebarBrand")->geometry().bottom() + 1;
        const QColor background = Theme::colors().sidebarBackground;
        const int x = int((sidebar.width() - 20) * scale);                  // Past the wordmark and anything beside it
        int rows = 0;                                                       // In the screen's pixels
        for (int y = int(from * scale); y < int(to * scale); ++y)
            if (image.pixelColor(x, y) != background) ++rows;
        // One line, one pixel thick: a row or two of the screen's, more on high-DPI screens
        QVERIFY2(rows >= 1 && rows <= qCeil(scale) + 1, qPrintable(QString("%1 rows at scale %2").arg(rows).arg(scale)));
    }

    void collapsingIsRemembered() {
        {
            SidebarWidget sidebar;
            sidebar.setUserId(1);
            QVERIFY(!sidebar.isCollapsed());
            auto* collapse = sidebar.findChild<QAbstractButton*>("collapseItem");
            collapse->click();
            QVERIFY(sidebar.isCollapsed());
            QCOMPARE(collapse->text(), QString("Expand"));
            QCOMPARE(Settings::instance().isSidebarCollapsed(), true);
        }
        SidebarWidget sidebar;
        QVERIFY(sidebar.isCollapsed());
        sidebar.setCollapsed(false, false);
        QCOMPARE(Settings::instance().isSidebarCollapsed(), false);
    }
};

QTEST_MAIN(TestSidebarWidget)
#include "test_sidebarwidget.moc"

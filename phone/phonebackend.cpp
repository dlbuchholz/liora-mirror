#include "phonebackend.h"
#include "account.h"
#include "applocale.h"
#include "budget.h"
#include "category.h"
#include "categorycolors.h"
#include "dashboard.h"
#include "settings.h"
#include "themecolors.h"
#include "transaction.h"
#include "transactionlistmodel.h"
#include "user.h"

#include <QGuiApplication>
#include <QJSEngine>
#include <QStyleHints>

namespace {

PhoneBackend* backendInstance = nullptr;

constexpr int SHOWN_CATEGORIES = 4;                  // The rest are summed up as "Other"

} // namespace

PhoneBackend::PhoneBackend(int userId, Qt::ColorScheme scheme, QObject* parent)
    : QObject(parent), userId(userId), scheme(scheme), model(new TransactionListModel(this)) {
    userName = UserDAO().getUserById(userId).getName();
    ThemeColors::setDark(isDark());
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
        ThemeColors::setDark(isDark());
        emit themeChanged();
        loadDashboard();                             // The tag colours depend on the theme
    });
    reload();
}

void PhoneBackend::setInstance(PhoneBackend* backend) {
    backendInstance = backend;
}

PhoneBackend* PhoneBackend::create(QQmlEngine*, QJSEngine*) {
    QJSEngine::setObjectOwnership(backendInstance, QJSEngine::CppOwnership);
    return backendInstance;
}

// The system's light or dark theme, like iOS apps do, unless main() chose one
bool PhoneBackend::isDark() const {
    const Qt::ColorScheme active = scheme != Qt::ColorScheme::Unknown ? scheme : QGuiApplication::styleHints()->colorScheme();
    return active != Qt::ColorScheme::Light;
}

QVariantMap PhoneBackend::theme() const {
    const ThemeColors colors = ThemeColors::current();
    const bool dark = ThemeColors::isDark();
    return {
        {"dark", dark},
        {"window", dark ? QColor(30, 30, 34) : QColor(242, 242, 245)},
        {"card", dark ? QColor(46, 46, 51) : QColor(Qt::white)},
        {"separator", colors.sidebarBorder},
        {"textPrimary", colors.textPrimary},
        {"textSecondary", colors.textSecondary},
        {"textMuted", colors.textMuted},
        {"accent", colors.accent},
        {"onAccent", colors.onAccent},
        {"danger", colors.danger},
        {"positive", colors.positive},
        {"track", colors.track},
    };
}

QVariantMap PhoneBackend::dashboard() const {
    return dashboardData;
}

QAbstractListModel* PhoneBackend::transactions() const {
    return model;
}

QString PhoneBackend::money(qint64 cents, bool showSign) const {
    return Settings::instance().formatCurrency(cents, showSign);
}

QVariantMap PhoneBackend::swatch(const QString& colorName) const {
    const CategoryColors::Swatch swatch = CategoryColors::swatch(colorName);
    return {{"background", swatch.background}, {"text", swatch.text}, {"solid", swatch.solid}};
}

QString PhoneBackend::dayLabel(int row) const {
    const QDate date = model->index(row).data(TransactionListModel::DateRole).toDate();
    const QDate today = QDate::currentDate();
    if (date == today) return tr("Today");
    if (date == today.addDays(-1)) return tr("Yesterday");
    return AppLocale::date(date, date.year() == today.year() ? AppLocale::DateFormat::WeekdayDayMonth
                                                             : AppLocale::DateFormat::DayMonthYear);
}

void PhoneBackend::toggleDay(int row) {
    model->toggleDay(model->index(row).data(TransactionListModel::DateRole).toDate());
}

void PhoneBackend::setSearch(const QString& text) {
    TransactionFilter filter = model->filter();
    filter.search = text;
    model->setFilter(filter);
}

void PhoneBackend::reload() {
    loadDashboard();
    loadTransactions();
}

void PhoneBackend::loadDashboard() {
    const QDate today = QDate::currentDate();
    const DashboardData data = Dashboard::load(userId, today);

    QVariantList categories;
    Cents shownCents = 0;
    Cents totalCents = 0;
    for (const CategorySpending& spending : data.spendingByCategory) {
        totalCents += spending.spentCents;
    }
    for (const CategorySpending& spending : data.spendingByCategory) {
        if (categories.size() == SHOWN_CATEGORIES) break;
        shownCents += spending.spentCents;
        categories.append(QVariantMap{
            {"name", spending.categoryId == 0 ? tr("No category") : displayCategoryName(spending.categoryName)},
            {"color", spending.categoryColor},
            {"amount", money(spending.spentCents)},
            {"share", totalCents > 0 ? double(spending.spentCents) / double(totalCents) : 0.0},
        });
    }
    if (totalCents > shownCents) {
        categories.append(QVariantMap{
            {"name", tr("Other")},
            {"color", QString()},
            {"amount", money(totalCents - shownCents)},
            {"share", double(totalCents - shownCents) / double(totalCents)},
        });
    }

    QVariantList budgets;
    for (const MonthBudget& budget : BudgetDAO().budgetsForMonth(userId, today)) {
        if (budget.limitCents <= 0) continue;
        budgets.append(QVariantMap{
            {"name", displayCategoryName(budget.categoryName)},
            {"color", budget.categoryColor},
            {"spent", money(budget.spentCents)},
            {"limit", money(budget.limitCents)},
            {"progress", qMin(1.0, double(budget.spentCents) / double(budget.limitCents))},
            {"over", budget.spentCents > budget.limitCents},
        });
    }

    const int accountCount = AccountDAO().getAllAccountsForUser(userId).size();
    dashboardData = {
        {"greeting", Dashboard::greeting(QTime::currentTime()) + ", " + userName},
        {"balance", money(data.totalBalanceCents)},
        {"accounts", AppLocale::count(accountCount, tr("%1 account"), tr("%1 accounts"))},
        {"month", AppLocale::monthName(today.month(), QLocale::ShortFormat)},
        {"income", money(data.thisMonth.incomeCents, true)},
        {"spent", money(data.thisMonth.expenseCents)},
        {"categories", categories},
        {"budgets", budgets},
        {"daysLeft", AppLocale::count(data.daysLeftInMonth, tr("%1 day left"), tr("%1 days left"))},
    };
    emit dataChanged();
}

void PhoneBackend::loadTransactions() {
    AccountDAO accountDao;
    QHash<int, QString> accountNames;
    QList<Transaction> transactions;
    TransactionDAO transactionDao;
    for (const Account& account : accountDao.getAllAccountsForUser(userId)) {
        accountNames.insert(account.getAccountId(), account.getName());
        transactions.append(transactionDao.getTransactionsForAccount(account.getAccountId()));
    }

    QHash<int, QString> categoryNames;
    QHash<int, QString> categoryColors;
    for (const Category& category : CategoryDAO(userId).getAllCategories()) {
        categoryNames.insert(category.getCategoryId(), category.getDisplayName());
        categoryColors.insert(category.getCategoryId(), category.getColor());
    }

    model->setCategoryColors(categoryColors);
    model->setTransactions(transactions, accountNames, categoryNames);
}

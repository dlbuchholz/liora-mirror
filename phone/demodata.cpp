#include "demodata.h"
#include "account.h"
#include "budget.h"
#include "category.h"
#include "transaction.h"
#include "user.h"

#include <QCoreApplication>
#include <QDate>
#include <QHash>

namespace DemoData {

namespace {

// Something that happens every month on the same day
struct Entry {
    int day;
    const char* payee;
    Cents cents;
    const char* category;
    bool varies = true;                              // Everyday spending changes a little each month
    bool income = false;
};

const QList<Entry>& monthlyEntries() {
    static const QList<Entry> entries = {
        {1, "Employer GmbH", 245000, "Salary", false, true},
        {3, "Hausverwaltung Meyer", 55000, "Rent", false},
        {10, "Stadtwerke", 8900, "Utilities"},
        {15, "Spotify", 1099, "Entertainment", false},
        {2, "REWE", 4120, "Groceries"},
        {9, "Aldi", 2865, "Groceries"},
        {16, "REWE", 5310, "Groceries"},
        {23, "Bakery", 860, "Groceries"},
        {27, "REWE", 3790, "Groceries"},
        {6, "Deutsche Bahn", 4900, "Transport"},
        {20, "Shell", 6150, "Transport"},
        {12, "Pizzeria Roma", 3400, "Eating out"},
        {25, "Café Kranz", 1280, "Eating out"},
        {18, "Cinema", 2400, "Entertainment"},
    };
    return entries;
}

} // namespace

int create() {
    UserDAO userDao;
    if (!userDao.createUser(User(0, "demo", "Alex"))) {
        return -1;
    }
    const int userId = userDao.findUserIdByUsername("demo");

    // By English name. The starters are named in the language of the texts (StarterNames), the
    // built-in ones stay English
    CategoryDAO categoryDao(userId);
    QHash<QString, int> idsByName;
    for (const Category& category : categoryDao.getAllCategories()) {
        idsByName.insert(category.getName(), category.getCategoryId());
    }
    QHash<QString, int> categoryIds;
    categoryIds.insert(InitialBalanceCategory, idsByName.value(InitialBalanceCategory));
    for (const char* starter : {"Salary", "Groceries", "Rent", "Utilities", "Entertainment"}) {
        categoryIds.insert(starter, idsByName.value(QCoreApplication::translate("Category", starter)));
    }
    categoryIds.insert("Transport",
                       categoryDao.addCategory(Category(-1, QCoreApplication::translate("DemoData", "Transport"),
                                                              "expense", "pink")));
    categoryIds.insert("Eating out",
                       categoryDao.addCategory(Category(-1, QCoreApplication::translate("DemoData", "Eating out"),
                                                              "expense", "red")));

    const int checking = AccountDAO::createAccount(userId, QCoreApplication::translate("DemoData", "Checking account"),
                                                  QCoreApplication::translate("Account", "Checking"), 0);
    const int savings = AccountDAO::createAccount(userId, QCoreApplication::translate("DemoData", "Savings"),
                                                  QCoreApplication::translate("Account", "Savings"), 0);
    if (checking <= 0 || savings <= 0) {
        return -1;
    }

    const QDate today = QDate::currentDate();
    const QDate firstMonth = today.addMonths(-3);
    const QDate start(firstMonth.year(), firstMonth.month(), 1);
    TransactionDAO transactionDao;
    const int initialId = categoryIds.value(InitialBalanceCategory);
    transactionDao.addTransaction(Transaction(-1, checking, "", 182000, start, QCoreApplication::translate("Account", "Initial deposit"), initialId, "income"));
    transactionDao.addTransaction(Transaction(-1, savings, "", 350000, start, QCoreApplication::translate("Account", "Initial deposit"), initialId, "income"));

    for (int month = 0; month <= 3; ++month) {
        const QDate first = start.addMonths(month);
        for (const Entry& entry : monthlyEntries()) {
            const QDate date(first.year(), first.month(), qMin(entry.day, first.daysInMonth()));
            if (date > today) {
                continue;
            }
            const Cents cents = !entry.varies ? entry.cents : entry.cents + (month * 37 + entry.day * 13) % 900 - 450;
            transactionDao.addTransaction(Transaction(-1, checking, entry.payee, cents, date,
                                                      "", categoryIds.value(entry.category),
                                                      entry.income ? "income" : "expense"));
        }
    }

    BudgetDAO budgetDao;
    budgetDao.setMonthlyLimit(userId, categoryIds.value("Groceries"), start, 20000);
    budgetDao.setMonthlyLimit(userId, categoryIds.value("Eating out"), start, 6000);
    budgetDao.setMonthlyLimit(userId, categoryIds.value("Transport"), start, 10000);
    return userId;
}

} // namespace DemoData

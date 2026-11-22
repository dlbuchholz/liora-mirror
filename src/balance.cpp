#include "balance.h"
#include "category.h"
#include "csvimport.h"
#include "db/databasemanager.h"
#include "transaction.h"

#include <QMap>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>

namespace Balance {

Cents onDate(int accountId, const QDate& date) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("SELECT COALESCE(SUM(CASE WHEN type = 'income' THEN amount ELSE -amount END), 0) "
                  "FROM transactions WHERE account_id = :account_id AND date <= :date");
    query.bindValue(":account_id", accountId);
    query.bindValue(":date", date.toString("yyyy-MM-dd"));
    if (!query.exec() || !query.next()) {
        qWarning() << "Balance::onDate:" << query.lastError().text();
        return 0;
    }
    return query.value(0).toLongLong();
}

int addCorrection(int accountId, const QDate& date, Cents differenceCents, const QString& note) {
    if (differenceCents == 0) {
        return 0;
    }
    QSqlQuery query(DatabaseManager::instance().database());
    // The account's user's own category
    query.prepare("SELECT c.id FROM categories c JOIN accounts a ON a.user_id = c.user_id "
                  "WHERE a.id = :account AND c.name = :name");
    query.bindValue(":account", accountId);
    query.bindValue(":name", BalanceCorrectionCategory);
    const int categoryId = query.exec() && query.next() ? query.value(0).toInt() : 0;   // No category if it was deleted

    const Transaction correction(-1, accountId, BalanceCorrectionCategory, qAbs(differenceCents), date, note, categoryId,
                                 differenceCents > 0 ? "income" : "expense");
    return TransactionDAO().addTransaction(correction);
}

namespace {

// The account's corrections on a day, with what each moves the balance by
QList<QPair<int, Cents>> correctionsOn(int accountId, const QDate& date) {
    QSqlQuery query(DatabaseManager::instance().database());
    query.prepare("SELECT t.id, CASE WHEN t.type = 'income' THEN t.amount ELSE -t.amount END FROM transactions t "
                  "JOIN categories c ON c.id = t.category_id "
                  "WHERE t.account_id = :account AND t.date = :date AND c.name = :name ORDER BY t.id");
    query.bindValue(":account", accountId);
    query.bindValue(":date", date.toString("yyyy-MM-dd"));
    query.bindValue(":name", BalanceCorrectionCategory);
    QList<QPair<int, Cents>> found;
    if (!query.exec()) {
        qWarning() << "Balance::correctionsOn:" << query.lastError().text();   // GCOVR_EXCL_LINE
        return found;                                                         // GCOVR_EXCL_LINE
    }
    while (query.next()) found.append({query.value(0).toInt(), query.value(1).toLongLong()});
    return found;
}

} // namespace

QList<PlannedCorrection> planCorrections(int accountId, const QList<Entry>& entries) {
    QMap<QDate, Cents> byDate;                           // In date order; a later entry for a date wins
    for (const Entry& entry : entries) {
        if (entry.date.isValid()) byDate.insert(entry.date, entry.balanceCents);
    }
    QList<PlannedCorrection> plan;
    Cents shift = 0;                                     // What the earlier entries change: new corrections less the old ones
    for (auto it = byDate.cbegin(); it != byDate.cend(); ++it) {
        PlannedCorrection planned;
        planned.date = it.key();
        planned.balanceCents = it.value();
        Cents old = 0;
        for (const auto& [id, cents] : correctionsOn(accountId, it.key())) {
            planned.replacedIds.append(id);
            old += cents;
        }
        planned.beforeCents = onDate(accountId, it.key()) - old + shift;
        shift += planned.correctionCents() - old;
        plan.append(planned);
    }
    return plan;
}

int applyCorrections(int accountId, const QList<PlannedCorrection>& plan, const QString& note) {
    int added = 0;
    for (const PlannedCorrection& planned : plan) {
        if (!planned.replacedIds.isEmpty() && !TransactionDAO().deleteTransactions(planned.replacedIds)) return -1;
        if (planned.correctionCents() == 0) continue;
        if (addCorrection(accountId, planned.date, planned.correctionCents(), note) <= 0) return -1;
        ++added;
    }
    return added;
}

QList<Entry> parseEntries(const QString& text, QChar decimalSeparator, int* skipped) {
    QList<Entry> entries;
    int unreadable = 0;
    const QStringList lines = text.split(QRegularExpression("\\r?\\n"), Qt::SkipEmptyParts);
    for (int i = 0; i < lines.size(); ++i) {
        const QString line = lines[i].trimmed();
        if (line.isEmpty()) continue;
        // A tab or semicolon splits the columns, and more columns after the balance don't matter;
        // without either, the first space does, and the rest is the balance ("31.08.2026 10 420,00")
        const QChar column = line.contains('\t') ? '\t' : line.contains(';') ? ';' : ' ';
        const qsizetype at = line.indexOf(column);
        const QString dateText = at > 0 ? line.left(at) : line;
        QString amountText = at > 0 ? line.mid(at + 1) : QString();
        if (column != ' ') amountText = amountText.section(column, 0, 0);
        const QDate date = CsvImport::parseDate(dateText);
        bool ok = false;
        const Cents cents = CsvImport::parseAmount(amountText, decimalSeparator, &ok);
        if (!date.isValid() || !ok) {
            if (i > 0 || !entries.isEmpty()) ++unreadable;   // The first line may be the headings
            continue;
        }
        entries.append({date, cents});
    }
    if (skipped) *skipped = unreadable;
    return entries;
}

} // namespace Balance

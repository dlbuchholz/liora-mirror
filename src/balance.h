#ifndef BALANCE_H
#define BALANCE_H

#include <QDate>
#include <QList>
#include <QString>
#include "money.h"

// Account balances on a given day and corrections to match the bank's balance
namespace Balance {

// Sum of all transactions of the account up to and including the date
Cents onDate(int accountId, const QDate& date);

// Adds a "Balance correction" transaction so the balance on the date changes by differenceCents.
// Returns the new transaction's ID, -1 on failure; does nothing (and returns 0) for a zero difference.
int addCorrection(int accountId, const QDate& date, Cents differenceCents, const QString& note);

// What an account was worth at the end of a day, as the user knows it (a depot's value, say)
struct Entry {
    QDate date;
    Cents balanceCents = 0;
};

// The correction one entry needs. Entries are taken in date order; corrections already on an
// entry's date are replaced, so entering a month again doesn't add up.
struct PlannedCorrection {
    QDate date;
    Cents balanceCents = 0;                          // As entered
    Cents beforeCents = 0;                           // Without this day's old corrections, after the earlier entries
    QList<int> replacedIds;                          // The day's old corrections
    Cents correctionCents() const { return balanceCents - beforeCents; }
    // Something to save: a new correction, or old ones to take away
    bool changesSomething() const { return correctionCents() != 0 || !replacedIds.isEmpty(); }
};

// One per date, in date order; for the same date twice, the later entry counts
QList<PlannedCorrection> planCorrections(int accountId, const QList<Entry>& entries);

// Replaces each date's old corrections with the planned one. Returns how many corrections were
// added, -1 if something couldn't be saved.
int applyCorrections(int accountId, const QList<PlannedCorrection>& plan, const QString& note);

// Dates and balances from text, one per line: two columns split by a tab, a semicolon or spaces,
// as copied from a spreadsheet. Lines that can't be read are counted in skipped; a first line that
// can't be read is taken for a header and isn't.
QList<Entry> parseEntries(const QString& text, QChar decimalSeparator, int* skipped = nullptr);

} // namespace Balance

#endif // BALANCE_H

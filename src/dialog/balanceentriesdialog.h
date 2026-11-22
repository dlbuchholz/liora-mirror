#ifndef BALANCEENTRIESDIALOG_H
#define BALANCEENTRIESDIALOG_H

#include <QDialog>
#include "balance.h"

class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;

// Many balances of one account at once, typed in or pasted from a spreadsheet: a depot's value at
// the end of each month, say. Each one becomes a "Balance correction" for the difference, replacing
// any correction already on that day.
class BalanceEntriesDialog : public QDialog {
    Q_OBJECT

public:
    BalanceEntriesDialog(int userId, int accountId, QWidget* parent = nullptr);

    // Dates and balances from text, one per line, as copied from a spreadsheet (also Ctrl+V in the table)
    void pasteText(const QString& text);
    int addedCount() const { return added; }         // After accept()

private:
    enum Column { DateColumn, BalanceColumn, BeforeColumn, CorrectionColumn };

    bool eventFilter(QObject* watched, QEvent* event) override;
    void addRow(const QString& date = QString(), const QString& balance = QString());
    void keepOneEmptyRow();
    void updatePlan();
    void save();

    QList<Balance::PlannedCorrection> plan;
    int added = 0;
    int skippedLines = 0;                            // By the last paste
    bool updating = false;

    QComboBox* accountCombo = nullptr;
    QTableWidget* table = nullptr;
    QLabel* statusLabel = nullptr;
    QPushButton* saveButton = nullptr;
};

#endif // BALANCEENTRIESDIALOG_H

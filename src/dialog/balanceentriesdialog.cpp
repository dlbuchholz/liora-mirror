#include "balanceentriesdialog.h"
#include "appname.h"
#include "applocale.h"
#include "account.h"
#include "csvimport.h"
#include "settings.h"
#include "theme.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

QChar decimalSeparator() {
    return Settings::instance().getDecimalSeparator() == Settings::DecimalSeparator::Comma ? ',' : '.';
}

// "1.234,50" or "-95.00": as typed, without the currency, so it reads back the same
QString plainAmount(Cents cents) {
    return (cents < 0 ? "-" : "") + Settings::instance().formatNumber(cents);
}

} // namespace

BalanceEntriesDialog::BalanceEntriesDialog(int userId, int accountId, QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Enter balances"));
    setMinimumSize(600, 480);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    auto* intro = new QLabel(tr("Enter what the account was worth at the end of each day, or paste two columns, dates and "
                                "balances, from a spreadsheet. Each difference becomes a balance correction: it counts "
                                "towards the balance, not towards income or spending. A day that already has a correction "
                                "gets the new one instead."), this);
    intro->setWordWrap(true);
    intro->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(intro);

    auto* form = new QFormLayout();
    accountCombo = new QComboBox(this);
    accountCombo->setObjectName("entriesAccountCombo");
    for (const Account& account : AccountDAO().getAllAccountsForUser(userId)) {
        accountCombo->addItem(account.getName(), account.getAccountId());
    }
    accountCombo->setCurrentIndex(qMax(0, accountCombo->findData(accountId)));
    form->addRow(tr("Account"), accountCombo);
    layout->addLayout(form);

    table = new QTableWidget(0, 4, this);
    table->setObjectName("entriesTable");
    table->setHorizontalHeaderLabels({tr("Date"), tr("Balance at day's end"), tr("In %1 now").arg(APP_NAME), tr("Correction")});
    table->horizontalHeaderItem(BalanceColumn)->setToolTip(tr("What the account was worth at the end of that day"));
    table->horizontalHeaderItem(BeforeColumn)->setToolTip(tr("The balance %1 shows for the end of that day, without a "
                                                             "correction already on it and with the rows above").arg(APP_NAME));
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    for (const int column : {BalanceColumn, BeforeColumn, CorrectionColumn}) {
        table->horizontalHeaderItem(column)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
    table->verticalHeader()->hide();
    table->verticalHeader()->setDefaultSectionSize(Theme::controlHeight);
    table->setShowGrid(false);                       // Rows apart by a hairline, like the app's lists
    table->setFrameShape(QFrame::NoFrame);
    table->setStyleSheet(QString("QTableWidget { background: transparent; } "
                                 "QTableWidget::item { border-bottom: 1px solid %1; padding: 0 6px; } "
                                 "QHeaderView::section { background: transparent; border: none; border-bottom: 1px solid %1; "
                                 "padding: 4px 6px; color: %2; }")
                             .arg(Theme::hairline(Theme::isDark()).name(QColor::HexArgb), Theme::colors().textSecondary.name()));
    table->setSelectionMode(QAbstractItemView::ContiguousSelection);
    table->installEventFilter(this);
    layout->addWidget(table, 1);

    // How to fill it in, and the paste button beside it
    auto* pasteRow = new QHBoxLayout();
    auto* hint = new QLabel(tr("Dates like %1, one balance a day. Paste with Ctrl+V or:")
                                .arg(QDate::currentDate().toString("dd.MM.yyyy")), this);
    hint->setObjectName("entriesHint");
    hint->setForegroundRole(QPalette::PlaceholderText);
    pasteRow->addWidget(hint, 1);
    auto* pasteButton = new QPushButton(tr("Paste from spreadsheet"), this);
    pasteButton->setObjectName("pasteEntriesButton");
    pasteButton->setAutoDefault(false);
    connect(pasteButton, &QPushButton::clicked, this, [this]() { pasteText(QApplication::clipboard()->text()); });
    pasteRow->addWidget(pasteButton);
    layout->addLayout(pasteRow);

    statusLabel = new QLabel(this);
    statusLabel->setObjectName("entriesStatusLabel");
    statusLabel->setWordWrap(true);
    layout->addWidget(statusLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    saveButton = buttons->addButton(tr("Add corrections"), QDialogButtonBox::AcceptRole);
    saveButton->setObjectName("saveEntriesButton");
    saveButton->setDefault(true);
    connect(saveButton, &QPushButton::clicked, this, &BalanceEntriesDialog::save);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(accountCombo, &QComboBox::currentIndexChanged, this, &BalanceEntriesDialog::updatePlan);
    connect(table, &QTableWidget::itemChanged, this, [this]() {
        if (updating) return;
        skippedLines = 0;
        keepOneEmptyRow();
        updatePlan();
    });
    keepOneEmptyRow();
    updatePlan();
}

void BalanceEntriesDialog::addRow(const QString& date, const QString& balance) {
    const int row = table->rowCount();
    table->insertRow(row);
    table->setItem(row, DateColumn, new QTableWidgetItem(date));
    auto* balanceItem = new QTableWidgetItem(balance);
    balanceItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    table->setItem(row, BalanceColumn, balanceItem);
    for (const int column : {BeforeColumn, CorrectionColumn}) {
        auto* shown = new QTableWidgetItem();
        shown->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);   // Worked out, not typed
        shown->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table->setItem(row, column, shown);
    }
}

// Always room for one more at the bottom
void BalanceEntriesDialog::keepOneEmptyRow() {
    const auto isEmpty = [this](int row) {
        return table->item(row, DateColumn)->text().trimmed().isEmpty() && table->item(row, BalanceColumn)->text().trimmed().isEmpty();
    };
    const bool wasUpdating = updating;
    updating = true;
    if (table->rowCount() == 0 || !isEmpty(table->rowCount() - 1)) addRow();
    updating = wasUpdating;
}

void BalanceEntriesDialog::pasteText(const QString& text) {
    const QList<Balance::Entry> entries = Balance::parseEntries(text, decimalSeparator(), &skippedLines);
    updating = true;
    while (table->rowCount() > 0 && table->item(table->rowCount() - 1, DateColumn)->text().trimmed().isEmpty()
           && table->item(table->rowCount() - 1, BalanceColumn)->text().trimmed().isEmpty()) {
        table->removeRow(table->rowCount() - 1);     // The empty ones go below the pasted rows
    }
    for (const Balance::Entry& entry : entries) addRow(entry.date.toString("dd.MM.yyyy"), plainAmount(entry.balanceCents));
    updating = false;
    keepOneEmptyRow();
    updatePlan();
}

bool BalanceEntriesDialog::eventFilter(QObject* watched, QEvent* event) {
    if (watched == table && event->type() == QEvent::KeyPress && static_cast<QKeyEvent*>(event)->matches(QKeySequence::Paste)) {
        pasteText(QApplication::clipboard()->text());
        return true;
    }
    return QDialog::eventFilter(watched, event);
}

void BalanceEntriesDialog::updatePlan() {
    const Settings& settings = Settings::instance();
    const Theme::Colors colors = Theme::colors();
    const int accountId = accountCombo->currentData().toInt();

    // Read each row; a row that can't be read says why, and nothing is saved until it can
    QList<Balance::Entry> entries;
    QList<int> entryRows;
    int unreadable = 0;
    updating = true;
    for (int row = 0; row < table->rowCount(); ++row) {
        const QString dateText = table->item(row, DateColumn)->text().trimmed();
        const QString balanceText = table->item(row, BalanceColumn)->text().trimmed();
        QTableWidgetItem* before = table->item(row, BeforeColumn);
        QTableWidgetItem* correction = table->item(row, CorrectionColumn);
        before->setText(QString());
        correction->setText(QString());
        correction->setToolTip(QString());
        correction->setForeground(colors.danger);
        if (dateText.isEmpty() && balanceText.isEmpty()) continue;
        const QDate date = CsvImport::parseDate(dateText);
        bool ok = false;
        const Cents cents = CsvImport::parseAmount(balanceText, decimalSeparator(), &ok);
        if (!date.isValid()) {
            correction->setText(tr("Which day?"));
            ++unreadable;
        } else if (balanceText.isEmpty() || !ok) {
            correction->setText(tr("Which balance?"));
            ++unreadable;
        } else {
            entries.append({date, cents});
            entryRows.append(row);
        }
    }

    plan = accountCombo->count() > 0 ? Balance::planCorrections(accountId, entries) : QList<Balance::PlannedCorrection>();
    int adds = 0;
    int replaces = 0;
    for (int i = 0; i < entries.size(); ++i) {
        const int row = entryRows[i];
        const auto planned = std::find_if(plan.cbegin(), plan.cend(), [&](const Balance::PlannedCorrection& p) { return p.date == entries[i].date; });
        QTableWidgetItem* correction = table->item(row, CorrectionColumn);
        // The same day further down wins
        const bool later = std::any_of(entries.cbegin() + i + 1, entries.cend(), [&](const Balance::Entry& e) { return e.date == entries[i].date; });
        if (later || planned == plan.cend()) {
            correction->setText(tr("Same day below"));
            correction->setForeground(colors.textMuted);
            continue;
        }
        table->item(row, BeforeColumn)->setText(settings.formatCurrency(planned->beforeCents));
        if (planned->correctionCents() == 0) {
            correction->setText(tr("Matches"));
            correction->setForeground(colors.textMuted);
        } else {
            correction->setText(settings.formatCurrency(planned->correctionCents(), true));
            correction->setForeground(planned->correctionCents() > 0 ? colors.positive : colors.danger);
            ++adds;
        }
        if (!planned->replacedIds.isEmpty()) {
            correction->setToolTip(tr("Replaces the correction already on this day"));
            ++replaces;
        }
    }
    updating = false;

    QStringList status;
    QColor color = colors.textSecondary;
    if (accountCombo->count() == 0) {
        status.append(tr("Add an account first."));
    } else if (unreadable > 0) {
        status.append(AppLocale::count(unreadable, tr("%1 row can't be read yet."), tr("%1 rows can't be read yet.")));
        color = colors.danger;
    } else if (entries.isEmpty()) {
        status.append(tr("Enter each day's balance as it was at the end of that day."));
    } else if (adds == 0 && replaces == 0) {
        status.append(tr("%1 already shows these balances. Nothing to correct.").arg(APP_NAME));
        color = colors.positive;
    } else if (adds == 0) {
        status.append(AppLocale::count(replaces, tr("Takes away %1 correction that's no longer needed."),
                                       tr("Takes away %1 corrections that are no longer needed.")));
        color = colors.textPrimary;
    } else {
        status.append(AppLocale::count(adds, tr("Adds %1 correction."), tr("Adds %1 corrections.")));
        if (replaces > 0) {
            status.append(AppLocale::count(replaces, tr("Replaces %1 correction already there."),
                                           tr("Replaces %1 corrections already there.")));
        }
        color = colors.textPrimary;
    }
    if (skippedLines > 0) {
        status.append(AppLocale::count(skippedLines, tr("%1 pasted line couldn't be read and was left out."),
                                       tr("%1 pasted lines couldn't be read and were left out.")));
    }
    statusLabel->setText(status.join(' '));
    QPalette palette = statusLabel->palette();
    palette.setColor(QPalette::WindowText, color);
    statusLabel->setPalette(palette);
    const bool changes = std::any_of(plan.cbegin(), plan.cend(), [](const Balance::PlannedCorrection& p) { return p.changesSomething(); });
    saveButton->setEnabled(unreadable == 0 && changes);
}

void BalanceEntriesDialog::save() {
    if (!saveButton->isEnabled()) return;           // GCOVR_EXCL_LINE: Enter on a disabled default button
    added = Balance::applyCorrections(accountCombo->currentData().toInt(), plan, tr("Entered as a balance"));
    if (added < 0) {
        statusLabel->setText(tr("The corrections couldn't all be saved."));   // GCOVR_EXCL_LINE: only when the database fails
        return;                                                               // GCOVR_EXCL_LINE
    }
    accept();
}

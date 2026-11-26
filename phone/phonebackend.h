#ifndef PHONEBACKEND_H
#define PHONEBACKEND_H

#include <QAbstractListModel>
#include <QObject>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;
class TransactionListModel;

// What the QML pages read from LioraCore, as "Backend" in QML: the colours, the dashboard
// numbers and the transaction list of one user. Amounts reach QML already formatted.
class PhoneBackend : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Backend)
    QML_SINGLETON
    Q_PROPERTY(QVariantMap theme READ theme NOTIFY themeChanged)
    Q_PROPERTY(QVariantMap dashboard READ dashboard NOTIFY dataChanged)
    Q_PROPERTY(QAbstractListModel* transactions READ transactions CONSTANT)

public:
    // scheme: Light or Dark to ignore the system's, Unknown to follow it
    PhoneBackend(int userId, Qt::ColorScheme scheme, QObject* parent = nullptr);

    // QML gets the one main() made
    static void setInstance(PhoneBackend* backend);
    static PhoneBackend* create(QQmlEngine*, QJSEngine*);

    QVariantMap theme() const;
    QVariantMap dashboard() const;
    QAbstractListModel* transactions() const;

    Q_INVOKABLE QString money(qint64 cents, bool showSign = false) const;
    Q_INVOKABLE QVariantMap swatch(const QString& colorName) const;   // background, text, solid
    Q_INVOKABLE QString dayLabel(int row) const;     // "Today", "Yesterday", "Mon, 20 Oct"
    Q_INVOKABLE void toggleDay(int row);
    Q_INVOKABLE void setSearch(const QString& text);
    Q_INVOKABLE void reload();

signals:
    void themeChanged();
    void dataChanged();

private:
    bool isDark() const;
    void loadDashboard();
    void loadTransactions();

    int userId;
    Qt::ColorScheme scheme;
    QString userName;
    QVariantMap dashboardData;
    TransactionListModel* model;
};

#endif // PHONEBACKEND_H

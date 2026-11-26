// LioraPhone: a QML phone interface on LioraCore. On the desktop it opens in a
// phone sized window; on iOS it fills the screen.

#include "appname.h"
#include "demodata.h"
#include "language.h"
#include "phonebackend.h"
#include "settings.h"
#include "starternames.h"
#include "user.h"
#include "db/databasemanager.h"
#include "db/migrationmanager.h"

#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTimer>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName(APP_NAME "Phone");       // Its own data folder, apart from the desktop app's
    app.setApplicationDisplayName(APP_NAME);
    QQuickStyle::setStyle("Basic");                  // The style meant for drawing your own controls

    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption databaseOption(
        "database", "Open this " APP_NAME " database instead of the demo one. Use a copy: it may be updated to the current version.",
        "file");
    parser.addOption(databaseOption);
    const QCommandLineOption schemeOption("color-scheme", "Use \"light\" or \"dark\" instead of the system's.", "scheme");
    parser.addOption(schemeOption);
    const QCommandLineOption languageOption("language", "Show the texts in this language (\"en\", \"de\") instead of the desktop app's.", "code");
    parser.addOption(languageOption);
    const QCommandLineOption tabOption("tab", "Start on this tab (0 = Overview, 1 = Transactions, …).", "index", "0");
    parser.addOption(tabOption);
    const QCommandLineOption screenshotOption(
        "screenshot", "Save a picture of the window to this file and quit (with QT_QPA_PLATFORM=offscreen no window shows).",
        "file");
    parser.addOption(screenshotOption);
    parser.process(app);

    // The desktop app's language setting (the same settings), before any text is made
    Language::install(Language::resolve(parser.isSet(languageOption) ? parser.value(languageOption) : Settings::instance().getLanguage()));

    Qt::ColorScheme scheme = Qt::ColorScheme::Unknown;
    if (parser.isSet(schemeOption)) {
        scheme = parser.value(schemeOption) == "light" ? Qt::ColorScheme::Light : Qt::ColorScheme::Dark;
    }

    // Without --database: a demo database of its own, filled on first start
    const bool demo = !parser.isSet(databaseOption);
    QString path = parser.value(databaseOption);
    if (demo) {
        const QString folder = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(folder);
        path = folder + "/demo.db";
    } else if (!QFileInfo::exists(path)) {
        qCritical() << "No such database:" << path;
        return 1;
    }

    if (!DatabaseManager::instance().openDatabase(path) || !MigrationManager(BuiltInMigrations).applyMigrations()) {
        qCritical() << "Couldn't open the database" << path;
        return 1;
    }
    StarterNames::followLanguage();                  // starter names in the language

    // The first user by name
    const QList<User> users = UserDAO().getAllUsers();
    const int userId = !users.isEmpty() ? users.first().getUserId() : demo ? DemoData::create() : -1;
    if (userId < 0) {
        qCritical() << "The database has no user";
        return 1;
    }
    qInfo() << "Database:" << path;

    PhoneBackend backend(userId, scheme);
    PhoneBackend::setInstance(&backend);

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.setInitialProperties({{"startTab", parser.value(tabOption).toInt()}});
    engine.loadFromModule("LioraPhone", "Main");

    if (parser.isSet(screenshotOption)) {
        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0));
        if (!window) return 1;
        // A moment after the first frame, so images and layouts have settled
        QObject::connect(window, &QQuickWindow::frameSwapped, window, [window, file = parser.value(screenshotOption)] {
            QTimer::singleShot(500, window, [window, file] {
                const bool saved = window->grabWindow().save(file);
                QCoreApplication::exit(saved ? 0 : 1);
            });
        }, Qt::SingleShotConnection);
    }
    return app.exec();
}

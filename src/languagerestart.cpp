#include "languagerestart.h"
#include "appname.h"

#include <QApplication>
#include <QProcess>

namespace Language {

QString restartProgram(const QString& applicationFilePath, const QString& appImage, const QString& appDir) {
    const bool insideAppImage = !appImage.isEmpty() && !appDir.isEmpty() && applicationFilePath.startsWith(appDir + '/');
    return insideAppImage ? appImage : applicationFilePath;
}

bool restartApplication() {
    const QString program = restartProgram(QCoreApplication::applicationFilePath(), qEnvironmentVariable("APPIMAGE"),
                                           qEnvironmentVariable("APPDIR"));
    if (!QProcess::startDetached(program, QCoreApplication::arguments().mid(1))) {
        qWarning() << APP_NAME << "couldn't be started again:" << program;
        return false;                                // Stays open, rather than just closing
    }
    QApplication::closeAllWindows();
    QCoreApplication::quit();
    return true;
}

} // namespace Language

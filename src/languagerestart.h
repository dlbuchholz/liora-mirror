#ifndef LANGUAGERESTART_H
#define LANGUAGERESTART_H

#include <QString>

// Restarting the desktop app so a new language takes effect. Apart from language.h because it
// closes the windows (Qt Widgets), and the rest of Language belongs to the core library.
namespace Language {

// Starts Liora again with the same arguments and quits this one. Returns false, and leaves this
// one open, when it couldn't be started.
bool restartApplication();

// The program that starts Liora again: the running one, or the AppImage file when it runs from
// inside one (the AppImage sets APPIMAGE and APPDIR; its mounted program goes when it ends).
// Only when the program is in that APPDIR: both are inherited from an AppImage that started Liora.
QString restartProgram(const QString& applicationFilePath, const QString& appImage, const QString& appDir);

} // namespace Language

#endif // LANGUAGERESTART_H

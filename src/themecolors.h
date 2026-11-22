#ifndef THEMECOLORS_H
#define THEMECOLORS_H

#include <QColor>

// The colour roles of the light and dark theme, without Qt Widgets, so the core library and a
// QML interface can use them too. Theme (theme.h) applies them to the desktop app.
struct ThemeColors {
    QColor sidebarBackground;
    QColor sidebarBorder;
    QColor itemHover;
    QColor itemSelected;
    QColor textPrimary;
    QColor textSecondary;
    QColor textMuted;
    QColor accent;
    QColor onAccent;
    QColor danger;
    QColor positive;                                 // Income amounts
    QColor track;
    QColor warning;                                  // Text and icons on a warning tint
    QColor onWarning;                                // Text and icons on a solid warning fill

    // Of either theme, whichever is active: for previews of the other one
    static ThemeColors forTheme(bool dark) {
        if (dark) {
            return {
                QColor(30, 30, 34),          // sidebarBackground
                QColor(255, 255, 255, 22),   // sidebarBorder
                QColor(255, 255, 255, 16),   // itemHover
                QColor(255, 255, 255, 30),   // itemSelected
                QColor(236, 236, 238),       // textPrimary
                QColor(205, 205, 210),       // textSecondary
                QColor(160, 160, 166),       // textMuted
                QColor(42, 130, 218),        // accent
                Qt::white,                   // onAccent
                QColor(229, 83, 75),         // danger
                QColor(87, 196, 125),        // positive
                QColor(255, 255, 255, 38),   // track
                QColor(250, 199, 117),       // warning
                QColor(45, 30, 5)            // onWarning
            };
        }
        return {
            QColor(233, 233, 236),           // sidebarBackground
            QColor(0, 0, 0, 20),             // sidebarBorder
            QColor(0, 0, 0, 13),             // itemHover
            QColor(0, 0, 0, 24),             // itemSelected
            QColor(30, 30, 30),              // textPrimary
            QColor(74, 74, 74),              // textSecondary
            QColor(110, 110, 114),           // textMuted
            QColor(42, 130, 218),            // accent
            Qt::white,                       // onAccent
            QColor(201, 55, 44),             // danger
            QColor(30, 135, 70),             // positive
            QColor(0, 0, 0, 26),             // track
            QColor(133, 79, 11),             // warning
            QColor(65, 36, 2)                // onWarning
        };
    }

    // The theme in use. The interface sets it when it applies a theme.
    static bool isDark() { return darkActive; }
    static void setDark(bool dark) { darkActive = dark; }
    static ThemeColors current() { return forTheme(darkActive); }

private:
    static inline bool darkActive = true;
};

#endif // THEMECOLORS_H

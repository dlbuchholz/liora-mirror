#include "icons.h"

#include <QAbstractButton>
#include <QEvent>
#include <QFile>
#include <QTemporaryDir>
#include <QGuiApplication>
#include <QHash>
#include <QPainter>
#include <QtMath>
#include <QRegularExpression>
#include <QSvgRenderer>

namespace Icons {

QPixmap tinted(const QString& name, const QColor& color, int size, qreal devicePixelRatio) {
    static QHash<QString, QPixmap> cache;
    const QString key = QString("%1|%2|%3|%4").arg(name, color.name(QColor::HexArgb)).arg(size).arg(devicePixelRatio);
    auto it = cache.constFind(key);
    if (it != cache.constEnd()) {
        return it.value();
    }

    QPixmap pixmap(QSize(size, size) * devicePixelRatio);
    pixmap.setDevicePixelRatio(devicePixelRatio);
    pixmap.fill(Qt::transparent);

    QSvgRenderer renderer(QString(":/icons/%1.svg").arg(name));
    if (!renderer.isValid()) {
        qWarning("Icon \"%s\" is missing from the resources; rerun CMake if it was just added", qPrintable(name));
    }
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    renderer.render(&painter, QRectF(0, 0, size, size));
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(QRectF(0, 0, size, size), color);
    painter.end();

    cache.insert(key, pixmap);
    return pixmap;
}

QIcon icon(const QString& name, const QColor& color, int size) {
    return QIcon(tinted(name, color, size, qApp->devicePixelRatio()));
}

QString file(const QString& name, const QColor& color, int size) {
    static QTemporaryDir folder;
    const QString base = QString("%1/%2-%3-%4").arg(folder.path(), name, color.name(QColor::HexArgb).mid(1)).arg(size);
    if (!QFile::exists(base + ".png")) {
        tinted(name, color, size * 2, 1.0).save(base + "@2x.png");
        tinted(name, color, size, 1.0).save(base + ".png");
    }
    return base + ".png";
}

namespace {

// Re-tints its button's icon when the palette changes
class ThemedIcon : public QObject {
public:
    ThemedIcon(QAbstractButton* button, const QString& name, int size) : QObject(button), name(name), size(size) {
        update(button);
        button->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::PaletteChange) {
            update(static_cast<QAbstractButton*>(watched));
        }
        return false;
    }

private:
    void update(QAbstractButton* button) const {
        button->setIcon(icon(name, button->palette().color(QPalette::ButtonText), size));
        button->setIconSize(QSize(size, size));
    }

    QString name;
    int size;
};

} // namespace

void setThemedIcon(QAbstractButton* button, const QString& name, int size) {
    new ThemedIcon(button, name, size);
}

QPixmap logo(int size, qreal devicePixelRatio) {
    QPixmap pixmap(QSize(size, size) * devicePixelRatio);
    pixmap.setDevicePixelRatio(devicePixelRatio);
    pixmap.fill(Qt::transparent);
    QSvgRenderer renderer(QString(":/liora-logo.svg"));
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    renderer.render(&painter, QRectF(0, 0, size, size));
    return pixmap;
}

namespace {

// The part of the wordmark's view box it draws in: found by drawing it large and looking at the
// pixels, so a new wordmark needs no new numbers. The same for every colour, so found once.
QRectF drawnBounds(QSvgRenderer& renderer) {
    static QRectF bounds;
    if (bounds.isNull()) {
        constexpr qreal scale = 4;
        const QRectF box = renderer.viewBoxF();
        QImage image((box.size() * scale).toSize(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        {
            QPainter painter(&image);
            renderer.render(&painter, QRectF(QPointF(0, 0), image.size()));
        }
        int left = image.width(), right = -1, top = image.height(), bottom = -1;
        for (int y = 0; y < image.height(); ++y) {
            const QRgb* line = reinterpret_cast<const QRgb*>(image.constScanLine(y));
            for (int x = 0; x < image.width(); ++x) {
                if (qAlpha(line[x]) == 0) continue;
                left = qMin(left, x);
                right = qMax(right, x);
                top = qMin(top, y);
                bottom = qMax(bottom, y);
            }
        }
        bounds = right < 0 ? box                                                   // Draws nothing: all of it
                           : QRectF(box.x() + left / scale, box.y() + top / scale,
                                    (right - left + 1) / scale, (bottom - top + 1) / scale);
    }
    return bounds;
}

} // namespace

QPixmap wordmark(int height, const QColor& ink, const QColor& leaf, qreal devicePixelRatio) {
    QFile file(":/liora-wordmark.svg");
    if (!file.open(QIODevice::ReadOnly)) return {};
    static const QRegularExpression stroke(R"(stroke="#[0-9A-Fa-f]{3,8}")");
    static const QRegularExpression fill(R"(fill="#[0-9A-Fa-f]{3,8}")");     // Not fill="none"
    const QString svg = QString::fromUtf8(file.readAll())
                            .replace(stroke, QString(R"(stroke="%1")").arg(ink.name()))
                            .replace(fill, QString(R"(fill="%1")").arg(leaf.name()));
    QSvgRenderer renderer(svg.toUtf8());
    const QRectF drawn = drawnBounds(renderer);
    const int width = qCeil(height * drawn.width() / drawn.height());

    QPixmap pixmap(QSize(width, height) * devicePixelRatio);
    pixmap.setDevicePixelRatio(devicePixelRatio);
    pixmap.fill(Qt::transparent);
    renderer.setViewBox(drawn);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    renderer.render(&painter, QRectF(0, 0, width, height));
    return pixmap;
}

QIcon appIcon() {
    // Drawn at the sizes Windows asks for (title bar, taskbar, Alt+Tab), so nothing gets scaled blurry
    QIcon icon;
    for (int size : {16, 20, 24, 32, 40, 48, 64, 128, 256}) {
        icon.addPixmap(logo(size, 1.0));
    }
    return icon;
}

} // namespace Icons

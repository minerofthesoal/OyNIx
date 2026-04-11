/*
 * OyNIx Browser v3.0 - Entry Point
 * Chromium-based desktop browser using Qt6 WebEngine, C++17
 */

#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>

#include "core/BrowserWindow.h"

static QString configDir()
{
#ifdef Q_OS_WIN
    return QDir::cleanPath(
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/oynix"));
#else
    return QDir::homePath() + QStringLiteral("/.config/oynix");
#endif
}

static void ensureConfigDirs()
{
    QDir dir;
    const QString base = configDir();
    const QStringList subdirs = {
        QStringLiteral("profiles"),
        QStringLiteral("extensions"),
        QStringLiteral("themes"),
        QStringLiteral("sessions"),
        QStringLiteral("downloads"),
        QStringLiteral("cache"),
    };

    dir.mkpath(base);
    for (const auto &sub : subdirs) {
        dir.mkpath(base + QLatin1Char('/') + sub);
    }
}

int main(int argc, char *argv[])
{
    // QtWebEngine must be initialized before QApplication
    QtWebEngineQuick::initialize();

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("OyNIx Browser"));
    app.setApplicationVersion(QStringLiteral("3.0"));
    app.setOrganizationName(QStringLiteral("OyNIx"));
    app.setOrganizationDomain(QStringLiteral("oynix.dev"));

    // High-DPI is automatic in Qt6; no explicit attribute needed

    ensureConfigDirs();

    BrowserWindow window;
    window.show();

    return app.exec();
}

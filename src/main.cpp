/*
 * OyNIx Browser v3.1 - Entry Point
 * Chromium-based desktop browser using Qt6 WebEngine, C++17
 *
 * Platform selection:
 *   --wayland   Force Wayland backend
 *   --x11       Force X11/XCB backend
 *   (default)   Auto-detect; Ubuntu/deb builds prefer Wayland
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

// Set the Qt platform plugin before QApplication is created.
// Checks CLI flags first, then env, then auto-detects.
static void selectPlatform(int argc, char *argv[])
{
    // Check CLI flags: --wayland or --x11
    for (int i = 1; i < argc; ++i) {
        const QByteArray arg(argv[i]);
        if (arg == "--wayland") {
            qputenv("QT_QPA_PLATFORM", "wayland");
            return;
        }
        if (arg == "--x11" || arg == "--xcb") {
            qputenv("QT_QPA_PLATFORM", "xcb");
            return;
        }
    }

    // If user already set QT_QPA_PLATFORM, respect it
    if (!qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        return;

#ifdef Q_OS_LINUX
    // Auto-detect: if WAYLAND_DISPLAY is set, prefer Wayland with XCB fallback
    if (!qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")) {
        qputenv("QT_QPA_PLATFORM", "wayland;xcb");
    }
#endif
}

int main(int argc, char *argv[])
{
    // Platform selection must happen before any Qt objects
    selectPlatform(argc, argv);

    // QtWebEngine must be initialized before QApplication
    QtWebEngineQuick::initialize();

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("OyNIx Browser"));
    app.setApplicationVersion(QStringLiteral("3.1"));
    app.setOrganizationName(QStringLiteral("OyNIx"));
    app.setOrganizationDomain(QStringLiteral("oynix.dev"));

    // High-DPI is automatic in Qt6; no explicit attribute needed

    ensureConfigDirs();

    BrowserWindow window;
    window.show();

    return app.exec();
}

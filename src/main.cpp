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
#include <QFileInfo>
#include <QLibrary>
#include <QStandardPaths>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>

#include <cstdio>
#include <cstdlib>
#ifdef Q_OS_LINUX
#include <unistd.h>  // readlink
#endif

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

// Resolve the directory containing the oynix binary.
// Uses /proc/self/exe on Linux, argv[0] fallback elsewhere.
static QByteArray resolveExeDir(const char *argv0)
{
#ifdef Q_OS_LINUX
    char buf[4096];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        // Return parent directory
        QByteArray path(buf, static_cast<int>(len));
        int slash = path.lastIndexOf('/');
        return (slash > 0) ? path.left(slash) : path;
    }
#endif
    // Fallback: use argv[0]
    QFileInfo fi(QString::fromLocal8Bit(argv0));
    return fi.absolutePath().toLocal8Bit();
}

// Set up library/plugin/resource paths relative to the binary.
// This makes the binary self-contained — it works whether launched via
// the wrapper script, directly, or from a development build directory.
// Must be called before QApplication is created.
static void setupBundledPaths(const char *argv0)
{
    const QByteArray exeDir = resolveExeDir(argv0);

    // Helper: set env var if the directory exists and env var is not already set
    auto setIfDir = [&](const char *envVar, const QByteArray &path) {
        if (qEnvironmentVariableIsEmpty(envVar) && QFileInfo::exists(QString::fromLocal8Bit(path)))
            qputenv(envVar, path);
    };

    // Qt plugin path — critical for platform plugins (xcb, wayland, etc.)
    setIfDir("QT_PLUGIN_PATH", exeDir + "/plugins");

    // QtWebEngine process and resources
    setIfDir("QTWEBENGINEPROCESS_PATH", exeDir + "/libexec/QtWebEngineProcess");
    setIfDir("QTWEBENGINE_RESOURCES_PATH", exeDir + "/resources");
    setIfDir("QTWEBENGINE_LOCALES_PATH", exeDir + "/translations");

    // LD_LIBRARY_PATH — prepend exe dir so bundled Qt libs are found first
#ifdef Q_OS_LINUX
    {
        QByteArray ldPath = qgetenv("LD_LIBRARY_PATH");
        if (!ldPath.contains(exeDir)) {
            if (ldPath.isEmpty())
                qputenv("LD_LIBRARY_PATH", exeDir);
            else
                qputenv("LD_LIBRARY_PATH", exeDir + ":" + ldPath);
        }
    }
#endif
}

// Detect whether a Wayland session is active.
// Checks multiple indicators since not all compositors set the same vars.
static bool isWaylandSession()
{
    if (!qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY"))
        return true;
    // Modern Ubuntu/GNOME/KDE set XDG_SESSION_TYPE=wayland
    if (qgetenv("XDG_SESSION_TYPE") == "wayland")
        return true;
    return false;
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
    // On Linux, prefer Wayland when a Wayland compositor is running.
    // Use "wayland;xcb" so Qt auto-falls back to XCB if Wayland fails.
    // If no Wayland session, use "xcb;wayland" so XCB is tried first
    // but Wayland is still available as a fallback.
    if (isWaylandSession()) {
        qputenv("QT_QPA_PLATFORM", "wayland;xcb");
    } else {
        qputenv("QT_QPA_PLATFORM", "xcb;wayland");
    }
#endif
}

// Check for libxcb-cursor0 which is required by Qt 6.5+ xcb platform plugin.
// Without it, Qt aborts with SIGABRT and no useful message for users.
static void checkXcbDeps()
{
#ifdef Q_OS_LINUX
    QLibrary xcbCursor(QStringLiteral("libxcb-cursor"), 0);
    if (!xcbCursor.load()) {
        std::fprintf(stderr,
            "\n"
            "  ╔═══════════════════════════════════════════════════════╗\n"
            "  ║  libxcb-cursor0 is required but not installed.       ║\n"
            "  ║  The XCB display backend will fail without it.       ║\n"
            "  ║                                                      ║\n"
            "  ║  Fix:  sudo apt install libxcb-cursor0               ║\n"
            "  ╚═══════════════════════════════════════════════════════╝\n"
            "\n");
    }
#endif
}

int main(int argc, char *argv[])
{
    // Set up bundled library/plugin paths relative to the binary.
    // Must happen first — before platform selection and QApplication.
    setupBundledPaths(argv[0]);

    // Platform selection must happen before any Qt objects
    selectPlatform(argc, argv);

    // Warn early about missing xcb dependencies
    checkXcbDeps();

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

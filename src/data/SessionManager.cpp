#include "SessionManager.h"
#include "core/TabWidget.h"
#include "core/WebView.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QDateTime>
#include <QStandardPaths>

// ── Constructor / Destructor ─────────────────────────────────────────
SessionManager::SessionManager(QObject *parent)
    : QObject(parent)
{
    QDir().mkpath(sessionDir());
}

SessionManager::~SessionManager() = default;

// ── Paths ────────────────────────────────────────────────────────────
QString SessionManager::sessionDir() const
{
#ifdef Q_OS_WIN
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
           + QStringLiteral("/oynix/sessions");
#else
    return QDir::homePath() + QStringLiteral("/.config/oynix/sessions");
#endif
}

QString SessionManager::sessionFilePath(const QString &name) const
{
    return sessionDir() + QLatin1Char('/') + name + QStringLiteral(".json");
}

// ── Save session ─────────────────────────────────────────────────────
bool SessionManager::saveSession(TabWidget *tabWidget, const QString &name)
{
    if (!tabWidget) return false;

    QJsonArray tabs;
    for (int i = 0; i < tabWidget->count(); ++i) {
        auto *view = tabWidget->webView(i);
        if (!view) continue;

        QJsonObject tab;
        tab[QStringLiteral("url")]    = view->url().toString();
        tab[QStringLiteral("title")]  = view->title();
        tab[QStringLiteral("pinned")] = tabWidget->isTabPinned(i);
        tab[QStringLiteral("active")] = (i == tabWidget->currentIndex());
        tabs.append(tab);
    }

    QJsonObject session;
    session[QStringLiteral("tabs")]      = tabs;
    session[QStringLiteral("timestamp")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    session[QStringLiteral("version")]   = 1;

    QFile f(sessionFilePath(name));
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(session).toJson());

    emit sessionSaved(name);
    return true;
}

// ── Load session ─────────────────────────────────────────────────────
QJsonArray SessionManager::loadSession(const QString &name) const
{
    QFile f(sessionFilePath(name));
    if (!f.open(QIODevice::ReadOnly)) return {};

    auto doc = QJsonDocument::fromJson(f.readAll());
    return doc.object()[QStringLiteral("tabs")].toArray();
}

// ── List sessions ────────────────────────────────────────────────────
QStringList SessionManager::availableSessions() const
{
    QStringList sessions;
    const QDir dir(sessionDir());
    for (const auto &entry : dir.entryInfoList({QStringLiteral("*.json")}, QDir::Files)) {
        sessions << entry.baseName();
    }
    return sessions;
}

// ── Delete session ───────────────────────────────────────────────────
bool SessionManager::deleteSession(const QString &name)
{
    return QFile::remove(sessionFilePath(name));
}

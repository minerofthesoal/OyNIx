#include "BookmarkManager.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

BookmarkManager::BookmarkManager(const QString &dataDir, QObject *parent)
    : QObject(parent)
    , m_filePath(dataDir + QStringLiteral("/bookmarks.json"))
{
    QDir().mkpath(dataDir);
    load();
}

void BookmarkManager::load()
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isArray())
        m_bookmarks = doc.array();
}

bool BookmarkManager::save() const
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    file.write(QJsonDocument(m_bookmarks).toJson(QJsonDocument::Indented));
    return true;
}

bool BookmarkManager::add(const QString &url, const QString &title, const QString &folder)
{
    // Avoid duplicates
    if (isBookmarked(url))
        return false;

    QJsonObject entry;
    entry[QStringLiteral("url")] = url;
    entry[QStringLiteral("title")] = title;
    entry[QStringLiteral("added")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    entry[QStringLiteral("folder")] = folder;

    m_bookmarks.append(entry);

    if (save()) {
        emit bookmarksChanged();
        return true;
    }
    return false;
}

bool BookmarkManager::remove(const QString &url)
{
    for (int i = 0; i < m_bookmarks.size(); ++i) {
        if (m_bookmarks[i].toObject()[QStringLiteral("url")].toString() == url) {
            m_bookmarks.removeAt(i);
            if (save()) {
                emit bookmarksChanged();
                return true;
            }
            return false;
        }
    }
    return false;
}

bool BookmarkManager::isBookmarked(const QString &url) const
{
    for (const QJsonValue &v : m_bookmarks) {
        if (v.toObject()[QStringLiteral("url")].toString() == url)
            return true;
    }
    return false;
}

QJsonArray BookmarkManager::getAll() const
{
    return m_bookmarks;
}

QJsonArray BookmarkManager::getByFolder(const QString &folder) const
{
    QJsonArray result;
    for (const QJsonValue &v : m_bookmarks) {
        if (v.toObject()[QStringLiteral("folder")].toString() == folder)
            result.append(v);
    }
    return result;
}

bool BookmarkManager::importBookmarks(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray())
        return false;

    const QJsonArray incoming = doc.array();
    for (const QJsonValue &v : incoming) {
        const QJsonObject obj = v.toObject();
        const QString url = obj[QStringLiteral("url")].toString();
        if (!url.isEmpty() && !isBookmarked(url))
            m_bookmarks.append(v);
    }

    if (save()) {
        emit bookmarksChanged();
        return true;
    }
    return false;
}

bool BookmarkManager::exportBookmarks(const QString &path) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    file.write(QJsonDocument(m_bookmarks).toJson(QJsonDocument::Indented));
    return true;
}

#include "WebCache.h"

#include <algorithm>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

WebCache &WebCache::instance()
{
    static WebCache s;
    return s;
}

WebCache::WebCache(QObject *parent)
    : QObject(parent)
{
#ifdef Q_OS_WIN
    m_cacheDir = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
                 + QStringLiteral("/oynix/webcache");
#else
    m_cacheDir = QDir::homePath() + QStringLiteral("/.cache/oynix/webcache");
#endif
    QDir().mkpath(m_cacheDir);
}

QString WebCache::cacheDir() const
{
    return m_cacheDir;
}

QString WebCache::urlToPath(const QString &url) const
{
    // Hash the URL to a safe filename
    const QByteArray hash = QCryptographicHash::hash(url.toUtf8(), QCryptographicHash::Sha256);
    const QString hex = QString::fromLatin1(hash.toHex().left(32));
    // Shard into subdirectories: first 2 chars / next 2 chars / rest
    return m_cacheDir + QLatin1Char('/') + hex.left(2) + QLatin1Char('/') + hex.mid(2, 2)
           + QLatin1Char('/') + hex.mid(4) + QStringLiteral(".json");
}

bool WebCache::store(const QString &url, const QString &title, const QString &content,
                     const QJsonObject &meta)
{
    const QString path = urlToPath(url);
    QDir().mkpath(QFileInfo(path).absolutePath());

    QJsonObject obj;
    obj[QStringLiteral("url")]        = url;
    obj[QStringLiteral("title")]      = title;
    obj[QStringLiteral("content")]    = content;
    obj[QStringLiteral("cached_at")]  = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    if (!meta.isEmpty())
        obj[QStringLiteral("meta")] = meta;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    return true;
}

QJsonObject WebCache::lookup(const QString &url) const
{
    const QString path = urlToPath(url);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError)
        return {};

    return doc.object();
}

bool WebCache::contains(const QString &url) const
{
    return QFile::exists(urlToPath(url));
}

void WebCache::evict(const QString &url)
{
    QFile::remove(urlToPath(url));
}

void WebCache::clear()
{
    QDir dir(m_cacheDir);
    dir.removeRecursively();
    QDir().mkpath(m_cacheDir);
}

void WebCache::pruneOldEntries(int maxAgeDays)
{
    const QDateTime cutoff = QDateTime::currentDateTimeUtc().addDays(-maxAgeDays);

    QDirIterator it(m_cacheDir, {QStringLiteral("*.json")},
                    QDir::Files, QDirIterator::Subdirectories);
    int removed = 0;
    while (it.hasNext()) {
        it.next();
        if (it.fileInfo().lastModified() < cutoff) {
            QFile::remove(it.filePath());
            ++removed;
        }
    }

    // Clean empty subdirectories (collect then remove bottom-up)
    QStringList dirs;
    QDirIterator dirIt(m_cacheDir, QDir::Dirs | QDir::NoDotAndDotDot,
                       QDirIterator::Subdirectories);
    while (dirIt.hasNext()) {
        dirIt.next();
        dirs.append(dirIt.filePath());
    }
    // Reverse so deepest dirs are removed first
    std::sort(dirs.begin(), dirs.end(), [](const QString &a, const QString &b) {
        return a.size() > b.size();
    });
    for (const auto &path : dirs) {
        QDir().rmdir(path); // only removes if empty
    }
}

qint64 WebCache::cacheSize() const
{
    int count = 0;
    qint64 total = 0;
    scanCache(count, total);
    return total;
}

QJsonObject WebCache::getStats() const
{
    int fileCount = 0;
    qint64 totalSize = 0;
    scanCache(fileCount, totalSize);

    QJsonObject stats;
    stats[QStringLiteral("cached_pages")] = fileCount;
    stats[QStringLiteral("cache_size_mb")] = qRound(totalSize / (1024.0 * 1024.0) * 100.0) / 100.0;
    stats[QStringLiteral("cache_dir")] = m_cacheDir;
    return stats;
}

void WebCache::scanCache(int &fileCount, qint64 &totalSize) const
{
    fileCount = 0;
    totalSize = 0;
    QDirIterator it(m_cacheDir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        ++fileCount;
        totalSize += it.fileInfo().size();
    }
}

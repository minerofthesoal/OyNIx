#include "ChromeImport.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>
#include <QDateTime>
#include <QTemporaryFile>

// Chrome stores timestamps as microseconds since Jan 1, 1601
static constexpr qint64 kChromeEpochOffset = 11644473600LL;

static QDateTime chromeTimestampToDateTime(qint64 chromeTimestamp)
{
    // Convert from microseconds since 1601 to seconds since Unix epoch
    const qint64 unixSeconds = (chromeTimestamp / 1000000) - kChromeEpochOffset;
    return QDateTime::fromSecsSinceEpoch(unixSeconds, Qt::UTC);
}

// ── Constructor / Destructor ─────────────────────────────────────────
ChromeImport::ChromeImport(QObject *parent)
    : QObject(parent)
    , m_chromePath(detectChromePath())
{
}

ChromeImport::~ChromeImport() = default;

// ── Detect Chrome path ───────────────────────────────────────────────
QString ChromeImport::detectChromePath()
{
#ifdef Q_OS_LINUX
    const QStringList candidates = {
        QDir::homePath() + QStringLiteral("/.config/google-chrome/Default/History"),
        QDir::homePath() + QStringLiteral("/.config/chromium/Default/History"),
        QDir::homePath() + QStringLiteral("/.config/google-chrome-beta/Default/History"),
    };
#elif defined(Q_OS_MAC)
    const QStringList candidates = {
        QDir::homePath() + QStringLiteral("/Library/Application Support/Google/Chrome/Default/History"),
        QDir::homePath() + QStringLiteral("/Library/Application Support/Chromium/Default/History"),
    };
#elif defined(Q_OS_WIN)
    const QString localAppData = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const QStringList candidates = {
        localAppData + QStringLiteral("/Google/Chrome/User Data/Default/History"),
        localAppData + QStringLiteral("/Chromium/User Data/Default/History"),
    };
#else
    const QStringList candidates;
#endif

    for (const auto &path : candidates) {
        if (QFileInfo::exists(path))
            return path;
    }
    return QString();
}

bool ChromeImport::isChromeAvailable() const
{
    return !m_chromePath.isEmpty() && QFileInfo::exists(m_chromePath);
}

QString ChromeImport::chromeHistoryPath() const
{
    return m_chromePath;
}

// ── Import history ───────────────────────────────────────────────────
QJsonArray ChromeImport::importHistory(int maxEntries) const
{
    QJsonArray results;
    if (!isChromeAvailable()) return results;

    // Chrome locks its DB, so copy it first
    QTemporaryFile tempFile;
    if (!tempFile.open()) return results;
    tempFile.close();

    QFile::copy(m_chromePath, tempFile.fileName());

    {
        const QString connName = QStringLiteral("chrome_import_%1").arg(
            reinterpret_cast<quintptr>(this));
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
        db.setDatabaseName(tempFile.fileName());

        if (!db.open()) return results;

        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT url, title, visit_count, last_visit_time "
            "FROM urls ORDER BY last_visit_time DESC LIMIT ?"));
        query.addBindValue(maxEntries);

        if (query.exec()) {
            int count = 0;
            while (query.next()) {
                QJsonObject entry;
                entry[QStringLiteral("url")]         = query.value(0).toString();
                entry[QStringLiteral("title")]       = query.value(1).toString();
                entry[QStringLiteral("visit_count")] = query.value(2).toInt();

                const qint64 chromeTime = query.value(3).toLongLong();
                const QDateTime dt = chromeTimestampToDateTime(chromeTime);
                entry[QStringLiteral("time")] = dt.toString(Qt::ISODate);

                results.append(entry);
                ++count;
                if (count % 500 == 0)
                    emit const_cast<ChromeImport *>(this)->importProgress(count, maxEntries);
            }
            emit const_cast<ChromeImport *>(this)->importComplete(count);
        }

        db.close();
        QSqlDatabase::removeDatabase(connName);
    }

    return results;
}

// ── Import search queries ────────────────────────────────────────────
QJsonArray ChromeImport::importSearchQueries(int maxEntries) const
{
    const QJsonArray history = importHistory(maxEntries);
    QJsonArray queries;
    QSet<QString> seen;

    for (const auto &val : history) {
        auto obj = val.toObject();
        const QString url = obj[QStringLiteral("url")].toString();
        const QString query = extractSearchQuery(url);

        if (!query.isEmpty() && !seen.contains(query)) {
            seen.insert(query);
            QJsonObject entry;
            entry[QStringLiteral("query")] = query;
            entry[QStringLiteral("time")]  = obj[QStringLiteral("time")].toString();
            entry[QStringLiteral("url")]   = url;
            queries.append(entry);
        }
    }

    return queries;
}

// ── Extract search query from URL ────────────────────────────────────
QString ChromeImport::extractSearchQuery(const QString &urlStr)
{
    const QUrl url(urlStr);
    const QString host = url.host().toLower();

    // Google
    if (host.contains(QStringLiteral("google."))) {
        return QUrlQuery(url).queryItemValue(QStringLiteral("q"));
    }
    // DuckDuckGo
    if (host.contains(QStringLiteral("duckduckgo."))) {
        return QUrlQuery(url).queryItemValue(QStringLiteral("q"));
    }
    // Bing
    if (host.contains(QStringLiteral("bing."))) {
        return QUrlQuery(url).queryItemValue(QStringLiteral("q"));
    }
    // Brave
    if (host.contains(QStringLiteral("search.brave."))) {
        return QUrlQuery(url).queryItemValue(QStringLiteral("q"));
    }

    return QString();
}

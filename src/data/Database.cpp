#include "Database.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

Database *Database::s_instance = nullptr;

Database *Database::instance()
{
    if (!s_instance)
        s_instance = new Database();
    return s_instance;
}

QString Database::configDir()
{
#ifdef Q_OS_WIN
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#else
    const QString home = QDir::homePath();
    return home + QStringLiteral("/.config/oynix");
#endif
}

Database::Database(QObject *parent)
    : QObject(parent)
{
    initDatabase();
}

Database::~Database()
{
    {
        QSqlDatabase db = QSqlDatabase::database(QStringLiteral("oynix_main"));
        if (db.isOpen())
            db.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("oynix_main"));
}

void Database::initDatabase()
{
    const QString dir = configDir();
    QDir().mkpath(dir);
    m_dbPath = dir + QStringLiteral("/nyx_index.db");

    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("oynix_main"));
        db.setDatabaseName(m_dbPath);
        if (db.open()) {
            // Enable WAL mode for better concurrency
            QSqlQuery pragma(db);
            pragma.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
            pragma.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));
            createTables();
        }
    }
}

void Database::createTables()
{
    QSqlDatabase db = QSqlDatabase::database(QStringLiteral("oynix_main"));
    QSqlQuery q(db);

    // ── Sites table with indexes ──
    q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS sites("
        "id INTEGER PRIMARY KEY, "
        "url TEXT UNIQUE, "
        "title TEXT, "
        "description TEXT, "
        "category TEXT, "
        "rating REAL DEFAULT 0.0, "
        "source TEXT, "
        "added_at TEXT)"
    ));
    q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_sites_category ON sites(category)"));
    q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_sites_rating ON sites(rating DESC)"));

    // ── Pages table with indexes ──
    q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS pages("
        "id INTEGER PRIMARY KEY, "
        "url TEXT UNIQUE, "
        "title TEXT, "
        "content TEXT, "
        "domain TEXT, "
        "source TEXT DEFAULT 'nyx', "
        "visited_at TEXT, "
        "visit_count INTEGER DEFAULT 1)"
    ));
    q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_pages_domain ON pages(domain)"));
    q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_pages_visited ON pages(visited_at DESC)"));
    q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_pages_visits ON pages(visit_count DESC)"));

    // Migrate: add source column if missing (existing DBs)
    q.exec(QStringLiteral("ALTER TABLE pages ADD COLUMN source TEXT DEFAULT 'nyx'"));
    // (ALTER TABLE ... ADD COLUMN silently fails if column already exists in SQLite 3.35+)

    // ── FTS5 with porter tokenizer and prefix indexes for fast autocomplete ──
    q.exec(QStringLiteral(
        "CREATE VIRTUAL TABLE IF NOT EXISTS pages_fts USING fts5("
        "url, title, content, domain, "
        "tokenize='porter unicode61', "
        "prefix='2,3')"
    ));

    // ── Sites FTS for full-text search on the curated database ──
    q.exec(QStringLiteral(
        "CREATE VIRTUAL TABLE IF NOT EXISTS sites_fts USING fts5("
        "url, title, description, category, "
        "tokenize='porter unicode61', "
        "prefix='2,3')"
    ));
}

bool Database::addSite(const QString &url, const QString &title, const QString &description,
                       const QString &category, double rating, const QString &source)
{
    QMutexLocker locker(&m_mutex);
    QSqlDatabase db = QSqlDatabase::database(QStringLiteral("oynix_main"));
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO sites(url, title, description, category, rating, source, added_at) "
        "VALUES(?, ?, ?, ?, ?, ?, ?)"
    ));
    q.addBindValue(url);
    q.addBindValue(title);
    q.addBindValue(description);
    q.addBindValue(category);
    q.addBindValue(rating);
    q.addBindValue(source);
    q.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    bool ok = q.exec();
    if (ok) {
        // Update FTS index for sites
        QSqlQuery fts(db);
        fts.prepare(QStringLiteral("DELETE FROM sites_fts WHERE url = ?"));
        fts.addBindValue(url);
        fts.exec();
        fts.prepare(QStringLiteral("INSERT INTO sites_fts(url, title, description, category) VALUES(?,?,?,?)"));
        fts.addBindValue(url);
        fts.addBindValue(title);
        fts.addBindValue(description);
        fts.addBindValue(category);
        fts.exec();
        emit databaseChanged();
    }
    return ok;
}

bool Database::addSitesBatch(const QJsonArray &sites)
{
    QMutexLocker locker(&m_mutex);
    QSqlDatabase db = QSqlDatabase::database(QStringLiteral("oynix_main"));

    if (!db.transaction())
        return false;

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO sites(url, title, description, category, rating, source, added_at) "
        "VALUES(?, ?, ?, ?, ?, ?, ?)"
    ));

    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    for (const QJsonValue &v : sites) {
        const QJsonObject obj = v.toObject();
        q.addBindValue(obj[QStringLiteral("url")].toString());
        q.addBindValue(obj[QStringLiteral("title")].toString());
        q.addBindValue(obj[QStringLiteral("description")].toString());
        q.addBindValue(obj[QStringLiteral("category")].toString());
        q.addBindValue(obj[QStringLiteral("rating")].toDouble());
        q.addBindValue(obj[QStringLiteral("source")].toString());
        q.addBindValue(now);
        if (!q.exec()) {
            db.rollback();
            return false;
        }
    }

    bool ok = db.commit();
    if (ok)
        emit databaseChanged();
    return ok;
}

QJsonArray Database::searchSites(const QString &query, int limit) const
{
    QMutexLocker locker(&m_mutex);
    QJsonArray results;
    QSqlDatabase db = QSqlDatabase::database(QStringLiteral("oynix_main"));

    // Try FTS5 search first (faster + ranked)
    QStringList words = query.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QStringList ftsTerms;
    for (const QString &word : words) {
        QString safe = word;
        safe.replace(QLatin1Char('"'), QString());
        if (!safe.isEmpty())
            ftsTerms << (QLatin1Char('"') + safe + QLatin1Char('"'));
    }

    if (!ftsTerms.isEmpty()) {
        const QString ftsQuery = ftsTerms.join(QStringLiteral(" AND "));
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "SELECT s.url, s.title, s.description, s.category, s.rating, s.source, s.added_at "
            "FROM sites_fts f JOIN sites s ON f.url = s.url "
            "WHERE sites_fts MATCH ? ORDER BY s.rating DESC LIMIT ?"
        ));
        q.addBindValue(ftsQuery);
        q.addBindValue(limit);

        if (q.exec()) {
            while (q.next()) {
                QJsonObject obj;
                obj[QStringLiteral("url")] = q.value(0).toString();
                obj[QStringLiteral("title")] = q.value(1).toString();
                obj[QStringLiteral("description")] = q.value(2).toString();
                obj[QStringLiteral("category")] = q.value(3).toString();
                obj[QStringLiteral("rating")] = q.value(4).toDouble();
                obj[QStringLiteral("source")] = q.value(5).toString();
                obj[QStringLiteral("added_at")] = q.value(6).toString();
                results.append(obj);
            }
        }
    }

    // Fallback to LIKE search if FTS returned nothing
    if (results.isEmpty()) {
        const QString pattern = QStringLiteral("%%1%").arg(query);
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "SELECT url, title, description, category, rating, source, added_at FROM sites "
            "WHERE url LIKE ? OR title LIKE ? OR description LIKE ? OR category LIKE ? "
            "ORDER BY rating DESC LIMIT ?"
        ));
        q.addBindValue(pattern);
        q.addBindValue(pattern);
        q.addBindValue(pattern);
        q.addBindValue(pattern);
        q.addBindValue(limit);

        if (q.exec()) {
            while (q.next()) {
                QJsonObject obj;
                obj[QStringLiteral("url")] = q.value(0).toString();
                obj[QStringLiteral("title")] = q.value(1).toString();
                obj[QStringLiteral("description")] = q.value(2).toString();
                obj[QStringLiteral("category")] = q.value(3).toString();
                obj[QStringLiteral("rating")] = q.value(4).toDouble();
                obj[QStringLiteral("source")] = q.value(5).toString();
                obj[QStringLiteral("added_at")] = q.value(6).toString();
                results.append(obj);
            }
        }
    }

    return results;
}

int Database::getSiteCount() const
{
    QMutexLocker locker(&m_mutex);
    QSqlDatabase db = QSqlDatabase::database(QStringLiteral("oynix_main"));
    QSqlQuery q(db);
    if (q.exec(QStringLiteral("SELECT COUNT(*) FROM sites")) && q.next())
        return q.value(0).toInt();
    return 0;
}

bool Database::compare_site(const QString &url1, const QString &url2) const
{
    QMutexLocker locker(&m_mutex);
    QSqlDatabase db = QSqlDatabase::database(QStringLiteral("oynix_main"));
    QSqlQuery q(db);

    auto fetchRating = [&](const QString &url) -> double {
        q.prepare(QStringLiteral("SELECT rating FROM sites WHERE url = ?"));
        q.addBindValue(url);
        if (q.exec() && q.next())
            return q.value(0).toDouble();
        return -1.0;
    };

    double r1 = fetchRating(url1);
    double r2 = fetchRating(url2);
    return r1 >= r2;
}

bool Database::addPage(const QString &url, const QString &title, const QString &content,
                       const QString &domain, const QString &source)
{
    QMutexLocker locker(&m_mutex);
    QSqlDatabase db = QSqlDatabase::database(QStringLiteral("oynix_main"));
    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    // Upsert into pages
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO pages(url, title, content, domain, source, visited_at, visit_count) "
        "VALUES(?, ?, ?, ?, ?, ?, 1) "
        "ON CONFLICT(url) DO UPDATE SET "
        "title=excluded.title, content=excluded.content, source=excluded.source, "
        "visited_at=excluded.visited_at, visit_count=visit_count+1"
    ));
    q.addBindValue(url);
    q.addBindValue(title);
    q.addBindValue(content);
    q.addBindValue(domain);
    q.addBindValue(source);
    q.addBindValue(now);

    if (!q.exec())
        return false;

    // Update FTS index: delete old entry then insert fresh
    QSqlQuery fts(db);
    fts.prepare(QStringLiteral("DELETE FROM pages_fts WHERE url = ?"));
    fts.addBindValue(url);
    fts.exec();

    fts.prepare(QStringLiteral("INSERT INTO pages_fts(url, title, content, domain) VALUES(?, ?, ?, ?)"));
    fts.addBindValue(url);
    fts.addBindValue(title);
    fts.addBindValue(content);
    fts.addBindValue(domain);
    fts.exec();

    emit databaseChanged();
    return true;
}

QJsonArray Database::searchPages(const QString &query, int limit) const
{
    QMutexLocker locker(&m_mutex);
    QJsonArray results;
    QSqlDatabase db = QSqlDatabase::database(QStringLiteral("oynix_main"));
    QSqlQuery q(db);

    // Build FTS5-safe query: split words, quote each, join with AND
    // This handles multi-word searches like "python tutorial" correctly
    QStringList words = query.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QStringList ftsTerms;
    for (const QString &word : words) {
        // Escape double quotes in the word and wrap in quotes for exact token matching
        QString safe = word;
        safe.replace(QLatin1Char('"'), QString());
        if (!safe.isEmpty())
            ftsTerms << (QLatin1Char('"') + safe + QLatin1Char('"'));
    }

    if (ftsTerms.isEmpty())
        return results;

    const QString ftsQuery = ftsTerms.join(QStringLiteral(" AND "));

    q.prepare(QStringLiteral(
        "SELECT f.url, f.title, snippet(pages_fts, 2, '<b>', '</b>', '...', 64), f.domain, "
        "COALESCE(p.source, 'nyx') "
        "FROM pages_fts f LEFT JOIN pages p ON f.url = p.url "
        "WHERE pages_fts MATCH ? ORDER BY rank LIMIT ?"
    ));
    q.addBindValue(ftsQuery);
    q.addBindValue(limit);

    if (q.exec()) {
        while (q.next()) {
            QJsonObject obj;
            obj[QStringLiteral("url")] = q.value(0).toString();
            obj[QStringLiteral("title")] = q.value(1).toString();
            obj[QStringLiteral("snippet")] = q.value(2).toString();
            obj[QStringLiteral("domain")] = q.value(3).toString();
            obj[QStringLiteral("source")] = q.value(4).toString();
            results.append(obj);
        }
    }

    return results;
}

bool Database::exportToJson(const QString &path) const
{
    QMutexLocker locker(&m_mutex);
    QSqlDatabase db = QSqlDatabase::database(QStringLiteral("oynix_main"));

    QJsonArray sites;
    QSqlQuery q(db);
    if (q.exec(QStringLiteral("SELECT url, title, description, category, rating, source, added_at FROM sites"))) {
        while (q.next()) {
            QJsonObject obj;
            obj[QStringLiteral("url")] = q.value(0).toString();
            obj[QStringLiteral("title")] = q.value(1).toString();
            obj[QStringLiteral("description")] = q.value(2).toString();
            obj[QStringLiteral("category")] = q.value(3).toString();
            obj[QStringLiteral("rating")] = q.value(4).toDouble();
            obj[QStringLiteral("source")] = q.value(5).toString();
            obj[QStringLiteral("added_at")] = q.value(6).toString();
            sites.append(obj);
        }
    }

    QJsonObject root;
    root[QStringLiteral("version")] = QStringLiteral("3.0");
    root[QStringLiteral("sites")] = sites;
    root[QStringLiteral("exported_at")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

bool Database::exportToJsonl(const QString &path) const
{
    QMutexLocker locker(&m_mutex);
    QSqlDatabase db = QSqlDatabase::database(QStringLiteral("oynix_main"));

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    // Export crawled pages (from pages table) as JSONL — one JSON object per line
    QSqlQuery q(db);
    if (q.exec(QStringLiteral("SELECT url, title, content, domain, source, visited_at FROM pages"))) {
        while (q.next()) {
            QJsonObject obj;
            obj[QStringLiteral("url")]        = q.value(0).toString();
            obj[QStringLiteral("title")]      = q.value(1).toString();
            obj[QStringLiteral("content")]    = q.value(2).toString();
            obj[QStringLiteral("domain")]     = q.value(3).toString();
            obj[QStringLiteral("source")]     = q.value(4).toString();
            obj[QStringLiteral("indexed_at")] = q.value(5).toString();
            file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
            file.write("\n");
        }
    }

    // Also export sites
    if (q.exec(QStringLiteral("SELECT url, title, description, category, rating, source, added_at FROM sites"))) {
        while (q.next()) {
            QJsonObject obj;
            obj[QStringLiteral("url")]         = q.value(0).toString();
            obj[QStringLiteral("title")]       = q.value(1).toString();
            obj[QStringLiteral("description")] = q.value(2).toString();
            obj[QStringLiteral("category")]    = q.value(3).toString();
            obj[QStringLiteral("rating")]      = q.value(4).toDouble();
            obj[QStringLiteral("source")]      = q.value(5).toString();
            obj[QStringLiteral("added_at")]    = q.value(6).toString();
            file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
            file.write("\n");
        }
    }

    return true;
}

bool Database::importFromJson(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull())
        return false;

    const QJsonObject root = doc.object();
    const QJsonArray sites = root[QStringLiteral("sites")].toArray();

    return addSitesBatch(sites);
}

QJsonObject Database::getStats() const
{
    QMutexLocker locker(&m_mutex);
    QSqlDatabase db = QSqlDatabase::database(QStringLiteral("oynix_main"));
    QJsonObject stats;

    QSqlQuery q(db);
    if (q.exec(QStringLiteral("SELECT COUNT(*) FROM sites")) && q.next())
        stats[QStringLiteral("site_count")] = q.value(0).toInt();

    if (q.exec(QStringLiteral("SELECT COUNT(*) FROM pages")) && q.next())
        stats[QStringLiteral("page_count")] = q.value(0).toInt();

    if (q.exec(QStringLiteral("SELECT COUNT(DISTINCT category) FROM sites")) && q.next())
        stats[QStringLiteral("category_count")] = q.value(0).toInt();

    if (q.exec(QStringLiteral("SELECT AVG(rating) FROM sites")) && q.next())
        stats[QStringLiteral("avg_rating")] = q.value(0).toDouble();

    if (q.exec(QStringLiteral("SELECT SUM(visit_count) FROM pages")) && q.next())
        stats[QStringLiteral("total_visits")] = q.value(0).toInt();

    // Count unique websites (distinct domains across pages + distinct URLs in sites)
    if (q.exec(QStringLiteral(
            "SELECT COUNT(*) FROM ("
            "  SELECT DISTINCT domain FROM pages WHERE domain != '' "
            "  UNION "
            "  SELECT DISTINCT REPLACE(REPLACE(REPLACE(url, 'https://', ''), 'http://', ''), "
            "    SUBSTR(REPLACE(REPLACE(url, 'https://', ''), 'http://', ''), "
            "      INSTR(REPLACE(REPLACE(url, 'https://', ''), 'http://', ''), '/')), '') "
            "  FROM sites WHERE url != ''"
            ")")) && q.next())
        stats[QStringLiteral("unique_websites")] = q.value(0).toInt();

    stats[QStringLiteral("db_path")] = m_dbPath;

    return stats;
}

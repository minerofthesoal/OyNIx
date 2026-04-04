#include "NyxSearch.h"
#include "WebResultsFetcher.h"
#include "../data/Database.h"

#include <QFile>
#include <QJsonDocument>
#include <QUrl>

NyxSearch *NyxSearch::s_instance = nullptr;

NyxSearch *NyxSearch::instance()
{
    if (!s_instance)
        s_instance = new NyxSearch();
    return s_instance;
}

NyxSearch::NyxSearch(QObject *parent)
    : QObject(parent)
    , m_db(Database::instance())
{
}

NyxSearch::~NyxSearch()
{
    if (m_fetcher) {
        m_fetcher->abort();
        delete m_fetcher;
    }
}

// ---------------------------------------------------------------------------
// Index a visited page into the FTS5 index
// ---------------------------------------------------------------------------
void NyxSearch::indexPage(const QString &url, const QString &title, const QString &contentSnippet)
{
    const QString domain = QUrl(url).host();
    m_db->addPage(url, title, contentSnippet, domain);
}

// ---------------------------------------------------------------------------
// Combined search: local FTS5 pages + curated site database + async web
// ---------------------------------------------------------------------------
QJsonObject NyxSearch::search(const QString &query)
{
    QJsonObject result;

    // 1. Local page index (FTS5)
    QJsonArray localPages = m_db->searchPages(query);
    QJsonArray localResults;
    for (const QJsonValue &v : localPages) {
        QJsonObject page = v.toObject();
        QJsonObject item;
        item[QStringLiteral("url")] = page[QStringLiteral("url")].toString();
        item[QStringLiteral("title")] = page[QStringLiteral("title")].toString();
        item[QStringLiteral("description")] = page[QStringLiteral("snippet")].toString();
        item[QStringLiteral("source")] = QStringLiteral("nyx");
        item[QStringLiteral("score")] = 1.0;  // local results ranked highest
        localResults.append(item);
    }
    result[QStringLiteral("local")] = localResults;

    // 2. Curated site database
    QJsonArray dbSites = m_db->searchSites(query);
    QJsonArray dbResults;
    for (const QJsonValue &v : dbSites) {
        QJsonObject site = v.toObject();
        QJsonObject item;
        item[QStringLiteral("url")] = site[QStringLiteral("url")].toString();
        item[QStringLiteral("title")] = site[QStringLiteral("title")].toString();
        item[QStringLiteral("description")] = site[QStringLiteral("description")].toString();
        item[QStringLiteral("source")] = QStringLiteral("database");
        item[QStringLiteral("score")] = site[QStringLiteral("rating")].toDouble();
        dbResults.append(item);
    }
    result[QStringLiteral("database")] = dbResults;

    // 3. Kick off async web search (DDG + Google)
    launchWebSearch(query);

    return result;
}

// ---------------------------------------------------------------------------
// Async web search via WebResultsFetcher thread
// ---------------------------------------------------------------------------
void NyxSearch::launchWebSearch(const QString &query)
{
    // Clean up any previous fetcher
    if (m_fetcher) {
        m_fetcher->abort();
        m_fetcher->deleteLater();
        m_fetcher = nullptr;
    }

    m_fetcher = new WebResultsFetcher(query, this);
    connect(m_fetcher, &WebResultsFetcher::resultsReady,
            this, &NyxSearch::onWebResults);
    m_fetcher->start();
}

void NyxSearch::onWebResults(const QJsonArray &results)
{
    // Add scores to web results
    QJsonArray scored;
    double score = 0.8;
    for (const QJsonValue &v : results) {
        QJsonObject item = v.toObject();
        item[QStringLiteral("score")] = score;
        scored.append(item);
        score = qMax(0.1, score - 0.01);
    }
    m_fetcher = nullptr;
    emit webResultsReady(scored);
}

// ---------------------------------------------------------------------------
// Stats
// ---------------------------------------------------------------------------
QJsonObject NyxSearch::getStats() const
{
    QJsonObject dbStats = m_db->getStats();
    QJsonObject stats;
    stats[QStringLiteral("indexed_pages")] = dbStats[QStringLiteral("page_count")].toInt();
    stats[QStringLiteral("database_sites")] = dbStats[QStringLiteral("site_count")].toInt();
    stats[QStringLiteral("total_visits")] = dbStats[QStringLiteral("total_visits")].toInt();
    return stats;
}

// ---------------------------------------------------------------------------
// Export index as JSON
// ---------------------------------------------------------------------------
bool NyxSearch::exportIndex(const QString &path) const
{
    return m_db->exportToJson(path);
}

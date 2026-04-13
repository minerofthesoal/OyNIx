/*
 * OyNIx Browser v3.1 — WebCrawler
 * Native C++ web crawler replacing the C# NativeAOT CoreBridge crawler.
 * Supports list-based and broad (BFS) crawling with async HTTP requests.
 */

#pragma once

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QMutex>
#include <QQueue>
#include <QSet>
#include <QUrl>
#include <QHash>
#include <QElapsedTimer>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

class WebCrawler : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Idle,
        Crawling,
        Paused,
        Stopping
    };
    Q_ENUM(State)

    static WebCrawler &instance();

    // ── Configuration ───────────────────────────────────────────────────
    void configure(int maxDepth, int maxPages, int concurrency, bool followExternal);
    void setPolitenessDelay(int milliseconds);

    // ── Crawl actions ───────────────────────────────────────────────────
    void startList(const QJsonArray &urls);
    void startBroad(const QString &seedUrl);
    void stop();
    void pause();
    void resume();

    // ── Status & results ────────────────────────────────────────────────
    [[nodiscard]] QJsonObject status() const;
    [[nodiscard]] QJsonArray results(int offset, int limit) const;
    [[nodiscard]] int count() const;
    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] State state() const;

signals:
    void crawlProgress(int crawled, int total);
    void pageDiscovered(QJsonObject page);
    void crawlFinished();
    void statusChanged(QString stateString);

private:
    explicit WebCrawler(QObject *parent = nullptr);
    ~WebCrawler() override;

    // ── Internal types ──────────────────────────────────────────────────
    struct CrawlTask {
        QUrl   url;
        int    depth = 0;
        QString referrer;
    };

    struct RobotsRules {
        QStringList disallowedPaths;
        bool        fetched = false;
        bool        pending = false;
    };

    // ── Core crawling logic ─────────────────────────────────────────────
    void scheduleNext();
    void fetchPage(const CrawlTask &task);
    void onPageFinished(QNetworkReply *reply, const CrawlTask &task);
    void onRobotsFetched(QNetworkReply *reply, const QString &domain);

    // ── HTML parsing helpers ────────────────────────────────────────────
    static QStringList extractLinks(const QString &html, const QUrl &baseUrl);
    static QString extractTitle(const QString &html);
    static QString extractTextContent(const QString &html, int maxLength = 500);

    // ── Robots.txt ──────────────────────────────────────────────────────
    void fetchRobotsTxt(const QUrl &url);
    bool isAllowedByRobots(const QUrl &url) const;
    static RobotsRules parseRobotsTxt(const QString &content);

    // ── Politeness ──────────────────────────────────────────────────────
    bool canFetchFromDomain(const QString &domain) const;
    void recordDomainAccess(const QString &domain);

    // ── URL filtering ───────────────────────────────────────────────────
    bool shouldCrawl(const QUrl &url, int depth) const;
    void setState(State newState);
    void reset();

    // ── Config ──────────────────────────────────────────────────────────
    int  m_maxDepth       = 2;
    int  m_maxPages       = 500;
    int  m_concurrency    = 4;
    bool m_followExternal = false;
    int  m_politenessMs   = 200;

    // ── State (protected by m_mutex) ────────────────────────────────────
    mutable QMutex m_mutex;
    State          m_state = State::Idle;

    QQueue<CrawlTask>   m_queue;
    QSet<QString>        m_visitedUrls;
    QJsonArray           m_results;
    int                  m_activeRequests = 0;
    int                  m_errorCount     = 0;

    // Seed domains for same-domain filtering
    QSet<QString>        m_seedDomains;

    // ── Robots.txt cache ────────────────────────────────────────────────
    QHash<QString, RobotsRules> m_robotsCache;

    // ── Politeness: last access time per domain ─────────────────────────
    QHash<QString, qint64> m_domainLastAccess;

    // ── Network ─────────────────────────────────────────────────────────
    QNetworkAccessManager *m_nam   = nullptr;
    QTimer                *m_scheduleTimer = nullptr;
};

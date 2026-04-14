/*
 * OyNIx Browser v3.1 — CrabCrawler ("Crabs")
 * Rewritten native C++ web crawler. Replaces the old WebCrawler with:
 *   - Streaming results to DB (no in-memory accumulation)
 *   - Proper mutex discipline (no signals/callbacks while locked)
 *   - Bounded memory usage
 *   - Politeness delays + robots.txt compliance
 */

#pragma once

#include <QObject>
#include <QJsonObject>
#include <QMutex>
#include <QQueue>
#include <QSet>
#include <QUrl>
#include <QHash>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

class CrabCrawler : public QObject
{
    Q_OBJECT

public:
    enum class State { Idle, Crawling, Paused, Stopping };
    Q_ENUM(State)

    static CrabCrawler &instance();

    // Configuration (call before start)
    void configure(int maxDepth, int maxPages, int concurrency, bool followExternal);
    void setPolitenessDelay(int milliseconds);
    void setLanguageFilter(const QString &langCode);
    [[nodiscard]] QString languageFilter() const;

    // Crawl actions
    void startList(const QStringList &urls);
    void startBroad(const QString &seedUrl);
    void stop();
    void pause();
    void resume();

    // Status (thread-safe reads)
    [[nodiscard]] int crawledCount() const;
    [[nodiscard]] int errorCount() const;
    [[nodiscard]] int queuedCount() const;
    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] State state() const;

signals:
    void progress(int crawled, int queued, int errors);
    void pageIndexed(const QString &url, const QString &title, const QString &status);
    void finished();
    void stateChanged(const QString &stateString);

private:
    explicit CrabCrawler(QObject *parent = nullptr);
    ~CrabCrawler() override;

    struct CrawlTask {
        QUrl    url;
        int     depth = 0;
        QString referrer;
    };

    struct RobotsRules {
        QStringList disallowedPaths;
        bool fetched = false;
        bool pending = false;
    };

    // Core logic
    void scheduleNext();
    void fetchPage(const CrawlTask &task);
    void onPageFinished(QNetworkReply *reply, const CrawlTask &task);

    // Robots.txt
    void fetchRobotsTxt(const QUrl &url);
    void onRobotsFetched(QNetworkReply *reply, const QString &domain);
    static RobotsRules parseRobotsTxt(const QString &content);
    bool isAllowedByRobots(const QUrl &url) const;

    // HTML parsing
    static QStringList extractLinks(const QString &html, const QUrl &baseUrl);
    static QString extractTitle(const QString &html);
    static QString extractTextContent(const QString &html, int maxLength = 500);

    // Politeness
    bool canFetchFromDomain(const QString &domain) const;
    void recordDomainAccess(const QString &domain);

    // Filtering
    bool shouldCrawl(const QUrl &url, int depth) const;
    void setState(State newState);
    void reset();
    void emitProgress();

    // Config
    int  m_maxDepth       = 2;
    int  m_maxPages       = 500;
    int  m_concurrency    = 2;
    bool m_followExternal = false;
    int  m_politenessMs   = 500;

    // State (protected by m_mutex)
    mutable QMutex m_mutex;
    State m_state = State::Idle;

    QQueue<CrawlTask>  m_queue;
    QSet<QString>      m_visited;       // canonical URLs already seen
    int                m_crawledCount = 0;
    int                m_errorCount   = 0;
    int                m_activeReqs   = 0;

    QSet<QString>      m_seedDomains;
    bool               m_isBroadMode = false;
    QString            m_languageFilter;

    // Robots.txt cache
    QHash<QString, RobotsRules> m_robotsCache;

    // Politeness: last access time per domain
    QHash<QString, qint64> m_domainLastAccess;

    // Network
    QNetworkAccessManager *m_nam   = nullptr;
    QTimer                *m_timer = nullptr;

    // Bounded visited set: if we exceed this, stop accepting new URLs
    static constexpr int MaxVisitedUrls = 200000;
};

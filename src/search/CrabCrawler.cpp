/*
 * OyNIx Browser v3.1 — CrabCrawler ("Crabs") implementation
 * Streams crawled pages directly to the Database/NyxSearch instead of
 * accumulating them in memory. Proper mutex discipline throughout.
 */

#include "CrabCrawler.h"
#include "NyxSearch.h"
#include "data/Database.h"
#include "data/WebCache.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>

// ── Singleton ───────────────────────────────────────────────────────────

CrabCrawler &CrabCrawler::instance()
{
    static CrabCrawler s;
    return s;
}

CrabCrawler::CrabCrawler(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_timer(new QTimer(this))
{
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &CrabCrawler::scheduleNext);
}

CrabCrawler::~CrabCrawler()
{
    stop();
}

// ── Configuration ───────────────────────────────────────────────────────

void CrabCrawler::configure(int maxDepth, int maxPages, int concurrency, bool followExternal)
{
    QMutexLocker lock(&m_mutex);
    m_maxDepth       = maxDepth;
    m_maxPages       = maxPages;
    m_concurrency    = qBound(1, concurrency, 8);
    m_followExternal = followExternal;
}

void CrabCrawler::setPolitenessDelay(int milliseconds)
{
    QMutexLocker lock(&m_mutex);
    m_politenessMs = qMax(100, milliseconds);
}

void CrabCrawler::setLanguageFilter(const QString &langCode)
{
    QMutexLocker lock(&m_mutex);
    m_languageFilter = langCode.trimmed().toLower();
}

QString CrabCrawler::languageFilter() const
{
    QMutexLocker lock(&m_mutex);
    return m_languageFilter;
}

// ── Crawl Actions ───────────────────────────────────────────────────────

void CrabCrawler::startList(const QStringList &urls)
{
    {
        QMutexLocker lock(&m_mutex);
        if (m_state == State::Crawling || m_state == State::Stopping)
            return;

        reset();
        m_isBroadMode = false;
        setState(State::Crawling);

        for (const auto &raw : urls) {
            const QUrl url(raw.trimmed());
            if (!url.isValid() || url.scheme().isEmpty())
                continue;
            const QString canon = url.toString(QUrl::RemoveFragment);
            if (!m_visited.contains(canon)) {
                m_seedDomains.insert(url.host());
                m_queue.enqueue(CrawlTask{url, 0, QString()});
            }
        }
    }
    scheduleNext();
}

void CrabCrawler::startBroad(const QString &seedUrl)
{
    {
        QMutexLocker lock(&m_mutex);
        if (m_state == State::Crawling || m_state == State::Stopping)
            return;

        reset();
        m_isBroadMode = true;
        setState(State::Crawling);

        const QUrl url(seedUrl.trimmed());
        if (url.isValid() && !url.scheme().isEmpty()) {
            m_seedDomains.insert(url.host());
            m_queue.enqueue(CrawlTask{url, 0, QString()});
        }
    }
    scheduleNext();
}

void CrabCrawler::stop()
{
    bool wasRunning = false;
    {
        QMutexLocker lock(&m_mutex);
        if (m_state == State::Idle)
            return;
        wasRunning = true;
        setState(State::Stopping);
        m_queue.clear();
        m_timer->stop();
    }

    // If no active requests, finish immediately
    if (wasRunning) {
        QMutexLocker lock(&m_mutex);
        if (m_activeReqs == 0) {
            setState(State::Idle);
            lock.unlock();
            emit finished();
        }
    }
}

void CrabCrawler::pause()
{
    QMutexLocker lock(&m_mutex);
    if (m_state == State::Crawling) {
        setState(State::Paused);
        m_timer->stop();
    }
}

void CrabCrawler::resume()
{
    {
        QMutexLocker lock(&m_mutex);
        if (m_state != State::Paused)
            return;
        setState(State::Crawling);
    }
    scheduleNext();
}

// ── Status (thread-safe) ────────────────────────────────────────────────

int CrabCrawler::crawledCount() const
{
    QMutexLocker lock(&m_mutex);
    return m_crawledCount;
}

int CrabCrawler::errorCount() const
{
    QMutexLocker lock(&m_mutex);
    return m_errorCount;
}

int CrabCrawler::queuedCount() const
{
    QMutexLocker lock(&m_mutex);
    return m_queue.size();
}

bool CrabCrawler::isRunning() const
{
    QMutexLocker lock(&m_mutex);
    return m_state == State::Crawling || m_state == State::Stopping;
}

CrabCrawler::State CrabCrawler::state() const
{
    QMutexLocker lock(&m_mutex);
    return m_state;
}

// ── Core Crawling Logic ─────────────────────────────────────────────────

void CrabCrawler::scheduleNext()
{
    QMutexLocker lock(&m_mutex);

    if (m_state != State::Crawling)
        return;

    while (m_activeReqs < m_concurrency && !m_queue.isEmpty()) {
        if (m_crawledCount >= m_maxPages) {
            m_queue.clear();
            break;
        }

        CrawlTask task = m_queue.dequeue();
        const QString canon = task.url.toString(QUrl::RemoveFragment);

        if (m_visited.contains(canon))
            continue;

        // Robots.txt check — must unlock before network call
        const QString domain = task.url.host();
        if (!m_robotsCache.contains(domain)) {
            m_queue.prepend(task);
            lock.unlock();
            fetchRobotsTxt(task.url);
            return;
        }

        if (!isAllowedByRobots(task.url))
            continue;

        // Politeness delay
        if (!canFetchFromDomain(domain)) {
            m_queue.prepend(task);
            if (!m_timer->isActive())
                m_timer->start(m_politenessMs);
            return;
        }

        m_visited.insert(canon);
        recordDomainAccess(domain);
        ++m_activeReqs;

        lock.unlock();
        fetchPage(task);
        lock.relock();
    }

    // Done?
    if (m_activeReqs == 0 && m_queue.isEmpty()) {
        setState(State::Idle);
        lock.unlock();
        emit finished();
    }
}

void CrabCrawler::fetchPage(const CrabCrawler::CrawlTask &task)
{
    QNetworkRequest request(task.url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
        QStringLiteral("OyNIxCrab/3.1 (+https://github.com/minerofthesoal/OyNIx)"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(15000);

    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, task]() {
        onPageFinished(reply, task);
        reply->deleteLater();
    });
}

void CrabCrawler::onPageFinished(QNetworkReply *reply, const CrabCrawler::CrawlTask &task)
{
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QUrl finalUrl  = reply->url();
    const QString canon  = finalUrl.toString(QUrl::RemoveFragment);

    QString pageTitle;
    QString pageSnippet;
    QString pageStatus;
    bool success = false;
    const QString source = m_isBroadMode ? QStringLiteral("nyx+") : QStringLiteral("nyx");

    if (reply->error() == QNetworkReply::NoError) {
        const QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();

        if (contentType.contains(QLatin1String("text/html"), Qt::CaseInsensitive) ||
            contentType.contains(QLatin1String("application/xhtml"), Qt::CaseInsensitive) ||
            contentType.isEmpty()) {

            const QByteArray body = reply->readAll();
            const QString html = QString::fromUtf8(body);

            // Language filter
            bool langOk = true;
            {
                QMutexLocker lock(&m_mutex);
                if (!m_languageFilter.isEmpty()) {
                    static const QRegularExpression langRx(
                        QStringLiteral("<html[^>]*\\slang=[\"']?([a-zA-Z-]+)"),
                        QRegularExpression::CaseInsensitiveOption);
                    auto m = langRx.match(html.left(2000));
                    if (m.hasMatch() && !m.captured(1).toLower().startsWith(m_languageFilter))
                        langOk = false;
                }
            }

            if (!langOk) {
                pageStatus = QStringLiteral("skipped-language");
            } else {
                pageTitle   = extractTitle(html);
                pageSnippet = extractTextContent(html);
                pageStatus  = QStringLiteral("ok");
                success = true;

                // Stream directly to search index + cache (no in-memory accumulation)
                const QString domain = finalUrl.host();
                Database::instance()->addPage(canon, pageTitle, pageSnippet, domain, source);
                WebCache::instance().store(canon, pageTitle, pageSnippet);
            }

            // Extract links for BFS
            int currentMaxDepth;
            {
                QMutexLocker lock(&m_mutex);
                currentMaxDepth = m_maxDepth;
            }

            if (task.depth < currentMaxDepth) {
                const QStringList links = extractLinks(html, finalUrl);
                QMutexLocker lock(&m_mutex);
                for (const auto &link : links) {
                    const QUrl linkUrl(link);
                    if (shouldCrawl(linkUrl, task.depth + 1)) {
                        const QString linkCanon = linkUrl.toString(QUrl::RemoveFragment);
                        if (!m_visited.contains(linkCanon))
                            m_queue.enqueue(CrawlTask{linkUrl, task.depth + 1, canon});
                    }
                }
            }
        } else {
            pageStatus = QStringLiteral("skipped-not-html");
        }
    } else {
        pageStatus = QStringLiteral("error");
    }

    // Update counters — mutex locked, then unlocked before signals
    int crawled, queued, errors;
    bool shouldFinish = false;
    {
        QMutexLocker lock(&m_mutex);
        ++m_crawledCount;
        if (!success)
            ++m_errorCount;
        --m_activeReqs;

        crawled = m_crawledCount;
        queued  = m_queue.size();
        errors  = m_errorCount;

        if (m_state == State::Stopping && m_activeReqs == 0) {
            setState(State::Idle);
            shouldFinish = true;
        }
    }

    // Emit signals WITHOUT mutex held (prevents deadlocks)
    emit pageIndexed(canon, pageTitle, pageStatus);
    emit progress(crawled, queued, errors);

    if (shouldFinish) {
        emit finished();
        return;
    }

    // Schedule more work via timer (keeps UI responsive)
    if (!m_timer->isActive())
        m_timer->start(m_politenessMs);
}

// ── Robots.txt ──────────────────────────────────────────────────────────

void CrabCrawler::fetchRobotsTxt(const QUrl &url)
{
    const QString domain = url.host();

    {
        QMutexLocker lock(&m_mutex);
        if (m_robotsCache.contains(domain))
            return;
        auto &rules = m_robotsCache[domain];
        if (rules.pending)
            return;
        rules.pending = true;
    }

    QUrl robotsUrl;
    robotsUrl.setScheme(url.scheme());
    robotsUrl.setHost(url.host());
    if (url.port() != -1)
        robotsUrl.setPort(url.port());
    robotsUrl.setPath(QStringLiteral("/robots.txt"));

    QNetworkRequest request(robotsUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader,
        QStringLiteral("OyNIxCrab/3.1 (+https://github.com/minerofthesoal/OyNIx)"));
    request.setTransferTimeout(10000);

    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, domain]() {
        onRobotsFetched(reply, domain);
        reply->deleteLater();
    });
}

void CrabCrawler::onRobotsFetched(QNetworkReply *reply, const QString &domain)
{
    {
        QMutexLocker lock(&m_mutex);
        if (reply->error() == QNetworkReply::NoError) {
            m_robotsCache[domain] = parseRobotsTxt(QString::fromUtf8(reply->readAll()));
        } else {
            m_robotsCache[domain] = RobotsRules{};
        }
        m_robotsCache[domain].fetched = true;
        m_robotsCache[domain].pending = false;
    }
    scheduleNext();
}

CrabCrawler::RobotsRules CrabCrawler::parseRobotsTxt(const QString &content)
{
    RobotsRules rules;
    rules.fetched = true;

    const QStringList lines = content.split(QLatin1Char('\n'));
    bool inBlock = false;

    for (const auto &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;

        if (line.startsWith(QLatin1String("User-agent:"), Qt::CaseInsensitive)) {
            const QString agent = line.mid(11).trimmed();
            inBlock = (agent == QLatin1String("*") ||
                       agent.contains(QLatin1String("OyNIx"), Qt::CaseInsensitive));
        } else if (inBlock && line.startsWith(QLatin1String("Disallow:"), Qt::CaseInsensitive)) {
            const QString path = line.mid(9).trimmed();
            if (!path.isEmpty())
                rules.disallowedPaths.append(path);
        }
    }
    return rules;
}

bool CrabCrawler::isAllowedByRobots(const QUrl &url) const
{
    const QString domain = url.host();
    auto it = m_robotsCache.constFind(domain);
    if (it == m_robotsCache.constEnd() || !it->fetched)
        return true;

    const QString path = url.path();
    for (const auto &disallowed : it->disallowedPaths) {
        if (path.startsWith(disallowed))
            return false;
    }
    return true;
}

// ── Politeness ──────────────────────────────────────────────────────────

bool CrabCrawler::canFetchFromDomain(const QString &domain) const
{
    auto it = m_domainLastAccess.constFind(domain);
    if (it == m_domainLastAccess.constEnd())
        return true;
    return (QDateTime::currentMSecsSinceEpoch() - it.value()) >= m_politenessMs;
}

void CrabCrawler::recordDomainAccess(const QString &domain)
{
    m_domainLastAccess[domain] = QDateTime::currentMSecsSinceEpoch();
}

// ── URL Filtering ───────────────────────────────────────────────────────

bool CrabCrawler::shouldCrawl(const QUrl &url, int depth) const
{
    if (!url.isValid())
        return false;

    const QString scheme = url.scheme().toLower();
    if (scheme != QLatin1String("http") && scheme != QLatin1String("https"))
        return false;

    if (depth > m_maxDepth)
        return false;

    // Bounded visited set
    if (m_visited.size() >= MaxVisitedUrls)
        return false;

    if (m_crawledCount + m_queue.size() >= m_maxPages)
        return false;

    // Same-domain check
    if (!m_followExternal) {
        const QString host = url.host();
        bool sameDomain = false;
        for (const auto &seed : m_seedDomains) {
            if (host == seed || host.endsWith(QLatin1Char('.') + seed)) {
                sameDomain = true;
                break;
            }
        }
        if (!sameDomain)
            return false;
    }

    // Skip non-content extensions
    const QString path = url.path().toLower();
    static const QStringList skipExt = {
        QStringLiteral(".jpg"),  QStringLiteral(".jpeg"), QStringLiteral(".png"),
        QStringLiteral(".gif"),  QStringLiteral(".svg"),  QStringLiteral(".webp"),
        QStringLiteral(".ico"),  QStringLiteral(".bmp"),  QStringLiteral(".pdf"),
        QStringLiteral(".zip"),  QStringLiteral(".tar"),  QStringLiteral(".gz"),
        QStringLiteral(".rar"),  QStringLiteral(".7z"),   QStringLiteral(".exe"),
        QStringLiteral(".dmg"),  QStringLiteral(".msi"),  QStringLiteral(".mp3"),
        QStringLiteral(".mp4"),  QStringLiteral(".avi"),  QStringLiteral(".mkv"),
        QStringLiteral(".wav"),  QStringLiteral(".flac"), QStringLiteral(".css"),
        QStringLiteral(".js"),   QStringLiteral(".woff"), QStringLiteral(".woff2"),
        QStringLiteral(".ttf"),  QStringLiteral(".eot"),
    };
    for (const auto &ext : skipExt) {
        if (path.endsWith(ext))
            return false;
    }

    return true;
}

// ── HTML Parsing ────────────────────────────────────────────────────────

QStringList CrabCrawler::extractLinks(const QString &html, const QUrl &baseUrl)
{
    QStringList links;

    static const QRegularExpression hrefRe(
        QStringLiteral("<a\\s[^>]*href\\s*=\\s*[\"']([^\"'#][^\"']*)[\"']"),
        QRegularExpression::CaseInsensitiveOption);

    auto it = hrefRe.globalMatch(html);
    while (it.hasNext()) {
        auto match = it.next();
        const QString raw = match.captured(1).trimmed();

        if (raw.isEmpty() || raw.startsWith(QLatin1String("javascript:")) ||
            raw.startsWith(QLatin1String("mailto:")) ||
            raw.startsWith(QLatin1String("tel:")) ||
            raw.startsWith(QLatin1String("data:")))
            continue;

        const QUrl resolved = baseUrl.resolved(QUrl(raw));
        if (resolved.isValid()) {
            QUrl clean = resolved;
            clean.setFragment(QString());
            links.append(clean.toString());
        }
    }

    return links;
}

QString CrabCrawler::extractTitle(const QString &html)
{
    static const QRegularExpression titleRe(
        QStringLiteral("<title[^>]*>([^<]*)</title>"),
        QRegularExpression::CaseInsensitiveOption);

    auto match = titleRe.match(html);
    if (match.hasMatch()) {
        QString title = match.captured(1).trimmed();
        title.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
        title.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
        title.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
        title.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
        title.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
        title.replace(QStringLiteral("&apos;"), QStringLiteral("'"));
        return title;
    }
    return {};
}

QString CrabCrawler::extractTextContent(const QString &html, int maxLength)
{
    QString text = html;

    static const QRegularExpression scriptRe(
        QStringLiteral("<script[^>]*>[\\s\\S]*?</script>"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression styleRe(
        QStringLiteral("<style[^>]*>[\\s\\S]*?</style>"),
        QRegularExpression::CaseInsensitiveOption);

    text.replace(scriptRe, QStringLiteral(" "));
    text.replace(styleRe, QStringLiteral(" "));

    static const QRegularExpression tagRe(QStringLiteral("<[^>]+>"));
    text.replace(tagRe, QStringLiteral(" "));

    text.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    text.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    text.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    text.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    text.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
    text.replace(QStringLiteral("&apos;"), QStringLiteral("'"));
    text.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));

    static const QRegularExpression entityRe(QStringLiteral("&#?\\w+;"));
    text.replace(entityRe, QStringLiteral(" "));

    static const QRegularExpression wsRe(QStringLiteral("\\s+"));
    text.replace(wsRe, QStringLiteral(" "));
    text = text.trimmed();

    if (text.length() > maxLength)
        text = text.left(maxLength - 3) + QStringLiteral("...");

    return text;
}

// ── State Management ────────────────────────────────────────────────────

void CrabCrawler::setState(State newState)
{
    if (m_state == newState)
        return;
    m_state = newState;

    QString str;
    switch (newState) {
    case State::Idle:     str = QStringLiteral("idle"); break;
    case State::Crawling: str = QStringLiteral("crawling"); break;
    case State::Paused:   str = QStringLiteral("paused"); break;
    case State::Stopping: str = QStringLiteral("stopping"); break;
    }

    QMetaObject::invokeMethod(this, [this, str]() {
        emit stateChanged(str);
    }, Qt::QueuedConnection);
}

void CrabCrawler::reset()
{
    m_queue.clear();
    m_visited.clear();
    m_crawledCount = 0;
    m_errorCount   = 0;
    m_activeReqs   = 0;
    m_seedDomains.clear();
    m_robotsCache.clear();
    m_domainLastAccess.clear();
}

void CrabCrawler::emitProgress()
{
    int c, q, e;
    {
        QMutexLocker lock(&m_mutex);
        c = m_crawledCount;
        q = m_queue.size();
        e = m_errorCount;
    }
    emit progress(c, q, e);
}

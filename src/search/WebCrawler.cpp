/*
 * OyNIx Browser v3.1 — WebCrawler implementation
 * Native C++ web crawler using QNetworkAccessManager for async HTTP.
 * Replaces the C# NativeAOT CoreBridge crawler dependency.
 */

#include "WebCrawler.h"
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

WebCrawler &WebCrawler::instance()
{
    static WebCrawler s;
    return s;
}

WebCrawler::WebCrawler(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_scheduleTimer(new QTimer(this))
{
    m_scheduleTimer->setSingleShot(true);
    connect(m_scheduleTimer, &QTimer::timeout, this, &WebCrawler::scheduleNext);
}

WebCrawler::~WebCrawler()
{
    stop();
}

// ── Configuration ───────────────────────────────────────────────────────

void WebCrawler::configure(int maxDepth, int maxPages, int concurrency, bool followExternal)
{
    QMutexLocker lock(&m_mutex);
    m_maxDepth       = maxDepth;
    m_maxPages       = maxPages;
    m_concurrency    = qBound(1, concurrency, 8);
    m_followExternal = followExternal;
}

void WebCrawler::setPolitenessDelay(int milliseconds)
{
    QMutexLocker lock(&m_mutex);
    m_politenessMs = qMax(0, milliseconds);
}

void WebCrawler::setLanguageFilter(const QString &langCode)
{
    QMutexLocker lock(&m_mutex);
    m_languageFilter = langCode.trimmed().toLower();
}

QString WebCrawler::languageFilter() const
{
    QMutexLocker lock(&m_mutex);
    return m_languageFilter;
}

// ── Crawl Actions ───────────────────────────────────────────────────────

void WebCrawler::startList(const QJsonArray &urls)
{
    {
        QMutexLocker lock(&m_mutex);
        if (m_state == State::Crawling || m_state == State::Stopping)
            return;

        reset();
        m_isBroadMode = false;
        setState(State::Crawling);

        for (const auto &val : urls) {
            const QUrl url(val.toString().trimmed());
            if (!url.isValid() || url.scheme().isEmpty())
                continue;

            const QString canon = url.toString(QUrl::RemoveFragment);
            if (m_visitedUrls.contains(canon))
                continue;

            m_seedDomains.insert(url.host());
            m_queue.enqueue(CrawlTask{url, 0, QString()});
        }
    }

    scheduleNext();
}

void WebCrawler::startBroad(const QString &seedUrl)
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

void WebCrawler::stop()
{
    QMutexLocker lock(&m_mutex);
    if (m_state == State::Idle)
        return;

    setState(State::Stopping);
    m_queue.clear();
    m_scheduleTimer->stop();

    // If no active requests, go idle immediately
    if (m_activeRequests == 0) {
        setState(State::Idle);
        QMetaObject::invokeMethod(this, [this]() {
            emit crawlFinished();
        }, Qt::QueuedConnection);
    }
}

void WebCrawler::pause()
{
    QMutexLocker lock(&m_mutex);
    if (m_state == State::Crawling) {
        setState(State::Paused);
        m_scheduleTimer->stop();
    }
}

void WebCrawler::resume()
{
    {
        QMutexLocker lock(&m_mutex);
        if (m_state != State::Paused)
            return;
        setState(State::Crawling);
    }
    scheduleNext();
}

// ── Status & Results ────────────────────────────────────────────────────

QJsonObject WebCrawler::status() const
{
    QMutexLocker lock(&m_mutex);
    QJsonObject obj;
    obj[QStringLiteral("running")] = (m_state == State::Crawling || m_state == State::Stopping);
    obj[QStringLiteral("crawled")] = m_results.size();
    obj[QStringLiteral("queued")]  = m_queue.size();
    obj[QStringLiteral("errored")] = m_errorCount;
    obj[QStringLiteral("active")]  = m_activeRequests;

    switch (m_state) {
    case State::Idle:     obj[QStringLiteral("state")] = QStringLiteral("idle"); break;
    case State::Crawling: obj[QStringLiteral("state")] = QStringLiteral("crawling"); break;
    case State::Paused:   obj[QStringLiteral("state")] = QStringLiteral("paused"); break;
    case State::Stopping: obj[QStringLiteral("state")] = QStringLiteral("stopping"); break;
    }

    return obj;
}

QJsonArray WebCrawler::results(int offset, int limit) const
{
    QMutexLocker lock(&m_mutex);
    QJsonArray slice;
    const int total = m_results.size();
    const int start = qBound(0, offset, total);
    const int end   = qMin(start + limit, total);

    for (int i = start; i < end; ++i)
        slice.append(m_results.at(i));

    return slice;
}

int WebCrawler::count() const
{
    QMutexLocker lock(&m_mutex);
    return m_results.size();
}

bool WebCrawler::isRunning() const
{
    QMutexLocker lock(&m_mutex);
    return m_state == State::Crawling || m_state == State::Stopping;
}

WebCrawler::State WebCrawler::state() const
{
    QMutexLocker lock(&m_mutex);
    return m_state;
}

// ── Core Crawling Logic ─────────────────────────────────────────────────

void WebCrawler::scheduleNext()
{
    QMutexLocker lock(&m_mutex);

    if (m_state != State::Crawling)
        return;

    // Launch requests up to the concurrency limit
    while (m_activeRequests < m_concurrency && !m_queue.isEmpty()) {
        // Check if we already have enough results
        if (m_results.size() >= m_maxPages) {
            m_queue.clear();
            break;
        }

        CrawlTask task = m_queue.dequeue();
        const QString canon = task.url.toString(QUrl::RemoveFragment);

        // Skip if already visited (may have been enqueued multiple times)
        if (m_visitedUrls.contains(canon))
            continue;

        // Check robots.txt
        const QString domain = task.url.host();
        if (!m_robotsCache.contains(domain)) {
            // Need to fetch robots.txt first; re-enqueue this task
            m_queue.prepend(task);
            fetchRobotsTxt(task.url);
            return; // Will resume after robots.txt is fetched
        }

        if (!isAllowedByRobots(task.url)) {
            qDebug() << "WebCrawler: blocked by robots.txt:" << canon;
            continue;
        }

        // Check politeness delay
        if (!canFetchFromDomain(domain)) {
            // Re-enqueue and try later
            m_queue.prepend(task);
            if (!m_scheduleTimer->isActive())
                m_scheduleTimer->start(m_politenessMs);
            return;
        }

        m_visitedUrls.insert(canon);
        recordDomainAccess(domain);
        ++m_activeRequests;

        // Unlock before making network call
        lock.unlock();
        fetchPage(task);
        lock.relock();
    }

    // If we have nothing active and queue is empty, we are done
    if (m_activeRequests == 0 && m_queue.isEmpty()) {
        setState(State::Idle);
        lock.unlock();
        emit crawlFinished();
    }
}

void WebCrawler::fetchPage(const CrawlTask &task)
{
    QNetworkRequest request(task.url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
        QStringLiteral("OyNIxCrawler/3.1 (+https://github.com/nicokimmel/OyNIx)"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(15000); // 15s timeout

    QNetworkReply *reply = m_nam->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply, task]() {
        onPageFinished(reply, task);
        reply->deleteLater();
    });
}

void WebCrawler::onPageFinished(QNetworkReply *reply, const CrawlTask &task)
{
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QUrl finalUrl  = reply->url(); // may differ due to redirects
    const QString canon  = finalUrl.toString(QUrl::RemoveFragment);

    QJsonObject pageObj;
    pageObj[QStringLiteral("url")]         = canon;
    pageObj[QStringLiteral("crawl_depth")] = task.depth;
    pageObj[QStringLiteral("status_code")] = statusCode;
    pageObj[QStringLiteral("source")]      = m_isBroadMode ? QStringLiteral("nyx+") : QStringLiteral("nyx");

    bool success = false;

    if (reply->error() == QNetworkReply::NoError) {
        const QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();

        // Only parse HTML responses
        if (contentType.contains(QLatin1String("text/html"), Qt::CaseInsensitive) ||
            contentType.contains(QLatin1String("application/xhtml"), Qt::CaseInsensitive) ||
            contentType.isEmpty()) {

            const QByteArray body = reply->readAll();
            const QString html = QString::fromUtf8(body);

            // Language filter: check <html lang="..."> attribute
            bool langOk = true;
            {
                QMutexLocker lock(&m_mutex);
                if (!m_languageFilter.isEmpty()) {
                    static const QRegularExpression langRx(
                        QStringLiteral("<html[^>]*\\slang=[\"']?([a-zA-Z-]+)"),
                        QRegularExpression::CaseInsensitiveOption);
                    auto m = langRx.match(html.left(2000)); // only scan head
                    if (m.hasMatch()) {
                        const QString pageLang = m.captured(1).toLower();
                        // Match "en" to "en", "en-US", etc.
                        if (!pageLang.startsWith(m_languageFilter))
                            langOk = false;
                    }
                }
            }

            if (!langOk) {
                pageObj[QStringLiteral("title")]           = QString();
                pageObj[QStringLiteral("content_snippet")] = QString();
                pageObj[QStringLiteral("status")]          = QStringLiteral("skipped-language");
            } else {
                const QString title   = extractTitle(html);
                const QString snippet = extractTextContent(html);

                pageObj[QStringLiteral("title")]           = title;
                pageObj[QStringLiteral("content_snippet")] = snippet;
                pageObj[QStringLiteral("status")]          = QStringLiteral("ok");
                success = true;

                // Cache the page content for offline search
                QJsonObject meta;
                meta[QStringLiteral("status_code")] = statusCode;
                meta[QStringLiteral("depth")]       = task.depth;
                WebCache::instance().store(canon, title, snippet, meta);
            }

            // Extract and enqueue links for BFS (even for skipped-language pages)
            if (task.depth < m_maxDepth) {
                const QStringList links = extractLinks(html, finalUrl);
                QMutexLocker lock(&m_mutex);
                for (const auto &link : links) {
                    const QUrl linkUrl(link);
                    if (shouldCrawl(linkUrl, task.depth + 1)) {
                        const QString linkCanon = linkUrl.toString(QUrl::RemoveFragment);
                        if (!m_visitedUrls.contains(linkCanon)) {
                            m_queue.enqueue(CrawlTask{linkUrl, task.depth + 1, canon});
                        }
                    }
                }
            }
        } else {
            pageObj[QStringLiteral("title")]           = QString();
            pageObj[QStringLiteral("content_snippet")] = QString();
            pageObj[QStringLiteral("status")]          = QStringLiteral("skipped-not-html");
        }
    } else {
        pageObj[QStringLiteral("title")]           = QString();
        pageObj[QStringLiteral("content_snippet")] = QString();
        pageObj[QStringLiteral("status")]          = QStringLiteral("error: %1").arg(reply->errorString());
        success = false;
    }

    // Update shared state
    {
        QMutexLocker lock(&m_mutex);
        if (!success)
            ++m_errorCount;
        m_results.append(pageObj);
        --m_activeRequests;
    }

    emit pageDiscovered(pageObj);

    {
        QMutexLocker lock(&m_mutex);
        emit crawlProgress(m_results.size(), m_maxPages);
    }

    // Check if we are stopping
    {
        QMutexLocker lock(&m_mutex);
        if (m_state == State::Stopping && m_activeRequests == 0) {
            setState(State::Idle);
            lock.unlock();
            emit crawlFinished();
            return;
        }
    }

    // Schedule more work — use a timer so the UI event loop stays responsive.
    // This prevents the browser from freezing during crawls.
    if (!m_scheduleTimer->isActive())
        m_scheduleTimer->start(m_politenessMs);
}

// ── Robots.txt ──────────────────────────────────────────────────────────

void WebCrawler::fetchRobotsTxt(const QUrl &url)
{
    const QString domain = url.host();

    {
        QMutexLocker lock(&m_mutex);
        // Avoid duplicate fetches
        if (m_robotsCache.contains(domain))
            return;

        RobotsRules &rules = m_robotsCache[domain];
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
        QStringLiteral("OyNIxCrawler/3.1 (+https://github.com/nicokimmel/OyNIx)"));
    request.setTransferTimeout(10000);

    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, domain]() {
        onRobotsFetched(reply, domain);
        reply->deleteLater();
    });
}

void WebCrawler::onRobotsFetched(QNetworkReply *reply, const QString &domain)
{
    {
        QMutexLocker lock(&m_mutex);

        if (reply->error() == QNetworkReply::NoError) {
            const QString content = QString::fromUtf8(reply->readAll());
            m_robotsCache[domain] = parseRobotsTxt(content);
        } else {
            // If robots.txt can't be fetched, allow everything
            m_robotsCache[domain] = RobotsRules{};
        }

        m_robotsCache[domain].fetched = true;
        m_robotsCache[domain].pending = false;
    }

    // Resume scheduling now that robots.txt is available
    scheduleNext();
}

WebCrawler::RobotsRules WebCrawler::parseRobotsTxt(const QString &content)
{
    RobotsRules rules;
    rules.fetched = true;

    // Simple parser: look for User-agent: * blocks and collect Disallow lines
    const QStringList lines = content.split(QLatin1Char('\n'));
    bool inWildcardBlock = false;

    for (const auto &rawLine : lines) {
        const QString line = rawLine.trimmed();

        // Skip comments and empty lines
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;

        if (line.startsWith(QLatin1String("User-agent:"), Qt::CaseInsensitive)) {
            const QString agent = line.mid(11).trimmed();
            inWildcardBlock = (agent == QLatin1String("*") ||
                               agent.contains(QLatin1String("OyNIx"), Qt::CaseInsensitive));
        } else if (inWildcardBlock && line.startsWith(QLatin1String("Disallow:"), Qt::CaseInsensitive)) {
            const QString path = line.mid(9).trimmed();
            if (!path.isEmpty())
                rules.disallowedPaths.append(path);
        }
    }

    return rules;
}

bool WebCrawler::isAllowedByRobots(const QUrl &url) const
{
    // m_mutex must already be locked by caller (or lockless context)
    const QString domain = url.host();
    auto it = m_robotsCache.constFind(domain);
    if (it == m_robotsCache.constEnd() || !it->fetched)
        return true; // Unknown domain — allow

    const QString path = url.path();
    for (const auto &disallowed : it->disallowedPaths) {
        if (path.startsWith(disallowed))
            return false;
    }
    return true;
}

// ── Politeness ──────────────────────────────────────────────────────────

bool WebCrawler::canFetchFromDomain(const QString &domain) const
{
    // m_mutex must already be locked
    auto it = m_domainLastAccess.constFind(domain);
    if (it == m_domainLastAccess.constEnd())
        return true;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    return (now - it.value()) >= m_politenessMs;
}

void WebCrawler::recordDomainAccess(const QString &domain)
{
    // m_mutex must already be locked
    m_domainLastAccess[domain] = QDateTime::currentMSecsSinceEpoch();
}

// ── URL Filtering ───────────────────────────────────────────────────────

bool WebCrawler::shouldCrawl(const QUrl &url, int depth) const
{
    // m_mutex must already be locked
    if (!url.isValid())
        return false;

    // Only HTTP(S)
    const QString scheme = url.scheme().toLower();
    if (scheme != QLatin1String("http") && scheme != QLatin1String("https"))
        return false;

    // Depth check
    if (depth > m_maxDepth)
        return false;

    // Max pages check
    if (m_results.size() + m_queue.size() >= m_maxPages)
        return false;

    // Domain check (unless following external links)
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

    // Skip common non-content file extensions
    const QString path = url.path().toLower();
    static const QStringList skipExtensions = {
        QStringLiteral(".jpg"), QStringLiteral(".jpeg"), QStringLiteral(".png"),
        QStringLiteral(".gif"), QStringLiteral(".svg"), QStringLiteral(".webp"),
        QStringLiteral(".ico"), QStringLiteral(".bmp"),
        QStringLiteral(".pdf"), QStringLiteral(".zip"), QStringLiteral(".tar"),
        QStringLiteral(".gz"), QStringLiteral(".rar"), QStringLiteral(".7z"),
        QStringLiteral(".exe"), QStringLiteral(".dmg"), QStringLiteral(".msi"),
        QStringLiteral(".mp3"), QStringLiteral(".mp4"), QStringLiteral(".avi"),
        QStringLiteral(".mkv"), QStringLiteral(".wav"), QStringLiteral(".flac"),
        QStringLiteral(".css"), QStringLiteral(".js"), QStringLiteral(".woff"),
        QStringLiteral(".woff2"), QStringLiteral(".ttf"), QStringLiteral(".eot"),
    };
    for (const auto &ext : skipExtensions) {
        if (path.endsWith(ext))
            return false;
    }

    return true;
}

// ── HTML Parsing Helpers ────────────────────────────────────────────────

QStringList WebCrawler::extractLinks(const QString &html, const QUrl &baseUrl)
{
    QStringList links;

    // Match href attributes in anchor tags
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
            // Normalize: remove fragment, ensure scheme
            QUrl clean = resolved;
            clean.setFragment(QString());
            links.append(clean.toString());
        }
    }

    return links;
}

QString WebCrawler::extractTitle(const QString &html)
{
    static const QRegularExpression titleRe(
        QStringLiteral("<title[^>]*>([^<]*)</title>"),
        QRegularExpression::CaseInsensitiveOption);

    auto match = titleRe.match(html);
    if (match.hasMatch()) {
        QString title = match.captured(1).trimmed();
        // Decode common HTML entities
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

QString WebCrawler::extractTextContent(const QString &html, int maxLength)
{
    QString text = html;

    // Remove script and style blocks entirely
    static const QRegularExpression scriptRe(
        QStringLiteral("<script[^>]*>[\\s\\S]*?</script>"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression styleRe(
        QStringLiteral("<style[^>]*>[\\s\\S]*?</style>"),
        QRegularExpression::CaseInsensitiveOption);

    text.replace(scriptRe, QStringLiteral(" "));
    text.replace(styleRe, QStringLiteral(" "));

    // Remove all HTML tags
    static const QRegularExpression tagRe(QStringLiteral("<[^>]+>"));
    text.replace(tagRe, QStringLiteral(" "));

    // Decode common HTML entities
    text.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    text.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    text.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    text.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    text.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
    text.replace(QStringLiteral("&apos;"), QStringLiteral("'"));
    text.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));

    // Remove numeric HTML entities
    static const QRegularExpression entityRe(QStringLiteral("&#?\\w+;"));
    text.replace(entityRe, QStringLiteral(" "));

    // Collapse whitespace
    static const QRegularExpression whitespaceRe(QStringLiteral("\\s+"));
    text.replace(whitespaceRe, QStringLiteral(" "));
    text = text.trimmed();

    // Truncate to maxLength
    if (text.length() > maxLength)
        text = text.left(maxLength - 3) + QStringLiteral("...");

    return text;
}

// ── State Management ────────────────────────────────────────────────────

void WebCrawler::setState(State newState)
{
    // m_mutex must already be locked
    if (m_state == newState)
        return;

    m_state = newState;

    QString stateStr;
    switch (newState) {
    case State::Idle:     stateStr = QStringLiteral("idle"); break;
    case State::Crawling: stateStr = QStringLiteral("crawling"); break;
    case State::Paused:   stateStr = QStringLiteral("paused"); break;
    case State::Stopping: stateStr = QStringLiteral("stopping"); break;
    }

    // Emit on the event loop to avoid deadlocks
    QMetaObject::invokeMethod(this, [this, stateStr]() {
        emit statusChanged(stateStr);
    }, Qt::QueuedConnection);
}

void WebCrawler::reset()
{
    // m_mutex must already be locked
    m_queue.clear();
    m_visitedUrls.clear();
    m_results      = QJsonArray();
    m_activeRequests = 0;
    m_errorCount    = 0;
    m_seedDomains.clear();
    m_robotsCache.clear();
    m_domainLastAccess.clear();
}

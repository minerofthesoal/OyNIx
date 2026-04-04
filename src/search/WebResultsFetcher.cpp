#include "WebResultsFetcher.h"

#include <QEventLoop>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSet>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

const QString WebResultsFetcher::UserAgent =
    QStringLiteral("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
                    "(KHTML, like Gecko) OyNIx/3.0 Chrome/120.0.0.0 Safari/537.36");

WebResultsFetcher::WebResultsFetcher(const QString &query, QObject *parent)
    : QThread(parent)
    , m_query(query)
{
}

WebResultsFetcher::~WebResultsFetcher()
{
    if (isRunning()) {
        quit();
        wait(2000);
    }
}

void WebResultsFetcher::run()
{
    QJsonArray ddgResults = _fetchDDG();
    QJsonArray googleResults = _fetchGoogle();
    QJsonArray merged = _deduplicateResults(ddgResults, googleResults);
    emit resultsReady(merged);
}

// ---------------------------------------------------------------------------
// DuckDuckGo HTML scraper
// ---------------------------------------------------------------------------
QJsonArray WebResultsFetcher::_fetchDDG()
{
    QJsonArray results;
    QNetworkAccessManager nam;

    QUrl url(QStringLiteral("https://html.duckduckgo.com/html/"));
    QUrlQuery params;
    params.addQueryItem(QStringLiteral("q"), m_query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
    request.setRawHeader("User-Agent", UserAgent.toUtf8());
    request.setTransferTimeout(RequestTimeoutMs);

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);

    QNetworkReply *reply = nam.post(request, params.toString(QUrl::FullyEncoded).toUtf8());
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(RequestTimeoutMs);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError || !timeout.isActive()) {
        reply->deleteLater();
        return results;
    }
    timeout.stop();

    const QString html = QString::fromUtf8(reply->readAll());
    reply->deleteLater();

    // Parse .result class items — each contains an <a class="result__a"> and
    // <a class="result__snippet">
    static const QRegularExpression resultBlock(
        QStringLiteral("<div[^>]*class=\"[^\"]*result[^\"]*\"[^>]*>(.*?)</div>\\s*</div>"),
        QRegularExpression::DotMatchesEverythingOption);

    // Simpler approach: find all result links and snippets directly
    static const QRegularExpression linkRx(
        QStringLiteral("<a[^>]*class=\"result__a\"[^>]*href=\"([^\"]*)\"[^>]*>(.*?)</a>"),
        QRegularExpression::DotMatchesEverythingOption);

    static const QRegularExpression snippetRx(
        QStringLiteral("<a[^>]*class=\"result__snippet\"[^>]*>(.*?)</a>"),
        QRegularExpression::DotMatchesEverythingOption);

    static const QRegularExpression tagStripper(QStringLiteral("<[^>]*>"));

    auto linkMatches = linkRx.globalMatch(html);
    auto snippetMatches = snippetRx.globalMatch(html);

    int count = 0;
    while (linkMatches.hasNext() && count < 30) {
        auto lm = linkMatches.next();
        QString href = lm.captured(1);
        QString title = lm.captured(2).remove(tagStripper).trimmed();

        // DDG wraps URLs through a redirect — extract actual URL
        if (href.contains(QStringLiteral("uddg="))) {
            QUrlQuery redirectQ(QUrl(href).query());
            href = QUrl::fromPercentEncoding(redirectQ.queryItemValue(QStringLiteral("uddg")).toUtf8());
        }

        QString snippet;
        if (snippetMatches.hasNext()) {
            snippet = snippetMatches.next().captured(1).remove(tagStripper).trimmed();
        }

        if (href.isEmpty() || title.isEmpty())
            continue;

        QJsonObject item;
        item[QStringLiteral("url")] = href;
        item[QStringLiteral("title")] = title;
        item[QStringLiteral("description")] = snippet;
        item[QStringLiteral("source")] = QStringLiteral("web");
        results.append(item);
        ++count;
    }

    return results;
}

// ---------------------------------------------------------------------------
// Google HTML scraper
// ---------------------------------------------------------------------------
QJsonArray WebResultsFetcher::_fetchGoogle()
{
    QJsonArray results;
    QNetworkAccessManager nam;

    QUrl url(QStringLiteral("https://www.google.com/search"));
    QUrlQuery params;
    params.addQueryItem(QStringLiteral("q"), m_query);
    params.addQueryItem(QStringLiteral("num"), QStringLiteral("30"));
    params.addQueryItem(QStringLiteral("hl"), QStringLiteral("en"));
    url.setQuery(params);

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", UserAgent.toUtf8());
    request.setRawHeader("Accept-Language", "en-US,en;q=0.9");
    request.setTransferTimeout(RequestTimeoutMs);

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);

    QNetworkReply *reply = nam.get(request);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(RequestTimeoutMs);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError || !timeout.isActive()) {
        reply->deleteLater();
        return results;
    }
    timeout.stop();

    const QString html = QString::fromUtf8(reply->readAll());
    reply->deleteLater();

    // Google wraps each result in <div class="g">
    static const QRegularExpression divG(
        QStringLiteral("<div class=\"g\">(.*?)</div>\\s*</div>\\s*</div>"),
        QRegularExpression::DotMatchesEverythingOption);

    static const QRegularExpression hrefRx(
        QStringLiteral("<a[^>]*href=\"(https?://[^\"]*)\"[^>]*>"));

    static const QRegularExpression h3Rx(
        QStringLiteral("<h3[^>]*>(.*?)</h3>"),
        QRegularExpression::DotMatchesEverythingOption);

    // Snippet often in a <div class="VwiC3b"> or similar; grab text after </h3>
    static const QRegularExpression spanSnippetRx(
        QStringLiteral("<span[^>]*>(.*?)</span>"),
        QRegularExpression::DotMatchesEverythingOption);

    static const QRegularExpression tagStripper(QStringLiteral("<[^>]*>"));

    auto divMatches = divG.globalMatch(html);
    int count = 0;

    while (divMatches.hasNext() && count < 30) {
        auto dm = divMatches.next();
        const QString block = dm.captured(1);

        auto hrefMatch = hrefRx.match(block);
        auto h3Match = h3Rx.match(block);

        if (!hrefMatch.hasMatch() || !h3Match.hasMatch())
            continue;

        const QString href = hrefMatch.captured(1);
        const QString title = h3Match.captured(1).remove(tagStripper).trimmed();

        // Collect snippet: all span text after the h3
        QString snippet;
        int h3End = h3Match.capturedEnd(0);
        QString afterH3 = block.mid(h3End);
        auto spanM = spanSnippetRx.match(afterH3);
        if (spanM.hasMatch()) {
            snippet = spanM.captured(1).remove(tagStripper).trimmed();
        }

        QJsonObject item;
        item[QStringLiteral("url")] = href;
        item[QStringLiteral("title")] = title;
        item[QStringLiteral("description")] = snippet;
        item[QStringLiteral("source")] = QStringLiteral("web");
        results.append(item);
        ++count;
    }

    return results;
}

// ---------------------------------------------------------------------------
// Deduplication
// ---------------------------------------------------------------------------
QJsonArray WebResultsFetcher::_deduplicateResults(const QJsonArray &ddg, const QJsonArray &google)
{
    QJsonArray merged;
    QSet<QString> seenUrls;

    auto addResults = [&](const QJsonArray &arr) {
        for (const QJsonValue &v : arr) {
            const QJsonObject obj = v.toObject();
            const QString url = QUrl(obj[QStringLiteral("url")].toString())
                                    .toString(QUrl::RemoveFragment | QUrl::StripTrailingSlash | QUrl::NormalizePathSegments);
            if (!seenUrls.contains(url)) {
                seenUrls.insert(url);
                merged.append(obj);
            }
        }
    };

    // Interleave: DDG first, then Google unique results
    addResults(ddg);
    addResults(google);

    return merged;
}

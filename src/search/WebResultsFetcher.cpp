#include "WebResultsFetcher.h"

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>
#include <QUrlQuery>

const QString WebResultsFetcher::UserAgent =
    QStringLiteral("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
                    "(KHTML, like Gecko) OyNIx/3.0 Chrome/120.0.0.0 Safari/537.36");

WebResultsFetcher::WebResultsFetcher(const QString &query, QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_query(query)
{
}

WebResultsFetcher::~WebResultsFetcher() = default;

void WebResultsFetcher::start()
{
    // Launch both requests in parallel — no threads needed
    fetchDDG();
    fetchGoogle();
}

void WebResultsFetcher::abort()
{
    m_aborted = true;
}

// ---------------------------------------------------------------------------
// DuckDuckGo request
// ---------------------------------------------------------------------------
void WebResultsFetcher::fetchDDG()
{
    QUrl url(QStringLiteral("https://html.duckduckgo.com/html/"));
    QUrlQuery params;
    params.addQueryItem(QStringLiteral("q"), m_query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
    request.setRawHeader("User-Agent", UserAgent.toUtf8());
    request.setTransferTimeout(RequestTimeoutMs);

    QNetworkReply *reply = m_nam->post(request, params.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onDDGFinished(reply);
    });
}

void WebResultsFetcher::onDDGFinished(QNetworkReply *reply)
{
    reply->deleteLater();
    if (!m_aborted && reply->error() == QNetworkReply::NoError) {
        m_ddgResults = parseDDG(QString::fromUtf8(reply->readAll()));
    }
    m_ddgDone = true;
    tryMergeAndEmit();
}

// ---------------------------------------------------------------------------
// Google request
// ---------------------------------------------------------------------------
void WebResultsFetcher::fetchGoogle()
{
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

    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onGoogleFinished(reply);
    });
}

void WebResultsFetcher::onGoogleFinished(QNetworkReply *reply)
{
    reply->deleteLater();
    if (!m_aborted && reply->error() == QNetworkReply::NoError) {
        m_googleResults = parseGoogle(QString::fromUtf8(reply->readAll()));
    }
    m_googleDone = true;
    tryMergeAndEmit();
}

// ---------------------------------------------------------------------------
// Merge when both done
// ---------------------------------------------------------------------------
void WebResultsFetcher::tryMergeAndEmit()
{
    if (!m_ddgDone || !m_googleDone || m_aborted)
        return;

    emit resultsReady(deduplicateResults(m_ddgResults, m_googleResults));
}

// ---------------------------------------------------------------------------
// DuckDuckGo HTML parser
// ---------------------------------------------------------------------------
QJsonArray WebResultsFetcher::parseDDG(const QString &html)
{
    QJsonArray results;

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
        QString title = lm.captured(2);
        title.remove(tagStripper);
        title = title.trimmed();

        // DDG wraps URLs through a redirect — extract actual URL
        if (href.contains(QStringLiteral("uddg="))) {
            QUrlQuery redirectQ(QUrl(href).query());
            href = QUrl::fromPercentEncoding(redirectQ.queryItemValue(QStringLiteral("uddg")).toUtf8());
        }

        QString snippet;
        if (snippetMatches.hasNext()) {
            snippet = snippetMatches.next().captured(1);
            snippet.remove(tagStripper);
            snippet = snippet.trimmed();
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
// Google HTML parser
// ---------------------------------------------------------------------------
QJsonArray WebResultsFetcher::parseGoogle(const QString &html)
{
    QJsonArray results;

    static const QRegularExpression divG(
        QStringLiteral("<div class=\"g\">(.*?)</div>\\s*</div>\\s*</div>"),
        QRegularExpression::DotMatchesEverythingOption);

    static const QRegularExpression hrefRx(
        QStringLiteral("<a[^>]*href=\"(https?://[^\"]*)\"[^>]*>"));

    static const QRegularExpression h3Rx(
        QStringLiteral("<h3[^>]*>(.*?)</h3>"),
        QRegularExpression::DotMatchesEverythingOption);

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
        QString title = h3Match.captured(1);
        title.remove(tagStripper);
        title = title.trimmed();

        // Collect snippet: all span text after the h3
        QString snippet;
        int h3End = h3Match.capturedEnd(0);
        QString afterH3 = block.mid(h3End);
        auto spanM = spanSnippetRx.match(afterH3);
        if (spanM.hasMatch()) {
            snippet = spanM.captured(1);
            snippet.remove(tagStripper);
            snippet = snippet.trimmed();
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
QJsonArray WebResultsFetcher::deduplicateResults(const QJsonArray &ddg, const QJsonArray &google)
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

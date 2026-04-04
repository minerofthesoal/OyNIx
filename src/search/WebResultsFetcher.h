#pragma once

#include <QJsonArray>
#include <QObject>

class QNetworkAccessManager;
class QNetworkReply;

/**
 * WebResultsFetcher — Async web search (DuckDuckGo + Google HTML scraping).
 * Uses QNetworkAccessManager on the main thread instead of QThread + QEventLoop,
 * which avoids SIGTRAP crashes from event-loop-in-thread misuse.
 */
class WebResultsFetcher : public QObject
{
    Q_OBJECT

public:
    explicit WebResultsFetcher(const QString &query, QObject *parent = nullptr);
    ~WebResultsFetcher() override;

    void start();
    void abort();

signals:
    void resultsReady(QJsonArray results);

private:
    void fetchDDG();
    void fetchGoogle();
    void onDDGFinished(QNetworkReply *reply);
    void onGoogleFinished(QNetworkReply *reply);
    void tryMergeAndEmit();
    QJsonArray parseDDG(const QString &html);
    QJsonArray parseGoogle(const QString &html);
    QJsonArray deduplicateResults(const QJsonArray &ddg, const QJsonArray &google);

    QNetworkAccessManager *m_nam = nullptr;
    QString m_query;
    QJsonArray m_ddgResults;
    QJsonArray m_googleResults;
    bool m_ddgDone = false;
    bool m_googleDone = false;
    bool m_aborted = false;

    static constexpr int RequestTimeoutMs = 8000;
    static const QString UserAgent;
};

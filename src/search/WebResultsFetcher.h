#pragma once

#include <QJsonArray>
#include <QThread>

class QNetworkAccessManager;

class WebResultsFetcher : public QThread
{
    Q_OBJECT

public:
    explicit WebResultsFetcher(const QString &query, QObject *parent = nullptr);
    ~WebResultsFetcher() override;

signals:
    void resultsReady(QJsonArray results);

protected:
    void run() override;

private:
    QJsonArray _fetchDDG();
    QJsonArray _fetchGoogle();
    QJsonArray _deduplicateResults(const QJsonArray &ddg, const QJsonArray &google);

    QString m_query;
    static constexpr int RequestTimeoutMs = 8000;
    static const QString UserAgent;
};

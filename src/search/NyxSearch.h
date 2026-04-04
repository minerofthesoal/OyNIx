#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>

class Database;
class WebResultsFetcher;

class NyxSearch : public QObject
{
    Q_OBJECT

public:
    static NyxSearch *instance();

    void indexPage(const QString &url, const QString &title, const QString &contentSnippet);
    [[nodiscard]] QJsonObject search(const QString &query);

    [[nodiscard]] QJsonObject getStats() const;
    bool exportIndex(const QString &path) const;

signals:
    void webResultsReady(QJsonArray results);

private:
    explicit NyxSearch(QObject *parent = nullptr);
    ~NyxSearch() override;

    void launchWebSearch(const QString &query);
    void onWebResults(const QJsonArray &results);

    Database *m_db = nullptr;
    WebResultsFetcher *m_fetcher = nullptr;
    static NyxSearch *s_instance;
};

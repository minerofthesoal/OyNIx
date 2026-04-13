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

    void indexPage(const QString &url, const QString &title, const QString &contentSnippet,
                   const QString &source = QStringLiteral("nyx"));
    void indexCrawledPage(const QJsonObject &page);
    [[nodiscard]] QJsonObject search(const QString &query);

    // Search mode: true = also fetch web results, false = internal only (OYN+Nyx)
    void setUseWebSearch(bool enabled);
    [[nodiscard]] bool useWebSearch() const;

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
    bool m_useWebSearch = true;
    static NyxSearch *s_instance;
};

#pragma once

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QMutex>
#include <QString>
#include <QStringList>

class QSqlDatabase;

class Database : public QObject
{
    Q_OBJECT

public:
    static Database *instance();

    [[nodiscard]] static QString configDir();

    // Site operations
    bool addSite(const QString &url, const QString &title, const QString &description,
                 const QString &category, double rating, const QString &source);
    bool addSitesBatch(const QJsonArray &sites);
    [[nodiscard]] QJsonArray searchSites(const QString &query, int limit = 50) const;
    [[nodiscard]] int getSiteCount() const;
    [[nodiscard]] bool compare_site(const QString &url1, const QString &url2) const;

    // Page operations
    bool addPage(const QString &url, const QString &title, const QString &content, const QString &domain);
    [[nodiscard]] QJsonArray searchPages(const QString &query, int limit = 50) const;

    // Import/export
    bool exportToJson(const QString &path) const;
    bool importFromJson(const QString &path);

    // Stats
    [[nodiscard]] QJsonObject getStats() const;

signals:
    void databaseChanged();

private:
    explicit Database(QObject *parent = nullptr);
    ~Database() override;

    void initDatabase();
    void createTables();

    QString m_dbPath;
    mutable QMutex m_mutex;
    static Database *s_instance;
};

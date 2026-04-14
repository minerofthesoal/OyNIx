#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>

class WebCache : public QObject
{
    Q_OBJECT

public:
    static WebCache &instance();

    [[nodiscard]] QString cacheDir() const;
    [[nodiscard]] qint64 cacheSize() const;

    // Store/retrieve cached page data (title, content, headers)
    bool store(const QString &url, const QString &title, const QString &content,
               const QJsonObject &meta = QJsonObject());
    [[nodiscard]] QJsonObject lookup(const QString &url) const;
    [[nodiscard]] bool contains(const QString &url) const;

    // Cache management
    void evict(const QString &url);
    void clear();
    void pruneOldEntries(int maxAgeDays = 30);

    [[nodiscard]] QJsonObject getStats() const;

private:
    explicit WebCache(QObject *parent = nullptr);

    [[nodiscard]] QString urlToPath(const QString &url) const;
    void scanCache(int &fileCount, qint64 &totalSize) const;

    QString m_cacheDir;
};

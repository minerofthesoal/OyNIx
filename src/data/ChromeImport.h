#pragma once

#include <QObject>
#include <QJsonArray>
#include <QString>

/**
 * ChromeImport — Import browsing history and search queries from Chrome.
 * Auto-detects Chrome's History database on Linux/macOS/Windows.
 */
class ChromeImport : public QObject
{
    Q_OBJECT

public:
    explicit ChromeImport(QObject *parent = nullptr);
    ~ChromeImport() override;

    /// Import up to maxEntries history items from Chrome
    [[nodiscard]] QJsonArray importHistory(int maxEntries = 5000) const;

    /// Extract search queries from Chrome history URLs
    [[nodiscard]] QJsonArray importSearchQueries(int maxEntries = 1000) const;

    /// Check if Chrome history database is accessible
    [[nodiscard]] bool isChromeAvailable() const;

    /// Get the detected Chrome history DB path
    [[nodiscard]] QString chromeHistoryPath() const;

signals:
    void importProgress(int current, int total);
    void importComplete(int count);

private:
    static QString detectChromePath();
    static QString extractSearchQuery(const QString &url);

    QString m_chromePath;
};

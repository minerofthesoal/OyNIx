#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

class GitHubSync : public QObject
{
    Q_OBJECT

public:
    explicit GitHubSync(QObject *parent = nullptr);
    ~GitHubSync() override;

    /// Configure GitHub sync parameters.
    /// @param token        Personal access token (classic or fine-grained).
    /// @param repo         Repository in "owner/repo" format.
    /// @param autoSync     Enable automatic periodic sync.
    /// @param intervalMinutes  Sync interval in minutes (minimum 30).
    void configure(const QString &token, const QString &repo,
                   bool autoSync, int intervalMinutes);

    void startAutoSync();
    void stopAutoSync();

    /// Upload a local JSON database file to the configured GitHub repo.
    void uploadDatabase(const QString &jsonFilePath);

    /// Import (download) a database JSON from a GitHub repo.
    /// @param repo  Repository in "owner/repo" format (can differ from configured repo).
    void importDatabase(const QString &repo);

    [[nodiscard]] bool isConfigured() const;
    [[nodiscard]] QJsonObject getConfig() const;

signals:
    void statusUpdate(QString message);
    void syncComplete(bool success, QString message);

private:
    void loadConfig();
    void saveConfig() const;

    [[nodiscard]] QString configDir() const;
    [[nodiscard]] QString configFilePath() const;

    void onUploadFinished(QNetworkReply *reply);
    void onImportFinished(QNetworkReply *reply);

    QNetworkAccessManager *m_nam = nullptr;
    QTimer *m_timer = nullptr;

    QString m_token;
    QString m_repo;
    bool m_autoSync = false;
    int m_intervalMinutes = 60;

    // Path to the file being uploaded (kept for the upload callback)
    QString m_pendingUploadPath;

    static constexpr const char *GitHubApiBase = "https://api.github.com";
    static constexpr const char *DefaultBranch = "main";
    static constexpr const char *DatabaseFileName = "oynix_database.json";
};

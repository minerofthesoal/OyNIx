#include "GitHubSync.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QStandardPaths>

GitHubSync::GitHubSync(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_timer(new QTimer(this))
{
    connect(m_timer, &QTimer::timeout, this, [this]() {
        emit statusUpdate(QStringLiteral("Auto-sync tick"));
    });
    loadConfig();
}

GitHubSync::~GitHubSync() = default;

QString GitHubSync::configDir() const {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QString GitHubSync::configFilePath() const {
    return configDir() + QStringLiteral("/sync/github_config.json");
}

void GitHubSync::loadConfig() {
    QFile file(configFilePath());
    if (!file.open(QIODevice::ReadOnly)) return;
    auto doc = QJsonDocument::fromJson(file.readAll());
    auto obj = doc.object();
    m_token = obj[QStringLiteral("token")].toString();
    m_repo = obj[QStringLiteral("repo")].toString();
    m_autoSync = obj[QStringLiteral("auto_sync")].toBool();
    m_intervalMinutes = qMax(30, obj[QStringLiteral("sync_interval_minutes")].toInt(60));
}

void GitHubSync::saveConfig() const {
    QDir().mkpath(configDir() + QStringLiteral("/sync"));
    QFile file(configFilePath());
    if (!file.open(QIODevice::WriteOnly)) return;
    QJsonObject obj;
    obj[QStringLiteral("token")] = m_token;
    obj[QStringLiteral("repo")] = m_repo;
    obj[QStringLiteral("auto_sync")] = m_autoSync;
    obj[QStringLiteral("sync_interval_minutes")] = m_intervalMinutes;
    file.write(QJsonDocument(obj).toJson());
}

void GitHubSync::configure(const QString &token, const QString &repo,
                           bool autoSync, int intervalMinutes) {
    m_token = token;
    m_repo = repo;
    m_autoSync = autoSync;
    m_intervalMinutes = qMax(30, intervalMinutes);
    saveConfig();
    stopAutoSync();
    if (autoSync) startAutoSync();
}

void GitHubSync::startAutoSync() {
    if (isConfigured() && m_autoSync) {
        m_timer->start(m_intervalMinutes * 60 * 1000);
        emit statusUpdate(QStringLiteral("Auto-sync enabled (every %1 min)").arg(m_intervalMinutes));
    }
}

void GitHubSync::stopAutoSync() { m_timer->stop(); }

bool GitHubSync::isConfigured() const {
    return !m_token.isEmpty() && !m_repo.isEmpty();
}

QJsonObject GitHubSync::getConfig() const {
    QJsonObject obj;
    obj[QStringLiteral("token")] = m_token;
    obj[QStringLiteral("repo")] = m_repo;
    obj[QStringLiteral("auto_sync")] = m_autoSync;
    obj[QStringLiteral("sync_interval_minutes")] = m_intervalMinutes;
    return obj;
}

void GitHubSync::uploadDatabase(const QString &jsonFilePath) {
    if (!isConfigured()) {
        emit statusUpdate(QStringLiteral("GitHub sync not configured"));
        return;
    }
    QFile file(jsonFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit syncComplete(false, QStringLiteral("Cannot read file"));
        return;
    }
    QByteArray content = file.readAll();
    QByteArray encoded = content.toBase64();
    QString urlStr = QStringLiteral("%1/repos/%2/contents/database/%3")
        .arg(QLatin1String(GitHubApiBase), m_repo, QLatin1String(DatabaseFileName));

    QNetworkRequest req{QUrl(urlStr)};
    req.setRawHeader("Authorization", ("token " + m_token).toUtf8());
    req.setRawHeader("Accept", "application/vnd.github.v3+json");
    m_pendingUploadPath = jsonFilePath;

    auto *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, encoded, urlStr]() {
        reply->deleteLater();
        QString sha;
        if (reply->error() == QNetworkReply::NoError) {
            auto doc = QJsonDocument::fromJson(reply->readAll());
            sha = doc.object()[QStringLiteral("sha")].toString();
        }
        QJsonObject data;
        data[QStringLiteral("message")] = QStringLiteral("Update OyNIx database");
        data[QStringLiteral("content")] = QString::fromLatin1(encoded);
        if (!sha.isEmpty()) data[QStringLiteral("sha")] = sha;

        QNetworkRequest putReq{QUrl(urlStr)};
        putReq.setRawHeader("Authorization", ("token " + m_token).toUtf8());
        putReq.setRawHeader("Accept", "application/vnd.github.v3+json");
        putReq.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        auto *putReply = m_nam->put(putReq, QJsonDocument(data).toJson());
        connect(putReply, &QNetworkReply::finished, this, [this, putReply]() {
            onUploadFinished(putReply);
        });
    });
    emit statusUpdate(QStringLiteral("Uploading to GitHub..."));
}

void GitHubSync::onUploadFinished(QNetworkReply *reply) {
    reply->deleteLater();
    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    emit syncComplete(status == 200 || status == 201,
        status == 200 || status == 201 ? QStringLiteral("Database uploaded")
                                       : QStringLiteral("Error: %1").arg(status));
}

void GitHubSync::importDatabase(const QString &repo) {
    QString targetRepo = repo.isEmpty() ? m_repo : repo;
    if (targetRepo.isEmpty()) { emit statusUpdate(QStringLiteral("No repo specified")); return; }
    QString urlStr = QStringLiteral("https://raw.githubusercontent.com/%1/%2/database/%3")
        .arg(targetRepo, QLatin1String(DefaultBranch), QLatin1String(DatabaseFileName));

    QNetworkRequest req{QUrl(urlStr)};
    if (!m_token.isEmpty())
        req.setRawHeader("Authorization", ("token " + m_token).toUtf8());
    auto *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onImportFinished(reply); });
    emit statusUpdate(QStringLiteral("Importing from GitHub..."));
}

void GitHubSync::onImportFinished(QNetworkReply *reply) {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        emit syncComplete(false, QStringLiteral("Import failed: ") + reply->errorString());
        return;
    }
    auto doc = QJsonDocument::fromJson(reply->readAll());
    if (doc.isArray()) {
        emit syncComplete(true, QStringLiteral("Imported %1 entries").arg(doc.array().size()));
    } else {
        emit syncComplete(false, QStringLiteral("Invalid format"));
    }
}

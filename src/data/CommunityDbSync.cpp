#include "CommunityDbSync.h"
#include "Database.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDebug>

// Community DB hosted on the project's GitHub repo (raw URL).
// Falls back gracefully if the URL 404s or the network is down.
static const char *kCommunityDbUrl =
    "https://raw.githubusercontent.com/minerofthesoal/OyNIx/main/data/community.jsonl";

CommunityDbSync &CommunityDbSync::instance()
{
    static CommunityDbSync s;
    return s;
}

CommunityDbSync::CommunityDbSync(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

void CommunityDbSync::fetchOnStartup()
{
    if (m_fetched)
        return;
    m_fetched = true;

    // Check if we already fetched today (avoid hammering GitHub on every launch)
    const QString cacheDir = Database::configDir();
    const QString stampFile = cacheDir + QStringLiteral("/.community_sync");
    QFile stamp(stampFile);
    if (stamp.exists()) {
        const auto lastModified = QFileInfo(stampFile).lastModified();
        if (lastModified.isValid() && lastModified.daysTo(QDateTime::currentDateTime()) < 1) {
            qDebug() << "CommunityDbSync: skipping — last sync was" << lastModified;
            return;
        }
    }

    qDebug() << "CommunityDbSync: fetching community database...";

    QNetworkRequest request(QUrl(QString::fromLatin1(kCommunityDbUrl)));
    request.setHeader(QNetworkRequest::UserAgentHeader,
        QStringLiteral("OyNIx/3.1 (+https://github.com/minerofthesoal/OyNIx)"));
    request.setTransferTimeout(15000);

    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onFetchFinished(reply);
        reply->deleteLater();
    });
}

void CommunityDbSync::onFetchFinished(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "CommunityDbSync: fetch failed:" << reply->errorString();
        return;
    }

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status != 200) {
        qDebug() << "CommunityDbSync: HTTP" << status << "— skipping";
        return;
    }

    const QByteArray data = reply->readAll();
    if (data.isEmpty()) {
        qDebug() << "CommunityDbSync: empty response";
        return;
    }

    // Import into database
    bool ok = Database::instance()->importFromJsonl(data);
    qDebug() << "CommunityDbSync: import" << (ok ? "succeeded" : "failed")
             << "(" << data.split('\n').count() << "lines)";

    // Write sync timestamp
    const QString cacheDir = Database::configDir();
    QDir().mkpath(cacheDir);
    QFile stamp(cacheDir + QStringLiteral("/.community_sync"));
    if (stamp.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        stamp.write(QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toUtf8());
    }
}

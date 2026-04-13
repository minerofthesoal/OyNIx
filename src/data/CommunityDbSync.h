#pragma once

#include <QObject>

class QNetworkAccessManager;
class QNetworkReply;

class CommunityDbSync : public QObject
{
    Q_OBJECT

public:
    static CommunityDbSync &instance();

    void fetchOnStartup();

private:
    explicit CommunityDbSync(QObject *parent = nullptr);

    void onFetchFinished(QNetworkReply *reply);

    QNetworkAccessManager *m_nam = nullptr;
    bool m_fetched = false;
};

#pragma once

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

class CredentialManager : public QObject
{
    Q_OBJECT

public:
    explicit CredentialManager(const QString &profileDir, QObject *parent = nullptr);
    ~CredentialManager() override = default;

    bool saveCredential(const QString &domain, const QString &username, const QString &password);
    [[nodiscard]] QJsonObject getCredential(const QString &domain) const;
    bool deleteCredential(const QString &domain);
    [[nodiscard]] QStringList getAllDomains() const;
    bool setAutoLogin(const QString &domain, bool enabled);
    [[nodiscard]] QString getAutoLoginJs(const QString &domain) const;

signals:
    void credentialsChanged();

private:
    void load();
    bool save() const;

    [[nodiscard]] static QByteArray deriveKey(const QString &domain);
    [[nodiscard]] static QByteArray xorEncrypt(const QByteArray &data, const QByteArray &key);

    static constexpr quint8 FormatVersion = 0x02;

    QString m_filePath;
    QJsonArray m_credentials;
};

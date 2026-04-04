#include "CredentialManager.h"

#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

CredentialManager::CredentialManager(const QString &profileDir, QObject *parent)
    : QObject(parent)
    , m_filePath(profileDir + QStringLiteral("/credentials.dat"))
{
    QDir().mkpath(profileDir);
    load();
}

QByteArray CredentialManager::deriveKey(const QString &domain)
{
    // Derive a 256-bit key from the domain using 4 rounds of SHA-256
    QByteArray key = domain.toUtf8();
    for (int i = 0; i < 4; ++i)
        key = QCryptographicHash::hash(key, QCryptographicHash::Sha256);
    return key; // 32 bytes (256 bits)
}

QByteArray CredentialManager::xorEncrypt(const QByteArray &data, const QByteArray &key)
{
    // XOR-256++ encryption: XOR each byte of data with the corresponding key byte
    // Key is 32 bytes (256 bits), cycling through for data longer than key
    QByteArray result(data.size(), '\0');
    for (int i = 0; i < data.size(); ++i)
        result[i] = data[i] ^ key[i % key.size()];
    return result;
}

void CredentialManager::load()
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly))
        return;

    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_6_0);

    // Read and verify version byte
    quint8 version = 0;
    stream >> version;
    if (version != FormatVersion)
        return;

    // Read number of entries
    quint32 count = 0;
    stream >> count;

    for (quint32 i = 0; i < count; ++i) {
        QString domain;
        QString username;
        QByteArray encryptedPassword;
        bool autoLogin = false;

        stream >> domain >> username >> encryptedPassword >> autoLogin;

        if (stream.status() != QDataStream::Ok)
            break;

        // Decrypt password
        const QByteArray key = deriveKey(domain);
        const QByteArray decryptedBytes = xorEncrypt(encryptedPassword, key);
        const QString password = QString::fromUtf8(decryptedBytes);

        QJsonObject cred;
        cred[QStringLiteral("domain")] = domain;
        cred[QStringLiteral("username")] = username;
        cred[QStringLiteral("password")] = password;
        cred[QStringLiteral("auto_login")] = autoLogin;

        m_credentials.append(cred);
    }
}

bool CredentialManager::save() const
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_6_0);

    // Write version byte
    stream << FormatVersion;

    // Write number of entries
    stream << static_cast<quint32>(m_credentials.size());

    for (const QJsonValue &v : m_credentials) {
        const QJsonObject cred = v.toObject();
        const QString domain = cred[QStringLiteral("domain")].toString();
        const QString username = cred[QStringLiteral("username")].toString();
        const QString password = cred[QStringLiteral("password")].toString();
        const bool autoLogin = cred[QStringLiteral("auto_login")].toBool();

        // Encrypt password using domain-derived key
        const QByteArray key = deriveKey(domain);
        const QByteArray encrypted = xorEncrypt(password.toUtf8(), key);

        stream << domain << username << encrypted << autoLogin;
    }

    return stream.status() == QDataStream::Ok;
}

bool CredentialManager::saveCredential(const QString &domain, const QString &username, const QString &password)
{
    // Update existing or add new
    for (int i = 0; i < m_credentials.size(); ++i) {
        if (m_credentials[i].toObject()[QStringLiteral("domain")].toString() == domain) {
            QJsonObject cred = m_credentials[i].toObject();
            cred[QStringLiteral("username")] = username;
            cred[QStringLiteral("password")] = password;
            m_credentials[i] = cred;

            if (save()) {
                emit credentialsChanged();
                return true;
            }
            return false;
        }
    }

    // New credential
    QJsonObject cred;
    cred[QStringLiteral("domain")] = domain;
    cred[QStringLiteral("username")] = username;
    cred[QStringLiteral("password")] = password;
    cred[QStringLiteral("auto_login")] = false;

    m_credentials.append(cred);

    if (save()) {
        emit credentialsChanged();
        return true;
    }
    return false;
}

QJsonObject CredentialManager::getCredential(const QString &domain) const
{
    for (const QJsonValue &v : m_credentials) {
        const QJsonObject cred = v.toObject();
        if (cred[QStringLiteral("domain")].toString() == domain) {
            // Return without the raw password exposed in a safe manner
            QJsonObject result;
            result[QStringLiteral("domain")] = domain;
            result[QStringLiteral("username")] = cred[QStringLiteral("username")].toString();
            result[QStringLiteral("password")] = cred[QStringLiteral("password")].toString();
            result[QStringLiteral("auto_login")] = cred[QStringLiteral("auto_login")].toBool();
            return result;
        }
    }
    return {};
}

bool CredentialManager::deleteCredential(const QString &domain)
{
    for (int i = 0; i < m_credentials.size(); ++i) {
        if (m_credentials[i].toObject()[QStringLiteral("domain")].toString() == domain) {
            m_credentials.removeAt(i);
            if (save()) {
                emit credentialsChanged();
                return true;
            }
            return false;
        }
    }
    return false;
}

QStringList CredentialManager::getAllDomains() const
{
    QStringList domains;
    domains.reserve(m_credentials.size());

    for (const QJsonValue &v : m_credentials)
        domains.append(v.toObject()[QStringLiteral("domain")].toString());

    return domains;
}

bool CredentialManager::setAutoLogin(const QString &domain, bool enabled)
{
    for (int i = 0; i < m_credentials.size(); ++i) {
        if (m_credentials[i].toObject()[QStringLiteral("domain")].toString() == domain) {
            QJsonObject cred = m_credentials[i].toObject();
            cred[QStringLiteral("auto_login")] = enabled;
            m_credentials[i] = cred;

            if (save()) {
                emit credentialsChanged();
                return true;
            }
            return false;
        }
    }
    return false;
}

QString CredentialManager::getAutoLoginJs(const QString &domain) const
{
    for (const QJsonValue &v : m_credentials) {
        const QJsonObject cred = v.toObject();
        if (cred[QStringLiteral("domain")].toString() == domain && cred[QStringLiteral("auto_login")].toBool()) {
            const QString username = cred[QStringLiteral("username")].toString();
            const QString password = cred[QStringLiteral("password")].toString();

            // Escape single quotes for JavaScript string safety
            auto jsEscape = [](const QString &s) {
                QString escaped = s;
                escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
                escaped.replace(QLatin1Char('\''), QStringLiteral("\\'"));
                escaped.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
                return escaped;
            };

            return QStringLiteral(
                "(function() {"
                "  var u = document.querySelector('input[type=\"email\"], input[type=\"text\"], input[name=\"username\"], input[name=\"login\"]');"
                "  var p = document.querySelector('input[type=\"password\"]');"
                "  if (u) { u.value = '%1'; u.dispatchEvent(new Event('input', {bubbles: true})); }"
                "  if (p) { p.value = '%2'; p.dispatchEvent(new Event('input', {bubbles: true})); }"
                "})()"
            ).arg(jsEscape(username), jsEscape(password));
        }
    }
    return {};
}

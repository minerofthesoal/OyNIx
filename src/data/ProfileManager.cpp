#include "ProfileManager.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

ProfileManager::ProfileManager(QObject *parent)
    : QObject(parent)
{
#ifdef Q_OS_WIN
    m_configDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#else
    m_configDir = QDir::homePath() + QStringLiteral("/.config/oynix");
#endif
    QDir().mkpath(m_configDir);
    m_filePath = m_configDir + QStringLiteral("/profiles.json");

    load();
    ensureDefaultProfile();
}

void ProfileManager::load()
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return;

    const QJsonObject root = doc.object();
    m_profiles = root[QStringLiteral("profiles")].toArray();
    m_activeProfileName = root[QStringLiteral("active")].toString();
}

bool ProfileManager::save() const
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    QJsonObject root;
    root[QStringLiteral("profiles")] = m_profiles;
    root[QStringLiteral("active")] = m_activeProfileName;

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

void ProfileManager::ensureDefaultProfile()
{
    if (!m_profiles.isEmpty())
        return;

    // Create default profile with no password
    const QString name = QStringLiteral("Default");
    const QString dir = profileDir(name);
    QDir().mkpath(dir);

    QJsonObject profile;
    profile[QStringLiteral("name")] = name;
    profile[QStringLiteral("data_dir")] = dir;
    profile[QStringLiteral("created_at")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    profile[QStringLiteral("is_active")] = true;
    profile[QStringLiteral("password_hash")] = QString();

    m_profiles.append(profile);
    m_activeProfileName = name;
    save();

    // Create empty per-profile files
    QFile(dir + QStringLiteral("/bookmarks.json")).open(QIODevice::WriteOnly);
    QFile(dir + QStringLiteral("/history.json")).open(QIODevice::WriteOnly);
    QFile(dir + QStringLiteral("/browser_config.json")).open(QIODevice::WriteOnly);
}

QString ProfileManager::hashPassword(const QString &password)
{
    if (password.isEmpty())
        return {};

    QByteArray data = password.toUtf8();
    for (int i = 0; i < 4; ++i)
        data = QCryptographicHash::hash(data, QCryptographicHash::Sha256);

    return QString::fromLatin1(data.toHex());
}

QString ProfileManager::profileDir(const QString &name) const
{
    return m_configDir + QStringLiteral("/profiles/") + name.toLower();
}

bool ProfileManager::createProfile(const QString &name, const QString &password)
{
    // Check for duplicate names
    for (const QJsonValue &v : m_profiles) {
        if (v.toObject()[QStringLiteral("name")].toString().compare(name, Qt::CaseInsensitive) == 0)
            return false;
    }

    const QString dir = profileDir(name);
    QDir().mkpath(dir);

    QJsonObject profile;
    profile[QStringLiteral("name")] = name;
    profile[QStringLiteral("data_dir")] = dir;
    profile[QStringLiteral("created_at")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    profile[QStringLiteral("is_active")] = false;
    profile[QStringLiteral("password_hash")] = hashPassword(password);

    m_profiles.append(profile);

    // Create empty per-profile files
    QFile(dir + QStringLiteral("/bookmarks.json")).open(QIODevice::WriteOnly);
    QFile(dir + QStringLiteral("/history.json")).open(QIODevice::WriteOnly);
    QFile(dir + QStringLiteral("/browser_config.json")).open(QIODevice::WriteOnly);

    return save();
}

bool ProfileManager::switchProfile(const QString &name, const QString &password)
{
    for (int i = 0; i < m_profiles.size(); ++i) {
        QJsonObject profile = m_profiles[i].toObject();
        if (profile[QStringLiteral("name")].toString().compare(name, Qt::CaseInsensitive) != 0)
            continue;

        // Verify password if one is set
        const QString storedHash = profile[QStringLiteral("password_hash")].toString();
        if (!storedHash.isEmpty()) {
            if (hashPassword(password) != storedHash)
                return false;
        }

        // Deactivate all profiles
        for (int j = 0; j < m_profiles.size(); ++j) {
            QJsonObject p = m_profiles[j].toObject();
            p[QStringLiteral("is_active")] = false;
            m_profiles[j] = p;
        }

        // Activate the target profile
        profile[QStringLiteral("is_active")] = true;
        m_profiles[i] = profile;
        m_activeProfileName = profile[QStringLiteral("name")].toString();

        if (save()) {
            emit profileChanged(m_activeProfileName);
            return true;
        }
        return false;
    }

    return false; // Profile not found
}

bool ProfileManager::deleteProfile(const QString &name)
{
    // Cannot delete the active profile
    if (name.compare(m_activeProfileName, Qt::CaseInsensitive) == 0)
        return false;

    for (int i = 0; i < m_profiles.size(); ++i) {
        if (m_profiles[i].toObject()[QStringLiteral("name")].toString().compare(name, Qt::CaseInsensitive) == 0) {
            // Remove the profile data directory
            const QString dir = profileDir(name);
            QDir(dir).removeRecursively();

            m_profiles.removeAt(i);
            return save();
        }
    }

    return false;
}

QJsonObject ProfileManager::activeProfile() const
{
    for (const QJsonValue &v : m_profiles) {
        const QJsonObject profile = v.toObject();
        if (profile[QStringLiteral("name")].toString() == m_activeProfileName)
            return profile;
    }
    return {};
}

QJsonArray ProfileManager::listProfiles() const
{
    // Return profiles without password hashes
    QJsonArray result;
    for (const QJsonValue &v : m_profiles) {
        QJsonObject profile = v.toObject();
        const bool hasPassword = !profile[QStringLiteral("password_hash")].toString().isEmpty();
        profile.remove(QStringLiteral("password_hash"));
        profile[QStringLiteral("has_password")] = hasPassword;
        result.append(profile);
    }
    return result;
}

QString ProfileManager::activeProfileDir() const
{
    return profileDir(m_activeProfileName);
}

QString ProfileManager::activeProfileName() const
{
    return m_activeProfileName;
}

#pragma once

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

class ProfileManager : public QObject
{
    Q_OBJECT

public:
    explicit ProfileManager(QObject *parent = nullptr);
    ~ProfileManager() override = default;

    bool createProfile(const QString &name, const QString &password = QString());
    bool switchProfile(const QString &name, const QString &password = QString());
    bool deleteProfile(const QString &name);

    [[nodiscard]] QJsonObject activeProfile() const;
    [[nodiscard]] QJsonArray listProfiles() const;
    [[nodiscard]] QString activeProfileDir() const;
    [[nodiscard]] QString activeProfileName() const;

signals:
    void profileChanged(const QString &name);

private:
    void load();
    bool save() const;
    void ensureDefaultProfile();

    [[nodiscard]] static QString hashPassword(const QString &password);
    [[nodiscard]] QString profileDir(const QString &name) const;

    QString m_configDir;
    QString m_filePath;
    QJsonArray m_profiles;
    QString m_activeProfileName;
};

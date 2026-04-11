#pragma once

#include <QObject>
#include <QJsonObject>
#include <QString>

/**
 * NydtaFormat — .nydta profile archive format (zip-based).
 * Exports/imports: history, database, settings, credentials, profile info.
 * Version 2.0 format.
 */
class NydtaFormat : public QObject
{
    Q_OBJECT

public:
    explicit NydtaFormat(QObject *parent = nullptr);
    ~NydtaFormat() override;

    /// Export current profile to .nydta file
    bool exportProfile(const QString &outputPath,
                       const QJsonObject &settings,
                       const QJsonObject &profileInfo) const;

    /// Import profile from .nydta file
    bool importProfile(const QString &nydtaPath) const;

    /// Read manifest from .nydta without full import
    [[nodiscard]] QJsonObject readManifest(const QString &nydtaPath) const;

signals:
    void exportProgress(int percent);
    void importProgress(int percent);
    void operationComplete(bool success, const QString &message);

private:
    static QString configDir();
    static constexpr const char *FormatVersion = "2.0";
    static constexpr const char *ManifestFile  = "manifest.json";
};

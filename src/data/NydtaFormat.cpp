#include "NydtaFormat.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDateTime>
#include <QProcess>
#include <QTemporaryDir>

// ── Constructor / Destructor ─────────────────────────────────────────
NydtaFormat::NydtaFormat(QObject *parent)
    : QObject(parent)
{
}

NydtaFormat::~NydtaFormat() = default;

QString NydtaFormat::configDir()
{
#ifdef Q_OS_WIN
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
           + QStringLiteral("/oynix");
#else
    return QDir::homePath() + QStringLiteral("/.config/oynix");
#endif
}

// ── Export profile ───────────────────────────────────────────────────
bool NydtaFormat::exportProfile(const QString &outputPath,
                                 const QJsonObject &settings,
                                 const QJsonObject &profileInfo) const
{
    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        emit const_cast<NydtaFormat *>(this)->operationComplete(false, QStringLiteral("Cannot create temp directory"));
        return false;
    }

    const QString base = tempDir.path();
    emit const_cast<NydtaFormat *>(this)->exportProgress(10);

    // 1. Manifest
    QJsonObject manifest;
    manifest[QStringLiteral("format")]  = QStringLiteral("nydta");
    manifest[QStringLiteral("version")] = QLatin1String(FormatVersion);
    manifest[QStringLiteral("created")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    manifest[QStringLiteral("app")]     = QStringLiteral("OyNIx Browser 3.0");
    {
        QFile f(base + QStringLiteral("/manifest.json"));
        if (f.open(QIODevice::WriteOnly))
            f.write(QJsonDocument(manifest).toJson());
    }

    emit const_cast<NydtaFormat *>(this)->exportProgress(20);

    // 2. Settings
    {
        // Human-readable
        QFile md(base + QStringLiteral("/settings.md"));
        if (md.open(QIODevice::WriteOnly)) {
            md.write("# OyNIx Settings Export\n\n");
            for (auto it = settings.begin(); it != settings.end(); ++it) {
                md.write(QStringLiteral("- **%1**: %2\n").arg(it.key(), it.value().toVariant().toString()).toUtf8());
            }
        }
        // Machine-readable
        QFile json(base + QStringLiteral("/settings.json"));
        if (json.open(QIODevice::WriteOnly))
            json.write(QJsonDocument(settings).toJson());
    }

    emit const_cast<NydtaFormat *>(this)->exportProgress(40);

    // 3. Profile info
    {
        QFile f(base + QStringLiteral("/profile_info.json"));
        if (f.open(QIODevice::WriteOnly))
            f.write(QJsonDocument(profileInfo).toJson());
    }

    // 4. Copy data files if they exist
    const QStringList dataFiles = {
        QStringLiteral("history.json"),
        QStringLiteral("bookmarks.json"),
        QStringLiteral("credentials.json"),
    };

    for (const auto &fileName : dataFiles) {
        const QString srcPath = configDir() + QLatin1Char('/') + fileName;
        if (QFileInfo::exists(srcPath)) {
            QFile::copy(srcPath, base + QLatin1Char('/') + fileName);
        }
    }

    emit const_cast<NydtaFormat *>(this)->exportProgress(70);

    // 5. Create zip archive (using system zip or QProcess)
    QProcess zip;
    zip.setWorkingDirectory(base);
    zip.start(QStringLiteral("zip"), {QStringLiteral("-r"), outputPath, QStringLiteral(".")});
    if (!zip.waitForFinished(30000)) {
        // Fallback: just copy the directory as-is if zip not available
        emit const_cast<NydtaFormat *>(this)->operationComplete(false,
            QStringLiteral("zip command not available. Install zip package."));
        return false;
    }

    emit const_cast<NydtaFormat *>(this)->exportProgress(100);
    emit const_cast<NydtaFormat *>(this)->operationComplete(true,
        QStringLiteral("Profile exported to %1").arg(outputPath));
    return true;
}

// ── Import profile ───────────────────────────────────────────────────
bool NydtaFormat::importProfile(const QString &nydtaPath) const
{
    if (!QFileInfo::exists(nydtaPath)) {
        emit const_cast<NydtaFormat *>(this)->operationComplete(false, QStringLiteral("File not found"));
        return false;
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) return false;

    emit const_cast<NydtaFormat *>(this)->importProgress(10);

    // Extract
    QProcess unzip;
    unzip.start(QStringLiteral("unzip"), {nydtaPath, QStringLiteral("-d"), tempDir.path()});
    if (!unzip.waitForFinished(30000)) {
        emit const_cast<NydtaFormat *>(this)->operationComplete(false, QStringLiteral("Cannot extract archive"));
        return false;
    }

    emit const_cast<NydtaFormat *>(this)->importProgress(30);

    // Read manifest
    QFile manifestFile(tempDir.path() + QStringLiteral("/manifest.json"));
    if (!manifestFile.open(QIODevice::ReadOnly)) {
        emit const_cast<NydtaFormat *>(this)->operationComplete(false, QStringLiteral("Invalid .nydta archive (no manifest)"));
        return false;
    }
    auto manifestDoc = QJsonDocument::fromJson(manifestFile.readAll());
    if (manifestDoc.object()[QStringLiteral("format")].toString() != QStringLiteral("nydta")) {
        emit const_cast<NydtaFormat *>(this)->operationComplete(false, QStringLiteral("Not a valid NYDTA file"));
        return false;
    }

    emit const_cast<NydtaFormat *>(this)->importProgress(50);

    // Copy data files to config dir
    const QStringList dataFiles = {
        QStringLiteral("history.json"),
        QStringLiteral("bookmarks.json"),
        QStringLiteral("credentials.json"),
        QStringLiteral("settings.json"),
    };

    const QString dest = configDir();
    QDir().mkpath(dest);

    for (const auto &fileName : dataFiles) {
        const QString srcPath = tempDir.path() + QLatin1Char('/') + fileName;
        const QString destPath = dest + QLatin1Char('/') + fileName;
        if (QFileInfo::exists(srcPath)) {
            // Don't overwrite — merge could be done here
            if (!QFileInfo::exists(destPath)) {
                QFile::copy(srcPath, destPath);
            }
        }
    }

    emit const_cast<NydtaFormat *>(this)->importProgress(100);
    emit const_cast<NydtaFormat *>(this)->operationComplete(true, QStringLiteral("Profile imported successfully"));
    return true;
}

// ── Read manifest ────────────────────────────────────────────────────
QJsonObject NydtaFormat::readManifest(const QString &nydtaPath) const
{
    QTemporaryDir tempDir;
    if (!tempDir.isValid()) return {};

    QProcess unzip;
    unzip.start(QStringLiteral("unzip"), {
        nydtaPath, QStringLiteral("manifest.json"),
        QStringLiteral("-d"), tempDir.path()});
    if (!unzip.waitForFinished(10000)) return {};

    QFile f(tempDir.path() + QStringLiteral("/manifest.json"));
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

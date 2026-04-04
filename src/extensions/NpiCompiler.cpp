#include "NpiCompiler.h"

#include <QBuffer>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>

NpiCompiler::NpiCompiler(QObject *parent)
    : QObject(parent)
{
}

NpiCompiler::~NpiCompiler() = default;

// ---------------------------------------------------------------------------
// Validate manifest
// ---------------------------------------------------------------------------
QStringList NpiCompiler::validate(const QJsonObject &manifest) const
{
    QStringList errors;

    if (!manifest.contains(QStringLiteral("name")) || manifest[QStringLiteral("name")].toString().isEmpty())
        errors.append(QStringLiteral("manifest missing required field: name"));

    if (!manifest.contains(QStringLiteral("version")) || manifest[QStringLiteral("version")].toString().isEmpty())
        errors.append(QStringLiteral("manifest missing required field: version"));

    if (!manifest.contains(QStringLiteral("manifest_version")))
        errors.append(QStringLiteral("manifest missing required field: manifest_version"));

    const int mv = manifest[QStringLiteral("manifest_version")].toInt();
    if (mv < 2 || mv > 3)
        errors.append(QStringLiteral("manifest_version must be 2 or 3, got: %1").arg(mv));

    // content_scripts validation
    if (manifest.contains(QStringLiteral("content_scripts"))) {
        const QJsonArray scripts = manifest[QStringLiteral("content_scripts")].toArray();
        for (int i = 0; i < scripts.size(); ++i) {
            const QJsonObject cs = scripts[i].toObject();
            if (!cs.contains(QStringLiteral("matches")) || cs[QStringLiteral("matches")].toArray().isEmpty())
                errors.append(QStringLiteral("content_scripts[%1] missing 'matches' array").arg(i));
            if (!cs.contains(QStringLiteral("js")) && !cs.contains(QStringLiteral("css")))
                errors.append(QStringLiteral("content_scripts[%1] must have at least 'js' or 'css'").arg(i));
        }
    }

    // background validation
    if (manifest.contains(QStringLiteral("background"))) {
        const QJsonObject bg = manifest[QStringLiteral("background")].toObject();
        if (mv >= 3) {
            if (!bg.contains(QStringLiteral("service_worker")))
                errors.append(QStringLiteral("MV3 background requires 'service_worker' field"));
        } else {
            if (!bg.contains(QStringLiteral("scripts")) && !bg.contains(QStringLiteral("page")))
                errors.append(QStringLiteral("MV2 background requires 'scripts' or 'page' field"));
        }
    }

    return errors;
}

// ---------------------------------------------------------------------------
// Compile: write files to a temp directory, then zip into .npi
// ---------------------------------------------------------------------------
bool NpiCompiler::compile(const QJsonObject &manifest,
                          const QMap<QString, QByteArray> &files,
                          const QString &outputPath)
{
    // Validate first
    const QStringList errors = validate(manifest);
    if (!errors.isEmpty())
        return false;

    QTemporaryDir tmpDir;
    if (!tmpDir.isValid())
        return false;

    const QString root = tmpDir.path();

    // Write manifest.json
    {
        QFile mf(root + QStringLiteral("/manifest.json"));
        if (!mf.open(QIODevice::WriteOnly))
            return false;
        mf.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented));
    }

    // Write all provided files
    for (auto it = files.constBegin(); it != files.constEnd(); ++it) {
        const QString filePath = root + QStringLiteral("/") + it.key();
        const QString dirPath = QFileInfo(filePath).absolutePath();
        QDir().mkpath(dirPath);

        QFile f(filePath);
        if (!f.open(QIODevice::WriteOnly))
            return false;
        f.write(it.value());
    }

    // Create .npi zip archive using system zip
    // Remove existing output file first
    QFile::remove(outputPath);

    QProcess zipProc;
    zipProc.setWorkingDirectory(root);
    zipProc.start(QStringLiteral("zip"),
                  {QStringLiteral("-r"), outputPath, QStringLiteral(".")});
    if (!zipProc.waitForFinished(30000))
        return false;

    return zipProc.exitCode() == 0 && QFile::exists(outputPath);
}

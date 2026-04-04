#include "ExtensionManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>

#include "../data/Database.h"

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
ExtensionManager::ExtensionManager(QObject *parent)
    : QObject(parent)
{
    QDir().mkpath(extensionsDir());
    loadRegistry();
}

ExtensionManager::~ExtensionManager() = default;

// ---------------------------------------------------------------------------
// Paths
// ---------------------------------------------------------------------------
QString ExtensionManager::extensionsDir() const
{
    return Database::configDir() + QStringLiteral("/extensions");
}

QString ExtensionManager::registryPath() const
{
    return extensionsDir() + QStringLiteral("/_registry.json");
}

// ---------------------------------------------------------------------------
// Registry persistence
// ---------------------------------------------------------------------------
void ExtensionManager::loadRegistry()
{
    QFile file(registryPath());
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isObject())
        m_registry = doc.object();
}

void ExtensionManager::saveRegistry() const
{
    QFile file(registryPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;

    file.write(QJsonDocument(m_registry).toJson(QJsonDocument::Indented));
}

// ---------------------------------------------------------------------------
// Install extension (.npi or .xpi)
// ---------------------------------------------------------------------------
bool ExtensionManager::installExtension(const QString &filePath)
{
    const QFileInfo fi(filePath);
    const QString suffix = fi.suffix().toLower();

    QString workPath = filePath;

    // If .xpi, convert to temporary .npi first
    if (suffix == QStringLiteral("xpi")) {
        const QString tmpNpi = extensionsDir() + QStringLiteral("/")
                               + fi.completeBaseName() + QStringLiteral(".npi");
        if (!convertXpiToNpi(filePath, tmpNpi))
            return false;
        workPath = tmpNpi;
    } else if (suffix != QStringLiteral("npi")) {
        return false;
    }

    // Create a temp directory for extraction
    const QString baseName = QFileInfo(workPath).completeBaseName();
    const QString extDir = extensionsDir() + QStringLiteral("/") + baseName;
    QDir().mkpath(extDir);

    if (!extractZip(workPath, extDir)) {
        QDir(extDir).removeRecursively();
        return false;
    }

    // Remove temp .npi if we converted from .xpi
    if (suffix == QStringLiteral("xpi"))
        QFile::remove(workPath);

    // Read and validate manifest
    const QJsonObject manifest = readManifest(extDir);
    if (manifest.isEmpty()) {
        QDir(extDir).removeRecursively();
        return false;
    }

    const QString name = manifest[QStringLiteral("name")].toString();
    if (name.isEmpty()) {
        QDir(extDir).removeRecursively();
        return false;
    }

    // Rename directory to the canonical extension name
    const QString canonicalDir = extensionsDir() + QStringLiteral("/") + name;
    if (canonicalDir != extDir) {
        QDir(canonicalDir).removeRecursively();
        QDir().rename(extDir, canonicalDir);
    }

    // Register
    QJsonObject entry;
    entry[QStringLiteral("name")] = name;
    entry[QStringLiteral("version")] = manifest[QStringLiteral("version")].toString();
    entry[QStringLiteral("description")] = manifest[QStringLiteral("description")].toString();
    entry[QStringLiteral("enabled")] = true;
    entry[QStringLiteral("path")] = canonicalDir;
    entry[QStringLiteral("permissions")] = manifest[QStringLiteral("permissions")];
    entry[QStringLiteral("content_scripts")] = manifest[QStringLiteral("content_scripts")];

    // Support both MV2 browser_action and MV3 action
    if (manifest.contains(QStringLiteral("action")))
        entry[QStringLiteral("action")] = manifest[QStringLiteral("action")];
    else if (manifest.contains(QStringLiteral("browser_action")))
        entry[QStringLiteral("action")] = manifest[QStringLiteral("browser_action")];

    if (manifest.contains(QStringLiteral("background")))
        entry[QStringLiteral("background")] = manifest[QStringLiteral("background")];

    if (manifest.contains(QStringLiteral("icons")))
        entry[QStringLiteral("icons")] = manifest[QStringLiteral("icons")];

    m_registry[name] = entry;
    saveRegistry();

    emit extensionInstalled(name);
    return true;
}

// ---------------------------------------------------------------------------
// Uninstall
// ---------------------------------------------------------------------------
bool ExtensionManager::uninstallExtension(const QString &name)
{
    if (!m_registry.contains(name))
        return false;

    const QJsonObject entry = m_registry[name].toObject();
    const QString path = entry[QStringLiteral("path")].toString();

    if (!path.isEmpty())
        QDir(path).removeRecursively();

    m_registry.remove(name);
    saveRegistry();

    emit extensionUninstalled(name);
    return true;
}

// ---------------------------------------------------------------------------
// Toggle enable/disable
// ---------------------------------------------------------------------------
bool ExtensionManager::toggleExtension(const QString &name)
{
    if (!m_registry.contains(name))
        return false;

    QJsonObject entry = m_registry[name].toObject();
    const bool newState = !entry[QStringLiteral("enabled")].toBool(true);
    entry[QStringLiteral("enabled")] = newState;
    m_registry[name] = entry;
    saveRegistry();

    emit extensionToggled(name, newState);
    return true;
}

// ---------------------------------------------------------------------------
// Content script injection for a given URL
// ---------------------------------------------------------------------------
ExtensionManager::ContentScript ExtensionManager::getContentScriptsForUrl(const QUrl &url) const
{
    ContentScript result;

    for (auto it = m_registry.begin(); it != m_registry.end(); ++it) {
        const QJsonObject entry = it.value().toObject();
        if (!entry[QStringLiteral("enabled")].toBool(true))
            continue;

        const QString extPath = entry[QStringLiteral("path")].toString();
        const QJsonArray scripts = entry[QStringLiteral("content_scripts")].toArray();

        for (const QJsonValue &csVal : scripts) {
            const QJsonObject cs = csVal.toObject();
            const QJsonArray matches = cs[QStringLiteral("matches")].toArray();

            bool matched = false;
            for (const QJsonValue &m : matches) {
                if (urlMatchesPattern(url, m.toString())) {
                    matched = true;
                    break;
                }
            }
            if (!matched)
                continue;

            // Collect JS files
            const QJsonArray jsArr = cs[QStringLiteral("js")].toArray();
            for (const QJsonValue &jsVal : jsArr) {
                const QString jsFilePath = extPath + QStringLiteral("/") + jsVal.toString();
                result.jsFiles.append(jsFilePath);

                QFile f(jsFilePath);
                if (f.open(QIODevice::ReadOnly))
                    result.jsContents.append(QString::fromUtf8(f.readAll()));
            }

            // Collect CSS files
            const QJsonArray cssArr = cs[QStringLiteral("css")].toArray();
            for (const QJsonValue &cssVal : cssArr) {
                const QString cssFilePath = extPath + QStringLiteral("/") + cssVal.toString();
                result.cssFiles.append(cssFilePath);

                QFile f(cssFilePath);
                if (f.open(QIODevice::ReadOnly))
                    result.cssContents.append(QString::fromUtf8(f.readAll()));
            }
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// List all extensions
// ---------------------------------------------------------------------------
QJsonArray ExtensionManager::getExtensions() const
{
    QJsonArray arr;
    for (auto it = m_registry.begin(); it != m_registry.end(); ++it) {
        QJsonObject entry = it.value().toObject();
        entry[QStringLiteral("id")] = it.key();
        arr.append(entry);
    }
    return arr;
}

// ---------------------------------------------------------------------------
// Convert Firefox XPI to OyNIx NPI
// ---------------------------------------------------------------------------
bool ExtensionManager::convertXpiToNpi(const QString &xpiPath, const QString &outputPath)
{
    // XPI and NPI are both ZIP files. The main difference is manifest keys.
    // Extract the XPI, transform the manifest, repack as NPI.

    const QString tmpDir = extensionsDir() + QStringLiteral("/_xpi_convert_tmp");
    QDir(tmpDir).removeRecursively();
    QDir().mkpath(tmpDir);

    if (!extractZip(xpiPath, tmpDir)) {
        QDir(tmpDir).removeRecursively();
        return false;
    }

    // Read original manifest
    QJsonObject manifest = readManifest(tmpDir);
    if (manifest.isEmpty()) {
        QDir(tmpDir).removeRecursively();
        return false;
    }

    // Transform: Firefox MV2 -> NPI format
    // Rename browser_action -> action (if not already present)
    if (!manifest.contains(QStringLiteral("action")) && manifest.contains(QStringLiteral("browser_action"))) {
        manifest[QStringLiteral("action")] = manifest[QStringLiteral("browser_action")];
        manifest.remove(QStringLiteral("browser_action"));
    }

    // Remove Firefox-specific keys
    manifest.remove(QStringLiteral("applications"));
    manifest.remove(QStringLiteral("browser_specific_settings"));

    // Set manifest_version to 3 if it was 2
    if (manifest[QStringLiteral("manifest_version")].toInt() < 3)
        manifest[QStringLiteral("manifest_version")] = 3;

    // Transform background scripts (MV2 array -> MV3 service_worker)
    if (manifest.contains(QStringLiteral("background"))) {
        QJsonObject bg = manifest[QStringLiteral("background")].toObject();
        if (bg.contains(QStringLiteral("scripts")) && !bg.contains(QStringLiteral("service_worker"))) {
            const QJsonArray scripts = bg[QStringLiteral("scripts")].toArray();
            if (!scripts.isEmpty())
                bg[QStringLiteral("service_worker")] = scripts.first().toString();
            bg.remove(QStringLiteral("scripts"));
            manifest[QStringLiteral("background")] = bg;
        }
    }

    // Write transformed manifest
    {
        QFile mf(tmpDir + QStringLiteral("/manifest.json"));
        if (mf.open(QIODevice::WriteOnly | QIODevice::Truncate))
            mf.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented));
    }

    // Repack as .npi (zip)
    // Use system zip command for simplicity (available on all target platforms)
    QProcess zipProc;
    zipProc.setWorkingDirectory(tmpDir);
    zipProc.start(QStringLiteral("zip"), {QStringLiteral("-r"), outputPath, QStringLiteral(".")});
    zipProc.waitForFinished(30000);

    QDir(tmpDir).removeRecursively();
    return QFile::exists(outputPath);
}

// ---------------------------------------------------------------------------
// Zip extraction using system unzip
// ---------------------------------------------------------------------------
bool ExtensionManager::extractZip(const QString &zipPath, const QString &destDir)
{
    QProcess proc;
    proc.start(QStringLiteral("unzip"), {QStringLiteral("-o"), zipPath, QStringLiteral("-d"), destDir});
    proc.waitForFinished(30000);
    return proc.exitCode() == 0;
}

// ---------------------------------------------------------------------------
// Read manifest.json from an extension directory
// ---------------------------------------------------------------------------
QJsonObject ExtensionManager::readManifest(const QString &extDir) const
{
    QFile file(extDir + QStringLiteral("/manifest.json"));
    if (!file.open(QIODevice::ReadOnly))
        return {};

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return {};

    const QJsonObject obj = doc.object();

    // Require at least a name field
    if (!obj.contains(QStringLiteral("name")))
        return {};

    return obj;
}

// ---------------------------------------------------------------------------
// URL pattern matching
// ---------------------------------------------------------------------------
bool ExtensionManager::urlMatchesPattern(const QUrl &url, const QString &pattern) const
{
    // Handle special patterns
    if (pattern == QStringLiteral("<all_urls>"))
        return true;

    const QString urlStr = url.toString();

    // Convert glob pattern to regex:
    //   *://*/* matches any http/https URL
    //   *.example.com/* matches example.com and subdomains
    //   https://example.com/* matches only https on that host

    // Split pattern into scheme://host/path
    static const QRegularExpression patternRx(
        QStringLiteral("^(\\*|https?|file|ftp)://(\\*|(?:\\*\\.)?[^/]*)/(.*)$"));

    auto match = patternRx.match(pattern);
    if (!match.hasMatch()) {
        // Fall back to simple glob: convert * to .* and ? to .
        QString regexStr = QRegularExpression::escape(pattern);
        regexStr.replace(QStringLiteral("\\*"), QStringLiteral(".*"));
        regexStr.replace(QStringLiteral("\\?"), QStringLiteral("."));
        QRegularExpression rx(QStringLiteral("^") + regexStr + QStringLiteral("$"));
        return rx.match(urlStr).hasMatch();
    }

    const QString schemePattern = match.captured(1);
    const QString hostPattern = match.captured(2);
    const QString pathPattern = match.captured(3);

    // Check scheme
    const QString urlScheme = url.scheme();
    if (schemePattern != QStringLiteral("*")) {
        if (urlScheme != schemePattern)
            return false;
    } else {
        // * matches only http and https
        if (urlScheme != QStringLiteral("http") && urlScheme != QStringLiteral("https"))
            return false;
    }

    // Check host
    const QString urlHost = url.host();
    if (hostPattern != QStringLiteral("*")) {
        if (hostPattern.startsWith(QStringLiteral("*."))) {
            // *.example.com matches example.com and all subdomains
            const QString baseDomain = hostPattern.mid(2);
            if (urlHost != baseDomain && !urlHost.endsWith(QStringLiteral(".") + baseDomain))
                return false;
        } else {
            if (urlHost != hostPattern)
                return false;
        }
    }

    // Check path (convert glob to regex)
    QString pathRegex = QRegularExpression::escape(pathPattern);
    pathRegex.replace(QStringLiteral("\\*"), QStringLiteral(".*"));
    pathRegex.replace(QStringLiteral("\\?"), QStringLiteral("."));
    QRegularExpression pathRx(QStringLiteral("^/") + pathRegex + QStringLiteral("$"));

    const QString urlPath = url.path().isEmpty() ? QStringLiteral("/") : url.path();
    return pathRx.match(urlPath).hasMatch();
}

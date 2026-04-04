#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QString>

class ExtensionManager : public QObject
{
    Q_OBJECT

public:
    explicit ExtensionManager(QObject *parent = nullptr);
    ~ExtensionManager() override;

    struct ContentScript {
        QStringList jsFiles;
        QStringList cssFiles;
        QStringList jsContents;
        QStringList cssContents;
    };

    bool installExtension(const QString &filePath);
    bool uninstallExtension(const QString &name);
    bool toggleExtension(const QString &name);

    [[nodiscard]] ContentScript getContentScriptsForUrl(const QUrl &url) const;
    [[nodiscard]] QJsonArray getExtensions() const;

    bool convertXpiToNpi(const QString &xpiPath, const QString &outputPath);

signals:
    void extensionInstalled(const QString &name);
    void extensionUninstalled(const QString &name);
    void extensionToggled(const QString &name, bool enabled);

private:
    void loadRegistry();
    void saveRegistry() const;

    [[nodiscard]] QString extensionsDir() const;
    [[nodiscard]] QString registryPath() const;

    bool extractZip(const QString &zipPath, const QString &destDir);
    [[nodiscard]] QJsonObject readManifest(const QString &extDir) const;
    [[nodiscard]] bool urlMatchesPattern(const QUrl &url, const QString &pattern) const;

    // name -> extension metadata
    QJsonObject m_registry;
};

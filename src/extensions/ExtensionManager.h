#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QString>

class QWebEnginePage;
class QWebEngineView;
class QWebChannel;
class TabWidget;
class BookmarkManager;
class ExtensionBridge;

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

    /// Set browser context needed for extension API bridge
    void setBrowserContext(TabWidget *tabWidget, BookmarkManager *bookmarkManager);

    /// Initialize background scripts for all enabled extensions
    void startBackgroundScripts();

    /// Get the popup HTML path for an extension (empty if none)
    [[nodiscard]] QString getPopupPath(const QString &extensionName) const;

    /// Create a popup QWebEngineView with the chrome.* API bridge injected
    QWebEngineView *createPopupView(const QString &extensionName, QWidget *parent);

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

    void runBackgroundScript(const QString &extensionName, const QJsonObject &entry);

    // name -> extension metadata
    QJsonObject m_registry;

    // Browser context
    TabWidget *m_tabWidget = nullptr;
    BookmarkManager *m_bookmarkManager = nullptr;

    // Background pages (one per extension with background script)
    QMap<QString, QWebEnginePage *> m_backgroundPages;
    QMap<QString, ExtensionBridge *> m_bridges;
};

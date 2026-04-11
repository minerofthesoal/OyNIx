#pragma once

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariantMap>

class QWebEngineView;
class QWebEnginePage;
class TabWidget;
class ExtensionManager;
class BookmarkManager;

/**
 * ExtensionBridge — Provides chrome.* / browser.* API to extension scripts.
 * Injected via QWebChannel into extension popups, content scripts, and background pages.
 * Implements: chrome.tabs, chrome.runtime, chrome.storage.local, chrome.bookmarks,
 *             chrome.notifications (basic).
 */
class ExtensionBridge : public QObject
{
    Q_OBJECT

    // Expose as properties for QWebChannel
    Q_PROPERTY(QString extensionId READ extensionId CONSTANT)

public:
    explicit ExtensionBridge(const QString &extensionId,
                             TabWidget *tabWidget,
                             ExtensionManager *extManager,
                             BookmarkManager *bookmarkManager,
                             QObject *parent = nullptr);
    ~ExtensionBridge() override;

    [[nodiscard]] QString extensionId() const { return m_extensionId; }

    /// Generate the chrome.* API shim JavaScript that bridges to QWebChannel
    static QString generateApiShim();

public slots:
    // ── chrome.tabs ──────────────────────────────────────────────────
    QJsonArray tabsQuery(const QJsonObject &queryInfo);
    QJsonObject tabsGetActive();
    void tabsCreate(const QJsonObject &createProperties);
    void tabsRemove(int tabId);
    void tabsUpdate(int tabId, const QJsonObject &updateProperties);
    void tabsReload(int tabId);
    QJsonObject tabsGet(int tabId);
    void tabsExecuteScript(int tabId, const QJsonObject &details);
    void tabsInsertCSS(int tabId, const QJsonObject &details);

    // ── chrome.runtime ───────────────────────────────────────────────
    QJsonObject runtimeGetManifest();
    QString runtimeGetURL(const QString &path);
    void runtimeSendMessage(const QJsonObject &message);

    // ── chrome.storage.local ─────────────────────────────────────────
    QJsonObject storageLocalGet(const QJsonArray &keys);
    void storageLocalSet(const QJsonObject &items);
    void storageLocalRemove(const QJsonArray &keys);
    void storageLocalClear();

    // ── chrome.bookmarks ─────────────────────────────────────────────
    QJsonArray bookmarksGetTree();
    void bookmarksCreate(const QJsonObject &bookmark);
    void bookmarksRemove(const QString &id);

    // ── chrome.notifications ─────────────────────────────────────────
    void notificationsCreate(const QString &id, const QJsonObject &options);

signals:
    void messageReceived(const QJsonObject &message);
    void tabUpdated(int tabId, const QJsonObject &changeInfo);

private:
    void loadStorage();
    void saveStorage();
    QString storagePath() const;

    QString m_extensionId;
    TabWidget *m_tabWidget = nullptr;
    ExtensionManager *m_extManager = nullptr;
    BookmarkManager *m_bookmarkManager = nullptr;
    QJsonObject m_storage;
};

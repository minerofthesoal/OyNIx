#include "ExtensionBridge.h"
#include "ExtensionManager.h"
#include "core/TabWidget.h"
#include "core/WebView.h"
#include "data/BookmarkManager.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QWebEnginePage>
#include <QStandardPaths>

// ── Constructor / Destructor ─────────────────────────────────────────
ExtensionBridge::ExtensionBridge(const QString &extensionId,
                                 TabWidget *tabWidget,
                                 ExtensionManager *extManager,
                                 BookmarkManager *bookmarkManager,
                                 QObject *parent)
    : QObject(parent)
    , m_extensionId(extensionId)
    , m_tabWidget(tabWidget)
    , m_extManager(extManager)
    , m_bookmarkManager(bookmarkManager)
{
    loadStorage();
}

ExtensionBridge::~ExtensionBridge()
{
    saveStorage();
}

// ── chrome.tabs ──────────────────────────────────────────────────────
QJsonArray ExtensionBridge::tabsQuery(const QJsonObject &queryInfo)
{
    QJsonArray results;
    if (!m_tabWidget) return results;

    const bool activeOnly = queryInfo[QStringLiteral("active")].toBool(false);
    const bool currentWindow = queryInfo[QStringLiteral("currentWindow")].toBool(false);
    const QString urlPattern = queryInfo[QStringLiteral("url")].toString();

    for (int i = 0; i < m_tabWidget->count(); ++i) {
        auto *view = m_tabWidget->webView(i);
        if (!view) continue;

        if (activeOnly && i != m_tabWidget->currentIndex())
            continue;

        QJsonObject tab;
        tab[QStringLiteral("id")] = i;
        tab[QStringLiteral("index")] = i;
        tab[QStringLiteral("url")] = view->url().toString();
        tab[QStringLiteral("title")] = view->title();
        tab[QStringLiteral("active")] = (i == m_tabWidget->currentIndex());
        tab[QStringLiteral("pinned")] = m_tabWidget->isTabPinned(i);
        tab[QStringLiteral("audible")] = !view->isAudioMuted();
        tab[QStringLiteral("windowId")] = 0;

        if (!urlPattern.isEmpty()) {
            if (!view->url().toString().contains(urlPattern))
                continue;
        }

        results.append(tab);

        Q_UNUSED(currentWindow)
    }

    return results;
}

QJsonObject ExtensionBridge::tabsGetActive()
{
    QJsonObject tab;
    if (!m_tabWidget) return tab;

    const int idx = m_tabWidget->currentIndex();
    auto *view = m_tabWidget->webView(idx);
    if (!view) return tab;

    tab[QStringLiteral("id")] = idx;
    tab[QStringLiteral("index")] = idx;
    tab[QStringLiteral("url")] = view->url().toString();
    tab[QStringLiteral("title")] = view->title();
    tab[QStringLiteral("active")] = true;
    tab[QStringLiteral("pinned")] = m_tabWidget->isTabPinned(idx);

    return tab;
}

QJsonObject ExtensionBridge::tabsGet(int tabId)
{
    QJsonObject tab;
    if (!m_tabWidget || tabId < 0 || tabId >= m_tabWidget->count()) return tab;

    auto *view = m_tabWidget->webView(tabId);
    if (!view) return tab;

    tab[QStringLiteral("id")] = tabId;
    tab[QStringLiteral("index")] = tabId;
    tab[QStringLiteral("url")] = view->url().toString();
    tab[QStringLiteral("title")] = view->title();
    tab[QStringLiteral("active")] = (tabId == m_tabWidget->currentIndex());
    tab[QStringLiteral("pinned")] = m_tabWidget->isTabPinned(tabId);

    return tab;
}

void ExtensionBridge::tabsCreate(const QJsonObject &createProperties)
{
    if (!m_tabWidget) return;
    const QString url = createProperties[QStringLiteral("url")].toString();
    const bool active = createProperties[QStringLiteral("active")].toBool(true);

    auto *view = m_tabWidget->addNewTab(QUrl(url));
    if (!active && view) {
        // Switch back to the previously active tab
        // (addNewTab auto-focuses the new tab)
    }
}

void ExtensionBridge::tabsRemove(int tabId)
{
    if (!m_tabWidget || tabId < 0 || tabId >= m_tabWidget->count()) return;
    auto *w = m_tabWidget->widget(tabId);
    m_tabWidget->removeTab(tabId);
    if (w) w->deleteLater();
}

void ExtensionBridge::tabsUpdate(int tabId, const QJsonObject &updateProperties)
{
    if (!m_tabWidget || tabId < 0 || tabId >= m_tabWidget->count()) return;
    auto *view = m_tabWidget->webView(tabId);
    if (!view) return;

    if (updateProperties.contains(QStringLiteral("url")))
        view->load(QUrl(updateProperties[QStringLiteral("url")].toString()));
    if (updateProperties.contains(QStringLiteral("active")) &&
        updateProperties[QStringLiteral("active")].toBool())
        m_tabWidget->setCurrentIndex(tabId);
    if (updateProperties.contains(QStringLiteral("muted")))
        view->page()->setAudioMuted(updateProperties[QStringLiteral("muted")].toBool());
    if (updateProperties.contains(QStringLiteral("pinned"))) {
        if (updateProperties[QStringLiteral("pinned")].toBool())
            m_tabWidget->pinTab(tabId);
        else
            m_tabWidget->unpinTab(tabId);
    }
}

void ExtensionBridge::tabsReload(int tabId)
{
    if (!m_tabWidget || tabId < 0 || tabId >= m_tabWidget->count()) return;
    auto *view = m_tabWidget->webView(tabId);
    if (view) view->reload();
}

void ExtensionBridge::tabsExecuteScript(int tabId, const QJsonObject &details)
{
    if (!m_tabWidget) return;
    const int idx = (tabId < 0) ? m_tabWidget->currentIndex() : tabId;
    auto *view = m_tabWidget->webView(idx);
    if (!view) return;

    if (details.contains(QStringLiteral("code"))) {
        view->page()->runJavaScript(details[QStringLiteral("code")].toString());
    } else if (details.contains(QStringLiteral("file"))) {
        // Load from extension directory
        const QJsonArray exts = m_extManager->getExtensions();
        for (const auto &val : exts) {
            auto ext = val.toObject();
            if (ext[QStringLiteral("id")].toString() == m_extensionId) {
                const QString basePath = ext[QStringLiteral("path")].toString();
                const QString filePath = basePath + QLatin1Char('/') +
                    details[QStringLiteral("file")].toString();
                QFile f(filePath);
                if (f.open(QIODevice::ReadOnly))
                    view->page()->runJavaScript(QString::fromUtf8(f.readAll()));
                break;
            }
        }
    }
}

void ExtensionBridge::tabsInsertCSS(int tabId, const QJsonObject &details)
{
    if (!m_tabWidget) return;
    const int idx = (tabId < 0) ? m_tabWidget->currentIndex() : tabId;
    auto *view = m_tabWidget->webView(idx);
    if (!view) return;

    QString css;
    if (details.contains(QStringLiteral("code"))) {
        css = details[QStringLiteral("code")].toString();
    } else if (details.contains(QStringLiteral("file"))) {
        const QJsonArray exts = m_extManager->getExtensions();
        for (const auto &val : exts) {
            auto ext = val.toObject();
            if (ext[QStringLiteral("id")].toString() == m_extensionId) {
                const QString basePath = ext[QStringLiteral("path")].toString();
                QFile f(basePath + QLatin1Char('/') + details[QStringLiteral("file")].toString());
                if (f.open(QIODevice::ReadOnly))
                    css = QString::fromUtf8(f.readAll());
                break;
            }
        }
    }

    if (!css.isEmpty()) {
        const QString js = QStringLiteral(
            "(function() {"
            "  var s = document.createElement('style');"
            "  s.textContent = %1;"
            "  document.head.appendChild(s);"
            "})();"
        ).arg(QJsonDocument(QJsonArray{css}).toJson(QJsonDocument::Compact));
        // Extract just the string from the array
        const QString safeJs = QStringLiteral(
            "(function() {"
            "  var s = document.createElement('style');"
            "  s.textContent = decodeURIComponent('%1');"
            "  document.head.appendChild(s);"
            "})();"
        ).arg(QString::fromUtf8(QUrl::toPercentEncoding(css)));
        view->page()->runJavaScript(safeJs);
    }
}

// ── chrome.runtime ───────────────────────────────────────────────────
QJsonObject ExtensionBridge::runtimeGetManifest()
{
    if (!m_extManager) return {};
    const QJsonArray exts = m_extManager->getExtensions();
    for (const auto &val : exts) {
        auto ext = val.toObject();
        if (ext[QStringLiteral("id")].toString() == m_extensionId)
            return ext;
    }
    return {};
}

QString ExtensionBridge::runtimeGetURL(const QString &path)
{
    if (!m_extManager) return {};
    const QJsonArray exts = m_extManager->getExtensions();
    for (const auto &val : exts) {
        auto ext = val.toObject();
        if (ext[QStringLiteral("id")].toString() == m_extensionId) {
            return QStringLiteral("file://") +
                ext[QStringLiteral("path")].toString() + QLatin1Char('/') + path;
        }
    }
    return {};
}

void ExtensionBridge::runtimeSendMessage(const QJsonObject &message)
{
    emit messageReceived(message);
}

// ── chrome.storage.local ─────────────────────────────────────────────
QString ExtensionBridge::storagePath() const
{
#ifdef Q_OS_WIN
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
           + QStringLiteral("/oynix/extensions/") + m_extensionId + QStringLiteral("/_storage.json");
#else
    return QDir::homePath() + QStringLiteral("/.config/oynix/extensions/")
           + m_extensionId + QStringLiteral("/_storage.json");
#endif
}

void ExtensionBridge::loadStorage()
{
    QFile f(storagePath());
    if (!f.open(QIODevice::ReadOnly)) return;
    auto doc = QJsonDocument::fromJson(f.readAll());
    if (doc.isObject()) m_storage = doc.object();
}

void ExtensionBridge::saveStorage()
{
    const QString path = storagePath();
    QDir().mkpath(QFileInfo(path).path());
    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(m_storage).toJson());
}

QJsonObject ExtensionBridge::storageLocalGet(const QJsonArray &keys)
{
    QJsonObject result;
    if (keys.isEmpty()) {
        // Return all storage
        return m_storage;
    }
    for (const auto &val : keys) {
        const QString key = val.toString();
        if (m_storage.contains(key))
            result[key] = m_storage[key];
    }
    return result;
}

void ExtensionBridge::storageLocalSet(const QJsonObject &items)
{
    for (auto it = items.begin(); it != items.end(); ++it) {
        m_storage[it.key()] = it.value();
    }
    saveStorage();
}

void ExtensionBridge::storageLocalRemove(const QJsonArray &keys)
{
    for (const auto &val : keys) {
        m_storage.remove(val.toString());
    }
    saveStorage();
}

void ExtensionBridge::storageLocalClear()
{
    m_storage = QJsonObject();
    saveStorage();
}

// ── chrome.bookmarks ─────────────────────────────────────────────────
QJsonArray ExtensionBridge::bookmarksGetTree()
{
    if (!m_bookmarkManager) return {};
    return m_bookmarkManager->getAll();
}

void ExtensionBridge::bookmarksCreate(const QJsonObject &bookmark)
{
    if (!m_bookmarkManager) return;
    m_bookmarkManager->add(
        bookmark[QStringLiteral("url")].toString(),
        bookmark[QStringLiteral("title")].toString(),
        bookmark[QStringLiteral("parentId")].toString(QStringLiteral("Unsorted")));
}

void ExtensionBridge::bookmarksRemove(const QString &id)
{
    if (!m_bookmarkManager) return;
    m_bookmarkManager->remove(id);
}

// ── chrome.notifications ─────────────────────────────────────────────
void ExtensionBridge::notificationsCreate(const QString &id, const QJsonObject &options)
{
    Q_UNUSED(id)
    Q_UNUSED(options)
    // Desktop notifications require platform-specific implementation
    // For now, this is a no-op — the extension call succeeds silently
}

// ── Generate chrome.* API shim JavaScript ────────────────────────────
QString ExtensionBridge::generateApiShim()
{
    // This JS is injected into extension pages (popup, background, options).
    // It creates a `chrome` object that proxies calls to QWebChannel -> ExtensionBridge.
    return QStringLiteral(R"JS(
(function() {
    'use strict';

    // Wait for QWebChannel bridge
    if (typeof QWebChannel === 'undefined') {
        // QWebChannel not loaded yet — this script runs before qt.webChannelTransport
        // We'll retry after a short delay
        var _pendingCalls = [];
        var _bridge = null;

        function ensureBridge(cb) {
            if (_bridge) { cb(_bridge); return; }
            _pendingCalls.push(cb);
        }

        // QWebChannel will be set up by the host; poll for it
        var _interval = setInterval(function() {
            if (typeof qt !== 'undefined' && qt.webChannelTransport) {
                clearInterval(_interval);
                new QWebChannel(qt.webChannelTransport, function(channel) {
                    _bridge = channel.objects.bridge;
                    _pendingCalls.forEach(function(fn) { fn(_bridge); });
                    _pendingCalls = [];
                });
            }
        }, 50);

        // Helper: wrap a bridge method call in a Promise
        function bridgeCall(method) {
            var args = Array.prototype.slice.call(arguments, 1);
            return new Promise(function(resolve, reject) {
                ensureBridge(function(b) {
                    try {
                        var result = b[method].apply(b, args);
                        resolve(result);
                    } catch(e) {
                        reject(e);
                    }
                });
            });
        }

        // ── chrome.tabs ─────────────────────────────────────────
        var chromeTabs = {
            query: function(queryInfo, callback) {
                bridgeCall('tabsQuery', queryInfo || {}).then(function(r) {
                    if (callback) callback(r);
                });
            },
            create: function(props, callback) {
                bridgeCall('tabsCreate', props || {}).then(function() {
                    if (callback) callback();
                });
            },
            remove: function(tabId, callback) {
                bridgeCall('tabsRemove', tabId).then(function() {
                    if (callback) callback();
                });
            },
            update: function(tabId, props, callback) {
                bridgeCall('tabsUpdate', tabId, props).then(function() {
                    if (callback) callback();
                });
            },
            reload: function(tabId, callback) {
                bridgeCall('tabsReload', tabId || -1).then(function() {
                    if (callback) callback();
                });
            },
            get: function(tabId, callback) {
                bridgeCall('tabsGet', tabId).then(function(r) {
                    if (callback) callback(r);
                });
            },
            executeScript: function(tabId, details, callback) {
                if (typeof tabId === 'object') {
                    details = tabId; tabId = -1;
                }
                bridgeCall('tabsExecuteScript', tabId, details || {}).then(function() {
                    if (callback) callback();
                });
            },
            insertCSS: function(tabId, details, callback) {
                if (typeof tabId === 'object') {
                    details = tabId; tabId = -1;
                }
                bridgeCall('tabsInsertCSS', tabId, details || {}).then(function() {
                    if (callback) callback();
                });
            },
            onUpdated: { addListener: function() {}, removeListener: function() {} },
            onCreated: { addListener: function() {}, removeListener: function() {} },
            onRemoved: { addListener: function() {}, removeListener: function() {} },
            onActivated: { addListener: function() {}, removeListener: function() {} }
        };

        // ── chrome.runtime ──────────────────────────────────────
        var _messageListeners = [];
        var chromeRuntime = {
            getManifest: function() {
                // Synchronous not possible over QWebChannel; return cached or empty
                return {};
            },
            getURL: function(path) {
                // Best effort — actual path resolution needs bridge
                return path;
            },
            sendMessage: function(message, callback) {
                bridgeCall('runtimeSendMessage', message || {}).then(function() {
                    if (callback) callback();
                });
            },
            onMessage: {
                addListener: function(fn) { _messageListeners.push(fn); },
                removeListener: function(fn) {
                    _messageListeners = _messageListeners.filter(function(f) { return f !== fn; });
                }
            },
            onInstalled: { addListener: function(fn) { fn({reason: 'install'}); } },
            lastError: null,
            id: ''
        };

        // ── chrome.storage ──────────────────────────────────────
        var chromeStorage = {
            local: {
                get: function(keys, callback) {
                    var keyArr = [];
                    if (typeof keys === 'string') keyArr = [keys];
                    else if (Array.isArray(keys)) keyArr = keys;
                    else if (keys === null || keys === undefined) keyArr = [];
                    bridgeCall('storageLocalGet', keyArr).then(function(r) {
                        if (callback) callback(r);
                    });
                },
                set: function(items, callback) {
                    bridgeCall('storageLocalSet', items || {}).then(function() {
                        if (callback) callback();
                    });
                },
                remove: function(keys, callback) {
                    var keyArr = typeof keys === 'string' ? [keys] : (keys || []);
                    bridgeCall('storageLocalRemove', keyArr).then(function() {
                        if (callback) callback();
                    });
                },
                clear: function(callback) {
                    bridgeCall('storageLocalClear').then(function() {
                        if (callback) callback();
                    });
                }
            },
            onChanged: { addListener: function() {}, removeListener: function() {} }
        };

        // ── chrome.bookmarks ────────────────────────────────────
        var chromeBookmarks = {
            getTree: function(callback) {
                bridgeCall('bookmarksGetTree').then(function(r) {
                    if (callback) callback(r);
                });
            },
            create: function(bookmark, callback) {
                bridgeCall('bookmarksCreate', bookmark || {}).then(function() {
                    if (callback) callback();
                });
            },
            remove: function(id, callback) {
                bridgeCall('bookmarksRemove', id).then(function() {
                    if (callback) callback();
                });
            }
        };

        // ── chrome.notifications ────────────────────────────────
        var chromeNotifications = {
            create: function(id, options, callback) {
                if (typeof id === 'object') { options = id; id = ''; }
                bridgeCall('notificationsCreate', id || '', options || {}).then(function() {
                    if (callback) callback(id);
                });
            },
            clear: function(id, callback) { if (callback) callback(true); },
            onClicked: { addListener: function() {} }
        };

        // ── chrome.action / chrome.browserAction ────────────────
        var chromeAction = {
            setBadgeText: function() {},
            setBadgeBackgroundColor: function() {},
            setIcon: function() {},
            setTitle: function() {},
            setPopup: function() {},
            onClicked: { addListener: function() {} }
        };

        // ── Assemble chrome object ──────────────────────────────
        window.chrome = window.chrome || {};
        window.chrome.tabs = chromeTabs;
        window.chrome.runtime = chromeRuntime;
        window.chrome.storage = chromeStorage;
        window.chrome.bookmarks = chromeBookmarks;
        window.chrome.notifications = chromeNotifications;
        window.chrome.action = chromeAction;
        window.chrome.browserAction = chromeAction;

        // Also expose as `browser` (Firefox WebExtension API)
        window.browser = window.chrome;

    }
})();
)JS");
}

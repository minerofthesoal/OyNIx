/*
 * OyNIx Browser v3.1 — CoreBridge implementation
 * Loads OyNIxCore.so (C# NativeAOT) via dlopen and resolves all exports.
 */

#include "CoreBridge.h"

#include <QCoreApplication>
#include <QDir>
#include <QJsonDocument>
#include <QDebug>

#include <dlfcn.h>

CoreBridge &CoreBridge::instance()
{
    static CoreBridge s;
    return s;
}

CoreBridge::~CoreBridge()
{
    shutdown();
}

// ── Helpers ──────────────────────────────────────────────────────────

template<typename T>
static T resolveSym(void *lib, const char *name)
{
    auto *sym = dlsym(lib, name);
    if (!sym)
        qWarning() << "CoreBridge: failed to resolve" << name;
    return reinterpret_cast<T>(sym);
}

QString CoreBridge::callStringFunc(void *funcPtr, const char *arg)
{
    if (!funcPtr || !m_free) return {};
    auto fn = reinterpret_cast<char*(*)(const char*)>(funcPtr);
    char *result = fn(arg);
    if (!result) return {};
    QString s = QString::fromUtf8(result);
    m_free(result);
    return s;
}

// ── Initialize ───────────────────────────────────────────────────────

bool CoreBridge::initialize(const QString &configDir)
{
    if (m_loaded) return true;

    // Find the .so alongside the executable, or in a dotnet/ subdirectory
    const QString appDir = QCoreApplication::applicationDirPath();
    QStringList searchPaths = {
        appDir + QStringLiteral("/OyNIxCore.so"),
        appDir + QStringLiteral("/dotnet/OyNIxCore.so"),
        appDir + QStringLiteral("/../lib/OyNIxCore.so"),
        appDir + QStringLiteral("/../lib/oynix/OyNIxCore.so"),
    };

    for (const auto &path : searchPaths) {
        if (QFile::exists(path)) {
            m_lib = dlopen(path.toUtf8().constData(), RTLD_NOW);
            if (m_lib) break;
            qWarning() << "CoreBridge: dlopen failed:" << dlerror();
        }
    }

    if (!m_lib) {
        qWarning() << "CoreBridge: OyNIxCore.so not found. C# core disabled.";
        return false;
    }

    // Resolve all symbols
    m_free = resolveSym<decltype(m_free)>(m_lib, "oynix_free");
    m_init = resolveSym<decltype(m_init)>(m_lib, "oynix_init");
    m_shutdown = resolveSym<decltype(m_shutdown)>(m_lib, "oynix_shutdown");

    // AI
    m_aiBuildPrompt   = resolveSym<decltype(m_aiBuildPrompt)>(m_lib, "oynix_ai_build_prompt");
    m_aiBuildMessages = resolveSym<decltype(m_aiBuildMessages)>(m_lib, "oynix_ai_build_messages");
    m_aiAddHistory    = resolveSym<decltype(m_aiAddHistory)>(m_lib, "oynix_ai_add_history");
    m_aiClearHistory  = resolveSym<decltype(m_aiClearHistory)>(m_lib, "oynix_ai_clear_history");
    m_aiFallback      = resolveSym<decltype(m_aiFallback)>(m_lib, "oynix_ai_fallback");
    m_aiGetConfig     = resolveSym<decltype(m_aiGetConfig)>(m_lib, "oynix_ai_get_config");
    m_aiSetConfig     = resolveSym<decltype(m_aiSetConfig)>(m_lib, "oynix_ai_set_config");
    m_aiSummarize     = resolveSym<decltype(m_aiSummarize)>(m_lib, "oynix_ai_summarize_prompt");
    m_aiExplain       = resolveSym<decltype(m_aiExplain)>(m_lib, "oynix_ai_explain_prompt");

    // Extensions
    m_extList          = resolveSym<decltype(m_extList)>(m_lib, "oynix_ext_list");
    m_extInstall       = resolveSym<decltype(m_extInstall)>(m_lib, "oynix_ext_install");
    m_extRemove        = resolveSym<decltype(m_extRemove)>(m_lib, "oynix_ext_remove");
    m_extSetEnabled    = resolveSym<decltype(m_extSetEnabled)>(m_lib, "oynix_ext_set_enabled");
    m_extContentScripts = resolveSym<decltype(m_extContentScripts)>(m_lib, "oynix_ext_content_scripts");
    m_extManifest      = resolveSym<decltype(m_extManifest)>(m_lib, "oynix_ext_manifest");

    // Bookmarks
    m_bmAdd    = resolveSym<decltype(m_bmAdd)>(m_lib, "oynix_bm_add");
    m_bmRemove = resolveSym<decltype(m_bmRemove)>(m_lib, "oynix_bm_remove");
    m_bmList   = resolveSym<decltype(m_bmList)>(m_lib, "oynix_bm_list");
    m_bmSearch = resolveSym<decltype(m_bmSearch)>(m_lib, "oynix_bm_search");

    // Session
    m_sessionSave = resolveSym<decltype(m_sessionSave)>(m_lib, "oynix_session_save");
    m_sessionLoad = resolveSym<decltype(m_sessionLoad)>(m_lib, "oynix_session_load");

    // Search
    m_searchUrl       = resolveSym<decltype(m_searchUrl)>(m_lib, "oynix_search_url");
    m_searchIsUrl     = resolveSym<decltype(m_searchIsUrl)>(m_lib, "oynix_search_is_url");
    m_searchFts5      = resolveSym<decltype(m_searchFts5)>(m_lib, "oynix_search_fts5");
    m_searchSetEngine = resolveSym<decltype(m_searchSetEngine)>(m_lib, "oynix_search_set_engine");
    m_searchEngines   = resolveSym<decltype(m_searchEngines)>(m_lib, "oynix_search_engines");

    // Security
    m_secIsLogin = resolveSym<decltype(m_secIsLogin)>(m_lib, "oynix_sec_is_login");
    m_secInfo    = resolveSym<decltype(m_secInfo)>(m_lib, "oynix_sec_info");

    // Crawler
    m_crawlerConfigure  = resolveSym<decltype(m_crawlerConfigure)>(m_lib, "oynix_crawler_configure");
    m_crawlerStartList  = resolveSym<decltype(m_crawlerStartList)>(m_lib, "oynix_crawler_start_list");
    m_crawlerStartBroad = resolveSym<decltype(m_crawlerStartBroad)>(m_lib, "oynix_crawler_start_broad");
    m_crawlerStop       = resolveSym<decltype(m_crawlerStop)>(m_lib, "oynix_crawler_stop");
    m_crawlerStatus     = resolveSym<decltype(m_crawlerStatus)>(m_lib, "oynix_crawler_status");
    m_crawlerResults    = resolveSym<decltype(m_crawlerResults)>(m_lib, "oynix_crawler_results");
    m_crawlerCount      = resolveSym<decltype(m_crawlerCount)>(m_lib, "oynix_crawler_count");
    m_crawlerIsRunning  = resolveSym<decltype(m_crawlerIsRunning)>(m_lib, "oynix_crawler_is_running");

    // Initialize the C# core
    if (m_init) {
        const QByteArray cfgBytes = configDir.toUtf8();
        m_init(cfgBytes.constData());
    }

    m_loaded = true;
    qDebug() << "CoreBridge: C# core loaded successfully";
    return true;
}

void CoreBridge::shutdown()
{
    if (m_shutdown && m_loaded)
        m_shutdown();
    if (m_lib) {
        dlclose(m_lib);
        m_lib = nullptr;
    }
    m_loaded = false;
}

// ── AI ───────────────────────────────────────────────────────────────

QString CoreBridge::aiBuildPrompt(const QString &message)
{
    return callStringFunc(reinterpret_cast<void*>(m_aiBuildPrompt), message.toUtf8().constData());
}

QString CoreBridge::aiBuildMessages(const QString &message)
{
    return callStringFunc(reinterpret_cast<void*>(m_aiBuildMessages), message.toUtf8().constData());
}

void CoreBridge::aiAddHistory(const QString &role, const QString &content)
{
    if (m_aiAddHistory)
        m_aiAddHistory(role.toUtf8().constData(), content.toUtf8().constData());
}

void CoreBridge::aiClearHistory()
{
    if (m_aiClearHistory) m_aiClearHistory();
}

QString CoreBridge::aiFallback(const QString &prompt)
{
    return callStringFunc(reinterpret_cast<void*>(m_aiFallback), prompt.toUtf8().constData());
}

QJsonObject CoreBridge::aiGetConfig()
{
    if (!m_aiGetConfig || !m_free) return {};
    char *json = m_aiGetConfig();
    if (!json) return {};
    auto doc = QJsonDocument::fromJson(QByteArray(json));
    m_free(json);
    return doc.object();
}

void CoreBridge::aiSetConfig(const QJsonObject &config)
{
    if (!m_aiSetConfig) return;
    const QByteArray json = QJsonDocument(config).toJson(QJsonDocument::Compact);
    m_aiSetConfig(json.constData());
}

QString CoreBridge::aiSummarizePrompt(const QString &content, const QString &title)
{
    if (!m_aiSummarize || !m_free) return {};
    char *result = m_aiSummarize(content.toUtf8().constData(), title.toUtf8().constData());
    if (!result) return {};
    QString s = QString::fromUtf8(result);
    m_free(result);
    return s;
}

QString CoreBridge::aiExplainPrompt(const QString &code)
{
    return callStringFunc(reinterpret_cast<void*>(m_aiExplain), code.toUtf8().constData());
}

// ── Extensions ───────────────────────────────────────────────────────

QJsonArray CoreBridge::extList()
{
    if (!m_extList || !m_free) return {};
    char *json = m_extList();
    if (!json) return {};
    auto doc = QJsonDocument::fromJson(QByteArray(json));
    m_free(json);
    return doc.array();
}

bool CoreBridge::extInstall(const QString &path)
{
    return m_extInstall && m_extInstall(path.toUtf8().constData());
}

bool CoreBridge::extRemove(const QString &name)
{
    return m_extRemove && m_extRemove(name.toUtf8().constData());
}

void CoreBridge::extSetEnabled(const QString &name, bool enabled)
{
    if (m_extSetEnabled)
        m_extSetEnabled(name.toUtf8().constData(), enabled ? 1 : 0);
}

QJsonObject CoreBridge::extContentScripts(const QString &url)
{
    if (!m_extContentScripts || !m_free) return {};
    char *json = m_extContentScripts(url.toUtf8().constData());
    if (!json) return {};
    auto doc = QJsonDocument::fromJson(QByteArray(json));
    m_free(json);
    return doc.object();
}

QJsonObject CoreBridge::extManifest(const QString &name)
{
    if (!m_extManifest || !m_free) return {};
    char *json = m_extManifest(name.toUtf8().constData());
    if (!json) return {};
    auto doc = QJsonDocument::fromJson(QByteArray(json));
    m_free(json);
    return doc.object();
}

// ── Bookmarks ────────────────────────────────────────────────────────

bool CoreBridge::bmAdd(const QString &url, const QString &title, const QString &folder)
{
    return m_bmAdd && m_bmAdd(url.toUtf8().constData(),
                               title.toUtf8().constData(),
                               folder.toUtf8().constData());
}

bool CoreBridge::bmRemove(const QString &url)
{
    return m_bmRemove && m_bmRemove(url.toUtf8().constData());
}

QJsonArray CoreBridge::bmList()
{
    if (!m_bmList || !m_free) return {};
    char *json = m_bmList();
    if (!json) return {};
    auto doc = QJsonDocument::fromJson(QByteArray(json));
    m_free(json);
    return doc.array();
}

QJsonArray CoreBridge::bmSearch(const QString &query)
{
    if (!m_bmSearch || !m_free) return {};
    char *json = m_bmSearch(query.toUtf8().constData());
    if (!json) return {};
    auto doc = QJsonDocument::fromJson(QByteArray(json));
    m_free(json);
    return doc.array();
}

// ── Session ──────────────────────────────────────────────────────────

bool CoreBridge::sessionSave(const QJsonArray &tabs)
{
    if (!m_sessionSave) return false;
    const QByteArray json = QJsonDocument(tabs).toJson(QJsonDocument::Compact);
    return m_sessionSave(json.constData());
}

QJsonArray CoreBridge::sessionLoad()
{
    if (!m_sessionLoad || !m_free) return {};
    char *json = m_sessionLoad();
    if (!json) return {};
    auto doc = QJsonDocument::fromJson(QByteArray(json));
    m_free(json);
    return doc.array();
}

// ── Search ───────────────────────────────────────────────────────────

QString CoreBridge::searchUrl(const QString &query)
{
    return callStringFunc(reinterpret_cast<void*>(m_searchUrl), query.toUtf8().constData());
}

bool CoreBridge::searchIsUrl(const QString &input)
{
    return m_searchIsUrl && m_searchIsUrl(input.toUtf8().constData());
}

QString CoreBridge::searchFts5(const QString &query)
{
    return callStringFunc(reinterpret_cast<void*>(m_searchFts5), query.toUtf8().constData());
}

void CoreBridge::searchSetEngine(const QString &name)
{
    if (m_searchSetEngine)
        m_searchSetEngine(name.toUtf8().constData());
}

QJsonArray CoreBridge::searchEngines()
{
    if (!m_searchEngines || !m_free) return {};
    char *json = m_searchEngines();
    if (!json) return {};
    auto doc = QJsonDocument::fromJson(QByteArray(json));
    m_free(json);
    return doc.array();
}

// ── Security ─────────────────────────────────────────────────────────

bool CoreBridge::secIsLogin(const QString &url)
{
    return m_secIsLogin && m_secIsLogin(url.toUtf8().constData());
}

QJsonObject CoreBridge::secInfo(const QString &url)
{
    if (!m_secInfo || !m_free) return {};
    char *json = m_secInfo(url.toUtf8().constData());
    if (!json) return {};
    auto doc = QJsonDocument::fromJson(QByteArray(json));
    m_free(json);
    return doc.object();
}

// ── Crawler ─────────────────────────────────────────────────────────

void CoreBridge::crawlerConfigure(int maxDepth, int maxPages, int concurrency, bool followExternal)
{
    if (m_crawlerConfigure)
        m_crawlerConfigure(maxDepth, maxPages, concurrency, followExternal ? 1 : 0);
}

void CoreBridge::crawlerStartList(const QJsonArray &urls)
{
    if (!m_crawlerStartList) return;
    const QByteArray json = QJsonDocument(urls).toJson(QJsonDocument::Compact);
    m_crawlerStartList(json.constData());
}

void CoreBridge::crawlerStartBroad(const QString &seedUrl)
{
    if (m_crawlerStartBroad)
        m_crawlerStartBroad(seedUrl.toUtf8().constData());
}

void CoreBridge::crawlerStop()
{
    if (m_crawlerStop) m_crawlerStop();
}

QJsonObject CoreBridge::crawlerStatus()
{
    if (!m_crawlerStatus || !m_free) return {};
    char *json = m_crawlerStatus();
    if (!json) return {};
    auto doc = QJsonDocument::fromJson(QByteArray(json));
    m_free(json);
    return doc.object();
}

QJsonArray CoreBridge::crawlerResults(int offset, int limit)
{
    if (!m_crawlerResults || !m_free) return {};
    char *json = m_crawlerResults(offset, limit);
    if (!json) return {};
    auto doc = QJsonDocument::fromJson(QByteArray(json));
    m_free(json);
    return doc.array();
}

int CoreBridge::crawlerCount()
{
    return m_crawlerCount ? m_crawlerCount() : 0;
}

bool CoreBridge::crawlerIsRunning()
{
    return m_crawlerIsRunning && m_crawlerIsRunning();
}

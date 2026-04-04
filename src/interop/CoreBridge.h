/*
 * OyNIx Browser v3.1 — CoreBridge
 * C++ wrapper around the C# NativeAOT core library (OyNIxCore.so).
 * Provides type-safe Qt-friendly access to the C# business logic.
 */

#pragma once

#include <QString>
#include <QJsonArray>
#include <QJsonObject>

class CoreBridge
{
public:
    static CoreBridge &instance();

    /// Loads the native library and resolves all symbols. Returns false on failure.
    bool initialize(const QString &configDir);
    void shutdown();

    // ── AI ───────────────────────────────────────────────────────────────
    QString aiBuildPrompt(const QString &message);
    QString aiBuildMessages(const QString &message);
    void aiAddHistory(const QString &role, const QString &content);
    void aiClearHistory();
    QString aiFallback(const QString &prompt);
    QJsonObject aiGetConfig();
    void aiSetConfig(const QJsonObject &config);
    QString aiSummarizePrompt(const QString &content, const QString &title);
    QString aiExplainPrompt(const QString &code);

    // ── Extensions ───────────────────────────────────────────────────────
    QJsonArray extList();
    bool extInstall(const QString &path);
    bool extRemove(const QString &name);
    void extSetEnabled(const QString &name, bool enabled);
    QJsonObject extContentScripts(const QString &url);
    QJsonObject extManifest(const QString &name);

    // ── Bookmarks ────────────────────────────────────────────────────────
    bool bmAdd(const QString &url, const QString &title, const QString &folder = QString());
    bool bmRemove(const QString &url);
    QJsonArray bmList();
    QJsonArray bmSearch(const QString &query);

    // ── Session ──────────────────────────────────────────────────────────
    bool sessionSave(const QJsonArray &tabs);
    QJsonArray sessionLoad();

    // ── Search ───────────────────────────────────────────────────────────
    QString searchUrl(const QString &query);
    bool searchIsUrl(const QString &input);
    QString searchFts5(const QString &query);
    void searchSetEngine(const QString &name);
    QJsonArray searchEngines();

    // ── Security ─────────────────────────────────────────────────────────
    bool secIsLogin(const QString &url);
    QJsonObject secInfo(const QString &url);

    [[nodiscard]] bool isLoaded() const { return m_loaded; }

private:
    CoreBridge() = default;
    ~CoreBridge();

    // Helper: call C export that returns a string, free it, return QString
    QString callStringFunc(void *funcPtr, const char *arg);

    void *m_lib = nullptr;
    bool m_loaded = false;

    // Function pointers (resolved from .so)
    void (*m_init)(const char*) = nullptr;
    void (*m_shutdown)() = nullptr;
    void (*m_free)(void*) = nullptr;

    // AI
    char* (*m_aiBuildPrompt)(const char*) = nullptr;
    char* (*m_aiBuildMessages)(const char*) = nullptr;
    void  (*m_aiAddHistory)(const char*, const char*) = nullptr;
    void  (*m_aiClearHistory)() = nullptr;
    char* (*m_aiFallback)(const char*) = nullptr;
    char* (*m_aiGetConfig)() = nullptr;
    void  (*m_aiSetConfig)(const char*) = nullptr;
    char* (*m_aiSummarize)(const char*, const char*) = nullptr;
    char* (*m_aiExplain)(const char*) = nullptr;

    // Extensions
    char* (*m_extList)() = nullptr;
    int   (*m_extInstall)(const char*) = nullptr;
    int   (*m_extRemove)(const char*) = nullptr;
    void  (*m_extSetEnabled)(const char*, int) = nullptr;
    char* (*m_extContentScripts)(const char*) = nullptr;
    char* (*m_extManifest)(const char*) = nullptr;

    // Bookmarks
    int   (*m_bmAdd)(const char*, const char*, const char*) = nullptr;
    int   (*m_bmRemove)(const char*) = nullptr;
    char* (*m_bmList)() = nullptr;
    char* (*m_bmSearch)(const char*) = nullptr;

    // Session
    int   (*m_sessionSave)(const char*) = nullptr;
    char* (*m_sessionLoad)() = nullptr;

    // Search
    char* (*m_searchUrl)(const char*) = nullptr;
    int   (*m_searchIsUrl)(const char*) = nullptr;
    char* (*m_searchFts5)(const char*) = nullptr;
    void  (*m_searchSetEngine)(const char*) = nullptr;
    char* (*m_searchEngines)() = nullptr;

    // Security
    int   (*m_secIsLogin)(const char*) = nullptr;
    char* (*m_secInfo)(const char*) = nullptr;
};

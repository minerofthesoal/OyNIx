using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using OyNIx.Core.AI;
using OyNIx.Core.Crawler;
using OyNIx.Core.Data;
using OyNIx.Core.Extensions;
using OyNIx.Core.GitHub;
using OyNIx.Core.Import;
using OyNIx.Core.Search;
using OyNIx.Core.Security;

namespace OyNIx.Core.Interop;

/// <summary>
/// Native C exports for the C++ Qt shell to call.
/// All functions use [UnmanagedCallersOnly] for zero-overhead NativeAOT interop.
/// String returns are allocated via Marshal and must be freed with oynix_free().
/// </summary>
public static unsafe class NativeExports
{
    private static IntPtr AllocString(string s)
    {
        return Marshal.StringToCoTaskMemUTF8(s);
    }

    // ── Lifecycle ────────────────────────────────────────────────────────

    [UnmanagedCallersOnly(EntryPoint = "oynix_init")]
    public static void Init(byte* configDirPtr)
    {
        var configDir = Marshal.PtrToStringUTF8((IntPtr)configDirPtr) ?? "";

        BookmarkStore.Instance.Initialize(configDir);
        SessionStore.Instance.Initialize(configDir);
        HistoryStore.Instance.Initialize(configDir);
        SiteDatabase.Instance.Initialize(configDir);
        ProfileManager.Instance.Initialize(configDir);
        CredentialStore.Instance.Initialize(configDir);
        GitHubSync.Instance.Initialize(configDir);
        ExtensionRegistry.Instance.Initialize(
            Path.Combine(configDir, "extensions"));
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_shutdown")]
    public static void Shutdown()
    {
        // Any cleanup needed
    }

    // ── Memory ───────────────────────────────────────────────────────────

    [UnmanagedCallersOnly(EntryPoint = "oynix_free")]
    public static void Free(void* ptr)
    {
        if (ptr != null)
            Marshal.FreeCoTaskMem((IntPtr)ptr);
    }

    // ── AI Engine ────────────────────────────────────────────────────────

    [UnmanagedCallersOnly(EntryPoint = "oynix_ai_build_prompt")]
    public static IntPtr AiBuildPrompt(byte* messagePtr)
    {
        var message = Marshal.PtrToStringUTF8((IntPtr)messagePtr) ?? "";
        return AllocString(AiEngine.Instance.BuildContextPrompt(message));
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_ai_build_messages")]
    public static IntPtr AiBuildMessages(byte* messagePtr)
    {
        var message = Marshal.PtrToStringUTF8((IntPtr)messagePtr) ?? "";
        return AllocString(AiEngine.Instance.BuildMessagesJson(message));
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_ai_add_history")]
    public static void AiAddHistory(byte* rolePtr, byte* contentPtr)
    {
        var role = Marshal.PtrToStringUTF8((IntPtr)rolePtr) ?? "";
        var content = Marshal.PtrToStringUTF8((IntPtr)contentPtr) ?? "";
        AiEngine.Instance.AddHistory(role, content);
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_ai_clear_history")]
    public static void AiClearHistory()
    {
        AiEngine.Instance.ClearHistory();
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_ai_fallback")]
    public static IntPtr AiFallback(byte* promptPtr)
    {
        var prompt = Marshal.PtrToStringUTF8((IntPtr)promptPtr) ?? "";
        return AllocString(AiEngine.Instance.FallbackResponse(prompt));
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_ai_get_config")]
    public static IntPtr AiGetConfig()
    {
        return AllocString(AiEngine.Instance.GetConfigJson());
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_ai_set_config")]
    public static void AiSetConfig(byte* jsonPtr)
    {
        var json = Marshal.PtrToStringUTF8((IntPtr)jsonPtr) ?? "";
        AiEngine.Instance.Configure(json);
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_ai_summarize_prompt")]
    public static IntPtr AiSummarizePrompt(byte* contentPtr, byte* titlePtr)
    {
        var content = Marshal.PtrToStringUTF8((IntPtr)contentPtr) ?? "";
        var title = Marshal.PtrToStringUTF8((IntPtr)titlePtr) ?? "";
        return AllocString(AiEngine.Instance.BuildSummarizePrompt(content, title));
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_ai_explain_prompt")]
    public static IntPtr AiExplainPrompt(byte* codePtr)
    {
        var code = Marshal.PtrToStringUTF8((IntPtr)codePtr) ?? "";
        return AllocString(AiEngine.Instance.BuildExplainPrompt(code));
    }

    // ── Extensions ───────────────────────────────────────────────────────

    [UnmanagedCallersOnly(EntryPoint = "oynix_ext_list")]
    public static IntPtr ExtList()
    {
        return AllocString(ExtensionRegistry.Instance.ListExtensionsJson());
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_ext_install")]
    public static int ExtInstall(byte* pathPtr)
    {
        var path = Marshal.PtrToStringUTF8((IntPtr)pathPtr) ?? "";
        return ExtensionRegistry.Instance.Install(path) ? 1 : 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_ext_remove")]
    public static int ExtRemove(byte* namePtr)
    {
        var name = Marshal.PtrToStringUTF8((IntPtr)namePtr) ?? "";
        return ExtensionRegistry.Instance.Remove(name) ? 1 : 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_ext_set_enabled")]
    public static void ExtSetEnabled(byte* namePtr, int enabled)
    {
        var name = Marshal.PtrToStringUTF8((IntPtr)namePtr) ?? "";
        ExtensionRegistry.Instance.SetEnabled(name, enabled != 0);
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_ext_content_scripts")]
    public static IntPtr ExtContentScripts(byte* urlPtr)
    {
        var url = Marshal.PtrToStringUTF8((IntPtr)urlPtr) ?? "";
        return AllocString(ExtensionRegistry.Instance.GetContentScriptsForUrl(url));
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_ext_manifest")]
    public static IntPtr ExtManifest(byte* namePtr)
    {
        var name = Marshal.PtrToStringUTF8((IntPtr)namePtr) ?? "";
        return AllocString(ExtensionRegistry.Instance.GetManifestJson(name) ?? "{}");
    }

    // ── Bookmarks ────────────────────────────────────────────────────────

    [UnmanagedCallersOnly(EntryPoint = "oynix_bm_add")]
    public static int BmAdd(byte* urlPtr, byte* titlePtr, byte* folderPtr)
    {
        var url = Marshal.PtrToStringUTF8((IntPtr)urlPtr) ?? "";
        var title = Marshal.PtrToStringUTF8((IntPtr)titlePtr) ?? "";
        var folder = Marshal.PtrToStringUTF8((IntPtr)folderPtr) ?? "";
        return BookmarkStore.Instance.Add(url, title, folder) ? 1 : 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_bm_remove")]
    public static int BmRemove(byte* urlPtr)
    {
        var url = Marshal.PtrToStringUTF8((IntPtr)urlPtr) ?? "";
        return BookmarkStore.Instance.Remove(url) ? 1 : 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_bm_list")]
    public static IntPtr BmList()
    {
        return AllocString(BookmarkStore.Instance.GetAllJson());
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_bm_search")]
    public static IntPtr BmSearch(byte* queryPtr)
    {
        var query = Marshal.PtrToStringUTF8((IntPtr)queryPtr) ?? "";
        return AllocString(BookmarkStore.Instance.SearchJson(query));
    }

    // ── Session ──────────────────────────────────────────────────────────

    [UnmanagedCallersOnly(EntryPoint = "oynix_session_save")]
    public static int SessionSave(byte* tabsJsonPtr)
    {
        var json = Marshal.PtrToStringUTF8((IntPtr)tabsJsonPtr) ?? "[]";
        return SessionStore.Instance.SaveSession(json) ? 1 : 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_session_load")]
    public static IntPtr SessionLoad()
    {
        return AllocString(SessionStore.Instance.LoadSession());
    }

    // ── Search ───────────────────────────────────────────────────────────

    [UnmanagedCallersOnly(EntryPoint = "oynix_search_url")]
    public static IntPtr SearchUrl(byte* queryPtr)
    {
        var query = Marshal.PtrToStringUTF8((IntPtr)queryPtr) ?? "";
        return AllocString(SearchEngine.Instance.GetSearchUrl(query));
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_search_is_url")]
    public static int SearchIsUrl(byte* inputPtr)
    {
        var input = Marshal.PtrToStringUTF8((IntPtr)inputPtr) ?? "";
        return SearchEngine.Instance.LooksLikeUrl(input) ? 1 : 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_search_fts5")]
    public static IntPtr SearchFts5(byte* queryPtr)
    {
        var query = Marshal.PtrToStringUTF8((IntPtr)queryPtr) ?? "";
        return AllocString(SearchEngine.Instance.BuildFts5Query(query));
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_search_set_engine")]
    public static void SearchSetEngine(byte* namePtr)
    {
        var name = Marshal.PtrToStringUTF8((IntPtr)namePtr) ?? "";
        SearchEngine.Instance.SetDefaultEngine(name);
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_search_engines")]
    public static IntPtr SearchEngines()
    {
        return AllocString(SearchEngine.Instance.GetAvailableEnginesJson());
    }

    // ── Security ─────────────────────────────────────────────────────────

    [UnmanagedCallersOnly(EntryPoint = "oynix_sec_is_login")]
    public static int SecIsLogin(byte* urlPtr)
    {
        var url = Marshal.PtrToStringUTF8((IntPtr)urlPtr) ?? "";
        return SecurityChecker.Instance.IsLoginPage(url) ? 1 : 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_sec_info")]
    public static IntPtr SecInfo(byte* urlPtr)
    {
        var url = Marshal.PtrToStringUTF8((IntPtr)urlPtr) ?? "";
        return AllocString(SecurityChecker.Instance.GetSecurityInfo(url));
    }

    // ── Web Crawler ─────────────────────────────────────────────────────

    [UnmanagedCallersOnly(EntryPoint = "oynix_crawler_configure")]
    public static void CrawlerConfigure(int maxDepth, int maxPages, int concurrency, int followExternal)
    {
        WebCrawler.Instance.Configure(maxDepth, maxPages, concurrency, followExternal != 0);
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_crawler_start_list")]
    public static void CrawlerStartList(byte* urlsJsonPtr)
    {
        var json = Marshal.PtrToStringUTF8((IntPtr)urlsJsonPtr) ?? "[]";
        // Parse JSON array of URL strings
        var urls = ParseStringArray(json);
        WebCrawler.Instance.StartFromList(urls);
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_crawler_start_broad")]
    public static void CrawlerStartBroad(byte* seedUrlPtr)
    {
        var seedUrl = Marshal.PtrToStringUTF8((IntPtr)seedUrlPtr) ?? "";
        WebCrawler.Instance.StartBroadCrawl(seedUrl);
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_crawler_stop")]
    public static void CrawlerStop()
    {
        WebCrawler.Instance.Stop();
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_crawler_status")]
    public static IntPtr CrawlerStatus()
    {
        return AllocString(WebCrawler.Instance.GetStatusJson());
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_crawler_results")]
    public static IntPtr CrawlerResults(int offset, int limit)
    {
        return AllocString(WebCrawler.Instance.GetResultsJson(offset, limit));
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_crawler_count")]
    public static int CrawlerCount()
    {
        return WebCrawler.Instance.GetCrawledCount();
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_crawler_is_running")]
    public static int CrawlerIsRunning()
    {
        return WebCrawler.Instance.IsRunning ? 1 : 0;
    }

    // ── Site Database ──────────────────────────────────────────────────

    [UnmanagedCallersOnly(EntryPoint = "oynix_sitedb_add")]
    public static int SiteDbAdd(byte* urlPtr, byte* titlePtr, byte* descPtr, byte* sourcePtr)
    {
        var url = Marshal.PtrToStringUTF8((IntPtr)urlPtr) ?? "";
        var title = Marshal.PtrToStringUTF8((IntPtr)titlePtr) ?? "";
        var desc = Marshal.PtrToStringUTF8((IntPtr)descPtr) ?? "";
        var source = Marshal.PtrToStringUTF8((IntPtr)sourcePtr) ?? "";
        return SiteDatabase.Instance.AddSite(url, title, desc, source) ? 1 : 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_sitedb_search")]
    public static IntPtr SiteDbSearch(byte* queryPtr)
    {
        var query = Marshal.PtrToStringUTF8((IntPtr)queryPtr) ?? "";
        return AllocString(SiteDatabase.Instance.SearchJson(query));
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_sitedb_categories")]
    public static IntPtr SiteDbCategories()
    {
        return AllocString(SiteDatabase.Instance.GetCategoriesJson());
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_sitedb_stats")]
    public static IntPtr SiteDbStats()
    {
        return AllocString(SiteDatabase.Instance.GetStatsJson());
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_sitedb_export")]
    public static IntPtr SiteDbExport()
    {
        return AllocString(SiteDatabase.Instance.ExportJson());
    }

    // ── History ─────────────────────────────────────────────────────────

    [UnmanagedCallersOnly(EntryPoint = "oynix_history_add")]
    public static void HistoryAdd(byte* urlPtr, byte* titlePtr)
    {
        var url = Marshal.PtrToStringUTF8((IntPtr)urlPtr) ?? "";
        var title = Marshal.PtrToStringUTF8((IntPtr)titlePtr) ?? "";
        HistoryStore.Instance.AddEntry(url, title);
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_history_get_all")]
    public static IntPtr HistoryGetAll()
    {
        return AllocString(HistoryStore.Instance.GetAllJson());
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_history_get_recent")]
    public static IntPtr HistoryGetRecent(int count)
    {
        return AllocString(HistoryStore.Instance.GetRecentJson(count));
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_history_search")]
    public static IntPtr HistorySearch(byte* queryPtr)
    {
        var query = Marshal.PtrToStringUTF8((IntPtr)queryPtr) ?? "";
        return AllocString(HistoryStore.Instance.SearchJson(query));
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_history_clear")]
    public static void HistoryClear()
    {
        HistoryStore.Instance.Clear();
    }

    // ── Profiles ────────────────────────────────────────────────────────

    [UnmanagedCallersOnly(EntryPoint = "oynix_profile_create")]
    public static int ProfileCreate(byte* namePtr, byte* displayNamePtr)
    {
        var name = Marshal.PtrToStringUTF8((IntPtr)namePtr) ?? "";
        var dn = Marshal.PtrToStringUTF8((IntPtr)displayNamePtr) ?? "";
        return ProfileManager.Instance.CreateProfile(name, dn) ? 1 : 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_profile_switch")]
    public static int ProfileSwitch(byte* namePtr, byte* passwordPtr)
    {
        var name = Marshal.PtrToStringUTF8((IntPtr)namePtr) ?? "";
        var pw = Marshal.PtrToStringUTF8((IntPtr)passwordPtr);
        return ProfileManager.Instance.SwitchProfile(name, pw) ? 1 : 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_profile_delete")]
    public static int ProfileDelete(byte* namePtr)
    {
        var name = Marshal.PtrToStringUTF8((IntPtr)namePtr) ?? "";
        return ProfileManager.Instance.DeleteProfile(name) ? 1 : 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_profile_list")]
    public static IntPtr ProfileList()
    {
        return AllocString(ProfileManager.Instance.ListProfilesJson());
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_profile_active")]
    public static IntPtr ProfileActive()
    {
        return AllocString(ProfileManager.Instance.ActiveProfileName);
    }

    // ── Credentials ─────────────────────────────────────────────────────

    [UnmanagedCallersOnly(EntryPoint = "oynix_cred_set_master")]
    public static int CredSetMaster(byte* passwordPtr)
    {
        var pw = Marshal.PtrToStringUTF8((IntPtr)passwordPtr) ?? "";
        return CredentialStore.Instance.SetMasterPassword(pw) ? 1 : 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_cred_unlock")]
    public static int CredUnlock(byte* passwordPtr)
    {
        var pw = Marshal.PtrToStringUTF8((IntPtr)passwordPtr) ?? "";
        return CredentialStore.Instance.Unlock(pw) ? 1 : 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_cred_save")]
    public static int CredSave(byte* domainPtr, byte* usernamePtr, byte* passwordPtr)
    {
        var domain = Marshal.PtrToStringUTF8((IntPtr)domainPtr) ?? "";
        var user = Marshal.PtrToStringUTF8((IntPtr)usernamePtr) ?? "";
        var pass = Marshal.PtrToStringUTF8((IntPtr)passwordPtr) ?? "";
        return CredentialStore.Instance.SaveCredential(domain, user, pass) ? 1 : 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_cred_get")]
    public static IntPtr CredGet(byte* domainPtr)
    {
        var domain = Marshal.PtrToStringUTF8((IntPtr)domainPtr) ?? "";
        return AllocString(CredentialStore.Instance.GetCredentialsJson(domain));
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_cred_domains")]
    public static IntPtr CredDomains()
    {
        return AllocString(CredentialStore.Instance.GetAllDomainsJson());
    }

    // ── GitHub Sync ─────────────────────────────────────────────────────

    [UnmanagedCallersOnly(EntryPoint = "oynix_gh_configure")]
    public static void GhConfigure(byte* tokenPtr, byte* repoPtr, int autoSync, int intervalMin)
    {
        var token = Marshal.PtrToStringUTF8((IntPtr)tokenPtr) ?? "";
        var repo = Marshal.PtrToStringUTF8((IntPtr)repoPtr) ?? "";
        GitHubSync.Instance.Configure(token, repo, autoSync != 0, intervalMin);
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_gh_upload")]
    public static IntPtr GhUpload(byte* jsonContentPtr)
    {
        var json = Marshal.PtrToStringUTF8((IntPtr)jsonContentPtr) ?? "";
        return AllocString(GitHubSync.Instance.Upload(json));
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_gh_import")]
    public static IntPtr GhImport(byte* repoPtr)
    {
        var repo = Marshal.PtrToStringUTF8((IntPtr)repoPtr);
        return AllocString(GitHubSync.Instance.Import(repo));
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_gh_config")]
    public static IntPtr GhConfig()
    {
        return AllocString(GitHubSync.Instance.GetConfigJson());
    }

    // ── Chrome Import ───────────────────────────────────────────────────

    [UnmanagedCallersOnly(EntryPoint = "oynix_chrome_available")]
    public static int ChromeAvailable()
    {
        return ChromeImporter.Instance.IsChromeAvailable() ? 1 : 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_chrome_import_history")]
    public static IntPtr ChromeImportHistory()
    {
        return AllocString(ChromeImporter.Instance.ImportHistory());
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_chrome_import_searches")]
    public static IntPtr ChromeImportSearches()
    {
        return AllocString(ChromeImporter.Instance.ImportSearchHistory());
    }

    // ── Nydta Archive ───────────────────────────────────────────────────

    [UnmanagedCallersOnly(EntryPoint = "oynix_nydta_export")]
    public static IntPtr NydtaExport(byte* filepathPtr, byte* historyPtr,
        byte* configPtr, byte* databasePtr)
    {
        var filepath = Marshal.PtrToStringUTF8((IntPtr)filepathPtr) ?? "";
        var history = Marshal.PtrToStringUTF8((IntPtr)historyPtr) ?? "[]";
        var config = Marshal.PtrToStringUTF8((IntPtr)configPtr) ?? "{}";
        var database = Marshal.PtrToStringUTF8((IntPtr)databasePtr) ?? "[]";
        return AllocString(NydtaArchive.Instance.Export(filepath, history, config, database));
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_nydta_import")]
    public static IntPtr NydtaImport(byte* filepathPtr)
    {
        var filepath = Marshal.PtrToStringUTF8((IntPtr)filepathPtr) ?? "";
        return AllocString(NydtaArchive.Instance.Import(filepath));
    }

    // ── Security (expanded) ─────────────────────────────────────────────

    [UnmanagedCallersOnly(EntryPoint = "oynix_sec_trust_domain")]
    public static void SecTrustDomain(byte* domainPtr)
    {
        var domain = Marshal.PtrToStringUTF8((IntPtr)domainPtr) ?? "";
        SecurityChecker.Instance.TrustDomain(domain);
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_sec_block_domain")]
    public static void SecBlockDomain(byte* domainPtr)
    {
        var domain = Marshal.PtrToStringUTF8((IntPtr)domainPtr) ?? "";
        SecurityChecker.Instance.BlockDomain(domain);
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_sec_should_prompt")]
    public static int SecShouldPrompt(byte* urlPtr)
    {
        var url = Marshal.PtrToStringUTF8((IntPtr)urlPtr) ?? "";
        return SecurityChecker.Instance.ShouldPrompt(url) ? 1 : 0;
    }

    // ── Bookmark (expanded) ─────────────────────────────────────────────

    [UnmanagedCallersOnly(EntryPoint = "oynix_bm_is_bookmarked")]
    public static int BmIsBookmarked(byte* urlPtr)
    {
        var url = Marshal.PtrToStringUTF8((IntPtr)urlPtr) ?? "";
        return BookmarkStore.Instance.IsBookmarked(url) ? 1 : 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_bm_folders")]
    public static IntPtr BmFolders()
    {
        return AllocString(BookmarkStore.Instance.GetFoldersJson());
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_bm_export")]
    public static IntPtr BmExport()
    {
        return AllocString(BookmarkStore.Instance.ExportJson());
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_bm_import")]
    public static int BmImport(byte* jsonPtr)
    {
        var json = Marshal.PtrToStringUTF8((IntPtr)jsonPtr) ?? "";
        return BookmarkStore.Instance.ImportJson(json) ? 1 : 0;
    }

    // ── Session (expanded) ──────────────────────────────────────────────

    [UnmanagedCallersOnly(EntryPoint = "oynix_session_list")]
    public static IntPtr SessionList()
    {
        return AllocString(SessionStore.Instance.ListSessionsJson());
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_session_save_named")]
    public static int SessionSaveNamed(byte* namePtr, byte* tabsPtr)
    {
        var name = Marshal.PtrToStringUTF8((IntPtr)namePtr) ?? "";
        var tabs = Marshal.PtrToStringUTF8((IntPtr)tabsPtr) ?? "[]";
        return SessionStore.Instance.SaveNamedSession(name, tabs) ? 1 : 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "oynix_session_delete")]
    public static int SessionDelete(byte* namePtr)
    {
        var name = Marshal.PtrToStringUTF8((IntPtr)namePtr) ?? "";
        return SessionStore.Instance.DeleteSession(name) ? 1 : 0;
    }

    // Helper to parse a JSON array of strings without System.Text.Json reflection
    private static string[] ParseStringArray(string json)
    {
        var results = new List<string>();
        json = json.Trim();
        if (!json.StartsWith("[") || !json.EndsWith("]"))
            return results.ToArray();

        json = json[1..^1]; // strip [ ]
        bool inString = false;
        bool escaped = false;
        int start = -1;
        for (int i = 0; i < json.Length; i++)
        {
            char c = json[i];
            if (escaped) { escaped = false; continue; }
            if (c == '\\') { escaped = true; continue; }

            if (c == '"')
            {
                if (!inString) { inString = true; start = i + 1; }
                else
                {
                    results.Add(json[start..i]
                        .Replace("\\\"", "\"")
                        .Replace("\\\\", "\\"));
                    inString = false;
                }
            }
        }
        return results.ToArray();
    }
}

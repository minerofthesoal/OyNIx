using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using OyNIx.Core.AI;
using OyNIx.Core.Data;
using OyNIx.Core.Extensions;
using OyNIx.Core.Search;
using OyNIx.Core.Crawler;
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

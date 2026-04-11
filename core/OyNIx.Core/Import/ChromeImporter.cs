using System.Text.Json.Nodes;

namespace OyNIx.Core.Import;

/// <summary>
/// Chrome/Chromium history and search import.
/// Port of oynix/core/chrome_import.py.
/// Reads Chrome's SQLite History DB and Google Takeout JSON.
/// </summary>
public sealed class ChromeImporter
{
    public static ChromeImporter Instance { get; } = new();

    /// <summary>Find Chrome's History database path on this system.</summary>
    public string? FindChromeHistoryDb()
    {
        var home = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
        var candidates = new List<string>();

        // Linux
        candidates.Add(Path.Combine(home, ".config", "google-chrome", "Default", "History"));
        candidates.Add(Path.Combine(home, ".config", "chromium", "Default", "History"));

        // macOS
        candidates.Add(Path.Combine(home, "Library", "Application Support",
            "Google", "Chrome", "Default", "History"));

        // Windows
        var local = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        if (!string.IsNullOrEmpty(local))
            candidates.Add(Path.Combine(local, "Google", "Chrome", "User Data", "Default", "History"));

        return candidates.FirstOrDefault(File.Exists);
    }

    /// <summary>
    /// Import browsing history from Chrome's SQLite database.
    /// Returns JSON: {"ok": true, "entries": [...], "message": "..."} or error.
    /// </summary>
    public string ImportHistory(string? dbPath = null, int maxEntries = 5000)
    {
        dbPath ??= FindChromeHistoryDb();
        if (string.IsNullOrEmpty(dbPath) || !File.Exists(dbPath))
            return "{\"ok\":false,\"entries\":[],\"message\":\"Chrome history database not found\"}";

        // Chrome locks its DB; copy to temp
        var tmpPath = Path.GetTempFileName();
        try
        {
            File.Copy(dbPath, tmpPath, true);

            // Use raw SQLite via Microsoft.Data.Sqlite pattern
            // Since we can't add NuGet packages in NativeAOT without issues,
            // we read the file and parse it manually via shell or fallback
            var entries = ReadChromeDb(tmpPath, maxEntries);

            var entriesArr = new JsonArray();
            foreach (var e in entries)
                entriesArr.Add(new JsonObject
                {
                    ["Url"] = e.Url, ["Title"] = e.Title,
                    ["Time"] = e.Time, ["VisitCount"] = e.VisitCount
                });
            return new JsonObject
            {
                ["ok"] = true,
                ["entries"] = entriesArr,
                ["message"] = $"Imported {entries.Count} entries from Chrome"
            }.ToJsonString();
        }
        catch (Exception ex)
        {
            return new JsonObject
            {
                ["ok"] = false,
                ["entries"] = new JsonArray(),
                ["message"] = $"Error: {ex.Message}"
            }.ToJsonString();
        }
        finally
        {
            try { File.Delete(tmpPath); } catch { }
        }
    }

    /// <summary>
    /// Extract search queries from Chrome history by parsing search URLs.
    /// </summary>
    public string ImportSearchHistory(string? dbPath = null, int maxEntries = 2000)
    {
        dbPath ??= FindChromeHistoryDb();
        if (string.IsNullOrEmpty(dbPath) || !File.Exists(dbPath))
            return "{\"ok\":false,\"queries\":[],\"message\":\"Chrome history not found\"}";

        var tmpPath = Path.GetTempFileName();
        try
        {
            File.Copy(dbPath, tmpPath, true);
            var entries = ReadChromeDb(tmpPath, maxEntries);

            var queriesArr = new JsonArray();
            foreach (var entry in entries)
            {
                var searchQuery = ExtractSearchQuery(entry.Url);
                if (!string.IsNullOrEmpty(searchQuery))
                {
                    queriesArr.Add(new JsonObject
                    {
                        ["query"] = searchQuery, ["time"] = entry.Time, ["url"] = entry.Url
                    });
                }
            }

            return new JsonObject
            {
                ["ok"] = true,
                ["queries"] = queriesArr,
                ["message"] = $"Found {queriesArr.Count} search queries"
            }.ToJsonString();
        }
        catch (Exception ex)
        {
            return new JsonObject
            {
                ["ok"] = false,
                ["queries"] = new JsonArray(),
                ["message"] = $"Error: {ex.Message}"
            }.ToJsonString();
        }
        finally
        {
            try { File.Delete(tmpPath); } catch { }
        }
    }

    /// <summary>Import from Google Takeout JSON export.</summary>
    public string ImportGoogleTakeout(string filePath)
    {
        if (!File.Exists(filePath))
            return "{\"ok\":false,\"entries\":[],\"message\":\"File not found\"}";

        try
        {
            var json = File.ReadAllText(filePath);
            var root = JsonNode.Parse(json);
            var entriesArr = new JsonArray();

            // Handle BrowserHistory.json format (array of {url, title, time_usec})
            if (root is JsonArray arr)
            {
                foreach (var item in arr)
                {
                    var obj = item?.AsObject();
                    if (obj == null) continue;
                    entriesArr.Add(new JsonObject
                    {
                        ["url"] = obj["url"]?.GetValue<string>() ?? "",
                        ["title"] = obj["title"]?.GetValue<string>() ?? "",
                        ["time"] = obj["time_usec"]?.GetValue<long>() is long t
                            ? DateTimeOffset.FromUnixTimeMilliseconds(t / 1000).ToString("yyyy-MM-dd HH:mm")
                            : "",
                        ["source"] = "takeout"
                    });
                }
            }

            return new JsonObject
            {
                ["ok"] = true,
                ["entries"] = entriesArr,
                ["message"] = $"Imported {entriesArr.Count} entries from Takeout"
            }.ToJsonString();
        }
        catch (Exception ex)
        {
            return new JsonObject
            {
                ["ok"] = false,
                ["entries"] = new JsonArray(),
                ["message"] = $"Error: {ex.Message}"
            }.ToJsonString();
        }
    }

    public bool IsChromeAvailable() => FindChromeHistoryDb() != null;

    private record HistoryEntry(string Url, string Title, string Time, int VisitCount);

    private List<HistoryEntry> ReadChromeDb(string dbPath, int maxEntries)
    {
        // SQLite binary parsing is complex; use the simpler approach of
        // reading via sqlite3 CLI if available, otherwise return empty
        var entries = new List<HistoryEntry>();

        try
        {
            // Try sqlite3 CLI (available on most Linux/macOS systems)
            var psi = new System.Diagnostics.ProcessStartInfo
            {
                FileName = "sqlite3",
                Arguments = $"\"{dbPath}\" \"SELECT url, title, visit_count, last_visit_time FROM urls ORDER BY last_visit_time DESC LIMIT {maxEntries}\"",
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                UseShellExecute = false,
                CreateNoWindow = true
            };

            using var proc = System.Diagnostics.Process.Start(psi);
            if (proc == null) return entries;

            var output = proc.StandardOutput.ReadToEnd();
            proc.WaitForExit(10000);

            foreach (var line in output.Split('\n', StringSplitOptions.RemoveEmptyEntries))
            {
                var parts = line.Split('|');
                if (parts.Length < 4) continue;

                var url = parts[0];
                var title = parts[1];
                int.TryParse(parts[2], out var visits);
                long.TryParse(parts[3], out var chromeTime);

                // Chrome time: microseconds since Jan 1, 1601
                var timeStr = "";
                if (chromeTime > 0)
                {
                    try
                    {
                        var epoch = new DateTime(1601, 1, 1, 0, 0, 0, DateTimeKind.Utc);
                        var dt = epoch.AddTicks(chromeTime * 10); // microseconds to ticks
                        timeStr = dt.ToString("yyyy-MM-dd HH:mm");
                    }
                    catch { }
                }

                entries.Add(new HistoryEntry(url, string.IsNullOrEmpty(title) ? url : title, timeStr, visits));
            }
        }
        catch { }

        return entries;
    }

    private static string? ExtractSearchQuery(string url)
    {
        try
        {
            var uri = new Uri(url);
            var query = System.Web.HttpUtility.ParseQueryString(uri.Query);

            // Google, DuckDuckGo, Bing, Brave
            var q = query["q"] ?? query["query"] ?? query["search_query"];
            if (!string.IsNullOrEmpty(q)) return q;
        }
        catch { }
        return null;
    }
}

using System.Net.Http.Headers;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using OyNIx.Core.Data;

namespace OyNIx.Core.GitHub;

/// <summary>
/// GitHub database sync — upload/import Nyx index to a GitHub repo.
/// Port of oynix/core/github_sync.py.
/// </summary>
public sealed class GitHubSync
{
    public static GitHubSync Instance { get; } = new();

    private string _configDir = "";
    private string _token = "";
    private string _repo = "";     // "owner/repo"
    private bool _autoSync;
    private int _syncIntervalMinutes = 60;
    private Timer? _syncTimer;

    private volatile bool _uploading;
    private volatile bool _importing;

    public bool IsConfigured => !string.IsNullOrEmpty(_token) && !string.IsNullOrEmpty(_repo);

    public void Initialize(string configDir)
    {
        _configDir = configDir;
        LoadConfig();
    }

    public void Configure(string token, string repo, bool autoSync, int intervalMinutes)
    {
        _token = token;
        _repo = repo;
        _autoSync = autoSync;
        _syncIntervalMinutes = Math.Max(30, intervalMinutes);
        SaveConfig();

        if (_autoSync && IsConfigured)
            StartAutoSync();
        else
            StopAutoSync();
    }

    public string GetConfigJson()
    {
        return JsonSerializer.Serialize(new
        {
            token = string.IsNullOrEmpty(_token) ? "" : "***configured***",
            repo = _repo,
            auto_sync = _autoSync,
            interval_minutes = _syncIntervalMinutes,
            configured = IsConfigured
        });
    }

    /// <summary>Upload a JSON file to the configured GitHub repo.</summary>
    public string Upload(string jsonContent, string targetPath = "database/nyx_index.json")
    {
        if (!IsConfigured) return "{\"ok\":false,\"error\":\"Not configured\"}";
        if (_uploading) return "{\"ok\":false,\"error\":\"Upload already in progress\"}";

        _uploading = true;
        try
        {
            using var client = CreateClient();

            var encoded = Convert.ToBase64String(Encoding.UTF8.GetBytes(jsonContent));
            var apiUrl = $"https://api.github.com/repos/{_repo}/contents/{targetPath}";

            // Check if file exists (get SHA for update)
            string? sha = null;
            try
            {
                var getResp = client.GetAsync(apiUrl).GetAwaiter().GetResult();
                if (getResp.IsSuccessStatusCode)
                {
                    var getJson = JsonNode.Parse(
                        getResp.Content.ReadAsStringAsync().GetAwaiter().GetResult());
                    sha = getJson?["sha"]?.GetValue<string>();
                }
            }
            catch { }

            var body = new JsonObject
            {
                ["message"] = $"Update OyNIx database - {DateTime.UtcNow:yyyy-MM-dd HH:mm}",
                ["content"] = encoded
            };
            if (sha != null) body["sha"] = sha;

            var content = new StringContent(body.ToJsonString(), Encoding.UTF8, "application/json");
            var resp = client.PutAsync(apiUrl, content).GetAwaiter().GetResult();

            return resp.IsSuccessStatusCode
                ? "{\"ok\":true,\"message\":\"Uploaded successfully\"}"
                : $"{{\"ok\":false,\"error\":\"HTTP {(int)resp.StatusCode}\"}}";
        }
        catch (Exception ex)
        {
            return $"{{\"ok\":false,\"error\":\"{JsonEscape(ex.Message)}\"}}";
        }
        finally
        {
            _uploading = false;
        }
    }

    /// <summary>Import a database from a GitHub repo.</summary>
    public string Import(string? repoOverride = null)
    {
        var repo = repoOverride ?? _repo;
        if (string.IsNullOrEmpty(repo))
            return "{\"ok\":false,\"error\":\"No repo specified\"}";
        if (_importing) return "{\"ok\":false,\"error\":\"Import already in progress\"}";

        _importing = true;
        try
        {
            using var client = CreateClient();
            var rawUrl = $"https://raw.githubusercontent.com/{repo}/main/database/nyx_index.json";

            var resp = client.GetAsync(rawUrl).GetAwaiter().GetResult();
            if (!resp.IsSuccessStatusCode)
                return $"{{\"ok\":false,\"error\":\"HTTP {(int)resp.StatusCode}\"}}";

            var content = resp.Content.ReadAsStringAsync().GetAwaiter().GetResult();
            return $"{{\"ok\":true,\"data\":{content}}}";
        }
        catch (Exception ex)
        {
            return $"{{\"ok\":false,\"error\":\"{JsonEscape(ex.Message)}\"}}";
        }
        finally
        {
            _importing = false;
        }
    }

    public void StartAutoSync()
    {
        StopAutoSync();
        if (!IsConfigured) return;
        _syncTimer = new Timer(_ => OnAutoSync(),
            null,
            TimeSpan.FromMinutes(_syncIntervalMinutes),
            TimeSpan.FromMinutes(_syncIntervalMinutes));
    }

    public void StopAutoSync()
    {
        _syncTimer?.Dispose();
        _syncTimer = null;
    }

    private void OnAutoSync()
    {
        // Export current database and upload
        var data = SiteDatabase.Instance.ExportJson();
        Upload(data);
    }

    private HttpClient CreateClient()
    {
        var client = new HttpClient { Timeout = TimeSpan.FromSeconds(30) };
        client.DefaultRequestHeaders.Accept.ParseAdd("application/vnd.github.v3+json");
        client.DefaultRequestHeaders.UserAgent.ParseAdd("OyNIxBrowser/3.1");
        if (!string.IsNullOrEmpty(_token))
            client.DefaultRequestHeaders.Authorization =
                new AuthenticationHeaderValue("token", _token);
        return client;
    }

    private void LoadConfig()
    {
        var path = Path.Combine(_configDir, "github_sync.json");
        if (!File.Exists(path)) return;
        try
        {
            var obj = JsonNode.Parse(File.ReadAllText(path))?.AsObject();
            if (obj == null) return;
            _token = obj["token"]?.GetValue<string>() ?? "";
            _repo = obj["repo"]?.GetValue<string>() ?? "";
            _autoSync = obj["auto_sync"]?.GetValue<bool>() ?? false;
            _syncIntervalMinutes = obj["interval"]?.GetValue<int>() ?? 60;
        }
        catch { }
    }

    private void SaveConfig()
    {
        var path = Path.Combine(_configDir, "github_sync.json");
        var obj = new JsonObject
        {
            ["token"] = _token,
            ["repo"] = _repo,
            ["auto_sync"] = _autoSync,
            ["interval"] = _syncIntervalMinutes
        };
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        File.WriteAllText(path, obj.ToJsonString());
    }

    private static string JsonEscape(string s) =>
        s.Replace("\\", "\\\\").Replace("\"", "\\\"");
}

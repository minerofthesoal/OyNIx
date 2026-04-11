using System.Text.Json;
using System.Text.Json.Nodes;

namespace OyNIx.Core.Data;

/// <summary>
/// JSON-based browsing history with max entry trimming.
/// Port of oynix/core/HistoryManager (C++) + Python history logic.
/// </summary>
public sealed class HistoryStore
{
    public static HistoryStore Instance { get; } = new();

    private string _filePath = "";
    private JsonArray _entries = new();
    private const int MaxEntries = 5000;

    public void Initialize(string configDir)
    {
        _filePath = Path.Combine(configDir, "history.json");
        Load();
    }

    public void AddEntry(string url, string title)
    {
        _entries.Add(new JsonObject
        {
            ["url"] = url,
            ["title"] = title,
            ["time"] = DateTime.UtcNow.ToString("O")
        });
        Trim();
        Save();
    }

    public string GetAllJson() => _entries.ToJsonString();

    public string GetRecentJson(int count = 20)
    {
        var recent = new JsonArray();
        int start = Math.Max(0, _entries.Count - count);
        for (int i = _entries.Count - 1; i >= start; i--)
            recent.Add(JsonNode.Parse(_entries[i]!.ToJsonString()));
        return recent.ToJsonString();
    }

    public string SearchJson(string query)
    {
        var lower = query.ToLowerInvariant();
        var results = new JsonArray();
        for (int i = _entries.Count - 1; i >= 0; i--)
        {
            var e = _entries[i]?.AsObject();
            var url = e?["url"]?.GetValue<string>()?.ToLowerInvariant() ?? "";
            var title = e?["title"]?.GetValue<string>()?.ToLowerInvariant() ?? "";
            if (url.Contains(lower) || title.Contains(lower))
                results.Add(JsonNode.Parse(_entries[i]!.ToJsonString()));
            if (results.Count >= 100) break;
        }
        return results.ToJsonString();
    }

    public void Clear()
    {
        _entries = new JsonArray();
        Save();
    }

    public int Count => _entries.Count;

    private void Load()
    {
        if (!File.Exists(_filePath)) return;
        try
        {
            _entries = JsonSerializer.Deserialize<JsonArray>(
                File.ReadAllText(_filePath)) ?? new JsonArray();
        }
        catch { _entries = new JsonArray(); }
    }

    private void Save()
    {
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(_filePath)!);
            File.WriteAllText(_filePath, _entries.ToJsonString());
        }
        catch { }
    }

    private void Trim()
    {
        while (_entries.Count > MaxEntries)
            _entries.RemoveAt(0);
    }
}

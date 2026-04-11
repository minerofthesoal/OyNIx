using System.Text.Json;
using System.Text.Json.Nodes;

namespace OyNIx.Core.Data;

/// <summary>
/// Bookmark storage — JSON-based bookmark manager with folder hierarchy.
/// </summary>
public sealed class BookmarkStore
{
    public static BookmarkStore Instance { get; } = new();

    private string _filePath = "";
    private JsonArray _bookmarks = new();
    private JsonArray _folders = new();

    public void Initialize(string configDir)
    {
        _filePath = Path.Combine(configDir, "bookmarks.json");
        Load();
    }

    private void Load()
    {
        if (!File.Exists(_filePath)) return;
        try
        {
            var doc = JsonNode.Parse(File.ReadAllText(_filePath));
            if (doc is JsonObject obj)
            {
                _bookmarks = obj["bookmarks"]?.AsArray() ?? new JsonArray();
                _folders = obj["folders"]?.AsArray() ?? new JsonArray();
            }
        }
        catch { /* start fresh */ }
    }

    private void Save()
    {
        var root = new JsonObject
        {
            ["bookmarks"] = JsonNode.Parse(_bookmarks.ToJsonString()),
            ["folders"] = JsonNode.Parse(_folders.ToJsonString())
        };
        Directory.CreateDirectory(Path.GetDirectoryName(_filePath)!);
        File.WriteAllText(_filePath, root.ToJsonString(new JsonSerializerOptions { WriteIndented = true }));
    }

    public bool Add(string url, string title, string folder = "")
    {
        // Check duplicate
        foreach (var bm in _bookmarks)
        {
            if (bm?["url"]?.GetValue<string>() == url) return false;
        }

        _bookmarks.Add(new JsonObject
        {
            ["url"] = url,
            ["title"] = title,
            ["folder"] = folder,
            ["added"] = DateTime.UtcNow.ToString("O")
        });
        Save();
        return true;
    }

    public bool Remove(string url)
    {
        for (int i = _bookmarks.Count - 1; i >= 0; i--)
        {
            if (_bookmarks[i]?["url"]?.GetValue<string>() == url)
            {
                _bookmarks.RemoveAt(i);
                Save();
                return true;
            }
        }
        return false;
    }

    public string GetAllJson() => _bookmarks.ToJsonString();

    public string SearchJson(string query)
    {
        var lower = query.ToLowerInvariant();
        var results = new JsonArray();
        foreach (var bm in _bookmarks)
        {
            var title = bm?["title"]?.GetValue<string>()?.ToLowerInvariant() ?? "";
            var url = bm?["url"]?.GetValue<string>()?.ToLowerInvariant() ?? "";
            if (title.Contains(lower) || url.Contains(lower))
                results.Add(JsonNode.Parse(bm!.ToJsonString()));
        }
        return results.ToJsonString();
    }

    public bool AddFolder(string name)
    {
        _folders.Add(new JsonObject { ["name"] = name });
        Save();
        return true;
    }

    public bool RenameFolder(string oldName, string newName)
    {
        foreach (var f in _folders)
        {
            if (f?["name"]?.GetValue<string>() == oldName)
            {
                f!.AsObject()["name"] = newName;
                // Update bookmarks in the old folder
                foreach (var bm in _bookmarks)
                {
                    if (bm?["folder"]?.GetValue<string>() == oldName)
                        bm!.AsObject()["folder"] = newName;
                }
                Save();
                return true;
            }
        }
        return false;
    }

    public string GetFoldersJson()
    {
        // Include default folders + custom ones
        var all = new JsonArray();
        all.Add(new JsonObject { ["name"] = "Quick Access" });
        all.Add(new JsonObject { ["name"] = "Reading List" });
        foreach (var f in _folders)
            all.Add(JsonNode.Parse(f!.ToJsonString()));
        return all.ToJsonString();
    }

    public string GetByFolderJson(string folder)
    {
        var results = new JsonArray();
        foreach (var bm in _bookmarks)
        {
            if (bm?["folder"]?.GetValue<string>() == folder)
                results.Add(JsonNode.Parse(bm!.ToJsonString()));
        }
        return results.ToJsonString();
    }

    public bool IsBookmarked(string url)
    {
        return _bookmarks.Any(bm => bm?["url"]?.GetValue<string>() == url);
    }

    public string ExportJson()
    {
        var root = new JsonObject
        {
            ["version"] = "3.1",
            ["exported_at"] = DateTime.UtcNow.ToString("O"),
            ["bookmarks"] = JsonNode.Parse(_bookmarks.ToJsonString()),
            ["folders"] = JsonNode.Parse(_folders.ToJsonString())
        };
        return root.ToJsonString(new JsonSerializerOptions { WriteIndented = true });
    }

    public bool ImportJson(string json)
    {
        try
        {
            var doc = JsonNode.Parse(json);
            if (doc is JsonObject obj)
            {
                var imported = obj["bookmarks"]?.AsArray();
                if (imported == null) return false;

                int added = 0;
                foreach (var bm in imported)
                {
                    var url = bm?["url"]?.GetValue<string>() ?? "";
                    if (string.IsNullOrEmpty(url)) continue;
                    if (_bookmarks.Any(b => b?["url"]?.GetValue<string>() == url))
                        continue;
                    _bookmarks.Add(JsonNode.Parse(bm!.ToJsonString()));
                    added++;
                }

                if (added > 0) Save();
                return true;
            }
        }
        catch { }
        return false;
    }
}

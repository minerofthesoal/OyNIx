using System.Text.Json;
using System.Text.Json.Nodes;

namespace OyNIx.Core.Data;

/// <summary>
/// Session persistence — saves/restores open tabs for session continuity.
/// </summary>
public sealed class SessionStore
{
    public static SessionStore Instance { get; } = new();

    private string _filePath = "";

    public void Initialize(string configDir)
    {
        _filePath = Path.Combine(configDir, "sessions", "last_session.json");
        Directory.CreateDirectory(Path.GetDirectoryName(_filePath)!);
    }

    public bool SaveSession(string tabsJson)
    {
        try
        {
            var session = new JsonObject
            {
                ["version"] = "3.1",
                ["timestamp"] = DateTime.UtcNow.ToString("O"),
                ["tabs"] = JsonNode.Parse(tabsJson)
            };
            File.WriteAllText(_filePath, session.ToJsonString(new JsonSerializerOptions { WriteIndented = true }));
            return true;
        }
        catch { return false; }
    }

    public string LoadSession()
    {
        if (!File.Exists(_filePath)) return "[]";
        try
        {
            var doc = JsonNode.Parse(File.ReadAllText(_filePath));
            return doc?["tabs"]?.ToJsonString() ?? "[]";
        }
        catch { return "[]"; }
    }

    /// <summary>Save a named session.</summary>
    public bool SaveNamedSession(string name, string tabsJson)
    {
        try
        {
            var dir = Path.GetDirectoryName(_filePath)!;
            var path = Path.Combine(dir, $"{SanitizeName(name)}.json");
            var session = new JsonObject
            {
                ["version"] = "3.1",
                ["name"] = name,
                ["timestamp"] = DateTime.UtcNow.ToString("O"),
                ["tabs"] = JsonNode.Parse(tabsJson)
            };
            File.WriteAllText(path,
                session.ToJsonString(new JsonSerializerOptions { WriteIndented = true }));
            return true;
        }
        catch { return false; }
    }

    /// <summary>Load a named session.</summary>
    public string LoadNamedSession(string name)
    {
        var dir = Path.GetDirectoryName(_filePath)!;
        var path = Path.Combine(dir, $"{SanitizeName(name)}.json");
        if (!File.Exists(path)) return "[]";
        try
        {
            var doc = JsonNode.Parse(File.ReadAllText(path));
            return doc?["tabs"]?.ToJsonString() ?? "[]";
        }
        catch { return "[]"; }
    }

    /// <summary>List available sessions as JSON array.</summary>
    public string ListSessionsJson()
    {
        var dir = Path.GetDirectoryName(_filePath)!;
        if (!Directory.Exists(dir)) return "[]";

        var sessions = new JsonArray();
        foreach (var file in Directory.GetFiles(dir, "*.json"))
        {
            try
            {
                var doc = JsonNode.Parse(File.ReadAllText(file));
                sessions.Add(new JsonObject
                {
                    ["name"] = doc?["name"]?.GetValue<string>() ??
                               Path.GetFileNameWithoutExtension(file),
                    ["timestamp"] = doc?["timestamp"]?.GetValue<string>() ?? "",
                    ["tab_count"] = doc?["tabs"]?.AsArray().Count ?? 0
                });
            }
            catch { }
        }
        return sessions.ToJsonString();
    }

    /// <summary>Delete a named session.</summary>
    public bool DeleteSession(string name)
    {
        var dir = Path.GetDirectoryName(_filePath)!;
        var path = Path.Combine(dir, $"{SanitizeName(name)}.json");
        if (!File.Exists(path)) return false;
        try { File.Delete(path); return true; }
        catch { return false; }
    }

    private static string SanitizeName(string name) =>
        System.Text.RegularExpressions.Regex.Replace(name, @"[^\w\-.]", "_");
}

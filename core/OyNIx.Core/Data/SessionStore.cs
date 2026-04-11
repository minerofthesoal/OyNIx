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
}

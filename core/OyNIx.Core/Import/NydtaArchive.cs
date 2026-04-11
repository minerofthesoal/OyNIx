using System.IO.Compression;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace OyNIx.Core.Import;

/// <summary>
/// .nydta profile archive format — zip-based export/import.
/// Port of oynix/core/nydta.py.
/// </summary>
public sealed class NydtaArchive
{
    public static NydtaArchive Instance { get; } = new();

    private const string NydtaVersion = "3.0";

    /// <summary>
    /// Export a .nydta archive containing history, database, settings, and profile data.
    /// </summary>
    public string Export(string filepath, string historyJson, string configJson,
                         string databaseJson, string? profilePicPath = null,
                         string? credentialsJson = null, string? profileInfoJson = null)
    {
        try
        {
            using var stream = new FileStream(filepath, FileMode.Create);
            using var archive = new ZipArchive(stream, ZipArchiveMode.Create);

            // Manifest
            var manifest = new JsonObject
            {
                ["nydta_version"] = NydtaVersion,
                ["exported_at"] = DateTime.UtcNow.ToString("yyyy-MM-dd HH:mm:ss"),
                ["browser_version"] = "3.1",
                ["contains"] = new JsonArray("history.jsonl", "database.jsonl",
                    "settings.md", "settings.json")
            };
            WriteEntry(archive, "manifest.json",
                manifest.ToJsonString(new JsonSerializerOptions { WriteIndented = true }));

            // History as JSONL
            var historyArr = JsonSerializer.Deserialize<JsonArray>(historyJson);
            if (historyArr != null)
            {
                var sb = new StringBuilder();
                foreach (var entry in historyArr)
                    sb.AppendLine(entry?.ToJsonString());
                WriteEntry(archive, "history.jsonl", sb.ToString());
            }

            // Database as JSONL
            var dbArr = JsonSerializer.Deserialize<JsonArray>(databaseJson);
            if (dbArr != null)
            {
                var sb = new StringBuilder();
                foreach (var entry in dbArr)
                    sb.AppendLine(entry?.ToJsonString());
                WriteEntry(archive, "database.jsonl", sb.ToString());
            }

            // Settings as JSON
            WriteEntry(archive, "settings.json", configJson);

            // Settings as Markdown
            WriteEntry(archive, "settings.md", ConfigToMarkdown(configJson));

            // Profile picture
            if (!string.IsNullOrEmpty(profilePicPath) && File.Exists(profilePicPath))
            {
                var ext = Path.GetExtension(profilePicPath).ToLowerInvariant();
                if (ext is ".png" or ".jpg" or ".jpeg" or ".webp" or ".jxl")
                {
                    var entry = archive.CreateEntry("profile" + ext);
                    using var entryStream = entry.Open();
                    using var picStream = File.OpenRead(profilePicPath);
                    picStream.CopyTo(entryStream);
                }
            }

            // Credentials (encrypted blob)
            if (!string.IsNullOrEmpty(credentialsJson))
                WriteEntry(archive, "credentials.json", credentialsJson);

            // Profile info
            if (!string.IsNullOrEmpty(profileInfoJson))
                WriteEntry(archive, "profile_info.json", profileInfoJson);

            return "{\"ok\":true,\"message\":\"Profile exported successfully\"}";
        }
        catch (Exception ex)
        {
            return $"{{\"ok\":false,\"message\":\"Export error: {JsonEscape(ex.Message)}\"}}";
        }
    }

    /// <summary>Import a .nydta archive. Returns JSON with extracted data.</summary>
    public string Import(string filepath)
    {
        if (!File.Exists(filepath))
            return "{\"ok\":false,\"message\":\"File not found\"}";

        try
        {
            using var archive = ZipFile.OpenRead(filepath);

            var result = new JsonObject
            {
                ["ok"] = true,
                ["message"] = "Import successful"
            };

            // Read manifest
            var manifestEntry = archive.GetEntry("manifest.json");
            if (manifestEntry != null)
                result["manifest"] = JsonNode.Parse(ReadEntry(manifestEntry));

            // Read settings
            var settingsEntry = archive.GetEntry("settings.json");
            if (settingsEntry != null)
                result["settings"] = JsonNode.Parse(ReadEntry(settingsEntry));

            // Read history (JSONL)
            var historyEntry = archive.GetEntry("history.jsonl");
            if (historyEntry != null)
            {
                var entries = new JsonArray();
                foreach (var line in ReadEntry(historyEntry).Split('\n',
                    StringSplitOptions.RemoveEmptyEntries))
                {
                    try { entries.Add(JsonNode.Parse(line)); }
                    catch { }
                }
                result["history"] = entries;
            }

            // Read database (JSONL)
            var dbEntry = archive.GetEntry("database.jsonl");
            if (dbEntry != null)
            {
                var entries = new JsonArray();
                foreach (var line in ReadEntry(dbEntry).Split('\n',
                    StringSplitOptions.RemoveEmptyEntries))
                {
                    try { entries.Add(JsonNode.Parse(line)); }
                    catch { }
                }
                result["database"] = entries;
            }

            // Credentials
            var credEntry = archive.GetEntry("credentials.json");
            if (credEntry != null)
                result["credentials"] = JsonNode.Parse(ReadEntry(credEntry));

            // Profile info
            var profileEntry = archive.GetEntry("profile_info.json");
            if (profileEntry != null)
                result["profile_info"] = JsonNode.Parse(ReadEntry(profileEntry));

            // Check for profile picture
            string[] picNames = { "profile.png", "profile.jpg", "profile.jpeg", "profile.webp" };
            foreach (var name in picNames)
            {
                if (archive.GetEntry(name) != null)
                {
                    result["has_profile_pic"] = true;
                    result["profile_pic_name"] = name;
                    break;
                }
            }

            return result.ToJsonString();
        }
        catch (Exception ex)
        {
            return $"{{\"ok\":false,\"message\":\"Import error: {JsonEscape(ex.Message)}\"}}";
        }
    }

    /// <summary>Read just the manifest from a .nydta file.</summary>
    public string ReadManifest(string filepath)
    {
        if (!File.Exists(filepath)) return "{}";
        try
        {
            using var archive = ZipFile.OpenRead(filepath);
            var entry = archive.GetEntry("manifest.json");
            return entry != null ? ReadEntry(entry) : "{}";
        }
        catch { return "{}"; }
    }

    private static void WriteEntry(ZipArchive archive, string name, string content)
    {
        var entry = archive.CreateEntry(name, CompressionLevel.Optimal);
        using var stream = entry.Open();
        using var writer = new StreamWriter(stream, Encoding.UTF8);
        writer.Write(content);
    }

    private static string ReadEntry(ZipArchiveEntry entry)
    {
        using var stream = entry.Open();
        using var reader = new StreamReader(stream, Encoding.UTF8);
        return reader.ReadToEnd();
    }

    private static string ConfigToMarkdown(string configJson)
    {
        var sb = new StringBuilder();
        sb.AppendLine("# OyNIx Browser Settings");
        sb.AppendLine($"Exported: {DateTime.UtcNow:yyyy-MM-dd HH:mm:ss} UTC");
        sb.AppendLine();

        try
        {
            var obj = JsonNode.Parse(configJson)?.AsObject();
            if (obj != null)
            {
                foreach (var kv in obj)
                    sb.AppendLine($"- **{kv.Key}**: `{kv.Value}`");
            }
        }
        catch
        {
            sb.AppendLine("*(Could not parse settings)*");
        }

        return sb.ToString();
    }

    private static string JsonEscape(string s) =>
        s.Replace("\\", "\\\\").Replace("\"", "\\\"");
}

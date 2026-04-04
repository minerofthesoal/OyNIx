using System.IO.Compression;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;

namespace OyNIx.Core.Extensions;

/// <summary>
/// Manages extension registration, manifest parsing, content script matching,
/// and enable/disable state. The actual WebEngine integration stays in C++.
/// </summary>
public sealed class ExtensionRegistry
{
    public static ExtensionRegistry Instance { get; } = new();

    private string _extensionsDir = "";
    private readonly List<ExtensionInfo> _extensions = new();

    public record ContentScriptMatch(string[] CssFiles, string[] JsFiles);

    public class ExtensionInfo
    {
        public string Name { get; set; } = "";
        public string Version { get; set; } = "1.0";
        public string Description { get; set; } = "";
        public bool Enabled { get; set; } = true;
        public string Path { get; set; } = "";
        public string PopupHtml { get; set; } = "";
        public string[] Permissions { get; set; } = Array.Empty<string>();
        public List<ContentScript> ContentScripts { get; set; } = new();
        public string? BackgroundScript { get; set; }
        public Dictionary<string, string> Icons { get; set; } = new();
    }

    public class ContentScript
    {
        public string[] Matches { get; set; } = Array.Empty<string>();
        public string[] CssFiles { get; set; } = Array.Empty<string>();
        public string[] JsFiles { get; set; } = Array.Empty<string>();
        public string RunAt { get; set; } = "document_idle";
    }

    public void Initialize(string extensionsDir)
    {
        _extensionsDir = extensionsDir;
        Directory.CreateDirectory(extensionsDir);
        ScanExtensions();
    }

    public void ScanExtensions()
    {
        _extensions.Clear();
        if (!Directory.Exists(_extensionsDir)) return;

        // Load registry
        var registryPath = System.IO.Path.Combine(_extensionsDir, "_registry.json");
        Dictionary<string, bool> enabledState = new();

        if (File.Exists(registryPath))
        {
            try
            {
                var regJson = JsonSerializer.Deserialize<JsonArray>(File.ReadAllText(registryPath));
                if (regJson != null)
                {
                    foreach (var entry in regJson)
                    {
                        var obj = entry?.AsObject();
                        if (obj == null) continue;
                        var name = obj["name"]?.GetValue<string>() ?? "";
                        var enabled = obj["enabled"]?.GetValue<bool>() ?? true;
                        enabledState[name] = enabled;
                    }
                }
            }
            catch { /* ignore corrupt registry */ }
        }

        foreach (var dir in Directory.GetDirectories(_extensionsDir))
        {
            var dirName = System.IO.Path.GetFileName(dir);
            if (dirName.StartsWith("_")) continue;

            var manifestPath = System.IO.Path.Combine(dir, "manifest.json");
            if (!File.Exists(manifestPath)) continue;

            try
            {
                var manifest = JsonNode.Parse(File.ReadAllText(manifestPath));
                if (manifest == null) continue;

                var ext = new ExtensionInfo
                {
                    Name = manifest["name"]?.GetValue<string>() ?? dirName,
                    Version = manifest["version"]?.GetValue<string>() ?? "1.0",
                    Description = manifest["description"]?.GetValue<string>() ?? "",
                    Path = dir,
                    Enabled = enabledState.GetValueOrDefault(dirName, true)
                };

                // Popup
                var action = manifest["action"] ?? manifest["browser_action"];
                if (action?["default_popup"] is JsonValue popupVal)
                    ext.PopupHtml = System.IO.Path.Combine(dir, popupVal.GetValue<string>());

                // Permissions
                if (manifest["permissions"] is JsonArray perms)
                    ext.Permissions = perms.Select(p => p?.GetValue<string>() ?? "").ToArray();

                // Content scripts
                if (manifest["content_scripts"] is JsonArray csArr)
                {
                    foreach (var cs in csArr)
                    {
                        if (cs == null) continue;
                        var script = new ContentScript();
                        if (cs["matches"] is JsonArray matches)
                            script.Matches = matches.Select(m => m?.GetValue<string>() ?? "").ToArray();
                        if (cs["css"] is JsonArray css)
                            script.CssFiles = css.Select(c => System.IO.Path.Combine(dir, c?.GetValue<string>() ?? "")).ToArray();
                        if (cs["js"] is JsonArray js)
                            script.JsFiles = js.Select(j => System.IO.Path.Combine(dir, j?.GetValue<string>() ?? "")).ToArray();
                        if (cs["run_at"] is JsonValue runAt)
                            script.RunAt = runAt.GetValue<string>();
                        ext.ContentScripts.Add(script);
                    }
                }

                // Background
                var bg = manifest["background"];
                if (bg?["service_worker"] is JsonValue sw)
                    ext.BackgroundScript = System.IO.Path.Combine(dir, sw.GetValue<string>());
                else if (bg?["scripts"] is JsonArray bgScripts && bgScripts.Count > 0)
                    ext.BackgroundScript = System.IO.Path.Combine(dir, bgScripts[0]?.GetValue<string>() ?? "");

                // Icons
                if (manifest["icons"] is JsonObject icons)
                {
                    foreach (var kv in icons)
                        ext.Icons[kv.Key] = System.IO.Path.Combine(dir, kv.Value?.GetValue<string>() ?? "");
                }

                _extensions.Add(ext);
            }
            catch { /* skip broken extensions */ }
        }
    }

    public string ListExtensionsJson()
    {
        return JsonSerializer.Serialize(_extensions.Select(e => new
        {
            e.Name, e.Version, e.Description, e.Enabled, e.Path,
            e.PopupHtml, e.Permissions, HasBackground = e.BackgroundScript != null,
            ContentScriptCount = e.ContentScripts.Count,
            Icon = e.Icons.GetValueOrDefault("48", e.Icons.GetValueOrDefault("32", ""))
        }));
    }

    public bool Install(string filePath)
    {
        if (!File.Exists(filePath)) return false;

        try
        {
            using var archive = ZipFile.OpenRead(filePath);
            var manifestEntry = archive.GetEntry("manifest.json");
            if (manifestEntry == null) return false;

            using var reader = new StreamReader(manifestEntry.Open());
            var manifest = JsonNode.Parse(reader.ReadToEnd());
            var name = manifest?["name"]?.GetValue<string>() ?? System.IO.Path.GetFileNameWithoutExtension(filePath);

            // Sanitize name for directory
            var safeName = Regex.Replace(name, @"[^\w\-.]", "_");
            var targetDir = System.IO.Path.Combine(_extensionsDir, safeName);

            if (Directory.Exists(targetDir))
                Directory.Delete(targetDir, true);

            ZipFile.ExtractToDirectory(filePath, targetDir);
            ScanExtensions();
            SaveRegistry();
            return true;
        }
        catch { return false; }
    }

    public bool Remove(string name)
    {
        var ext = _extensions.FirstOrDefault(e => e.Name == name);
        if (ext == null) return false;

        try
        {
            if (Directory.Exists(ext.Path))
                Directory.Delete(ext.Path, true);
            _extensions.Remove(ext);
            SaveRegistry();
            return true;
        }
        catch { return false; }
    }

    public void SetEnabled(string name, bool enabled)
    {
        var ext = _extensions.FirstOrDefault(e => e.Name == name);
        if (ext != null)
        {
            ext.Enabled = enabled;
            SaveRegistry();
        }
    }

    /// <summary>Returns JSON with css/js file contents matching the given URL.</summary>
    public string GetContentScriptsForUrl(string url)
    {
        var cssContents = new List<string>();
        var jsContents = new List<string>();

        foreach (var ext in _extensions.Where(e => e.Enabled))
        {
            foreach (var cs in ext.ContentScripts)
            {
                if (!cs.Matches.Any(pattern => UrlMatchesPattern(url, pattern)))
                    continue;

                foreach (var cssFile in cs.CssFiles)
                {
                    if (File.Exists(cssFile))
                        cssContents.Add(File.ReadAllText(cssFile));
                }

                foreach (var jsFile in cs.JsFiles)
                {
                    if (File.Exists(jsFile))
                        jsContents.Add(File.ReadAllText(jsFile));
                }
            }
        }

        return JsonSerializer.Serialize(new { css = cssContents, js = jsContents });
    }

    public string? GetManifestJson(string name)
    {
        var ext = _extensions.FirstOrDefault(e => e.Name == name);
        if (ext == null) return null;

        var manifestPath = System.IO.Path.Combine(ext.Path, "manifest.json");
        return File.Exists(manifestPath) ? File.ReadAllText(manifestPath) : null;
    }

    private static bool UrlMatchesPattern(string url, string pattern)
    {
        if (pattern == "<all_urls>") return true;

        // Convert Chrome match pattern to regex: *://*.example.com/*
        var escaped = Regex.Escape(pattern)
            .Replace(@"\*", ".*")
            .Replace(@"\?", ".");
        try
        {
            return Regex.IsMatch(url, $"^{escaped}$", RegexOptions.IgnoreCase);
        }
        catch { return false; }
    }

    private void SaveRegistry()
    {
        var registry = new JsonArray();
        foreach (var ext in _extensions)
        {
            registry.Add(new JsonObject
            {
                ["name"] = ext.Name,
                ["version"] = ext.Version,
                ["enabled"] = ext.Enabled,
                ["path"] = ext.Path
            });
        }

        var registryPath = System.IO.Path.Combine(_extensionsDir, "_registry.json");
        File.WriteAllText(registryPath, registry.ToJsonString());
    }
}

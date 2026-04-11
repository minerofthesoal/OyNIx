using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace OyNIx.Core.Data;

/// <summary>
/// Multi-profile manager. Each profile has isolated config, history,
/// bookmarks, and credentials. Port of oynix/core/profiles.py.
/// </summary>
public sealed class ProfileManager
{
    public static ProfileManager Instance { get; } = new();

    private string _profilesDir = "";
    private string _indexPath = "";
    private string _activeProfileName = "default";
    private readonly List<ProfileInfo> _profiles = new();

    public string ActiveProfileName => _activeProfileName;
    public string ActiveProfileDir => Path.Combine(_profilesDir, _activeProfileName);

    public record ProfileInfo(
        string Name, string DisplayName, string AvatarPath, string CreatedAt);

    public void Initialize(string configDir)
    {
        _profilesDir = Path.Combine(configDir, "profiles");
        _indexPath = Path.Combine(_profilesDir, "profiles.json");
        Directory.CreateDirectory(_profilesDir);
        LoadIndex();

        // Ensure default profile exists
        if (_profiles.Count == 0)
            CreateProfile("default", "Default");

        // Read active profile
        var activePath = Path.Combine(configDir, "active_profile");
        if (File.Exists(activePath))
            _activeProfileName = File.ReadAllText(activePath).Trim();
        if (string.IsNullOrEmpty(_activeProfileName))
            _activeProfileName = "default";
    }

    public bool CreateProfile(string name, string displayName, string avatarPath = "")
    {
        if (_profiles.Any(p => p.Name == name)) return false;

        var dir = Path.Combine(_profilesDir, name);
        Directory.CreateDirectory(dir);

        _profiles.Add(new ProfileInfo(
            name, displayName, avatarPath,
            DateTime.UtcNow.ToString("O")));

        SaveIndex();
        return true;
    }

    public bool SwitchProfile(string name, string? password = null)
    {
        if (!_profiles.Any(p => p.Name == name)) return false;

        var dir = Path.Combine(_profilesDir, name);
        var hashFile = Path.Combine(dir, ".password_hash");

        if (File.Exists(hashFile))
        {
            if (string.IsNullOrEmpty(password)) return false;
            var stored = File.ReadAllText(hashFile).Trim();
            if (HashPassword(password) != stored) return false;
        }

        _activeProfileName = name;
        var activePath = Path.Combine(Path.GetDirectoryName(_profilesDir)!, "active_profile");
        File.WriteAllText(activePath, name);
        return true;
    }

    public bool DeleteProfile(string name)
    {
        if (name == _activeProfileName) return false;
        if (name == "default") return false;

        var profile = _profiles.FirstOrDefault(p => p.Name == name);
        if (profile == null) return false;

        var dir = Path.Combine(_profilesDir, name);
        if (Directory.Exists(dir))
            Directory.Delete(dir, true);

        _profiles.Remove(profile);
        SaveIndex();
        return true;
    }

    public bool SetProfilePassword(string profileName, string password)
    {
        var dir = Path.Combine(_profilesDir, profileName);
        if (!Directory.Exists(dir)) return false;

        var hashFile = Path.Combine(dir, ".password_hash");
        File.WriteAllText(hashFile, HashPassword(password));
        return true;
    }

    public bool DuplicateProfile(string sourceName, string newName)
    {
        var sourceDir = Path.Combine(_profilesDir, sourceName);
        if (!Directory.Exists(sourceDir)) return false;
        if (_profiles.Any(p => p.Name == newName)) return false;

        var targetDir = Path.Combine(_profilesDir, newName);
        CopyDirectory(sourceDir, targetDir);

        var source = _profiles.FirstOrDefault(p => p.Name == sourceName);
        var displayName = source != null ? source.DisplayName + " (Copy)" : newName;

        _profiles.Add(new ProfileInfo(
            newName, displayName, "",
            DateTime.UtcNow.ToString("O")));

        SaveIndex();
        return true;
    }

    public string ListProfilesJson()
    {
        var arr = new JsonArray();
        foreach (var p in _profiles)
        {
            arr.Add(new JsonObject
            {
                ["Name"] = p.Name, ["DisplayName"] = p.DisplayName,
                ["AvatarPath"] = p.AvatarPath, ["CreatedAt"] = p.CreatedAt,
                ["IsActive"] = p.Name == _activeProfileName,
                ["HasPassword"] = File.Exists(Path.Combine(_profilesDir, p.Name, ".password_hash"))
            });
        }
        return arr.ToJsonString();
    }

    public static string HashPassword(string password)
    {
        var data = Encoding.UTF8.GetBytes(password);
        for (int i = 0; i < 4; i++)
            data = SHA256.HashData(data);
        return Convert.ToHexString(data).ToLowerInvariant();
    }

    private void LoadIndex()
    {
        _profiles.Clear();
        if (!File.Exists(_indexPath)) return;
        try
        {
            var arr = JsonNode.Parse(File.ReadAllText(_indexPath)) as JsonArray;
            if (arr == null) return;
            foreach (var val in arr)
            {
                var obj = val?.AsObject();
                if (obj == null) continue;
                _profiles.Add(new ProfileInfo(
                    obj["name"]?.GetValue<string>() ?? "",
                    obj["display_name"]?.GetValue<string>() ?? "",
                    obj["avatar"]?.GetValue<string>() ?? "",
                    obj["created"]?.GetValue<string>() ?? ""
                ));
            }
        }
        catch { }
    }

    private void SaveIndex()
    {
        var arr = new JsonArray();
        foreach (var p in _profiles)
            arr.Add(new JsonObject
            {
                ["name"] = p.Name,
                ["display_name"] = p.DisplayName,
                ["avatar"] = p.AvatarPath,
                ["created"] = p.CreatedAt
            });
        File.WriteAllText(_indexPath,
            arr.ToJsonString(new JsonSerializerOptions { WriteIndented = true }));
    }

    private static void CopyDirectory(string source, string target)
    {
        Directory.CreateDirectory(target);
        foreach (var file in Directory.GetFiles(source))
            File.Copy(file, Path.Combine(target, Path.GetFileName(file)), true);
        foreach (var dir in Directory.GetDirectories(source))
            CopyDirectory(dir, Path.Combine(target, Path.GetFileName(dir)));
    }
}

using System.Security.Cryptography;
using System.Text;
using System.Text.Json.Nodes;

namespace OyNIx.Core.Data;

/// <summary>
/// Encrypted credential storage with XOR-256++ encryption.
/// Port of oynix/core/credentials.py (business logic only, no UI).
/// </summary>
public sealed class CredentialStore
{
    public static CredentialStore Instance { get; } = new();

    private string _credDir = "";
    private string _credFile = "";
    private string _passkeyFile = "";
    private string _masterHashFile = "";
    private byte[]? _masterKey;
    private bool _unlocked;

    private JsonArray _credentials = new();
    private JsonArray _passkeys = new();

    public bool IsUnlocked => _unlocked;
    public bool HasMasterPassword => File.Exists(_masterHashFile);

    public void Initialize(string configDir)
    {
        _credDir = Path.Combine(configDir, "credentials");
        _credFile = Path.Combine(_credDir, "passwords.enc");
        _passkeyFile = Path.Combine(_credDir, "passkeys.json");
        _masterHashFile = Path.Combine(_credDir, ".master_hash");
        Directory.CreateDirectory(_credDir);
        LoadPasskeys();
    }

    public bool SetMasterPassword(string password)
    {
        _masterKey = DeriveKey(password);
        var hash = Sha128(Encoding.UTF8.GetBytes(password));
        File.WriteAllText(_masterHashFile, Convert.ToBase64String(hash));
        _unlocked = true;
        SaveCredentials();
        return true;
    }

    public bool Unlock(string password)
    {
        if (!File.Exists(_masterHashFile)) return false;

        var storedHash = Convert.FromBase64String(File.ReadAllText(_masterHashFile).Trim());
        var inputHash = Sha128(Encoding.UTF8.GetBytes(password));

        if (!storedHash.SequenceEqual(inputHash)) return false;

        _masterKey = DeriveKey(password);
        _unlocked = true;
        LoadCredentials();
        return true;
    }

    public void Lock()
    {
        _masterKey = null;
        _unlocked = false;
        _credentials = new JsonArray();
    }

    public bool SaveCredential(string domain, string username, string password)
    {
        if (!_unlocked || _masterKey == null) return false;

        // Remove existing for this domain+username
        for (int i = _credentials.Count - 1; i >= 0; i--)
        {
            var c = _credentials[i]?.AsObject();
            if (c?["domain"]?.GetValue<string>() == domain &&
                c?["username"]?.GetValue<string>() == username)
                _credentials.RemoveAt(i);
        }

        _credentials.Add(new JsonObject
        {
            ["domain"] = domain,
            ["username"] = username,
            ["password"] = password,
            ["saved_at"] = DateTime.UtcNow.ToString("O")
        });

        SaveCredentials();
        return true;
    }

    public string GetCredentialsJson(string domain)
    {
        if (!_unlocked) return "[]";

        var results = new JsonArray();
        foreach (var val in _credentials)
        {
            var c = val?.AsObject();
            if (c?["domain"]?.GetValue<string>() == domain)
                results.Add(JsonNode.Parse(c!.ToJsonString()));
        }
        return results.ToJsonString();
    }

    public string GetAllDomainsJson()
    {
        if (!_unlocked) return "[]";
        var domains = new HashSet<string>();
        foreach (var val in _credentials)
        {
            var d = val?.AsObject()?["domain"]?.GetValue<string>();
            if (!string.IsNullOrEmpty(d)) domains.Add(d);
        }
        var arr = new JsonArray();
        foreach (var d2 in domains) arr.Add((JsonNode)d2);
        return arr.ToJsonString();
    }

    public bool DeleteCredential(string domain, string username)
    {
        if (!_unlocked) return false;
        for (int i = _credentials.Count - 1; i >= 0; i--)
        {
            var c = _credentials[i]?.AsObject();
            if (c?["domain"]?.GetValue<string>() == domain &&
                c?["username"]?.GetValue<string>() == username)
            {
                _credentials.RemoveAt(i);
                SaveCredentials();
                return true;
            }
        }
        return false;
    }

    // ── Passkeys ────────────────────────────────────────────────────

    public bool RegisterPasskey(string domain, string credentialId, string displayName)
    {
        _passkeys.Add(new JsonObject
        {
            ["domain"] = domain,
            ["credential_id"] = credentialId,
            ["display_name"] = displayName,
            ["created"] = DateTime.UtcNow.ToString("O")
        });
        SavePasskeys();
        return true;
    }

    public string GetPasskeysJson(string domain)
    {
        var results = new JsonArray();
        foreach (var val in _passkeys)
        {
            var p = val?.AsObject();
            if (p?["domain"]?.GetValue<string>() == domain)
                results.Add(JsonNode.Parse(p!.ToJsonString()));
        }
        return results.ToJsonString();
    }

    // ── XOR-256++ Encryption ────────────────────────────────────────

    internal static byte[] Xor256ppEncrypt(byte[] data, byte[] key, int rounds = 4)
    {
        var result = new byte[data.Length];
        Array.Copy(data, result, data.Length);
        int length = result.Length;
        if (length == 0) return result;

        for (int r = 0; r < rounds; r++)
        {
            var roundKey = SHA256.HashData(
                key.Concat(BitConverter.GetBytes(r).Reverse().ToArray()).ToArray());
            var expanded = ExpandKey(roundKey, length);

            // XOR with round key
            for (int i = 0; i < length; i++)
                result[i] ^= expanded[i];

            // Carry-add diffusion
            byte carry = roundKey[r % 32];
            for (int i = 0; i < length; i++)
            {
                byte old = result[i];
                result[i] = (byte)((result[i] + carry) & 0xFF);
                carry = (byte)((old ^ carry ^ (r + 1)) & 0xFF);
            }

            // Rotate bytes
            int offset = roundKey[(r + 7) % 32] % Math.Max(length, 1);
            result = result.Skip(offset).Concat(result.Take(offset)).ToArray();
        }
        return result;
    }

    internal static byte[] Xor256ppDecrypt(byte[] data, byte[] key, int rounds = 4)
    {
        var result = new byte[data.Length];
        Array.Copy(data, result, data.Length);
        int length = result.Length;
        if (length == 0) return result;

        for (int r = rounds - 1; r >= 0; r--)
        {
            var roundKey = SHA256.HashData(
                key.Concat(BitConverter.GetBytes(r).Reverse().ToArray()).ToArray());
            var expanded = ExpandKey(roundKey, length);

            // Reverse rotation
            int offset = roundKey[(r + 7) % 32] % Math.Max(length, 1);
            if (offset > 0)
                result = result.Skip(length - offset).Concat(result.Take(length - offset)).ToArray();

            // Reverse carry-add
            byte carry = roundKey[r % 32];
            var carries = new byte[length];
            carries[0] = carry;
            for (int i = 0; i < length - 1; i++)
            {
                byte before = (byte)((result[i] - carries[i]) & 0xFF);
                carries[i + 1] = (byte)((before ^ carries[i] ^ (r + 1)) & 0xFF);
            }
            for (int i = 0; i < length; i++)
                result[i] = (byte)((result[i] - carries[i]) & 0xFF);

            // Reverse XOR
            for (int i = 0; i < length; i++)
                result[i] ^= expanded[i];
        }
        return result;
    }

    private static byte[] DeriveKey(string password, string salt = "oynix_cred_salt_v1")
    {
        return Rfc2898DeriveBytes.Pbkdf2(
            Encoding.UTF8.GetBytes(password),
            Encoding.UTF8.GetBytes(salt),
            100000,
            HashAlgorithmName.SHA256,
            32);
    }

    private static byte[] Sha128(byte[] data)
    {
        var hash = SHA256.HashData(data);
        return hash.Take(16).ToArray();
    }

    private static byte[] ExpandKey(byte[] key, int length)
    {
        var expanded = new byte[length];
        for (int i = 0; i < length; i++)
            expanded[i] = key[i % key.Length];
        return expanded;
    }

    private void LoadCredentials()
    {
        if (!File.Exists(_credFile) || _masterKey == null) return;
        try
        {
            var encrypted = Convert.FromBase64String(File.ReadAllText(_credFile));
            var decrypted = Xor256ppDecrypt(encrypted, _masterKey);
            var json = Encoding.UTF8.GetString(decrypted);
            _credentials = (JsonNode.Parse(json) as JsonArray) ?? new JsonArray();
        }
        catch { _credentials = new JsonArray(); }
    }

    private void SaveCredentials()
    {
        if (_masterKey == null) return;
        try
        {
            var json = _credentials.ToJsonString();
            var encrypted = Xor256ppEncrypt(Encoding.UTF8.GetBytes(json), _masterKey);
            File.WriteAllText(_credFile, Convert.ToBase64String(encrypted));
        }
        catch { }
    }

    private void LoadPasskeys()
    {
        if (!File.Exists(_passkeyFile)) return;
        try
        {
            _passkeys = (JsonNode.Parse(File.ReadAllText(_passkeyFile)) as JsonArray)
                ?? new JsonArray();
        }
        catch { }
    }

    private void SavePasskeys()
    {
        try
        {
            File.WriteAllText(_passkeyFile, _passkeys.ToJsonString());
        }
        catch { }
    }
}

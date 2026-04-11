using System.Text.Json;
using System.Text.RegularExpressions;

namespace OyNIx.Core.Security;

/// <summary>
/// Detects login/credential pages to warn users about phishing.
/// Checks URL patterns against known service login pages.
/// </summary>
public sealed class SecurityChecker
{
    public static SecurityChecker Instance { get; } = new();

    private static readonly (string Service, string Pattern)[] LoginPatterns =
    {
        ("Google", @"accounts\.google\.com/(signin|o/oauth2|ServiceLogin)"),
        ("Microsoft", @"login\.microsoftonline\.com|login\.live\.com"),
        ("Apple", @"appleid\.apple\.com/(auth|sign-in)"),
        ("Amazon", @"(www\.)?amazon\.(com|co\.\w+)/ap/signin"),
        ("Facebook", @"(www\.)?facebook\.com/login"),
        ("Twitter/X", @"(twitter\.com|x\.com)/(login|i/flow/login)"),
        ("GitHub", @"github\.com/(login|session)"),
        ("Netflix", @"(www\.)?netflix\.com/login"),
        ("PayPal", @"(www\.)?paypal\.com/(signin|cgi-bin/webscr\?cmd=_login)"),
        ("LinkedIn", @"(www\.)?linkedin\.com/(login|uas/login|checkpoint)"),
        ("Instagram", @"(www\.)?instagram\.com/accounts/login"),
        ("Discord", @"(www\.)?discord\.com/(login|register)"),
        ("Reddit", @"(www\.)?reddit\.com/(login|register)"),
        ("Dropbox", @"(www\.)?dropbox\.com/(login|signin)"),
        ("Spotify", @"accounts\.spotify\.com/(login|en/login)"),
        ("Steam", @"store\.steampowered\.com/login|steamcommunity\.com/(login|openid)"),
        ("Yahoo", @"login\.yahoo\.com"),
        ("Twitch", @"(www\.)?twitch\.tv/(login|signup)"),
        ("Adobe", @"auth\.services\.adobe\.com|adobeid-na1\.services\.adobe\.com"),
        ("Bank of America", @"(www\.)?bankofamerica\.com/(smallbusiness/)?resources/sign-in"),
        ("Chase", @"secure\d*\.chase\.com"),
        ("Wells Fargo", @"connect\.secure\.wellsfargo\.com"),
    };

    public bool IsLoginPage(string url)
    {
        if (string.IsNullOrEmpty(url)) return false;
        return LoginPatterns.Any(lp =>
            Regex.IsMatch(url, lp.Pattern, RegexOptions.IgnoreCase));
    }

    private readonly HashSet<string> _trustedDomains = new();
    private readonly HashSet<string> _blockedDomains = new();
    private readonly HashSet<string> _suppressedDomains = new();
    private bool _promptsEnabled = true;

    public void TrustDomain(string domain)
    {
        _trustedDomains.Add(domain.ToLowerInvariant());
        _blockedDomains.Remove(domain.ToLowerInvariant());
    }

    public void BlockDomain(string domain)
    {
        _blockedDomains.Add(domain.ToLowerInvariant());
        _trustedDomains.Remove(domain.ToLowerInvariant());
    }

    public void SuppressDomain(string domain) =>
        _suppressedDomains.Add(domain.ToLowerInvariant());

    public bool IsTrusted(string domain) =>
        _trustedDomains.Contains(domain.ToLowerInvariant());

    public bool IsBlocked(string domain) =>
        _blockedDomains.Contains(domain.ToLowerInvariant());

    public void SetPromptsEnabled(bool enabled) => _promptsEnabled = enabled;

    public bool ShouldPrompt(string url)
    {
        if (!_promptsEnabled) return false;
        if (!IsLoginPage(url)) return false;

        try
        {
            var domain = new Uri(url).Host.ToLowerInvariant();
            if (_trustedDomains.Contains(domain)) return false;
            if (_suppressedDomains.Contains(domain)) return false;
        }
        catch { }

        return true;
    }

    public string GetSecurityInfo(string url)
    {
        string domain;
        try { domain = new Uri(url).Host.ToLowerInvariant(); }
        catch { domain = ""; }

        var isHttps = url.StartsWith("https://", StringComparison.OrdinalIgnoreCase);

        foreach (var (service, pattern) in LoginPatterns)
        {
            if (Regex.IsMatch(url, pattern, RegexOptions.IgnoreCase))
            {
                return JsonSerializer.Serialize(new
                {
                    isLogin = true,
                    service,
                    level = "high",
                    isHttps,
                    isTrusted = _trustedDomains.Contains(domain),
                    isBlocked = _blockedDomains.Contains(domain),
                    message = $"This appears to be a {service} login page. " +
                              "Verify the URL before entering credentials."
                });
            }
        }

        // Check for generic login indicators
        var lower = url.ToLowerInvariant();
        var isGenericLogin = lower.Contains("/login") || lower.Contains("/signin") ||
                            lower.Contains("/auth") || lower.Contains("/password");

        return JsonSerializer.Serialize(new
        {
            isLogin = isGenericLogin,
            service = isGenericLogin ? "Unknown" : "",
            level = isGenericLogin ? "medium" : (isHttps ? "low" : "medium"),
            isHttps,
            isTrusted = _trustedDomains.Contains(domain),
            isBlocked = _blockedDomains.Contains(domain),
            message = isGenericLogin
                ? "This page may contain a login form. Verify the URL."
                : ""
        });
    }
}

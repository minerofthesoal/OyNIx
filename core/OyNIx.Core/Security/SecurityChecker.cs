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

    public string GetSecurityInfo(string url)
    {
        foreach (var (service, pattern) in LoginPatterns)
        {
            if (Regex.IsMatch(url, pattern, RegexOptions.IgnoreCase))
            {
                return JsonSerializer.Serialize(new
                {
                    isLogin = true,
                    service,
                    message = $"This appears to be a {service} login page. " +
                              "Verify the URL before entering credentials."
                });
            }
        }

        return JsonSerializer.Serialize(new
        {
            isLogin = false,
            service = "",
            message = ""
        });
    }
}

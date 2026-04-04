using System.Text;
using System.Text.RegularExpressions;

namespace OyNIx.Core.Search;

/// <summary>
/// Search query processing — builds FTS5 queries, handles multi-word input,
/// generates search URLs, and manages search engine configuration.
/// </summary>
public sealed class SearchEngine
{
    public static SearchEngine Instance { get; } = new();

    private string _defaultEngine = "DuckDuckGo";

    public void SetDefaultEngine(string name) => _defaultEngine = name;
    public string GetDefaultEngine() => _defaultEngine;

    /// <summary>Builds a proper FTS5 MATCH query from raw user input.</summary>
    public string BuildFts5Query(string rawQuery)
    {
        var words = rawQuery.Trim().Split(' ', StringSplitOptions.RemoveEmptyEntries);
        if (words.Length == 0) return "";

        var terms = words
            .Select(w => w.Replace("\"", ""))
            .Where(w => w.Length > 0)
            .Select(w => $"\"{w}\"");

        return string.Join(" AND ", terms);
    }

    /// <summary>Returns the search URL for the given query using the default engine.</summary>
    public string GetSearchUrl(string query)
    {
        return GetSearchUrl(query, _defaultEngine);
    }

    public string GetSearchUrl(string query, string engine)
    {
        var encoded = Uri.EscapeDataString(query);
        return engine switch
        {
            "Google" => $"https://www.google.com/search?q={encoded}",
            "Bing" => $"https://www.bing.com/search?q={encoded}",
            "Brave" => $"https://search.brave.com/search?q={encoded}",
            "Startpage" => $"https://www.startpage.com/sp/search?query={encoded}",
            _ => $"https://duckduckgo.com/?q={encoded}"
        };
    }

    /// <summary>Determines if the input looks like a URL vs a search query.</summary>
    public bool LooksLikeUrl(string input)
    {
        if (string.IsNullOrWhiteSpace(input)) return false;

        // Explicit schemes
        if (input.StartsWith("http://", StringComparison.OrdinalIgnoreCase) ||
            input.StartsWith("https://", StringComparison.OrdinalIgnoreCase) ||
            input.StartsWith("file://", StringComparison.OrdinalIgnoreCase) ||
            input.StartsWith("oyn://", StringComparison.OrdinalIgnoreCase))
            return true;

        // Localhost or IP
        if (input.StartsWith("localhost", StringComparison.OrdinalIgnoreCase) ||
            input.StartsWith("127.", StringComparison.Ordinal))
            return true;

        // Contains a space → search query, not URL
        if (input.Contains(' ')) return false;

        // Domain-like: word.tld or word.tld/path
        return Regex.IsMatch(input, @"^[\w\-]+(\.[\w\-]+)+(/.*)?$");
    }

    /// <summary>Returns available search engines as a JSON array.</summary>
    public string GetAvailableEnginesJson()
    {
        return "[" +
            "{\"id\":\"DuckDuckGo\",\"name\":\"DuckDuckGo\",\"url\":\"https://duckduckgo.com/?q={query}\"}," +
            "{\"id\":\"Google\",\"name\":\"Google\",\"url\":\"https://www.google.com/search?q={query}\"}," +
            "{\"id\":\"Bing\",\"name\":\"Bing\",\"url\":\"https://www.bing.com/search?q={query}\"}," +
            "{\"id\":\"Brave\",\"name\":\"Brave Search\",\"url\":\"https://search.brave.com/search?q={query}\"}," +
            "{\"id\":\"Startpage\",\"name\":\"Startpage\",\"url\":\"https://www.startpage.com/sp/search?query={query}\"}" +
            "]";
    }
}

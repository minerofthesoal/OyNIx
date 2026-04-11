using System.Collections.Concurrent;
using System.Net;
using System.Text.RegularExpressions;

namespace OyNIx.Core.Crawler;

/// <summary>
/// Local web crawler that fetches pages, extracts links and content,
/// and stores results for indexing. Supports crawling from a URL list
/// or broad web crawling with depth limits.
/// </summary>
public sealed class WebCrawler
{
    public static WebCrawler Instance { get; } = new();

    // Crawl state
    private readonly ConcurrentDictionary<string, CrawledPage> _crawledPages = new();
    private readonly ConcurrentQueue<CrawlJob> _queue = new();
    private readonly HashSet<string> _visited = new();
    private readonly object _visitedLock = new();

    private volatile bool _running;
    private volatile bool _stopRequested;
    private int _activeWorkers;
    private CrawlConfig _config = new();

    // Stats
    private int _pagesQueued;
    private int _pagesCrawled;
    private int _pagesErrored;

    public bool IsRunning => _running;

    public void Configure(int maxDepth, int maxPages, int concurrency, bool followExternal)
    {
        _config = new CrawlConfig
        {
            MaxDepth = Math.Clamp(maxDepth, 1, 20),
            MaxPages = Math.Clamp(maxPages, 1, 100000),
            Concurrency = Math.Clamp(concurrency, 1, 8),
            FollowExternalLinks = followExternal
        };
    }

    /// <summary>
    /// Start crawling from a list of seed URLs (the "crawl from list" mode).
    /// Each URL is crawled to the configured depth.
    /// </summary>
    public void StartFromList(string[] urls)
    {
        if (_running) return;
        Reset();
        _running = true;

        foreach (var url in urls)
        {
            var normalized = NormalizeUrl(url);
            if (!string.IsNullOrEmpty(normalized))
                EnqueueUrl(normalized, 0, normalized);
        }

        StartWorkers();
    }

    /// <summary>
    /// Start broad web crawling from a single seed URL.
    /// Follows links up to MaxDepth, respects MaxPages.
    /// </summary>
    public void StartBroadCrawl(string seedUrl)
    {
        if (_running) return;
        Reset();
        _running = true;

        var normalized = NormalizeUrl(seedUrl);
        if (!string.IsNullOrEmpty(normalized))
            EnqueueUrl(normalized, 0, normalized);

        StartWorkers();
    }

    public void Stop()
    {
        _stopRequested = true;
    }

    public string GetStatusJson()
    {
        return "{" +
            "\"running\":" + (_running ? "true" : "false") + "," +
            "\"queued\":" + _pagesQueued + "," +
            "\"crawled\":" + _pagesCrawled + "," +
            "\"errored\":" + _pagesErrored + "," +
            "\"total_found\":" + _crawledPages.Count +
            "}";
    }

    public string GetResultsJson(int offset, int limit)
    {
        var pages = _crawledPages.Values
            .Skip(offset)
            .Take(limit)
            .Select(p =>
                "{\"url\":" + JsonEscape(p.Url) +
                ",\"title\":" + JsonEscape(p.Title) +
                ",\"domain\":" + JsonEscape(p.Domain) +
                ",\"depth\":" + p.Depth +
                ",\"links_found\":" + p.LinksFound +
                ",\"content_length\":" + p.ContentLength +
                ",\"status\":" + JsonEscape(p.Status) +
                "}")
            .ToArray();

        return "[" + string.Join(",", pages) + "]";
    }

    public int GetCrawledCount() => _crawledPages.Count;

    private void Reset()
    {
        _stopRequested = false;
        _crawledPages.Clear();
        while (_queue.TryDequeue(out _)) { }
        lock (_visitedLock) { _visited.Clear(); }
        _pagesQueued = 0;
        _pagesCrawled = 0;
        _pagesErrored = 0;
        _activeWorkers = 0;
    }

    private void EnqueueUrl(string url, int depth, string sourceUrl)
    {
        if (depth > _config.MaxDepth) return;
        if (_pagesCrawled + _pagesQueued >= _config.MaxPages) return;

        lock (_visitedLock)
        {
            if (!_visited.Add(url)) return;
        }

        _queue.Enqueue(new CrawlJob(url, depth, sourceUrl));
        Interlocked.Increment(ref _pagesQueued);
    }

    private void StartWorkers()
    {
        for (int i = 0; i < _config.Concurrency; i++)
        {
            var thread = new Thread(WorkerLoop)
            {
                IsBackground = true,
                Name = $"Crawler-{i}"
            };
            Interlocked.Increment(ref _activeWorkers);
            thread.Start();
        }
    }

    private void WorkerLoop()
    {
        using var client = CreateHttpClient();

        while (!_stopRequested)
        {
            if (!_queue.TryDequeue(out var job))
            {
                // No work left; wait briefly then check again
                Thread.Sleep(100);

                // If queue is still empty and no other workers are producing, we're done
                if (_queue.IsEmpty)
                    break;

                continue;
            }

            Interlocked.Decrement(ref _pagesQueued);

            try
            {
                var page = CrawlPage(client, job);
                if (page != null)
                {
                    _crawledPages.TryAdd(page.Url, page);
                    Interlocked.Increment(ref _pagesCrawled);
                }
            }
            catch
            {
                Interlocked.Increment(ref _pagesErrored);
            }

            if (_pagesCrawled >= _config.MaxPages)
                break;
        }

        if (Interlocked.Decrement(ref _activeWorkers) == 0)
            _running = false;
    }

    private CrawledPage? CrawlPage(HttpClient client, CrawlJob job)
    {
        HttpResponseMessage? response = null;
        try
        {
            var request = new HttpRequestMessage(HttpMethod.Get, job.Url);
            response = client.Send(request);

            if (!response.IsSuccessStatusCode)
            {
                return new CrawledPage
                {
                    Url = job.Url,
                    Domain = GetDomain(job.Url),
                    Depth = job.Depth,
                    Status = $"error:{(int)response.StatusCode}",
                    Title = "",
                    ContentLength = 0,
                    LinksFound = 0
                };
            }

            var contentType = response.Content.Headers.ContentType?.MediaType ?? "";
            if (!contentType.Contains("text/html", StringComparison.OrdinalIgnoreCase))
            {
                return new CrawledPage
                {
                    Url = job.Url,
                    Domain = GetDomain(job.Url),
                    Depth = job.Depth,
                    Status = "skipped:not-html",
                    Title = "",
                    ContentLength = 0,
                    LinksFound = 0
                };
            }

            var html = response.Content.ReadAsStringAsync().GetAwaiter().GetResult();
            var title = ExtractTitle(html);
            var textContent = ExtractTextContent(html);
            var links = ExtractLinks(html, job.Url);

            // Enqueue discovered links for further crawling
            int linksEnqueued = 0;
            foreach (var link in links)
            {
                if (_stopRequested) break;

                var linkDomain = GetDomain(link);
                var sourceDomain = GetDomain(job.SourceDomain);

                if (!_config.FollowExternalLinks && linkDomain != sourceDomain)
                    continue;

                EnqueueUrl(link, job.Depth + 1, job.SourceDomain);
                linksEnqueued++;
            }

            return new CrawledPage
            {
                Url = job.Url,
                Title = title,
                Domain = GetDomain(job.Url),
                Content = textContent,
                Depth = job.Depth,
                Status = "ok",
                LinksFound = links.Count,
                ContentLength = textContent.Length
            };
        }
        catch (Exception ex)
        {
            return new CrawledPage
            {
                Url = job.Url,
                Domain = GetDomain(job.Url),
                Depth = job.Depth,
                Status = $"error:{ex.GetType().Name}",
                Title = "",
                ContentLength = 0,
                LinksFound = 0
            };
        }
        finally
        {
            response?.Dispose();
        }
    }

    private static HttpClient CreateHttpClient()
    {
        var handler = new HttpClientHandler
        {
            AllowAutoRedirect = true,
            MaxAutomaticRedirections = 5,
            AutomaticDecompression = DecompressionMethods.GZip | DecompressionMethods.Deflate
        };

        var client = new HttpClient(handler)
        {
            Timeout = TimeSpan.FromSeconds(15)
        };
        client.DefaultRequestHeaders.UserAgent.ParseAdd(
            "Mozilla/5.0 (X11; Linux x86_64) OyNIxCrawler/3.1");
        client.DefaultRequestHeaders.Accept.ParseAdd("text/html,application/xhtml+xml");

        return client;
    }

    private static string ExtractTitle(string html)
    {
        var match = Regex.Match(html, @"<title[^>]*>(.*?)</title>",
            RegexOptions.IgnoreCase | RegexOptions.Singleline);
        if (match.Success)
            return WebUtility.HtmlDecode(match.Groups[1].Value.Trim());
        return "";
    }

    private static string ExtractTextContent(string html)
    {
        // Remove script and style blocks
        var cleaned = Regex.Replace(html, @"<(script|style|noscript)[^>]*>.*?</\1>",
            "", RegexOptions.IgnoreCase | RegexOptions.Singleline);
        // Remove HTML tags
        cleaned = Regex.Replace(cleaned, @"<[^>]+>", " ");
        // Collapse whitespace
        cleaned = Regex.Replace(cleaned, @"\s+", " ");
        cleaned = WebUtility.HtmlDecode(cleaned).Trim();

        // Cap at 5000 chars for storage
        if (cleaned.Length > 5000)
            cleaned = cleaned[..5000];

        return cleaned;
    }

    private static List<string> ExtractLinks(string html, string baseUrl)
    {
        var links = new List<string>();
        Uri? baseUri = null;
        try { baseUri = new Uri(baseUrl); }
        catch { return links; }

        var matches = Regex.Matches(html, @"<a\s[^>]*href\s*=\s*[""']([^""'#]+)[""']",
            RegexOptions.IgnoreCase);

        foreach (Match m in matches)
        {
            var href = m.Groups[1].Value.Trim();
            if (href.StartsWith("javascript:", StringComparison.OrdinalIgnoreCase) ||
                href.StartsWith("mailto:", StringComparison.OrdinalIgnoreCase) ||
                href.StartsWith("tel:", StringComparison.OrdinalIgnoreCase))
                continue;

            try
            {
                var resolved = new Uri(baseUri, href);
                if (resolved.Scheme == "http" || resolved.Scheme == "https")
                {
                    // Strip fragment and normalize
                    var clean = resolved.GetLeftPart(UriPartial.Query);
                    links.Add(clean);
                }
            }
            catch { /* skip malformed URLs */ }
        }

        return links.Distinct().ToList();
    }

    private static string NormalizeUrl(string url)
    {
        url = url.Trim();
        if (!url.StartsWith("http://", StringComparison.OrdinalIgnoreCase) &&
            !url.StartsWith("https://", StringComparison.OrdinalIgnoreCase))
            url = "https://" + url;

        try
        {
            var uri = new Uri(url);
            return uri.GetLeftPart(UriPartial.Query);
        }
        catch { return ""; }
    }

    private static string GetDomain(string url)
    {
        try { return new Uri(url).Host; }
        catch { return ""; }
    }

    private static string JsonEscape(string s)
    {
        if (string.IsNullOrEmpty(s)) return "\"\"";
        s = s.Replace("\\", "\\\\")
             .Replace("\"", "\\\"")
             .Replace("\n", "\\n")
             .Replace("\r", "\\r")
             .Replace("\t", "\\t");
        return "\"" + s + "\"";
    }

    // Internal types
    private sealed class CrawlConfig
    {
        public int MaxDepth { get; init; } = 2;
        public int MaxPages { get; init; } = 500;
        public int Concurrency { get; init; } = 4;
        public bool FollowExternalLinks { get; init; }
    }

    private readonly record struct CrawlJob(string Url, int Depth, string SourceDomain);

    public sealed class CrawledPage
    {
        public string Url { get; init; } = "";
        public string Title { get; init; } = "";
        public string Domain { get; init; } = "";
        public string Content { get; init; } = "";
        public string Status { get; init; } = "";
        public int Depth { get; init; }
        public int LinksFound { get; init; }
        public int ContentLength { get; init; }
    }
}

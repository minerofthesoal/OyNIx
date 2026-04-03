/*
 * OyNIx Turbo Indexer — High-level indexing pipeline.
 * Handles concurrent page crawling, text extraction, and batch scoring.
 * Works with the browser's JSONL database format.
 *
 * Coded by Claude (Anthropic)
 */

using System.Collections.Concurrent;
using System.Diagnostics;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace OyNIx.TurboIndexer;

/// <summary>
/// A site entry in the OyNIx database.
/// </summary>
public sealed class SiteEntry
{
    [JsonPropertyName("url")]
    public string Url { get; set; } = "";

    [JsonPropertyName("title")]
    public string Title { get; set; } = "";

    [JsonPropertyName("description")]
    public string Description { get; set; } = "";

    [JsonPropertyName("keywords")]
    public string Keywords { get; set; } = "";

    [JsonPropertyName("category")]
    public string Category { get; set; } = "";

    [JsonPropertyName("score")]
    public double Score { get; set; }

    [JsonPropertyName("indexed_at")]
    public string IndexedAt { get; set; } = "";
}

/// <summary>
/// Result of scoring a document.
/// </summary>
public sealed class ScoredSite
{
    public int Id { get; set; }
    public string Url { get; set; } = "";
    public string Title { get; set; } = "";
    public double Score { get; set; }
}

/// <summary>
/// High-performance indexing pipeline for the Nyx search engine.
/// </summary>
public sealed class IndexPipeline
{
    private readonly int _maxThreads;
    private readonly int _batchSize;

    public IndexPipeline(int maxThreads = 0, int batchSize = 100)
    {
        _maxThreads = maxThreads > 0 ? maxThreads : Environment.ProcessorCount;
        _batchSize = batchSize;
    }

    /// <summary>
    /// Load a JSONL database file into memory.
    /// </summary>
    public List<SiteEntry> LoadDatabase(string jsonlPath)
    {
        var entries = new List<SiteEntry>();
        if (!File.Exists(jsonlPath)) return entries;

        foreach (var line in File.ReadLines(jsonlPath))
        {
            var trimmed = line.Trim();
            if (string.IsNullOrEmpty(trimmed)) continue;
            try
            {
                var entry = JsonSerializer.Deserialize<SiteEntry>(trimmed);
                if (entry != null) entries.Add(entry);
            }
            catch (JsonException)
            {
                // Skip malformed lines
            }
        }
        return entries;
    }

    /// <summary>
    /// Score all entries against a query using the native turbo indexer.
    /// Returns results sorted by relevance.
    /// </summary>
    public List<ScoredSite> Search(string query, List<SiteEntry> database, int topN = 50)
    {
        if (string.IsNullOrWhiteSpace(query) || database.Count == 0)
            return new List<ScoredSite>();

        // Build JSON docs array for native batch scoring
        var docs = new List<object>();
        for (int i = 0; i < database.Count; i++)
        {
            var entry = database[i];
            var text = $"{entry.Title} {entry.Description} {entry.Keywords}";
            docs.Add(new { id = i, url = entry.Url, title = entry.Title, text });
        }

        var docsJson = JsonSerializer.Serialize(docs);
        var resultJson = NativeIndexer.BatchScore(query, docsJson, _maxThreads, topN);

        // Parse results
        var scored = new List<ScoredSite>();
        try
        {
            using var doc = JsonDocument.Parse(resultJson);
            foreach (var elem in doc.RootElement.EnumerateArray())
            {
                int id = elem.GetProperty("id").GetInt32();
                double score = elem.GetProperty("score").GetDouble();
                if (id >= 0 && id < database.Count)
                {
                    scored.Add(new ScoredSite
                    {
                        Id = id,
                        Url = database[id].Url,
                        Title = database[id].Title,
                        Score = score
                    });
                }
            }
        }
        catch (JsonException)
        {
            // Fallback: score individually if batch parse fails
            scored = SearchFallback(query, database, topN);
        }

        return scored;
    }

    /// <summary>
    /// Fallback: score each document individually (still uses native lib).
    /// </summary>
    private List<ScoredSite> SearchFallback(string query, List<SiteEntry> database, int topN)
    {
        var results = new ConcurrentBag<ScoredSite>();

        Parallel.For(0, database.Count,
            new ParallelOptions { MaxDegreeOfParallelism = _maxThreads },
            i =>
            {
                var entry = database[i];
                var text = $"{entry.Title} {entry.Description} {entry.Keywords}";
                double score = NativeIndexer.Score(query, text, entry.Title, entry.Url);
                if (score > 0.01)
                {
                    results.Add(new ScoredSite
                    {
                        Id = i,
                        Url = entry.Url,
                        Title = entry.Title,
                        Score = score
                    });
                }
            });

        return results.OrderByDescending(r => r.Score).Take(topN).ToList();
    }

    /// <summary>
    /// Index a batch of new entries: extract terms, deduplicate URLs, assign scores.
    /// </summary>
    public List<SiteEntry> IndexBatch(List<SiteEntry> newEntries, List<SiteEntry> existingDb)
    {
        var existingUrls = new HashSet<string>(
            existingDb.Select(e => NativeIndexer.NormalizeUrl(e.Url)),
            StringComparer.OrdinalIgnoreCase);

        var added = new ConcurrentBag<SiteEntry>();
        var timestamp = DateTime.UtcNow.ToString("yyyy-MM-dd HH:mm:ss");

        Parallel.ForEach(newEntries,
            new ParallelOptions { MaxDegreeOfParallelism = _maxThreads },
            entry =>
            {
                var normalizedUrl = NativeIndexer.NormalizeUrl(entry.Url);
                if (existingUrls.Contains(normalizedUrl)) return;

                entry.IndexedAt = timestamp;
                if (string.IsNullOrEmpty(entry.Category))
                {
                    entry.Category = CategorizeByDomain(entry.Url);
                }
                added.Add(entry);
            });

        return added.ToList();
    }

    /// <summary>
    /// Auto-categorize a URL based on its domain.
    /// </summary>
    private static string CategorizeByDomain(string url)
    {
        var domain = NativeIndexer.ExtractDomain(url);
        return domain switch
        {
            var d when d.Contains("github") || d.Contains("gitlab") => "Development",
            var d when d.Contains("stackoverflow") || d.Contains("stackexchange") => "Development",
            var d when d.Contains("docs.") || d.Contains("wiki") => "Documentation",
            var d when d.Contains("reddit") || d.Contains("twitter") || d.Contains("mastodon") => "Social",
            var d when d.Contains("youtube") || d.Contains("vimeo") || d.Contains("twitch") => "Media",
            var d when d.Contains("news") || d.Contains("bbc") || d.Contains("cnn") => "News",
            var d when d.Contains("arxiv") || d.Contains("scholar") || d.Contains(".edu") => "Academic",
            _ => "General"
        };
    }

    /// <summary>
    /// Export database to JSONL format.
    /// </summary>
    public static void ExportJsonl(string outputPath, List<SiteEntry> entries)
    {
        using var writer = new StreamWriter(outputPath);
        foreach (var entry in entries)
        {
            writer.WriteLine(JsonSerializer.Serialize(entry));
        }
    }

    /// <summary>
    /// Merge two databases, deduplicating by URL.
    /// </summary>
    public List<SiteEntry> MergeDatabases(List<SiteEntry> db1, List<SiteEntry> db2)
    {
        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var merged = new List<SiteEntry>();

        foreach (var entry in db1.Concat(db2))
        {
            var norm = NativeIndexer.NormalizeUrl(entry.Url);
            if (seen.Add(norm))
            {
                merged.Add(entry);
            }
        }
        return merged;
    }

    /// <summary>
    /// Run a benchmark against the native library.
    /// </summary>
    public static BenchmarkResult RunBenchmark(int docCount = 1000, int iterations = 10)
    {
        var sw = Stopwatch.StartNew();

        // Generate sample docs
        var sampleText = "The quick brown fox jumps over the lazy dog. " +
                          "Programming in Python and C++ for web development. " +
                          "Machine learning neural networks deep learning AI.";

        // Tokenization benchmark
        for (int i = 0; i < iterations * 100; i++)
            NativeIndexer.Tokenize(sampleText);
        var tokenizeMs = sw.ElapsedMilliseconds;

        // Scoring benchmark
        sw.Restart();
        for (int i = 0; i < iterations * 100; i++)
            NativeIndexer.Score("python programming", sampleText, "Test Page", "http://test.com");
        var scoreMs = sw.ElapsedMilliseconds;

        sw.Stop();

        return new BenchmarkResult
        {
            TokenizeOpsPerSec = (int)(iterations * 100 * 1000.0 / Math.Max(tokenizeMs, 1)),
            ScoreOpsPerSec = (int)(iterations * 100 * 1000.0 / Math.Max(scoreMs, 1)),
            CpuThreads = NativeIndexer.CpuThreads,
            NativeVersion = NativeIndexer.Version
        };
    }
}

public sealed class BenchmarkResult
{
    public int TokenizeOpsPerSec { get; set; }
    public int ScoreOpsPerSec { get; set; }
    public int CpuThreads { get; set; }
    public string NativeVersion { get; set; } = "";

    public override string ToString() =>
        $"Turbo Indexer Benchmark:\n" +
        $"  Tokenize: {TokenizeOpsPerSec:N0} ops/sec\n" +
        $"  Score:    {ScoreOpsPerSec:N0} ops/sec\n" +
        $"  Threads:  {CpuThreads}\n" +
        $"  Native:   {NativeVersion}";
}

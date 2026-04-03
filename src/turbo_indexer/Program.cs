/*
 * OyNIx Turbo Indexer — CLI entry point.
 *
 * Commands:
 *   search <query> <database.jsonl>     Search the database
 *   index <input.jsonl> <database.jsonl> Index new entries
 *   merge <db1.jsonl> <db2.jsonl> <out>  Merge two databases
 *   tokenize <text>                      Tokenize text
 *   bench                                Run performance benchmark
 *   version                              Show version
 *
 * Coded by Claude (Anthropic)
 */

using System.Text.Json;
using OyNIx.TurboIndexer;

if (args.Length < 1)
{
    Console.Error.WriteLine(
        """
        OyNIx Turbo Indexer — C# + C++ search engine
        Usage:
          turbo-indexer search <query> <database.jsonl> [top_n]
          turbo-indexer index <new.jsonl> <database.jsonl>
          turbo-indexer merge <db1.jsonl> <db2.jsonl> <output.jsonl>
          turbo-indexer tokenize <text>
          turbo-indexer ngrams <text> [n]
          turbo-indexer bench
          turbo-indexer version
        """);
    return 1;
}

var command = args[0].ToLowerInvariant();
var pipeline = new IndexPipeline();

try
{
    switch (command)
    {
        case "search":
        {
            if (args.Length < 3)
            {
                Console.Error.WriteLine("Usage: turbo-indexer search <query> <database.jsonl> [top_n]");
                return 1;
            }
            var query = args[1];
            var dbPath = args[2];
            int topN = args.Length >= 4 ? int.Parse(args[3]) : 20;

            var db = pipeline.LoadDatabase(dbPath);
            Console.Error.WriteLine($"Loaded {db.Count} entries from {dbPath}");

            var results = pipeline.Search(query, db, topN);
            Console.Error.WriteLine($"Found {results.Count} results for \"{query}\"");
            Console.Error.WriteLine();

            foreach (var r in results)
            {
                Console.WriteLine($"[{r.Score:F2}] {r.Title}");
                Console.WriteLine($"       {r.Url}");
            }
            break;
        }

        case "index":
        {
            if (args.Length < 3)
            {
                Console.Error.WriteLine("Usage: turbo-indexer index <new.jsonl> <database.jsonl>");
                return 1;
            }
            var newPath = args[1];
            var dbPath = args[2];

            var newEntries = pipeline.LoadDatabase(newPath);
            var existingDb = File.Exists(dbPath) ? pipeline.LoadDatabase(dbPath) : new List<SiteEntry>();

            Console.Error.WriteLine($"New entries: {newEntries.Count}, Existing: {existingDb.Count}");

            var added = pipeline.IndexBatch(newEntries, existingDb);
            Console.Error.WriteLine($"Added {added.Count} new entries (duplicates skipped)");

            // Append to database
            using var writer = new StreamWriter(dbPath, append: true);
            foreach (var entry in added)
            {
                writer.WriteLine(JsonSerializer.Serialize(entry));
            }
            Console.Error.WriteLine($"Database updated: {dbPath}");
            break;
        }

        case "merge":
        {
            if (args.Length < 4)
            {
                Console.Error.WriteLine("Usage: turbo-indexer merge <db1.jsonl> <db2.jsonl> <output.jsonl>");
                return 1;
            }
            var db1 = pipeline.LoadDatabase(args[1]);
            var db2 = pipeline.LoadDatabase(args[2]);
            var merged = pipeline.MergeDatabases(db1, db2);

            IndexPipeline.ExportJsonl(args[3], merged);
            Console.Error.WriteLine($"Merged {db1.Count} + {db2.Count} -> {merged.Count} entries");
            break;
        }

        case "tokenize":
        {
            if (args.Length < 2)
            {
                Console.Error.WriteLine("Usage: turbo-indexer tokenize <text>");
                return 1;
            }
            Console.WriteLine(NativeIndexer.Tokenize(args[1]));
            break;
        }

        case "ngrams":
        {
            if (args.Length < 2)
            {
                Console.Error.WriteLine("Usage: turbo-indexer ngrams <text> [n]");
                return 1;
            }
            int n = args.Length >= 3 ? int.Parse(args[2]) : 2;
            Console.WriteLine(NativeIndexer.ExtractNGrams(args[1], n));
            break;
        }

        case "bench":
        {
            Console.WriteLine("Running benchmark...");
            var result = IndexPipeline.RunBenchmark();
            Console.WriteLine(result);
            break;
        }

        case "version":
        {
            try
            {
                Console.WriteLine($"OyNIx Turbo Indexer (C#)");
                Console.WriteLine($"Native engine: {NativeIndexer.Version}");
                Console.WriteLine($"CPU threads: {NativeIndexer.CpuThreads}");
            }
            catch (DllNotFoundException)
            {
                Console.WriteLine("OyNIx Turbo Indexer (C#)");
                Console.WriteLine("Native engine: not found (compile libturbo_index.so first)");
            }
            break;
        }

        default:
            Console.Error.WriteLine($"Unknown command: {command}");
            return 1;
    }
}
catch (DllNotFoundException ex)
{
    Console.Error.WriteLine($"Native library not found: {ex.Message}");
    Console.Error.WriteLine("Build it first:");
    Console.Error.WriteLine("  g++ -O3 -shared -fPIC -std=c++17 -pthread \\");
    Console.Error.WriteLine("      -o libturbo_index.so src/native/turbo_index.cpp");
    return 2;
}

return 0;

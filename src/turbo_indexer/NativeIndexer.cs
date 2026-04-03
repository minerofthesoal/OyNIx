/*
 * OyNIx Turbo Indexer — C# P/Invoke bridge to the C++ turbo_index library.
 * Loads libturbo_index.so / turbo_index.dll and exposes managed wrappers.
 *
 * Coded by Claude (Anthropic)
 */

using System.Runtime.InteropServices;
using System.Text.Json;

namespace OyNIx.TurboIndexer;

/// <summary>
/// P/Invoke declarations for the native C++ turbo indexer.
/// </summary>
internal static partial class NativeLib
{
    private const string LibName = "turbo_index";

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr turbo_tokenize(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string text, int textLen);

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr turbo_ngrams(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string text, int textLen, int n);

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    public static extern double turbo_score(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string query,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string docText,
        int docLen,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string? title,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string? url);

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr turbo_batch_score(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string query,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string docsJson,
        int numThreads, int topN);

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr turbo_normalize_url(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string url);

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr turbo_extract_domain(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string url);

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int turbo_cpu_threads();

    [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr turbo_version();
}

/// <summary>
/// Managed wrapper around the native turbo indexer.
/// </summary>
public static class NativeIndexer
{
    private static string PtrToString(IntPtr ptr)
        => Marshal.PtrToStringUTF8(ptr) ?? "";

    /// <summary>Tokenize text, returning JSON array of tokens.</summary>
    public static string Tokenize(string text)
        => PtrToString(NativeLib.turbo_tokenize(text, text.Length));

    /// <summary>Extract n-grams from text.</summary>
    public static string ExtractNGrams(string text, int n = 2)
        => PtrToString(NativeLib.turbo_ngrams(text, text.Length, n));

    /// <summary>Score a single document against a query.</summary>
    public static double Score(string query, string docText,
                                string? title = null, string? url = null)
        => NativeLib.turbo_score(query, docText, docText.Length, title, url);

    /// <summary>Batch-score documents (multi-threaded).</summary>
    public static string BatchScore(string query, string docsJson,
                                     int threads = 0, int topN = 50)
        => PtrToString(NativeLib.turbo_batch_score(query, docsJson, threads, topN));

    /// <summary>Normalize a URL.</summary>
    public static string NormalizeUrl(string url)
        => PtrToString(NativeLib.turbo_normalize_url(url));

    /// <summary>Extract domain from URL.</summary>
    public static string ExtractDomain(string url)
        => PtrToString(NativeLib.turbo_extract_domain(url));

    /// <summary>Get available CPU threads.</summary>
    public static int CpuThreads => NativeLib.turbo_cpu_threads();

    /// <summary>Get native library version.</summary>
    public static string Version => PtrToString(NativeLib.turbo_version());
}

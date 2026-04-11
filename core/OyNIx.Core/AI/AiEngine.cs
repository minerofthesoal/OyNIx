using System.Text;
using System.Text.Json.Nodes;

namespace OyNIx.Core.AI;

/// <summary>
/// AI engine — manages conversation history, prompt construction, config, and fallback responses.
/// HTTP inference stays in C++ (Qt network). This handles the logic layer.
/// </summary>
public sealed class AiEngine
{
    public static AiEngine Instance { get; } = new();

    private readonly List<ChatMessage> _history = new();
    private AiConfig _config = new();
    private const int MaxHistory = 30;
    private const int MaxContentLength = 6000;

    public record ChatMessage(string Role, string Content);

    public class AiConfig
    {
        public string Backend { get; set; } = "ollama";
        public string Endpoint { get; set; } = "http://localhost:11434";
        public string ApiKey { get; set; } = "";
        public string Model { get; set; } = "tinyllama";
        public string SystemPrompt { get; set; } =
            "You are Nyx, the AI assistant built into OyNIx Browser. " +
            "Be helpful, concise, and knowledgeable. Format responses with markdown when appropriate.";
    }

    public void Configure(string jsonConfig)
    {
        try
        {
            var obj = JsonNode.Parse(jsonConfig)?.AsObject();
            if (obj != null)
            {
                _config = new AiConfig
                {
                    Backend = obj["Backend"]?.GetValue<string>() ?? _config.Backend,
                    Endpoint = obj["Endpoint"]?.GetValue<string>() ?? _config.Endpoint,
                    ApiKey = obj["ApiKey"]?.GetValue<string>() ?? _config.ApiKey,
                    Model = obj["Model"]?.GetValue<string>() ?? _config.Model,
                    SystemPrompt = obj["SystemPrompt"]?.GetValue<string>() ?? _config.SystemPrompt,
                };
            }
        }
        catch { /* keep existing config */ }
    }

    public string GetConfigJson()
    {
        return new JsonObject
        {
            ["Backend"] = _config.Backend,
            ["Endpoint"] = _config.Endpoint,
            ["ApiKey"] = _config.ApiKey,
            ["Model"] = _config.Model,
            ["SystemPrompt"] = _config.SystemPrompt,
        }.ToJsonString();
    }

    public void AddHistory(string role, string content)
    {
        _history.Add(new ChatMessage(role, content));
        while (_history.Count > MaxHistory)
            _history.RemoveAt(0);
    }

    public void ClearHistory() => _history.Clear();

    /// <summary>Build a context prompt for Ollama-style (single prompt string) backends.</summary>
    public string BuildContextPrompt(string userMessage)
    {
        var sb = new StringBuilder();
        sb.AppendLine(_config.SystemPrompt);
        sb.AppendLine();

        foreach (var msg in _history)
        {
            var label = msg.Role == "user" ? "User" : "Assistant";
            sb.AppendLine($"{label}: {msg.Content}");
        }

        sb.Append($"User: {userMessage}\nAssistant:");
        return sb.ToString();
    }

    /// <summary>Build a messages array for OpenAI-compatible (chat completions) backends.</summary>
    public string BuildMessagesJson(string userMessage)
    {
        var messages = new JsonArray();

        messages.Add(new JsonObject
        {
            ["role"] = "system",
            ["content"] = _config.SystemPrompt
        });

        foreach (var msg in _history)
        {
            messages.Add(new JsonObject
            {
                ["role"] = msg.Role,
                ["content"] = msg.Content
            });
        }

        messages.Add(new JsonObject
        {
            ["role"] = "user",
            ["content"] = userMessage
        });

        return messages.ToJsonString();
    }

    /// <summary>Rule-based fallback when no LLM backend is available.</summary>
    public string FallbackResponse(string prompt)
    {
        var lower = prompt.ToLowerInvariant();

        if (lower.Contains("hello") || lower.Contains("hi ") || lower == "hi")
            return "Hello! I'm Nyx, the OyNIx Browser AI assistant. " +
                   "I'm running in fallback mode — to enable full AI, install Ollama " +
                   "and configure it in Settings > AI.";

        if (lower.Contains("summarize") || lower.Contains("summary") || lower.Contains("tldr"))
            return "I'm in fallback mode and can't summarize pages right now. " +
                   "To enable AI summarization, configure an Ollama or OpenAI endpoint in Settings > AI.";

        if (lower.Contains("search") || lower.Contains("find"))
            return "Use the address bar (Ctrl+L) to search the web, or Ctrl+K to open the command palette " +
                   "for quick actions. Nyx Search indexes your browsing history for faster results.";

        if (lower.Contains("bookmark"))
            return "Press Ctrl+D to bookmark the current page, or Ctrl+B to open the bookmarks panel. " +
                   "You can organize bookmarks into folders and export them.";

        if (lower.Contains("theme") || lower.Contains("dark") || lower.Contains("light"))
            return "OyNIx ships with several themes. Go to View > Themes to switch between them, " +
                   "or customize colors in Settings > Appearance.";

        if (lower.Contains("extension") || lower.Contains("npi") || lower.Contains("addon"))
            return "OyNIx supports NPI extensions (compatible with Chrome extension manifests). " +
                   "Open the Extensions panel from the sidebar to manage installed extensions.";

        if (lower.Contains("shortcut") || lower.Contains("keybind"))
            return "Key shortcuts:\n" +
                   "• Ctrl+T — New tab\n• Ctrl+W — Close tab\n• Ctrl+L — Focus address bar\n" +
                   "• Ctrl+K — Command palette\n• Ctrl+Shift+A — AI panel\n" +
                   "• Ctrl+F — Find in page\n• F11 — Fullscreen";

        if (lower.Contains("help") || lower.Contains("what can"))
            return "I can help with:\n" +
                   "• Summarizing web pages\n• Explaining code snippets\n" +
                   "• Answering browser questions\n• Searching your history\n\n" +
                   "For full AI capabilities, configure Ollama or an OpenAI-compatible API in Settings > AI.";

        if (lower.Contains("privacy") || lower.Contains("tracking"))
            return "OyNIx respects your privacy. All data stays local — browsing history, bookmarks, " +
                   "and AI conversations are stored on your machine. No telemetry is collected.";

        if (lower.Contains("download"))
            return "Downloads appear in the Downloads panel (Ctrl+J). You can pause, resume, and " +
                   "manage downloads from there.";

        if (lower.Contains("tab") || lower.Contains("workspace"))
            return "Use the tree tab sidebar (Ctrl+Shift+T) for hierarchical tab management. " +
                   "Right-click tabs for options like pinning, duplicating, and grouping.";

        return "I'm Nyx, running in offline fallback mode. I can answer basic browser questions. " +
               "For full AI capabilities, set up Ollama (localhost:11434) or an OpenAI-compatible " +
               "endpoint in Settings > AI.";
    }

    /// <summary>Build a summarization prompt.</summary>
    public string BuildSummarizePrompt(string pageContent, string pageTitle)
    {
        var content = pageContent.Length > MaxContentLength
            ? pageContent[..MaxContentLength]
            : pageContent;

        return $"Summarize the following web page titled \"{pageTitle}\" in 3-5 concise bullet points:\n\n{content}";
    }

    /// <summary>Build a code explanation prompt.</summary>
    public string BuildExplainPrompt(string code)
    {
        var trimmed = code.Length > MaxContentLength ? code[..MaxContentLength] : code;
        return $"Explain the following code concisely:\n\n```\n{trimmed}\n```";
    }
}

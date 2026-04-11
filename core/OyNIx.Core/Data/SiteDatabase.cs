using System.Text.Json;
using System.Text.Json.Nodes;

namespace OyNIx.Core.Data;

/// <summary>
/// Auto-expanding site database. Starts with curated sites + grows via
/// browsing and search discovery. Persists discovered sites to JSON.
/// Port of oynix/core/database.py.
/// </summary>
public sealed class SiteDatabase
{
    public static SiteDatabase Instance { get; } = new();

    private string _dbPath = "";
    private readonly object _lock = new();
    private readonly Dictionary<string, List<SiteEntry>> _curated = new();
    private readonly Dictionary<string, List<SiteEntry>> _discovered = new();
    private readonly HashSet<string> _knownUrls = new();

    public int TotalCurated { get; private set; }
    public int TotalSites => TotalCurated + _discovered.Values.Sum(v => v.Count);
    public int TotalCategories => _curated.Keys.Union(_discovered.Keys).Count();

    public record SiteEntry(string Title, string Url, string Description, string Rating);

    // Domain -> category hints
    private static readonly Dictionary<string, string> DomainHints = new()
    {
        ["github.com"] = "tools", ["gitlab.com"] = "tools",
        ["stackoverflow.com"] = "tools", ["codepen.io"] = "tools",
        ["python.org"] = "python", ["pypi.org"] = "python",
        ["npmjs.com"] = "javascript", ["nodejs.org"] = "javascript",
        ["react.dev"] = "javascript", ["vuejs.org"] = "javascript",
        ["rust-lang.org"] = "rust", ["crates.io"] = "rust",
        ["go.dev"] = "go", ["golang.org"] = "go",
        ["docker.com"] = "devops", ["kubernetes.io"] = "devops",
        ["aws.amazon.com"] = "cloud", ["cloud.google.com"] = "cloud",
        ["azure.microsoft.com"] = "cloud",
        ["huggingface.co"] = "ai", ["openai.com"] = "ai",
        ["kaggle.com"] = "ai", ["pytorch.org"] = "ai",
        ["tensorflow.org"] = "ai", ["anthropic.com"] = "ai",
        ["archlinux.org"] = "linux", ["ubuntu.com"] = "linux",
        ["debian.org"] = "linux", ["kernel.org"] = "linux",
        ["figma.com"] = "design", ["dribbble.com"] = "design",
        ["leetcode.com"] = "learning", ["hackerrank.com"] = "learning",
        ["coursera.org"] = "learning", ["udemy.com"] = "learning",
        ["youtube.com"] = "media", ["twitch.tv"] = "media",
        ["reddit.com"] = "social", ["twitter.com"] = "social",
        ["x.com"] = "social", ["mastodon.social"] = "social",
        ["wikipedia.org"] = "reference", ["wikimedia.org"] = "reference",
        ["medium.com"] = "blogs", ["dev.to"] = "blogs",
        ["news.ycombinator.com"] = "news_tech",
        ["techcrunch.com"] = "news_tech", ["arstechnica.com"] = "news_tech",
    };

    private static readonly Dictionary<string, string> PathHints = new()
    {
        ["python"] = "python", ["django"] = "web_frameworks",
        ["flask"] = "web_frameworks", ["react"] = "javascript",
        ["vue"] = "javascript", ["angular"] = "javascript",
        ["rust"] = "rust", ["golang"] = "go",
        ["docker"] = "devops", ["kubernetes"] = "devops", ["k8s"] = "devops",
        ["machine-learning"] = "ai", ["deep-learning"] = "ai",
        ["linux"] = "linux", ["ubuntu"] = "linux",
        ["security"] = "security", ["crypto"] = "security",
        ["game"] = "gaming", ["unity"] = "gaming", ["unreal"] = "gaming",
        ["database"] = "databases", ["sql"] = "databases", ["postgres"] = "databases",
        ["api"] = "api", ["graphql"] = "api", ["rest"] = "api",
        ["test"] = "testing", ["selenium"] = "testing", ["cypress"] = "testing",
    };

    public void Initialize(string configDir)
    {
        _dbPath = Path.Combine(configDir, "database", "discovered.json");
        Directory.CreateDirectory(Path.GetDirectoryName(_dbPath)!);
        BuildCurated();
        LoadDiscovered();
    }

    public string GuessCategory(string url, string title = "")
    {
        try
        {
            var uri = new Uri(url);
            var domain = uri.Host.ToLowerInvariant().TrimStart('w', 'w', 'w', '.');
            var path = uri.AbsolutePath.ToLowerInvariant();

            foreach (var (hintDomain, cat) in DomainHints)
            {
                if (domain == hintDomain || domain.EndsWith("." + hintDomain))
                    return cat;
            }

            var combined = (path + " " + title).ToLowerInvariant();
            foreach (var (keyword, cat) in PathHints)
            {
                if (combined.Contains(keyword))
                    return cat;
            }
        }
        catch { }
        return "discovered";
    }

    public bool AddSite(string url, string title, string description, string source)
    {
        lock (_lock)
        {
            var norm = NormalizeUrl(url);
            if (_knownUrls.Contains(norm)) return false;

            var category = GuessCategory(url, title);
            var entry = new SiteEntry(title, url, description, "N/A");

            if (!_discovered.ContainsKey(category))
                _discovered[category] = new List<SiteEntry>();
            _discovered[category].Add(entry);
            _knownUrls.Add(norm);
            SaveDiscovered();
            return true;
        }
    }

    public bool AddSitesBatch(JsonArray sites, string source)
    {
        lock (_lock)
        {
            int added = 0;
            foreach (var val in sites)
            {
                var obj = val?.AsObject();
                if (obj == null) continue;

                var url = obj["url"]?.GetValue<string>() ?? "";
                var title = obj["title"]?.GetValue<string>() ?? "";
                var desc = obj["description"]?.GetValue<string>() ?? "";

                var norm = NormalizeUrl(url);
                if (_knownUrls.Contains(norm)) continue;

                var category = GuessCategory(url, title);
                var entry = new SiteEntry(title, url, desc, "N/A");

                if (!_discovered.ContainsKey(category))
                    _discovered[category] = new List<SiteEntry>();
                _discovered[category].Add(entry);
                _knownUrls.Add(norm);
                added++;
            }

            if (added > 0) SaveDiscovered();
            return added > 0;
        }
    }

    public string SearchJson(string query, int limit = 50)
    {
        var lower = query.ToLowerInvariant();
        var results = new JsonArray();

        void SearchIn(Dictionary<string, List<SiteEntry>> source, string sourceType)
        {
            foreach (var (cat, sites) in source)
            {
                foreach (var s in sites)
                {
                    if (results.Count >= limit) return;
                    if (s.Title.Contains(lower, StringComparison.OrdinalIgnoreCase) ||
                        s.Url.Contains(lower, StringComparison.OrdinalIgnoreCase) ||
                        s.Description.Contains(lower, StringComparison.OrdinalIgnoreCase) ||
                        cat.Contains(lower, StringComparison.OrdinalIgnoreCase))
                    {
                        results.Add(new JsonObject
                        {
                            ["url"] = s.Url,
                            ["title"] = s.Title,
                            ["description"] = s.Description,
                            ["category"] = cat,
                            ["rating"] = s.Rating,
                            ["source"] = sourceType
                        });
                    }
                }
            }
        }

        lock (_lock)
        {
            SearchIn(_curated, "curated");
            SearchIn(_discovered, "discovered");
        }

        return results.ToJsonString();
    }

    public string GetByCategoryJson(string category)
    {
        var results = new JsonArray();
        lock (_lock)
        {
            if (_curated.TryGetValue(category, out var curated))
                foreach (var s in curated)
                    results.Add(new JsonObject
                    {
                        ["url"] = s.Url, ["title"] = s.Title,
                        ["description"] = s.Description, ["rating"] = s.Rating
                    });

            if (_discovered.TryGetValue(category, out var disc))
                foreach (var s in disc)
                    results.Add(new JsonObject
                    {
                        ["url"] = s.Url, ["title"] = s.Title,
                        ["description"] = s.Description, ["rating"] = s.Rating
                    });
        }
        return results.ToJsonString();
    }

    public string GetCategoriesJson()
    {
        lock (_lock)
        {
            var cats = _curated.Keys.Union(_discovered.Keys).OrderBy(c => c);
            return JsonSerializer.Serialize(cats);
        }
    }

    public string GetStatsJson()
    {
        lock (_lock)
        {
            return JsonSerializer.Serialize(new
            {
                total_curated = TotalCurated,
                total_discovered = _discovered.Values.Sum(v => v.Count),
                total_sites = TotalSites,
                total_categories = TotalCategories
            });
        }
    }

    public string ExportJson()
    {
        lock (_lock)
        {
            var all = new JsonArray();
            void Add(Dictionary<string, List<SiteEntry>> src, string sourceType)
            {
                foreach (var (cat, sites) in src)
                    foreach (var s in sites)
                        all.Add(new JsonObject
                        {
                            ["url"] = s.Url, ["title"] = s.Title,
                            ["description"] = s.Description, ["category"] = cat,
                            ["rating"] = s.Rating, ["source"] = sourceType
                        });
            }
            Add(_curated, "curated");
            Add(_discovered, "discovered");
            return all.ToJsonString();
        }
    }

    public void ClearDiscovered()
    {
        lock (_lock)
        {
            foreach (var url in _discovered.Values.SelectMany(v => v).Select(s => NormalizeUrl(s.Url)))
                _knownUrls.Remove(url);
            _discovered.Clear();
            SaveDiscovered();
        }
    }

    private static string NormalizeUrl(string url)
    {
        try
        {
            var uri = new Uri(url.StartsWith("http") ? url : "https://" + url);
            return uri.Host.ToLowerInvariant().TrimStart('w', 'w', 'w', '.') +
                   uri.AbsolutePath.TrimEnd('/');
        }
        catch { return url.ToLowerInvariant(); }
    }

    private void LoadDiscovered()
    {
        if (!File.Exists(_dbPath)) return;
        try
        {
            var json = File.ReadAllText(_dbPath);
            var arr = JsonSerializer.Deserialize<JsonArray>(json);
            if (arr == null) return;

            foreach (var val in arr)
            {
                var obj = val?.AsObject();
                if (obj == null) continue;
                var url = obj["url"]?.GetValue<string>() ?? "";
                var title = obj["title"]?.GetValue<string>() ?? "";
                var desc = obj["description"]?.GetValue<string>() ?? "";
                var cat = obj["category"]?.GetValue<string>() ?? "discovered";
                var rating = obj["rating"]?.GetValue<string>() ?? "N/A";

                if (!_discovered.ContainsKey(cat))
                    _discovered[cat] = new List<SiteEntry>();
                _discovered[cat].Add(new SiteEntry(title, url, desc, rating));
                _knownUrls.Add(NormalizeUrl(url));
            }
        }
        catch { }
    }

    private void SaveDiscovered()
    {
        try
        {
            var arr = new JsonArray();
            foreach (var (cat, sites) in _discovered)
                foreach (var s in sites)
                    arr.Add(new JsonObject
                    {
                        ["url"] = s.Url, ["title"] = s.Title,
                        ["description"] = s.Description, ["category"] = cat,
                        ["rating"] = s.Rating
                    });

            Directory.CreateDirectory(Path.GetDirectoryName(_dbPath)!);
            File.WriteAllText(_dbPath, arr.ToJsonString());
        }
        catch { }
    }

    private void BuildCurated()
    {
        // Representative curated sites across 30 categories
        AddCurated("tools", new[] {
            ("GitHub", "https://github.com", "Code hosting and collaboration"),
            ("GitLab", "https://gitlab.com", "DevOps platform"),
            ("Stack Overflow", "https://stackoverflow.com", "Developer Q&A"),
            ("CodePen", "https://codepen.io", "Online code editor"),
            ("JSFiddle", "https://jsfiddle.net", "JS/HTML/CSS playground"),
            ("Replit", "https://replit.com", "Online IDE"),
            ("VS Code", "https://code.visualstudio.com", "Code editor"),
        });
        AddCurated("python", new[] {
            ("Python.org", "https://python.org", "Official Python site"),
            ("PyPI", "https://pypi.org", "Python package index"),
            ("Real Python", "https://realpython.com", "Python tutorials"),
            ("Django", "https://djangoproject.com", "Python web framework"),
            ("Flask", "https://flask.palletsprojects.com", "Lightweight web framework"),
            ("FastAPI", "https://fastapi.tiangolo.com", "Modern Python API framework"),
        });
        AddCurated("javascript", new[] {
            ("MDN Web Docs", "https://developer.mozilla.org", "Web documentation"),
            ("npm", "https://npmjs.com", "JS package registry"),
            ("React", "https://react.dev", "UI library"),
            ("Vue.js", "https://vuejs.org", "Progressive JS framework"),
            ("Next.js", "https://nextjs.org", "React framework"),
            ("TypeScript", "https://typescriptlang.org", "Typed JavaScript"),
        });
        AddCurated("rust", new[] {
            ("Rust Lang", "https://rust-lang.org", "Systems programming language"),
            ("crates.io", "https://crates.io", "Rust package registry"),
            ("Rust Book", "https://doc.rust-lang.org/book/", "Official Rust book"),
        });
        AddCurated("go", new[] {
            ("Go.dev", "https://go.dev", "Go programming language"),
            ("Go Packages", "https://pkg.go.dev", "Go package docs"),
        });
        AddCurated("ai", new[] {
            ("Hugging Face", "https://huggingface.co", "ML models and datasets"),
            ("OpenAI", "https://openai.com", "AI research lab"),
            ("Anthropic", "https://anthropic.com", "AI safety research"),
            ("Kaggle", "https://kaggle.com", "ML competitions and datasets"),
            ("PyTorch", "https://pytorch.org", "ML framework"),
            ("TensorFlow", "https://tensorflow.org", "ML framework"),
            ("Papers With Code", "https://paperswithcode.com", "ML papers + code"),
        });
        AddCurated("devops", new[] {
            ("Docker", "https://docker.com", "Container platform"),
            ("Kubernetes", "https://kubernetes.io", "Container orchestration"),
            ("Terraform", "https://terraform.io", "Infrastructure as code"),
            ("Ansible", "https://ansible.com", "Automation platform"),
        });
        AddCurated("cloud", new[] {
            ("AWS", "https://aws.amazon.com", "Amazon cloud services"),
            ("Google Cloud", "https://cloud.google.com", "Google cloud platform"),
            ("Azure", "https://azure.microsoft.com", "Microsoft cloud"),
            ("DigitalOcean", "https://digitalocean.com", "Cloud infrastructure"),
            ("Vercel", "https://vercel.com", "Frontend cloud platform"),
        });
        AddCurated("linux", new[] {
            ("Arch Linux", "https://archlinux.org", "Lightweight Linux distro"),
            ("Ubuntu", "https://ubuntu.com", "Popular Linux distro"),
            ("Debian", "https://debian.org", "Stable Linux distro"),
            ("Fedora", "https://fedoraproject.org", "Red Hat community distro"),
            ("Linux Kernel", "https://kernel.org", "Linux kernel source"),
        });
        AddCurated("databases", new[] {
            ("PostgreSQL", "https://postgresql.org", "Advanced open-source RDBMS"),
            ("Redis", "https://redis.io", "In-memory data store"),
            ("MongoDB", "https://mongodb.com", "Document database"),
            ("SQLite", "https://sqlite.org", "Embedded SQL database"),
        });
        AddCurated("design", new[] {
            ("Figma", "https://figma.com", "Collaborative design tool"),
            ("Dribbble", "https://dribbble.com", "Design showcase"),
            ("Behance", "https://behance.net", "Creative portfolio platform"),
            ("Canva", "https://canva.com", "Online design tool"),
        });
        AddCurated("learning", new[] {
            ("freeCodeCamp", "https://freecodecamp.org", "Learn to code free"),
            ("Coursera", "https://coursera.org", "Online courses"),
            ("Khan Academy", "https://khanacademy.org", "Free education"),
            ("LeetCode", "https://leetcode.com", "Coding challenges"),
            ("HackerRank", "https://hackerrank.com", "Coding practice"),
        });
        AddCurated("media", new[] {
            ("YouTube", "https://youtube.com", "Video sharing"),
            ("Twitch", "https://twitch.tv", "Live streaming"),
            ("Spotify", "https://spotify.com", "Music streaming"),
            ("SoundCloud", "https://soundcloud.com", "Audio platform"),
        });
        AddCurated("social", new[] {
            ("Reddit", "https://reddit.com", "Community forums"),
            ("Twitter/X", "https://x.com", "Social network"),
            ("Mastodon", "https://mastodon.social", "Federated social"),
            ("Discord", "https://discord.com", "Chat platform"),
        });
        AddCurated("news_tech", new[] {
            ("Hacker News", "https://news.ycombinator.com", "Tech news"),
            ("TechCrunch", "https://techcrunch.com", "Startup news"),
            ("Ars Technica", "https://arstechnica.com", "Tech journalism"),
            ("The Verge", "https://theverge.com", "Tech and culture"),
        });
        AddCurated("reference", new[] {
            ("Wikipedia", "https://en.wikipedia.org", "Free encyclopedia"),
            ("Wolfram Alpha", "https://wolframalpha.com", "Computational engine"),
            ("Archive.org", "https://archive.org", "Internet archive"),
        });
        AddCurated("blogs", new[] {
            ("Medium", "https://medium.com", "Publishing platform"),
            ("Dev.to", "https://dev.to", "Developer community"),
            ("Hashnode", "https://hashnode.com", "Developer blogging"),
        });
        AddCurated("web_frameworks", new[] {
            ("Ruby on Rails", "https://rubyonrails.org", "Ruby web framework"),
            ("Laravel", "https://laravel.com", "PHP web framework"),
            ("Spring", "https://spring.io", "Java framework"),
            ("ASP.NET", "https://dotnet.microsoft.com/apps/aspnet", ".NET web framework"),
        });
        AddCurated("security", new[] {
            ("OWASP", "https://owasp.org", "Web security standards"),
            ("Have I Been Pwned", "https://haveibeenpwned.com", "Breach checker"),
            ("CVE", "https://cve.mitre.org", "Vulnerability database"),
        });
        AddCurated("gaming", new[] {
            ("Unity", "https://unity.com", "Game engine"),
            ("Unreal Engine", "https://unrealengine.com", "Game engine"),
            ("Godot", "https://godotengine.org", "Open-source game engine"),
            ("itch.io", "https://itch.io", "Indie game marketplace"),
        });
        AddCurated("api", new[] {
            ("Postman", "https://postman.com", "API platform"),
            ("Swagger", "https://swagger.io", "API documentation"),
            ("RapidAPI", "https://rapidapi.com", "API marketplace"),
        });
        AddCurated("testing", new[] {
            ("Selenium", "https://selenium.dev", "Browser automation"),
            ("Cypress", "https://cypress.io", "E2E testing"),
            ("Jest", "https://jestjs.io", "JavaScript testing"),
        });

        TotalCurated = _curated.Values.Sum(v => v.Count);
        foreach (var sites in _curated.Values)
            foreach (var s in sites)
                _knownUrls.Add(NormalizeUrl(s.Url));
    }

    private void AddCurated(string category, (string title, string url, string desc)[] sites)
    {
        if (!_curated.ContainsKey(category))
            _curated[category] = new List<SiteEntry>();
        foreach (var (title, url, desc) in sites)
            _curated[category].Add(new SiteEntry(title, url, desc, "9.0"));
    }
}

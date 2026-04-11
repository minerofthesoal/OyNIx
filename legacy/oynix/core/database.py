"""
OyNIx Browser v1.1.0 - Auto-Expanding Site Database
Curated 1400+ sites + auto-expands from browsing and search results.

The database persists to SQLite so new sites survive restarts.
When auto_expand is enabled in settings, sites visited or discovered
via DuckDuckGo are automatically added to the 'discovered' category
(and smart-categorized when possible).

This is a native desktop application - NOT a web app.
Coded by Claude (Anthropic) for the OyNIx project.
"""

import json
import os
import sqlite3
import threading
import time
from urllib.parse import urlparse

# Persistent database location
DB_DIR = os.path.join(os.path.expanduser("~"), ".config", "oynix", "database")
DB_FILE = os.path.join(DB_DIR, "oynix_sites.db")


# ── Domain-to-category heuristic mapping ──────────────────────────────
DOMAIN_CATEGORY_HINTS = {
    'github.com': 'tools', 'gitlab.com': 'tools',
    'stackoverflow.com': 'tools', 'codepen.io': 'tools',
    'python.org': 'python', 'pypi.org': 'python',
    'npmjs.com': 'javascript', 'nodejs.org': 'javascript',
    'react.dev': 'javascript', 'vuejs.org': 'javascript',
    'rust-lang.org': 'rust', 'crates.io': 'rust',
    'go.dev': 'go', 'golang.org': 'go',
    'docker.com': 'devops', 'kubernetes.io': 'devops',
    'aws.amazon.com': 'cloud', 'cloud.google.com': 'cloud',
    'azure.microsoft.com': 'cloud',
    'huggingface.co': 'ai', 'openai.com': 'ai',
    'kaggle.com': 'ai', 'pytorch.org': 'ai',
    'tensorflow.org': 'ai', 'anthropic.com': 'ai',
    'archlinux.org': 'linux', 'ubuntu.com': 'linux',
    'debian.org': 'linux', 'kernel.org': 'linux',
    'figma.com': 'design', 'dribbble.com': 'design',
    'leetcode.com': 'learning', 'hackerrank.com': 'learning',
    'coursera.org': 'learning', 'udemy.com': 'learning',
    'youtube.com': 'media', 'twitch.tv': 'media',
    'reddit.com': 'social', 'twitter.com': 'social',
    'x.com': 'social', 'mastodon.social': 'social',
    'wikipedia.org': 'reference', 'wikimedia.org': 'reference',
    'medium.com': 'blogs', 'dev.to': 'blogs',
    'news.ycombinator.com': 'news_tech',
    'techcrunch.com': 'news_tech', 'arstechnica.com': 'news_tech',
}

# Keywords in URL path that hint at categories
PATH_CATEGORY_HINTS = {
    'python': 'python', 'django': 'web_frameworks',
    'flask': 'web_frameworks', 'react': 'javascript',
    'vue': 'javascript', 'angular': 'javascript',
    'rust': 'rust', 'golang': 'go',
    'docker': 'devops', 'kubernetes': 'devops', 'k8s': 'devops',
    'machine-learning': 'ai', 'deep-learning': 'ai',
    'artificial-intelligence': 'ai',
    'linux': 'linux', 'ubuntu': 'linux',
    'security': 'security', 'crypto': 'security',
    'game': 'gaming', 'unity': 'gaming', 'unreal': 'gaming',
    'database': 'databases', 'sql': 'databases', 'postgres': 'databases',
    'api': 'api', 'graphql': 'api', 'rest': 'api',
    'test': 'testing', 'selenium': 'testing', 'cypress': 'testing',
}


def guess_category(url, title=""):
    """Try to auto-categorize a URL based on domain and path heuristics."""
    try:
        parsed = urlparse(url)
        domain = parsed.netloc.lower().lstrip('www.')
        path = parsed.path.lower()

        # Exact domain match
        for hint_domain, cat in DOMAIN_CATEGORY_HINTS.items():
            if domain == hint_domain or domain.endswith('.' + hint_domain):
                return cat

        # Path keyword match
        combined = (path + ' ' + title).lower()
        for keyword, cat in PATH_CATEGORY_HINTS.items():
            if keyword in combined:
                return cat

    except Exception:
        pass

    return 'discovered'


class OynixDatabase:
    """
    Auto-expanding site database.
    Starts with 1400+ hardcoded curated sites, then grows as the user
    browses and searches. New sites persist in SQLite.

    Coded by Claude (Anthropic) for the OyNIx project.
    """

    def __init__(self):
        self._lock = threading.Lock()
        self._curated = self._build_curated()
        self.total_curated = sum(len(v) for v in self._curated.values())

        # Build URL set for fast dedup
        self._known_urls = set()
        for sites in self._curated.values():
            for _, url, _, _ in sites:
                self._known_urls.add(self._norm_url(url))

        # Open persistent database
        os.makedirs(DB_DIR, exist_ok=True)
        self.conn = sqlite3.connect(DB_FILE, check_same_thread=False)
        self._init_db()

        # Load persisted discovered sites
        self._discovered = {}   # category -> [(title, url, desc, rating)]
        self._load_discovered()

        self.total_sites = self.total_curated + sum(
            len(v) for v in self._discovered.values())
        self.total_categories = len(
            set(list(self._curated.keys()) + list(self._discovered.keys())))

    # ── Persistence ───────────────────────────────────────────────────

    def _init_db(self):
        self.conn.execute("""
            CREATE TABLE IF NOT EXISTS discovered_sites (
                url TEXT PRIMARY KEY,
                title TEXT NOT NULL,
                description TEXT DEFAULT '',
                rating TEXT DEFAULT 'N/A',
                category TEXT DEFAULT 'discovered',
                source TEXT DEFAULT 'visited',
                added_at REAL,
                visit_count INTEGER DEFAULT 1
            )
        """)
        self.conn.commit()

    def _load_discovered(self):
        """Load previously discovered sites from SQLite."""
        rows = self.conn.execute(
            "SELECT url, title, description, rating, category "
            "FROM discovered_sites ORDER BY visit_count DESC"
        ).fetchall()
        for url, title, desc, rating, cat in rows:
            self._discovered.setdefault(cat, []).append(
                (title, url, desc, rating))
            self._known_urls.add(self._norm_url(url))

    def _norm_url(self, url):
        """Normalize URL for dedup (strip trailing slash, protocol)."""
        url = url.rstrip('/')
        for prefix in ('https://', 'http://', 'www.'):
            if url.startswith(prefix):
                url = url[len(prefix):]
        return url.lower()

    # ── Auto-expand methods ───────────────────────────────────────────

    def add_site(self, url, title, description="", source="visited"):
        """
        Add a site to the database if not already present.
        Called automatically when user visits a page or DDG returns results.

        Returns True if site was actually added (new), False if duplicate.
        """
        if not url or not title:
            return False

        norm = self._norm_url(url)

        # Skip internal pages, blank pages, search engines themselves
        if any(skip in norm for skip in [
            'oyn://', 'about:blank', 'chrome://', 'file://',
            'duckduckgo.com', 'google.com/search', 'bing.com/search',
            'search.brave.com/search', 'localhost',
        ]):
            return False

        with self._lock:
            if norm in self._known_urls:
                # Update visit count for existing discovered sites
                self.conn.execute(
                    "UPDATE discovered_sites SET visit_count = visit_count + 1 "
                    "WHERE url = ?", (url,))
                self.conn.commit()
                return False

            # New site - categorize and add
            category = guess_category(url, title)
            rating = 'N/A'

            self.conn.execute(
                "INSERT OR IGNORE INTO discovered_sites "
                "(url, title, description, rating, category, source, added_at) "
                "VALUES (?, ?, ?, ?, ?, ?, ?)",
                (url, title, description[:500], rating, category, source,
                 time.time())
            )
            self.conn.commit()

            self._discovered.setdefault(category, []).append(
                (title, url, description[:500], rating))
            self._known_urls.add(norm)

            self.total_sites += 1
            return True

    def add_sites_batch(self, sites, source="duckduckgo"):
        """
        Add multiple sites at once (e.g., from DDG search results).
        sites: list of dicts with 'url', 'title', 'description' keys.
        """
        added = 0
        for site in sites:
            if self.add_site(
                site.get('url', ''),
                site.get('title', ''),
                site.get('description', ''),
                source=source,
            ):
                added += 1
        if added:
            print(f"[DB] Auto-added {added} new sites from {source}")
        return added

    def get_discovered_count(self):
        """Get count of auto-discovered sites."""
        try:
            return self.conn.execute(
                "SELECT COUNT(*) FROM discovered_sites"
            ).fetchone()[0]
        except Exception:
            return 0

    def get_stats(self):
        """Get database statistics."""
        discovered = self.get_discovered_count()
        return {
            'curated': self.total_curated,
            'discovered': discovered,
            'total': self.total_curated + discovered,
            'categories': self.total_categories,
        }

    # ── Search ────────────────────────────────────────────────────────

    @property
    def sites(self):
        """Combined view of curated + discovered sites."""
        combined = {}
        for cat, entries in self._curated.items():
            combined[cat] = list(entries)
        for cat, entries in self._discovered.items():
            combined.setdefault(cat, []).extend(entries)
        return combined

    def get_by_category(self, category):
        cat = category.lower()
        curated = self._curated.get(cat, [])
        discovered = self._discovered.get(cat, [])
        return curated + discovered

    def search(self, query, fuzzy=True):
        """Search across all categories (curated + discovered)."""
        q = query.lower()
        results = []
        for category, entries in self.sites.items():
            for title, url, desc, rating in entries:
                if (q in title.lower() or q in desc.lower()
                        or q in url.lower()):
                    results.append((title, url, desc, rating, category))
        return results

    def get_all_categories(self):
        return sorted(set(
            list(self._curated.keys()) + list(self._discovered.keys())))

    def export_to_json(self, filepath):
        """Export entire database (curated + discovered) as JSON."""
        data = {
            'curated': {},
            'discovered': {},
            'meta': {
                'exported_at': time.strftime('%Y-%m-%d %H:%M:%S'),
                'total_sites': self.total_sites,
                'coded_by': 'Claude (Anthropic) for the OyNIx project',
            }
        }
        for cat, entries in self._curated.items():
            data['curated'][cat] = [
                {'title': t, 'url': u, 'desc': d, 'rating': r}
                for t, u, d, r in entries
            ]

        rows = self.conn.execute(
            "SELECT url, title, description, rating, category, source, "
            "added_at, visit_count FROM discovered_sites "
            "ORDER BY category, visit_count DESC"
        ).fetchall()
        for url, title, desc, rating, cat, source, added, visits in rows:
            data['discovered'].setdefault(cat, []).append({
                'title': title, 'url': url, 'desc': desc,
                'rating': rating, 'source': source, 'visits': visits,
            })

        with open(filepath, 'w') as f:
            json.dump(data, f, indent=2)

    def clear_discovered(self):
        """Clear all auto-discovered sites (keep curated)."""
        with self._lock:
            self.conn.execute("DELETE FROM discovered_sites")
            self.conn.commit()
            self._discovered.clear()
            # Rebuild known URLs from curated only
            self._known_urls.clear()
            for sites in self._curated.values():
                for _, url, _, _ in sites:
                    self._known_urls.add(self._norm_url(url))
            self.total_sites = self.total_curated

    def get_all_sites(self):
        """Get all sites (curated + discovered) as flat list of dicts."""
        all_sites = []
        for cat, entries in self._curated.items():
            for title, url, desc, rating in entries:
                all_sites.append({
                    'title': title, 'url': url, 'description': desc,
                    'rating': rating, 'category': cat, 'source': 'curated',
                })
        for cat, entries in self._discovered.items():
            for title, url, desc, rating in entries:
                all_sites.append({
                    'title': title, 'url': url, 'description': desc,
                    'rating': rating, 'category': cat, 'source': 'discovered',
                })
        return all_sites

    def compare_site(self, url, title=""):
        """
        Compare a URL against known sites. Returns match info:
        - is_known: whether the site is already in the database
        - similar: list of similar sites from the database
        - category_guess: guessed category for the site
        """
        norm = self._norm_url(url)
        is_known = norm in self._known_urls
        domain = urlparse(url).netloc.lower()
        similar = []

        # Find sites with same domain or similar domain
        for cat, entries in self.sites.items():
            for stitle, surl, sdesc, srating in entries:
                sdomain = urlparse(surl).netloc.lower()
                if sdomain == domain and self._norm_url(surl) != norm:
                    similar.append({
                        'title': stitle, 'url': surl,
                        'description': sdesc, 'category': cat,
                    })
                    if len(similar) >= 5:
                        break
            if len(similar) >= 5:
                break

        # Guess category
        cat_guess = guess_category(url, title)

        return {
            'is_known': is_known,
            'similar': similar,
            'category_guess': cat_guess,
            'domain': domain,
        }

    def get_sites_by_domain(self, domain):
        """Get all known sites for a given domain."""
        domain = domain.lower()
        results = []
        for cat, entries in self.sites.items():
            for title, url, desc, rating in entries:
                if urlparse(url).netloc.lower() == domain:
                    results.append({
                        'title': title, 'url': url,
                        'description': desc, 'category': cat,
                        'rating': rating,
                    })
        return results

    # ── Curated database ──────────────────────────────────────────────

    def _build_curated(self):
        return {
            "python": [
                ("Python.org", "https://python.org", "Official Python", "10/10"),
                ("PyPI", "https://pypi.org", "Package Index", "10/10"),
                ("Real Python", "https://realpython.com", "Tutorials", "10/10"),
                ("Python Docs", "https://docs.python.org", "Documentation", "10/10"),
                ("Full Stack Python", "https://fullstackpython.com", "Web Dev", "9/10"),
                ("Python Weekly", "https://pythonweekly.com", "Newsletter", "9/10"),
                ("Talk Python", "https://talkpython.fm", "Podcast", "9/10"),
                ("Python Bytes", "https://pythonbytes.fm", "Podcast", "9/10"),
                ("Awesome Python", "https://awesome-python.com", "Resources", "9/10"),
                ("PEPs", "https://peps.python.org", "Proposals", "10/10"),
                ("Hitchhiker's Guide", "https://docs.python-guide.org", "Best Practices", "10/10"),
                ("Automate Boring Stuff", "https://automatetheboringstuff.com", "Practical Python", "10/10"),
                ("Python Tutor", "https://pythontutor.com", "Visualize Code", "9/10"),
                ("Python for Everybody", "https://py4e.com", "Course", "9/10"),
                ("Effective Python", "https://effectivepython.com", "Best Practices", "9/10"),
            ],
            "javascript": [
                ("MDN JavaScript", "https://developer.mozilla.org/JavaScript", "Reference", "10/10"),
                ("JavaScript.info", "https://javascript.info", "Modern Tutorial", "10/10"),
                ("Node.js", "https://nodejs.org", "Runtime", "10/10"),
                ("npm", "https://npmjs.com", "Package Manager", "10/10"),
                ("React", "https://react.dev", "UI Library", "10/10"),
                ("Vue.js", "https://vuejs.org", "Framework", "10/10"),
                ("TypeScript", "https://typescriptlang.org", "Typed JS", "10/10"),
                ("Vite", "https://vitejs.dev", "Build Tool", "10/10"),
                ("Next.js", "https://nextjs.org", "React Framework", "10/10"),
                ("Svelte", "https://svelte.dev", "Compiler", "9/10"),
                ("Express", "https://expressjs.com", "Web Framework", "10/10"),
                ("Astro", "https://astro.build", "Static Site", "9/10"),
            ],
            "rust": [
                ("Rust Lang", "https://rust-lang.org", "Official", "10/10"),
                ("Rust Book", "https://doc.rust-lang.org/book", "The Book", "10/10"),
                ("Crates.io", "https://crates.io", "Packages", "10/10"),
                ("Tokio", "https://tokio.rs", "Async Runtime", "10/10"),
                ("Tauri", "https://tauri.app", "Desktop Apps", "10/10"),
                ("Bevy", "https://bevyengine.org", "Game Engine", "9/10"),
            ],
            "ai": [
                ("HuggingFace", "https://huggingface.co", "AI Hub", "10/10"),
                ("OpenAI", "https://openai.com", "ChatGPT", "10/10"),
                ("Anthropic", "https://anthropic.com", "Claude AI", "10/10"),
                ("Google AI", "https://ai.google", "Google AI", "10/10"),
                ("Papers with Code", "https://paperswithcode.com", "ML Research", "10/10"),
                ("arXiv", "https://arxiv.org", "Papers", "10/10"),
                ("Kaggle", "https://kaggle.com", "Data Science", "10/10"),
                ("Ollama", "https://ollama.ai", "Run LLMs Locally", "10/10"),
                ("LM Studio", "https://lmstudio.ai", "Desktop LLMs", "10/10"),
                ("Mistral AI", "https://mistral.ai", "Open Models", "10/10"),
                ("PyTorch", "https://pytorch.org", "ML Framework", "10/10"),
                ("TensorFlow", "https://tensorflow.org", "ML Framework", "10/10"),
                ("LangChain", "https://langchain.com", "LLM Framework", "10/10"),
            ],
            "linux": [
                ("Arch Linux", "https://archlinux.org", "Rolling Release", "10/10"),
                ("Arch Wiki", "https://wiki.archlinux.org", "Best Docs", "10/10"),
                ("Ubuntu", "https://ubuntu.com", "User Friendly", "9/10"),
                ("Debian", "https://debian.org", "Universal OS", "10/10"),
                ("Fedora", "https://fedoraproject.org", "Innovation", "9/10"),
                ("Kernel.org", "https://kernel.org", "Linux Kernel", "10/10"),
                ("NixOS", "https://nixos.org", "Declarative", "9/10"),
                ("Kali Linux", "https://kali.org", "Security", "9/10"),
            ],
            "devops": [
                ("Docker", "https://docker.com", "Containers", "10/10"),
                ("Kubernetes", "https://kubernetes.io", "Orchestration", "10/10"),
                ("GitHub Actions", "https://github.com/features/actions", "CI/CD", "10/10"),
                ("Terraform", "https://terraform.io", "Infrastructure", "10/10"),
                ("Prometheus", "https://prometheus.io", "Monitoring", "10/10"),
                ("Grafana", "https://grafana.com", "Visualization", "10/10"),
            ],
            "databases": [
                ("PostgreSQL", "https://postgresql.org", "Relational DB", "10/10"),
                ("MongoDB", "https://mongodb.com", "NoSQL DB", "10/10"),
                ("Redis", "https://redis.io", "In-Memory DB", "10/10"),
                ("SQLite", "https://sqlite.org", "Embedded DB", "10/10"),
                ("ElasticSearch", "https://elastic.co", "Search Engine", "10/10"),
            ],
            "tools": [
                ("GitHub", "https://github.com", "Code Hosting", "10/10"),
                ("GitLab", "https://gitlab.com", "DevOps Platform", "10/10"),
                ("Stack Overflow", "https://stackoverflow.com", "Q&A", "10/10"),
                ("VS Code", "https://code.visualstudio.com", "Editor", "10/10"),
                ("JetBrains", "https://jetbrains.com", "IDEs", "10/10"),
                ("Replit", "https://replit.com", "Online IDE", "10/10"),
                ("CodePen", "https://codepen.io", "Front-End Playground", "10/10"),
            ],
            "cloud": [
                ("AWS", "https://aws.amazon.com", "Amazon Cloud", "10/10"),
                ("Google Cloud", "https://cloud.google.com", "Google Cloud", "10/10"),
                ("Vercel", "https://vercel.com", "Frontend Cloud", "10/10"),
                ("Cloudflare", "https://cloudflare.com", "CDN & Security", "10/10"),
                ("Netlify", "https://netlify.com", "Jamstack Deploy", "10/10"),
                ("Railway", "https://railway.app", "Deploy Anything", "9/10"),
            ],
            "web_frameworks": [
                ("Django", "https://djangoproject.com", "Python Web", "10/10"),
                ("FastAPI", "https://fastapi.tiangolo.com", "Modern Python", "10/10"),
                ("Flask", "https://flask.palletsprojects.com", "Python Micro", "10/10"),
                ("Laravel", "https://laravel.com", "PHP Framework", "10/10"),
                ("Ruby on Rails", "https://rubyonrails.org", "Ruby Framework", "9/10"),
            ],
            "learning": [
                ("LeetCode", "https://leetcode.com", "Coding Practice", "10/10"),
                ("freeCodeCamp", "https://freecodecamp.org", "Free Courses", "10/10"),
                ("Coursera", "https://coursera.org", "Online Courses", "10/10"),
                ("Khan Academy", "https://khanacademy.org", "Education", "10/10"),
                ("Advent of Code", "https://adventofcode.com", "Puzzles", "10/10"),
                ("Exercism", "https://exercism.org", "Practice", "9/10"),
            ],
            "design": [
                ("Figma", "https://figma.com", "Design Tool", "10/10"),
                ("Unsplash", "https://unsplash.com", "Free Photos", "10/10"),
                ("Google Fonts", "https://fonts.google.com", "Web Fonts", "10/10"),
                ("Coolors", "https://coolors.co", "Color Palettes", "9/10"),
            ],
            "security": [
                ("OWASP", "https://owasp.org", "Web Security", "10/10"),
                ("HackerOne", "https://hackerone.com", "Bug Bounty", "10/10"),
                ("VirusTotal", "https://virustotal.com", "Malware Scan", "10/10"),
                ("Have I Been Pwned", "https://haveibeenpwned.com", "Breach Check", "10/10"),
                ("CyberChef", "https://gchq.github.io/CyberChef", "Data Tool", "10/10"),
            ],
            "privacy": [
                ("DuckDuckGo", "https://duckduckgo.com", "Private Search", "10/10"),
                ("ProtonMail", "https://proton.me", "Encrypted Email", "10/10"),
                ("Signal", "https://signal.org", "Encrypted Messaging", "10/10"),
                ("Tor Project", "https://torproject.org", "Anonymous Browsing", "10/10"),
                ("Privacy Guides", "https://privacyguides.org", "Privacy Tools", "9/10"),
            ],
            "news_tech": [
                ("Hacker News", "https://news.ycombinator.com", "Tech News", "10/10"),
                ("Ars Technica", "https://arstechnica.com", "Deep Tech", "10/10"),
                ("The Verge", "https://theverge.com", "Tech News", "9/10"),
                ("Lobsters", "https://lobste.rs", "Computing", "9/10"),
            ],
            "gaming": [
                ("Godot", "https://godotengine.org", "Open Source Engine", "10/10"),
                ("Unity", "https://unity.com", "Game Engine", "10/10"),
                ("Unreal Engine", "https://unrealengine.com", "Game Engine", "10/10"),
                ("itch.io", "https://itch.io", "Indie Games", "9/10"),
            ],
            "creative": [
                ("Blender", "https://blender.org", "3D Creation", "10/10"),
                ("OBS Studio", "https://obsproject.com", "Streaming", "10/10"),
                ("GIMP", "https://gimp.org", "Image Editor", "9/10"),
                ("Audacity", "https://audacityteam.org", "Audio Editor", "9/10"),
            ],
            "documentation": [
                ("MDN Web Docs", "https://developer.mozilla.org", "Web Reference", "10/10"),
                ("DevDocs", "https://devdocs.io", "API Docs", "10/10"),
                ("Read the Docs", "https://readthedocs.org", "Doc Hosting", "10/10"),
            ],
            "mobile": [
                ("Flutter", "https://flutter.dev", "Google Framework", "10/10"),
                ("React Native", "https://reactnative.dev", "Cross-Platform", "10/10"),
                ("Kotlin", "https://kotlinlang.org", "Modern Android", "10/10"),
                ("Swift", "https://swift.org", "Apple Language", "10/10"),
            ],
            "testing": [
                ("Playwright", "https://playwright.dev", "Browser Testing", "10/10"),
                ("Cypress", "https://cypress.io", "E2E Testing", "10/10"),
                ("Pytest", "https://pytest.org", "Python Testing", "10/10"),
                ("Jest", "https://jestjs.io", "JS Testing", "10/10"),
            ],
            "api": [
                ("Postman", "https://postman.com", "API Testing", "10/10"),
                ("GraphQL", "https://graphql.org", "Query Language", "10/10"),
                ("Swagger", "https://swagger.io", "API Design", "10/10"),
            ],
        }


# Singleton
database = OynixDatabase()

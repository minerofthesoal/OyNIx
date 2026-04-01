"""
OyNIx Browser - Nyx Search Engine
Custom search engine with auto-indexing as you browse.

Features:
- Auto-indexes every page you visit (title, URL, content snippets)
- Full-text search across your browsing index using Whoosh
- Falls back to DuckDuckGo for web results
- Combined local index + web results in Nyx theme
- Database export for GitHub site database upload
"""

import os
import json
import time
import sqlite3
import hashlib
import threading
from urllib.parse import quote_plus, urlparse

from PyQt6.QtCore import QThread, pyqtSignal, QObject

# Try importing Whoosh for full-text search; fall back to SQLite FTS
try:
    from whoosh.index import create_in, open_dir, exists_in
    from whoosh.fields import Schema, TEXT, ID, STORED, DATETIME, NUMERIC
    from whoosh.qparser import MultifieldParser, OrGroup
    from whoosh import scoring
    HAS_WHOOSH = True
except ImportError:
    HAS_WHOOSH = False

try:
    import requests
    from bs4 import BeautifulSoup
    HAS_REQUESTS = True
except ImportError:
    HAS_REQUESTS = False

from .database import database


# ── Index directory ───────────────────────────────────────────────────
INDEX_DIR = os.path.join(os.path.expanduser("~"), ".config", "oynix", "search_index")
DB_PATH = os.path.join(os.path.expanduser("~"), ".config", "oynix", "nyx_index.db")


class NyxIndexer:
    """
    Maintains a local search index of every page the user visits.
    Uses Whoosh for full-text search if available, else SQLite FTS5.
    """

    def __init__(self):
        os.makedirs(INDEX_DIR, exist_ok=True)
        self._lock = threading.Lock()

        if HAS_WHOOSH:
            self._init_whoosh()
        else:
            self._init_sqlite()

    # ── Whoosh backend ────────────────────────────────────────────────

    def _init_whoosh(self):
        self.schema = Schema(
            url=ID(stored=True, unique=True),
            title=TEXT(stored=True),
            content=TEXT(stored=True),
            domain=TEXT(stored=True),
            timestamp=STORED,
            visit_count=STORED,
        )
        if exists_in(INDEX_DIR):
            self.ix = open_dir(INDEX_DIR)
        else:
            self.ix = create_in(INDEX_DIR, self.schema)

    def _init_sqlite(self):
        self.conn = sqlite3.connect(DB_PATH, check_same_thread=False)
        self.conn.execute("""
            CREATE TABLE IF NOT EXISTS pages (
                url TEXT PRIMARY KEY,
                title TEXT,
                content TEXT,
                domain TEXT,
                timestamp REAL,
                visit_count INTEGER DEFAULT 1
            )
        """)
        # Try FTS5
        try:
            self.conn.execute("""
                CREATE VIRTUAL TABLE IF NOT EXISTS pages_fts
                USING fts5(url, title, content, domain, content=pages)
            """)
            self._has_fts = True
        except Exception:
            self._has_fts = False
        self.conn.commit()

    def index_page(self, url, title, content_snippet=""):
        """Add or update a page in the search index."""
        if not url or not title:
            return

        domain = urlparse(url).netloc
        timestamp = time.time()

        try:
            with self._lock:
                if HAS_WHOOSH:
                    writer = self.ix.writer()
                    writer.update_document(
                        url=url,
                        title=title,
                        content=content_snippet[:2000],
                        domain=domain,
                        timestamp=timestamp,
                        visit_count=1,
                    )
                    writer.commit()
                else:
                    self.conn.execute("""
                        INSERT OR REPLACE INTO pages
                        (url, title, content, domain, timestamp, visit_count)
                        VALUES (?, ?, ?, ?, ?, COALESCE(
                            (SELECT visit_count FROM pages WHERE url=?) + 1, 1
                        ))
                    """, (url, title, content_snippet[:2000], domain,
                          timestamp, url))
                    if self._has_fts:
                        self.conn.execute("""
                            INSERT OR REPLACE INTO pages_fts
                            (url, title, content, domain)
                            VALUES (?, ?, ?, ?)
                        """, (url, title, content_snippet[:2000], domain))
                    self.conn.commit()
        except Exception as e:
            print(f"[NyxIndex] Error indexing {url}: {e}")

    def search(self, query, limit=20):
        """Search the local index. Returns list of dicts."""
        results = []
        try:
            with self._lock:
                if HAS_WHOOSH:
                    with self.ix.searcher(weighting=scoring.BM25F()) as searcher:
                        parser = MultifieldParser(
                            ["title", "content", "domain"],
                            schema=self.ix.schema,
                            group=OrGroup,
                        )
                        q = parser.parse(query)
                        hits = searcher.search(q, limit=limit)
                        for hit in hits:
                            results.append({
                                'url': hit['url'],
                                'title': hit['title'],
                                'description': (hit.get('content', '')[:200]
                                                or hit['title']),
                                'source': 'nyx_index',
                            })
                else:
                    if self._has_fts:
                        rows = self.conn.execute("""
                            SELECT url, title, content FROM pages_fts
                            WHERE pages_fts MATCH ? LIMIT ?
                        """, (query, limit)).fetchall()
                    else:
                        like = f"%{query}%"
                        rows = self.conn.execute("""
                            SELECT url, title, content FROM pages
                            WHERE title LIKE ? OR content LIKE ? OR url LIKE ?
                            ORDER BY visit_count DESC, timestamp DESC
                            LIMIT ?
                        """, (like, like, like, limit)).fetchall()

                    for url, title, content in rows:
                        results.append({
                            'url': url,
                            'title': title,
                            'description': (content[:200] if content
                                            else title),
                            'source': 'nyx_index',
                        })
        except Exception as e:
            print(f"[NyxSearch] Search error: {e}")

        return results

    def get_index_stats(self):
        """Get stats about the index."""
        try:
            if HAS_WHOOSH:
                with self.ix.searcher() as s:
                    return {'total_pages': s.doc_count()}
            else:
                count = self.conn.execute(
                    "SELECT COUNT(*) FROM pages"
                ).fetchone()[0]
                return {'total_pages': count}
        except Exception:
            return {'total_pages': 0}

    def export_index(self, filepath):
        """Export the index as JSON for GitHub upload."""
        data = []
        try:
            if HAS_WHOOSH:
                with self.ix.searcher() as s:
                    for i in range(s.doc_count()):
                        doc = s.stored_fields(i)
                        data.append(doc)
            else:
                rows = self.conn.execute(
                    "SELECT url, title, content, domain, timestamp, visit_count FROM pages"
                ).fetchall()
                for url, title, content, domain, ts, vc in rows:
                    data.append({
                        'url': url, 'title': title,
                        'content': content[:500], 'domain': domain,
                        'timestamp': ts, 'visit_count': vc,
                    })

            with open(filepath, 'w') as f:
                json.dump(data, f, indent=2, default=str)
            return True
        except Exception as e:
            print(f"[NyxIndex] Export error: {e}")
            return False


class DuckDuckGoFetcher(QThread):
    """Fetch search results from DuckDuckGo in the background."""
    results_ready = pyqtSignal(list)

    def __init__(self, query):
        super().__init__()
        self.query = query

    def run(self):
        if not HAS_REQUESTS:
            self.results_ready.emit([])
            return

        try:
            results = []
            headers = {
                'User-Agent': ('Mozilla/5.0 (X11; Linux x86_64; rv:120.0) '
                               'Gecko/20100101 Firefox/120.0')
            }
            url = f"https://html.duckduckgo.com/html/?q={quote_plus(self.query)}"
            resp = requests.get(url, headers=headers, timeout=8)
            soup = BeautifulSoup(resp.text, 'html.parser')

            for item in soup.select('.result')[:15]:
                title_el = item.select_one('.result__a')
                snippet_el = item.select_one('.result__snippet')
                if title_el:
                    title = title_el.get_text(strip=True)
                    link = title_el.get('href', '')
                    # DDG wraps links; try to extract real URL
                    if 'uddg=' in link:
                        from urllib.parse import unquote, parse_qs
                        params = parse_qs(urlparse(link).query)
                        link = unquote(params.get('uddg', [link])[0])
                    desc = (snippet_el.get_text(strip=True)
                            if snippet_el else '')
                    if link.startswith('http'):
                        results.append({
                            'url': link,
                            'title': title,
                            'description': desc,
                            'source': 'duckduckgo',
                        })

            self.results_ready.emit(results)
        except Exception as e:
            print(f"[DuckDuckGo] Fetch error: {e}")
            self.results_ready.emit([])


class NyxSearchEngine(QObject):
    """
    The Nyx Search Engine orchestrator.
    Combines:
     1. Local Nyx auto-index
     2. Curated site database (1400+)
     3. DuckDuckGo web results
    """

    web_results_ready = pyqtSignal(list)

    def __init__(self):
        super().__init__()
        self.indexer = NyxIndexer()
        self.database = database
        self._ddg_thread = None
        self.auto_expand_database = True  # Controlled by settings

    def index_page(self, url, title, content_snippet=""):
        """Auto-index a visited page (called by browser on page load)."""
        threading.Thread(
            target=self.indexer.index_page,
            args=(url, title, content_snippet),
            daemon=True,
        ).start()

    def search(self, query, engine='nyx'):
        """
        Perform a search.

        Args:
            query: Search query
            engine: 'nyx' (local + DDG), 'duckduckgo', 'google', 'brave'

        Returns:
            dict with 'local', 'database', keys.
            Web results arrive async via web_results_ready signal.
        """
        local_results = []
        db_results = []

        if engine in ('nyx', 'duckduckgo'):
            # Search local Nyx index
            local_results = self.indexer.search(query)

            # Search curated database
            raw_db = self.database.search(query)
            for title, url, desc, rating, category in raw_db:
                db_results.append({
                    'url': url,
                    'title': title,
                    'description': desc,
                    'rating': rating,
                    'category': category,
                    'source': 'database',
                })

        # Start web search in background
        if engine == 'nyx' or engine == 'duckduckgo':
            self._ddg_thread = DuckDuckGoFetcher(query)
            self._ddg_thread.results_ready.connect(
                self._on_web_results)
            self._ddg_thread.start()

        return {
            'local': local_results,
            'database': db_results,
        }

    def get_web_search_url(self, query, engine='duckduckgo'):
        """Get a direct web search URL for external engines."""
        engines = {
            'duckduckgo': f'https://duckduckgo.com/?q={quote_plus(query)}',
            'google': f'https://www.google.com/search?q={quote_plus(query)}',
            'brave': f'https://search.brave.com/search?q={quote_plus(query)}',
            'bing': f'https://www.bing.com/search?q={quote_plus(query)}',
        }
        return engines.get(engine, engines['duckduckgo'])

    def _on_web_results(self, results):
        """Forward web results via signal and auto-index them."""
        self.web_results_ready.emit(results)
        # Auto-index web results into Nyx index
        for r in results:
            self.indexer.index_page(r['url'], r['title'], r.get('description', ''))
        # Auto-expand database with DDG results (if enabled)
        if self.auto_expand_database:
            self.database.add_sites_batch(results, source="duckduckgo")

    def get_stats(self):
        """Get search engine statistics."""
        idx_stats = self.indexer.get_index_stats()
        return {
            'indexed_pages': idx_stats['total_pages'],
            'database_sites': self.database.total_sites,
            'backend': 'Whoosh' if HAS_WHOOSH else 'SQLite FTS',
        }

    def export_index(self, filepath):
        """Export Nyx index for GitHub upload."""
        return self.indexer.export_index(filepath)


# Singleton
nyx_search = NyxSearchEngine()

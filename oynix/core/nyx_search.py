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
        # Try FTS5 — use content='' (standalone) to avoid external content sync issues
        try:
            self.conn.execute("""
                CREATE VIRTUAL TABLE IF NOT EXISTS pages_fts
                USING fts5(url, title, content, domain)
            """)
            self._has_fts = True
        except Exception:
            # FTS5 may be unavailable or corrupted — try rebuilding
            try:
                self.conn.execute("DROP TABLE IF EXISTS pages_fts")
                self.conn.execute("""
                    CREATE VIRTUAL TABLE pages_fts
                    USING fts5(url, title, content, domain)
                """)
                # Re-populate from pages table
                self.conn.execute("""
                    INSERT INTO pages_fts (url, title, content, domain)
                    SELECT url, title, content, domain FROM pages
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
                        try:
                            # Delete old FTS entry if exists, then insert new
                            self.conn.execute(
                                "DELETE FROM pages_fts WHERE url = ?", (url,))
                            self.conn.execute("""
                                INSERT INTO pages_fts
                                (url, title, content, domain)
                                VALUES (?, ?, ?, ?)
                            """, (url, title, content_snippet[:2000], domain))
                        except Exception:
                            pass  # FTS insert failure is non-critical
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
                    rows = None
                    if self._has_fts:
                        try:
                            rows = self.conn.execute("""
                                SELECT url, title, content FROM pages_fts
                                WHERE pages_fts MATCH ? LIMIT ?
                            """, (query, limit)).fetchall()
                        except Exception:
                            # FTS5 corrupted — rebuild and fall back to LIKE
                            self._rebuild_fts()
                            rows = None
                    if rows is None:
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

    def _rebuild_fts(self):
        """Drop and rebuild FTS5 index from the pages table."""
        try:
            self.conn.execute("DROP TABLE IF EXISTS pages_fts")
            self.conn.execute("""
                CREATE VIRTUAL TABLE pages_fts
                USING fts5(url, title, content, domain)
            """)
            self.conn.execute("""
                INSERT INTO pages_fts (url, title, content, domain)
                SELECT url, title, content, domain FROM pages
            """)
            self.conn.commit()
            self._has_fts = True
            print("[NyxSearch] FTS5 index rebuilt successfully")
        except Exception as e:
            self._has_fts = False
            print(f"[NyxSearch] FTS5 rebuild failed, using LIKE fallback: {e}")

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


class WebResultsFetcher(QThread):
    """Fetch supplemental web results from DDG/Google in the background.
    Never navigates to external search sites — only scrapes result cards."""
    results_ready = pyqtSignal(list)

    def __init__(self, query):
        super().__init__()
        self.query = query

    def run(self):
        if not HAS_REQUESTS:
            self.results_ready.emit([])
            return

        # Fetch from BOTH DDG and Google for maximum coverage
        results = self._fetch_ddg()
        google_results = self._fetch_google()
        # Merge, dedup by URL
        seen = {r['url'].rstrip('/').lower() for r in results}
        for r in google_results:
            key = r['url'].rstrip('/').lower()
            if key not in seen:
                results.append(r)
                seen.add(key)
        self.results_ready.emit(results)

    def _fetch_ddg(self):
        """Try DuckDuckGo HTML endpoint."""
        try:
            headers = {
                'User-Agent': ('Mozilla/5.0 (X11; Linux x86_64; rv:120.0) '
                               'Gecko/20100101 Firefox/120.0')
            }
            url = f"https://html.duckduckgo.com/html/?q={quote_plus(self.query)}"
            resp = requests.get(url, headers=headers, timeout=8)
            soup = BeautifulSoup(resp.text, 'html.parser')
            results = []

            for item in soup.select('.result')[:30]:
                title_el = item.select_one('.result__a')
                snippet_el = item.select_one('.result__snippet')
                if title_el:
                    title = title_el.get_text(strip=True)
                    link = title_el.get('href', '')
                    if 'uddg=' in link:
                        from urllib.parse import unquote, parse_qs as _pqs
                        params = _pqs(urlparse(link).query)
                        link = unquote(params.get('uddg', [link])[0])
                    desc = (snippet_el.get_text(strip=True)
                            if snippet_el else '')
                    if link.startswith('http'):
                        results.append({
                            'url': link,
                            'title': title,
                            'description': desc,
                            'source': 'web',
                        })
            return results
        except Exception as e:
            print(f"[Nyx] DDG fetch failed: {e}")
            return []

    def _fetch_google(self):
        """Fallback: try Google search scrape."""
        try:
            headers = {
                'User-Agent': ('Mozilla/5.0 (X11; Linux x86_64; rv:120.0) '
                               'Gecko/20100101 Firefox/120.0'),
                'Accept-Language': 'en-US,en;q=0.9',
            }
            url = f"https://www.google.com/search?q={quote_plus(self.query)}&hl=en"
            resp = requests.get(url, headers=headers, timeout=8)
            soup = BeautifulSoup(resp.text, 'html.parser')
            results = []

            for div in soup.select('div.g')[:30]:
                a_tag = div.select_one('a[href]')
                title_el = div.select_one('h3')
                if a_tag and title_el:
                    link = a_tag.get('href', '')
                    title = title_el.get_text(strip=True)
                    # Extract snippet
                    desc = ''
                    for span in div.select('span'):
                        text = span.get_text(strip=True)
                        if len(text) > 40:
                            desc = text
                            break
                    if link.startswith('http'):
                        results.append({
                            'url': link,
                            'title': title,
                            'description': desc,
                            'source': 'web',
                        })
            return results
        except Exception as e:
            print(f"[Nyx] Google fetch failed: {e}")
            return []


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
        self._web_thread = None
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
            dict with 'local', 'database', 'relevant_sites' keys.
            Web results arrive async via web_results_ready signal.
        """
        local_results = []
        db_results = []
        relevant_sites = []

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

            # Filter and rank: deduplicate, score by relevance
            local_results = self._rank_and_filter(local_results, query)
            db_results = self._rank_and_filter(db_results, query)

            # Find relevant sites from DB that relate to the query topic
            relevant_sites = self._find_relevant_sites(query, db_results)

        # Start web search in background (supplements Nyx results, never navigates)
        if engine in ('nyx', 'duckduckgo'):
            self._web_thread = WebResultsFetcher(query)
            self._web_thread.results_ready.connect(
                self._on_web_results)
            self._web_thread.start()

        return {
            'local': local_results,
            'database': db_results,
            'relevant_sites': relevant_sites,
        }

    def _rank_and_filter(self, results, query):
        """Score, deduplicate, and rank results by relevance to query."""
        if not results:
            return results

        query_terms = set(query.lower().split())
        seen_urls = set()
        scored = []

        for r in results:
            url = r.get('url', '')
            if url in seen_urls:
                continue
            seen_urls.add(url)

            score = 0.0
            title = (r.get('title') or '').lower()
            desc = (r.get('description') or '').lower()
            domain = urlparse(url).netloc.lower()

            for term in query_terms:
                # Title match (highest weight)
                if term in title:
                    score += 10.0
                    if title.startswith(term):
                        score += 5.0
                # Domain match
                if term in domain:
                    score += 8.0
                # Description match
                if term in desc:
                    score += 3.0
                # URL path match
                if term in url.lower():
                    score += 2.0

            # Boost rated sites
            rating = r.get('rating', '')
            if rating:
                try:
                    score += float(rating) * 2
                except (ValueError, TypeError):
                    pass

            r['_score'] = score
            scored.append(r)

        # Sort by score descending
        scored.sort(key=lambda x: x.get('_score', 0), reverse=True)
        # Remove internal score key
        for r in scored:
            r.pop('_score', None)
        return scored

    def _find_relevant_sites(self, query, already_found):
        """Find related sites from the database that the user might want."""
        already_urls = {r['url'] for r in already_found}
        relevant = []

        # Extract likely categories from query
        query_lower = query.lower()
        category_hints = {
            'python': 'development', 'javascript': 'development', 'code': 'development',
            'programming': 'development', 'github': 'development', 'api': 'development',
            'react': 'development', 'rust': 'development', 'linux': 'development',
            'news': 'news', 'reddit': 'social', 'twitter': 'social',
            'youtube': 'media', 'video': 'media', 'music': 'media',
            'email': 'productivity', 'docs': 'productivity', 'office': 'productivity',
            'game': 'gaming', 'steam': 'gaming',
            'shop': 'shopping', 'buy': 'shopping', 'amazon': 'shopping',
            'learn': 'education', 'course': 'education', 'tutorial': 'education',
            'ai': 'ai_ml', 'machine learning': 'ai_ml', 'chatgpt': 'ai_ml',
        }

        matched_categories = set()
        for keyword, cat in category_hints.items():
            if keyword in query_lower:
                matched_categories.add(cat)

        if matched_categories:
            all_sites = self.database.get_all_sites()
            for site in all_sites:
                url = site.get('url', '')
                if url in already_urls:
                    continue
                cat = site.get('category', '').lower()
                if cat in matched_categories:
                    relevant.append({
                        'url': url,
                        'title': site.get('title', urlparse(url).netloc),
                        'description': site.get('description', ''),
                        'category': site.get('category', ''),
                        'source': 'related',
                    })
                    if len(relevant) >= 25:
                        break

        return relevant

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
            self.database.add_sites_batch(results, source="web")

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


# Lazy singleton - must not create QObject before QApplication exists
_nyx_search = None

def _get_nyx_search():
    global _nyx_search
    if _nyx_search is None:
        _nyx_search = NyxSearchEngine()
    return _nyx_search

class _NyxSearchProxy:
    """Proxy that delays NyxSearchEngine creation until first access."""
    def __getattr__(self, name):
        return getattr(_get_nyx_search(), name)

nyx_search = _NyxSearchProxy()

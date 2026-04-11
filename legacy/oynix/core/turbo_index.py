"""
OyNIx Browser - Turbo Indexer Python Bridge
Loads the C++ turbo_index native library for high-performance
tokenization, n-gram extraction, and BM25 scoring.
Falls back to pure Python if the native lib isn't available.

Usage:
    from oynix.core.turbo_index import turbo

    tokens = turbo.tokenize("some text here")
    score  = turbo.score("query", "document text", title="Page Title")
    results = turbo.batch_score("query", docs_list)
"""

import ctypes
import json
import os
import sys
import math
import re
from pathlib import Path


class TurboIndexer:
    """Wrapper around the C++ turbo_index native library."""

    def __init__(self):
        self._lib = None
        self._native = False
        self._load_native()

    def _load_native(self):
        """Try to load the native turbo_index shared library."""
        search_paths = [
            # Development build
            Path(__file__).parent.parent.parent / "build" / "libturbo_index.so",
            Path(__file__).parent.parent.parent / "build" / "libturbo_index.dylib",
            # Installed locations
            Path("/usr/lib/oynix/build/libturbo_index.so"),
            Path("/app/lib/oynix/build/libturbo_index.so"),  # Flatpak
            # Relative to package
            Path(__file__).parent.parent / "build" / "libturbo_index.so",
            # Same directory
            Path(__file__).parent / "libturbo_index.so",
            # Windows
            Path(__file__).parent.parent.parent / "build" / "turbo_index.dll",
        ]

        for path in search_paths:
            if path.exists():
                try:
                    self._lib = ctypes.CDLL(str(path))
                    self._setup_functions()
                    self._native = True
                    return
                except OSError:
                    continue

        # Try system library path
        for name in ["libturbo_index.so", "libturbo_index.dylib", "turbo_index.dll"]:
            try:
                self._lib = ctypes.CDLL(name)
                self._setup_functions()
                self._native = True
                return
            except OSError:
                continue

    def _setup_functions(self):
        """Set up ctypes function signatures."""
        lib = self._lib

        lib.turbo_tokenize.argtypes = [ctypes.c_char_p, ctypes.c_int]
        lib.turbo_tokenize.restype = ctypes.c_char_p

        lib.turbo_ngrams.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_int]
        lib.turbo_ngrams.restype = ctypes.c_char_p

        lib.turbo_score.argtypes = [
            ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int,
            ctypes.c_char_p, ctypes.c_char_p
        ]
        lib.turbo_score.restype = ctypes.c_double

        lib.turbo_batch_score.argtypes = [
            ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_int
        ]
        lib.turbo_batch_score.restype = ctypes.c_char_p

        lib.turbo_normalize_url.argtypes = [ctypes.c_char_p]
        lib.turbo_normalize_url.restype = ctypes.c_char_p

        lib.turbo_extract_domain.argtypes = [ctypes.c_char_p]
        lib.turbo_extract_domain.restype = ctypes.c_char_p

        lib.turbo_cpu_threads.argtypes = []
        lib.turbo_cpu_threads.restype = ctypes.c_int

        lib.turbo_version.argtypes = []
        lib.turbo_version.restype = ctypes.c_char_p

    @property
    def is_native(self):
        return self._native

    @property
    def version(self):
        if self._native:
            return self._lib.turbo_version().decode('utf-8')
        return "2.1.0-python-fallback"

    @property
    def cpu_threads(self):
        if self._native:
            return self._lib.turbo_cpu_threads()
        return os.cpu_count() or 1

    def tokenize(self, text):
        """Tokenize text and return list of {t, c, tf, pos} dicts."""
        if self._native:
            result = self._lib.turbo_tokenize(
                text.encode('utf-8'), len(text.encode('utf-8')))
            return json.loads(result.decode('utf-8'))
        return self._py_tokenize(text)

    def ngrams(self, text, n=2):
        """Extract n-grams from text."""
        if self._native:
            result = self._lib.turbo_ngrams(
                text.encode('utf-8'), len(text.encode('utf-8')), n)
            return json.loads(result.decode('utf-8'))
        return self._py_ngrams(text, n)

    def score(self, query, doc_text, title=None, url=None):
        """Score a document against a query."""
        if self._native:
            return self._lib.turbo_score(
                query.encode('utf-8'),
                doc_text.encode('utf-8'),
                len(doc_text.encode('utf-8')),
                title.encode('utf-8') if title else None,
                url.encode('utf-8') if url else None)
        return self._py_score(query, doc_text, title, url)

    def batch_score(self, query, docs, threads=0, top_n=50):
        """
        Batch-score documents against a query.

        Args:
            query: Search query string
            docs: List of dicts with 'id', 'text', 'title', 'url' keys
            threads: Number of threads (0 = auto)
            top_n: Return top N results

        Returns:
            List of {id, score} dicts sorted by score descending
        """
        if self._native:
            docs_json = json.dumps(docs).encode('utf-8')
            result = self._lib.turbo_batch_score(
                query.encode('utf-8'), docs_json, threads, top_n)
            return json.loads(result.decode('utf-8'))
        return self._py_batch_score(query, docs, top_n)

    def normalize_url(self, url):
        """Normalize a URL (strip protocol, www, trailing slash)."""
        if self._native:
            result = self._lib.turbo_normalize_url(url.encode('utf-8'))
            return result.decode('utf-8')
        return self._py_normalize_url(url)

    def extract_domain(self, url):
        """Extract domain from a URL."""
        if self._native:
            result = self._lib.turbo_extract_domain(url.encode('utf-8'))
            return result.decode('utf-8')
        return self._py_extract_domain(url)

    # ── Pure Python Fallbacks ──────────────────────────────────────

    _STOP_WORDS = {
        'a', 'an', 'the', 'is', 'it', 'in', 'on', 'at', 'to', 'of',
        'and', 'or', 'not', 'for', 'with', 'as', 'by', 'this', 'that',
        'from', 'be', 'are', 'was', 'were', 'been', 'have', 'has', 'had',
        'do', 'does', 'did', 'will', 'would', 'could', 'should', 'may',
        'might', 'can', 'but', 'if', 'so', 'than', 'too', 'very', 'just',
        'about', 'up', 'out', 'no', 'all', 'any', 'each', 'every', 'both',
        'few', 'more', 'most', 'other', 'some', 'such', 'only', 'own',
        'same', 'into', 'over', 'after', 'before', 'between', 'under',
        'again', 'then', 'once', 'here', 'there', 'when', 'where', 'why',
        'how', 'what', 'which', 'who', 'whom', 'its', 'his', 'her', 'my',
        'your', 'our', 'their', 'he', 'she', 'we', 'they', 'me', 'him',
        'us', 'them', 'i', 'you', 'being',
    }

    def _py_tokenize_raw(self, text):
        words = re.findall(r'[a-zA-Z0-9]+', text.lower())
        return [w for w in words if 2 <= len(w) <= 40 and w not in self._STOP_WORDS]

    def _py_tokenize(self, text):
        tokens = self._py_tokenize_raw(text)
        counts = {}
        for i, t in enumerate(tokens):
            if t not in counts:
                counts[t] = {'t': t, 'c': 0, 'pos': i}
            counts[t]['c'] += 1
        total = max(len(tokens), 1)
        result = list(counts.values())
        for r in result:
            r['tf'] = r['c'] / total
        result.sort(key=lambda x: -x['c'])
        return result

    def _py_ngrams(self, text, n):
        tokens = self._py_tokenize_raw(text)
        if len(tokens) < n:
            return []
        counts = {}
        for i in range(len(tokens) - n + 1):
            ng = ' '.join(tokens[i:i+n])
            counts[ng] = counts.get(ng, 0) + 1
        result = [{'ng': ng, 'c': c} for ng, c in counts.items() if c >= 1]
        result.sort(key=lambda x: -x['c'])
        return result

    def _py_score(self, query, doc_text, title=None, url=None):
        q_tokens = self._py_tokenize_raw(query)
        d_tokens = self._py_tokenize_raw(doc_text)
        if not q_tokens or not d_tokens:
            return 0.0

        tf = {}
        for t in d_tokens:
            tf[t] = tf.get(t, 0) + 1

        k1, b = 1.5, 0.75
        avg_dl = 500.0
        dl = len(d_tokens)
        score = 0.0

        for qt in q_tokens:
            if qt not in tf:
                continue
            f = tf[qt]
            score += (f * (k1 + 1)) / (f + k1 * (1 - b + b * dl / avg_dl))

        # Title/URL bonus
        if title:
            tl = title.lower()
            for qt in q_tokens:
                if qt in tl:
                    score += 3.0
        if url:
            ul = url.lower()
            for qt in q_tokens:
                if qt in ul:
                    score += 1.5

        return score

    def _py_batch_score(self, query, docs, top_n):
        results = []
        for doc in docs:
            s = self._py_score(
                query, doc.get('text', ''),
                doc.get('title'), doc.get('url'))
            if s > 0.01:
                results.append({'id': doc.get('id', 0), 'score': s})
        results.sort(key=lambda x: -x['score'])
        return results[:top_n]

    def _py_normalize_url(self, url):
        s = url
        for prefix in ('https://', 'http://', '//'):
            if s.startswith(prefix):
                s = s[len(prefix):]
                break
        if s.startswith('www.'):
            s = s[4:]
        s = s.rstrip('/')
        return s.lower()

    def _py_extract_domain(self, url):
        s = self._py_normalize_url(url)
        s = s.split('/')[0]
        s = s.split(':')[0]
        return s


# Singleton instance
turbo = TurboIndexer()

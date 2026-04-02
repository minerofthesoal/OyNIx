"""
OyNIx Browser - Native C++ Bridge
Loads the fast_index shared library via ctypes for accelerated
text processing. Falls back to pure Python if unavailable.

Coded by Claude (Anthropic)
"""

import os
import ctypes
import json

# Paths to search for the native library
_LIB_SEARCH = [
    os.path.join(os.path.dirname(__file__), '..', '..', 'build', 'libnyx_index.so'),
    os.path.join(os.path.dirname(__file__), '..', '..', 'build', 'libnyx_index.dylib'),
    '/usr/lib/oynix/build/libnyx_index.so',
    '/usr/local/lib/oynix/libnyx_index.so',
]

_lib = None


def _load_lib():
    """Try to load the native library."""
    global _lib
    if _lib is not None:
        return _lib

    for path in _LIB_SEARCH:
        path = os.path.abspath(path)
        if os.path.isfile(path):
            try:
                _lib = ctypes.CDLL(path)
                # Set up function signatures
                _lib.nyx_tokenize.restype = ctypes.c_char_p
                _lib.nyx_tokenize.argtypes = [ctypes.c_char_p, ctypes.c_int]

                _lib.nyx_score.restype = ctypes.c_double
                _lib.nyx_score.argtypes = [
                    ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]

                _lib.nyx_normalize_url.restype = ctypes.c_char_p
                _lib.nyx_normalize_url.argtypes = [ctypes.c_char_p]

                _lib.nyx_extract_domain.restype = ctypes.c_char_p
                _lib.nyx_extract_domain.argtypes = [ctypes.c_char_p]

                _lib.nyx_native_version.restype = ctypes.c_char_p
                return _lib
            except OSError:
                continue
    return None


def has_native():
    """Check if native library is available."""
    return _load_lib() is not None


def native_version():
    """Get native library version."""
    lib = _load_lib()
    if lib:
        return lib.nyx_native_version().decode()
    return None


def fast_tokenize(text):
    """
    Tokenize text using native C++ (10x faster) or fall back to Python.
    Returns list of {token, count, tf} dicts.
    """
    lib = _load_lib()
    if lib:
        raw = lib.nyx_tokenize(text.encode('utf-8', errors='replace'), len(text))
        if raw:
            try:
                data = json.loads(raw.decode())
                return [{'token': d['t'], 'count': d['c'], 'tf': d['tf']}
                        for d in data]
            except (json.JSONDecodeError, KeyError):
                pass

    # Pure Python fallback
    return _py_tokenize(text)


def fast_score(query, doc_text):
    """Score query relevance against document text."""
    lib = _load_lib()
    if lib:
        return lib.nyx_score(
            query.encode('utf-8', errors='replace'),
            doc_text.encode('utf-8', errors='replace'),
            len(doc_text))

    # Python fallback
    return _py_score(query, doc_text)


def fast_normalize_url(url):
    """Normalize URL using native C++ or Python fallback."""
    lib = _load_lib()
    if lib:
        result = lib.nyx_normalize_url(url.encode('utf-8', errors='replace'))
        if result:
            return result.decode()

    # Python fallback
    return _py_normalize_url(url)


def fast_extract_domain(url):
    """Extract domain from URL."""
    lib = _load_lib()
    if lib:
        result = lib.nyx_extract_domain(url.encode('utf-8', errors='replace'))
        if result:
            return result.decode()

    # Python fallback
    return _py_extract_domain(url)


# ── Python Fallbacks ─────────────────────────────────────────────────

_STOP = {
    'a', 'an', 'the', 'is', 'it', 'in', 'on', 'at', 'to', 'of',
    'and', 'or', 'not', 'for', 'with', 'as', 'by', 'this', 'that',
    'from', 'be', 'are', 'was', 'were', 'been', 'have', 'has', 'had',
    'do', 'does', 'did', 'will', 'would', 'could', 'should', 'may',
    'but', 'if', 'so', 'than', 'too', 'very', 'just', 'about',
    'i', 'me', 'my', 'you', 'your', 'he', 'she', 'we', 'they',
}


def _py_tokenize(text):
    import re
    words = re.findall(r'[a-zA-Z0-9]{2,40}', text.lower())
    words = [w for w in words if w not in _STOP]
    counts = {}
    for w in words:
        counts[w] = counts.get(w, 0) + 1
    total = len(words) or 1
    return [{'token': t, 'count': c, 'tf': c / total}
            for t, c in counts.items()]


def _py_score(query, doc_text):
    q_tokens = [w.lower() for w in query.split() if w.lower() not in _STOP]
    doc_lower = doc_text.lower()
    score = 0.0
    for qt in q_tokens:
        count = doc_lower.count(qt)
        if count > 0:
            score += 1.0 + (count * 0.1)
    return score


def _py_normalize_url(url):
    u = url
    for prefix in ('https://', 'http://'):
        if u.startswith(prefix):
            u = u[len(prefix):]
            break
    if u.startswith('www.'):
        u = u[4:]
    return u.rstrip('/').lower()


def _py_extract_domain(url):
    norm = _py_normalize_url(url)
    return norm.split('/')[0].split(':')[0]

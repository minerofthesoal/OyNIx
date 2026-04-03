"""
OyNIx Browser - Google Chrome History/Search Import
Imports Chrome's browsing history and search history from the History SQLite DB
and Google Takeout JSON exports.
"""
import os
import json
import sqlite3
import shutil
import tempfile
from pathlib import Path


def find_chrome_history_db():
    """Locate Chrome's History database on the user's system."""
    candidates = []
    home = Path.home()

    # Linux
    candidates.append(home / '.config' / 'google-chrome' / 'Default' / 'History')
    candidates.append(home / '.config' / 'chromium' / 'Default' / 'History')
    # macOS
    candidates.append(home / 'Library' / 'Application Support' / 'Google' / 'Chrome' / 'Default' / 'History')
    # Windows
    local = os.environ.get('LOCALAPPDATA', '')
    if local:
        candidates.append(Path(local) / 'Google' / 'Chrome' / 'User Data' / 'Default' / 'History')

    for path in candidates:
        if path.is_file():
            return str(path)
    return None


def import_chrome_history(db_path=None, max_entries=5000):
    """
    Import browsing history from Chrome's SQLite database.
    Returns list of {url, title, time, visit_count} dicts.
    """
    if db_path is None:
        db_path = find_chrome_history_db()
    if not db_path or not os.path.isfile(db_path):
        return [], "Chrome history database not found"

    # Chrome locks the DB, so copy to temp
    tmp = tempfile.NamedTemporaryFile(suffix='.db', delete=False)
    tmp.close()
    try:
        shutil.copy2(db_path, tmp.name)
        conn = sqlite3.connect(tmp.name)
        cursor = conn.cursor()

        # Get browsing history
        cursor.execute("""
            SELECT u.url, u.title, u.visit_count, u.last_visit_time
            FROM urls u
            ORDER BY u.last_visit_time DESC
            LIMIT ?
        """, (max_entries,))

        entries = []
        for url, title, visits, chrome_time in cursor.fetchall():
            # Chrome timestamps are microseconds since Jan 1, 1601
            import datetime
            if chrome_time:
                epoch = datetime.datetime(1601, 1, 1)
                try:
                    dt = epoch + datetime.timedelta(microseconds=chrome_time)
                    time_str = dt.strftime('%Y-%m-%d %H:%M')
                except (OverflowError, OSError):
                    time_str = ''
            else:
                time_str = ''
            entries.append({
                'url': url,
                'title': title or url,
                'time': time_str,
                'visit_count': visits or 1,
                'source': 'chrome'
            })

        conn.close()
        return entries, f"Imported {len(entries)} entries from Chrome"
    except Exception as e:
        return [], f"Error reading Chrome history: {e}"
    finally:
        try:
            os.unlink(tmp.name)
        except OSError:
            pass


def import_chrome_search_history(db_path=None, max_entries=2000):
    """
    Extract search queries from Chrome's history by parsing Google search URLs.
    Returns list of {query, time, url} dicts.
    """
    if db_path is None:
        db_path = find_chrome_history_db()
    if not db_path or not os.path.isfile(db_path):
        return [], "Chrome history database not found"

    tmp = tempfile.NamedTemporaryFile(suffix='.db', delete=False)
    tmp.close()
    try:
        shutil.copy2(db_path, tmp.name)
        conn = sqlite3.connect(tmp.name)
        cursor = conn.cursor()

        cursor.execute("""
            SELECT u.url, u.title, u.last_visit_time
            FROM urls u
            WHERE u.url LIKE '%google.com/search?%'
               OR u.url LIKE '%duckduckgo.com/?q=%'
               OR u.url LIKE '%bing.com/search?%'
               OR u.url LIKE '%search.brave.com/search?%'
            ORDER BY u.last_visit_time DESC
            LIMIT ?
        """, (max_entries,))

        from urllib.parse import urlparse, parse_qs
        import datetime
        searches = []
        seen_queries = set()

        for url, title, chrome_time in cursor.fetchall():
            parsed = urlparse(url)
            params = parse_qs(parsed.query)
            query = params.get('q', params.get('query', ['']))[0]
            if not query or query.lower() in seen_queries:
                continue
            seen_queries.add(query.lower())

            time_str = ''
            if chrome_time:
                try:
                    epoch = datetime.datetime(1601, 1, 1)
                    dt = epoch + datetime.timedelta(microseconds=chrome_time)
                    time_str = dt.strftime('%Y-%m-%d %H:%M')
                except (OverflowError, OSError):
                    pass

            searches.append({
                'query': query,
                'time': time_str,
                'url': url,
                'source': 'chrome_search'
            })

        conn.close()
        return searches, f"Found {len(searches)} unique search queries"
    except Exception as e:
        return [], f"Error: {e}"
    finally:
        try:
            os.unlink(tmp.name)
        except OSError:
            pass


def import_google_takeout(filepath):
    """
    Import from Google Takeout JSON files.
    Supports: BrowserHistory.json, SearchHistory.json, MyActivity.json
    Returns (history_entries, search_entries, message)
    """
    try:
        with open(filepath) as f:
            data = json.load(f)
    except (json.JSONDecodeError, FileNotFoundError) as e:
        return [], [], f"Error reading file: {e}"

    history = []
    searches = []

    # Google Takeout "Browser History" format
    if isinstance(data, list):
        for item in data:
            if isinstance(item, dict):
                url = item.get('url', '')
                title = item.get('title', item.get('name', url))
                time_str = item.get('time', item.get('time_usec', ''))
                if isinstance(time_str, (int, float)):
                    import datetime
                    try:
                        dt = datetime.datetime.fromtimestamp(time_str / 1e6 if time_str > 1e12 else time_str)
                        time_str = dt.strftime('%Y-%m-%d %H:%M')
                    except (OverflowError, OSError, ValueError):
                        time_str = ''
                history.append({'url': url, 'title': title, 'time': str(time_str), 'source': 'takeout'})

    # Google Takeout "My Activity" format
    elif isinstance(data, dict):
        items = data.get('items', data.get('entries', data.get('browserHistory', [])))
        for item in items:
            if isinstance(item, dict):
                url = item.get('url', item.get('titleUrl', ''))
                title = item.get('title', item.get('header', url))
                time_str = item.get('time', '')
                entry = {'url': url, 'title': title, 'time': str(time_str), 'source': 'takeout'}

                # Check if it's a search query
                if 'google.com/search' in url or 'Search' in title:
                    from urllib.parse import urlparse, parse_qs
                    parsed = urlparse(url)
                    params = parse_qs(parsed.query)
                    query = params.get('q', [''])[0]
                    if query:
                        searches.append({
                            'query': query, 'time': str(time_str), 'url': url, 'source': 'takeout'
                        })
                history.append(entry)

    msg = f"Imported {len(history)} history entries and {len(searches)} search queries"
    return history, searches, msg

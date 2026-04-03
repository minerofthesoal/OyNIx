"""
OyNIx Browser - GitHub Database Sync
Uploads and syncs the Nyx search index and site database to a GitHub repository.

Features:
- Export Nyx index + database as JSON
- Upload to GitHub Pages (site database)
- Import shared databases from GitHub
- Auto-sync on configurable interval
"""

import os
import json
import time
import hashlib
import threading

from PyQt6.QtCore import QThread, pyqtSignal, QObject, QTimer

try:
    import requests
    HAS_REQUESTS = True
except ImportError:
    HAS_REQUESTS = False

SYNC_DIR = os.path.join(os.path.expanduser("~"), ".config", "oynix", "sync")


class GitHubUploader(QThread):
    """Upload database JSON to a GitHub repository."""
    progress = pyqtSignal(str)
    finished = pyqtSignal(bool, str)

    def __init__(self, token, repo, filepath, target_path="database/nyx_index.json"):
        super().__init__()
        self.token = token
        self.repo = repo  # "user/repo"
        self.filepath = filepath
        self.target_path = target_path

    def run(self):
        if not HAS_REQUESTS:
            self.finished.emit(False, "requests library not installed")
            return

        if not self.token:
            self.finished.emit(False, "No GitHub token configured")
            return

        try:
            self.progress.emit("Preparing upload...")

            with open(self.filepath, 'r') as f:
                content = f.read()

            import base64
            encoded = base64.b64encode(content.encode()).decode()

            # Check if file exists already (need SHA for update)
            api_url = (f"https://api.github.com/repos/{self.repo}"
                       f"/contents/{self.target_path}")
            headers = {
                'Authorization': f'token {self.token}',
                'Accept': 'application/vnd.github.v3+json',
            }

            sha = None
            resp = requests.get(api_url, headers=headers, timeout=10)
            if resp.status_code == 200:
                sha = resp.json().get('sha')

            # Create or update file
            self.progress.emit("Uploading to GitHub...")
            data = {
                'message': f'Update OyNIx database - {time.strftime("%Y-%m-%d %H:%M")}',
                'content': encoded,
            }
            if sha:
                data['sha'] = sha

            resp = requests.put(api_url, headers=headers,
                                json=data, timeout=30)

            if resp.status_code in (200, 201):
                self.progress.emit("Upload successful!")
                self.finished.emit(True, "Database uploaded to GitHub")
            else:
                self.finished.emit(
                    False,
                    f"GitHub API error: {resp.status_code} - {resp.text[:200]}"
                )

        except Exception as e:
            self.finished.emit(False, f"Upload error: {e}")


class GitHubImporter(QThread):
    """Import a shared database from a GitHub repository."""
    progress = pyqtSignal(str)
    finished = pyqtSignal(bool, list)  # success, imported_entries

    def __init__(self, repo, target_path="database/nyx_index.json", token=None):
        super().__init__()
        self.repo = repo
        self.target_path = target_path
        self.token = token

    def run(self):
        if not HAS_REQUESTS:
            self.finished.emit(False, [])
            return

        try:
            self.progress.emit(f"Fetching database from {self.repo}...")

            # Try raw content URL first (works for public repos without token)
            raw_url = (f"https://raw.githubusercontent.com/{self.repo}"
                       f"/main/{self.target_path}")

            headers = {}
            if self.token:
                headers['Authorization'] = f'token {self.token}'

            resp = requests.get(raw_url, headers=headers, timeout=15)

            if resp.status_code != 200:
                # Try API
                api_url = (f"https://api.github.com/repos/{self.repo}"
                           f"/contents/{self.target_path}")
                resp = requests.get(api_url, headers=headers, timeout=15)
                if resp.status_code == 200:
                    import base64
                    content = base64.b64decode(
                        resp.json()['content']).decode()
                    data = json.loads(content)
                else:
                    self.finished.emit(False, [])
                    return
            else:
                data = resp.json()

            if isinstance(data, list):
                self.progress.emit(f"Imported {len(data)} entries")
                self.finished.emit(True, data)
            else:
                self.finished.emit(False, [])

        except Exception as e:
            self.progress.emit(f"Import error: {e}")
            self.finished.emit(False, [])


class GitHubSync(QObject):
    """
    Manages GitHub sync for the OyNIx database.
    """

    status_update = pyqtSignal(str)

    def __init__(self):
        super().__init__()
        self.config = self._load_config()
        self._upload_thread = None
        self._import_thread = None
        self._auto_timer = QTimer(self)
        self._auto_timer.timeout.connect(self._auto_sync_tick)
        self._json_export_func = None  # set by browser to export DB JSON

    def _load_config(self):
        """Load GitHub sync configuration."""
        config_path = os.path.join(SYNC_DIR, "github_config.json")
        os.makedirs(SYNC_DIR, exist_ok=True)
        try:
            with open(config_path, 'r') as f:
                return json.load(f)
        except (FileNotFoundError, json.JSONDecodeError):
            return {
                'token': '',
                'repo': '',
                'auto_sync': False,
                'sync_interval_minutes': 60,
                'last_sync': 0,
            }

    def save_config(self):
        """Save GitHub sync configuration."""
        config_path = os.path.join(SYNC_DIR, "github_config.json")
        os.makedirs(SYNC_DIR, exist_ok=True)
        with open(config_path, 'w') as f:
            json.dump(self.config, f, indent=2)

    # ── Auto-Sync Timer ────────────────────────────────────────────
    def start_auto_sync(self, export_func=None):
        """Start the auto-sync timer.  export_func(path) should write the
        database JSON to *path* so it can be uploaded."""
        if export_func:
            self._json_export_func = export_func
        interval = max(self.config.get('sync_interval_minutes', 60), 30)
        if self.config.get('auto_sync') and self.is_configured():
            self._auto_timer.start(interval * 60 * 1000)
            self.status_update.emit(
                f"Auto-sync enabled (every {interval} min)")

    def stop_auto_sync(self):
        """Stop the auto-sync timer."""
        self._auto_timer.stop()

    def _auto_sync_tick(self):
        """Called by the QTimer — performs one upload cycle."""
        if not self.is_configured() or not self._json_export_func:
            return
        import tempfile
        tmp = tempfile.NamedTemporaryFile(suffix='.json', delete=False)
        tmp.close()
        try:
            self._json_export_func(tmp.name)
            self.upload_database(tmp.name, callback=self._auto_sync_done)
        except Exception as e:
            self.status_update.emit(f"Auto-sync error: {e}")
            try:
                os.unlink(tmp.name)
            except OSError:
                pass

    def _auto_sync_done(self, success, msg):
        self.config['last_sync'] = int(time.time())
        self.save_config()
        self.status_update.emit(f"Auto-sync: {msg}")
        # Also flush community upload queue
        self.flush_upload_queue()

    def configure(self, token, repo, auto_sync=False, interval=60):
        """Configure GitHub sync settings."""
        self.config.update({
            'token': token,
            'repo': repo,
            'auto_sync': auto_sync,
            'sync_interval_minutes': max(interval, 30),
        })
        self.save_config()
        # Restart or stop auto-sync based on new settings
        self.stop_auto_sync()
        if auto_sync:
            self.start_auto_sync()

    def upload_database(self, json_filepath, callback=None):
        """Upload database to GitHub."""
        if not self.config.get('token') or not self.config.get('repo'):
            self.status_update.emit("GitHub sync not configured")
            return

        self._upload_thread = GitHubUploader(
            self.config['token'],
            self.config['repo'],
            json_filepath,
        )
        self._upload_thread.progress.connect(self.status_update.emit)
        if callback:
            self._upload_thread.finished.connect(callback)
        self._upload_thread.start()

    def import_database(self, repo=None, callback=None):
        """Import a shared database from GitHub."""
        target_repo = repo or self.config.get('repo', '')
        if not target_repo:
            self.status_update.emit("No repository specified")
            return

        self._import_thread = GitHubImporter(
            target_repo,
            token=self.config.get('token'),
        )
        self._import_thread.progress.connect(self.status_update.emit)
        if callback:
            self._import_thread.finished.connect(callback)
        self._import_thread.start()

    def is_configured(self):
        """Check if GitHub sync is configured."""
        return bool(self.config.get('token') and self.config.get('repo'))

    # ── Auto-Compare and Update ────────────────────────────────────
    def auto_compare_and_update(self, current_url, current_title, database_ref):
        """
        Compare the current site against the known database.
        If it's a new site, add it to the local DB and optionally
        queue it for community upload.

        Args:
            current_url: URL currently being visited
            current_title: page title
            database_ref: reference to the OynixDatabase instance

        Returns:
            dict with comparison results or None
        """
        if not current_url or not current_url.startswith('http'):
            return None

        comparison = database_ref.compare_site(current_url, current_title)

        if not comparison['is_known']:
            # New site — add to local discovered database
            database_ref.add_sites_batch([{
                'url': current_url,
                'title': current_title,
                'description': '',
            }], source='browsing')

            self.status_update.emit(
                f"New site discovered: {comparison['domain']} "
                f"(category: {comparison['category_guess']})"
            )

        return comparison

    def queue_for_community_upload(self, sites):
        """Queue newly discovered sites for periodic community upload."""
        queue_file = os.path.join(SYNC_DIR, "upload_queue.json")
        os.makedirs(SYNC_DIR, exist_ok=True)
        existing = []
        try:
            with open(queue_file, 'r') as f:
                existing = json.load(f)
        except (FileNotFoundError, json.JSONDecodeError):
            pass

        known_urls = {s['url'] for s in existing}
        for site in sites:
            if site.get('url') and site['url'] not in known_urls:
                existing.append(site)
                known_urls.add(site['url'])

        with open(queue_file, 'w') as f:
            json.dump(existing, f, indent=2)

    def flush_upload_queue(self, callback=None):
        """Upload all queued sites to the community database."""
        queue_file = os.path.join(SYNC_DIR, "upload_queue.json")
        try:
            with open(queue_file, 'r') as f:
                queued = json.load(f)
        except (FileNotFoundError, json.JSONDecodeError):
            queued = []

        if not queued:
            return

        self.community_upload(queued, callback=callback)
        # Clear queue after upload
        with open(queue_file, 'w') as f:
            json.dump([], f)

    # ── Community Folder Scanner ──────────────────────────────────────
    def scan_community_folder(self, on_new_files=None, on_done=None):
        """Scan the GitHub repo's database/community/ folder for new DB files."""
        repo = self.config.get('repo', '')
        if not repo:
            self.status_update.emit("No repository configured for community sync")
            return
        self._scanner = CommunityFolderScanner(repo, self.config.get('token'))
        self._scanner.progress.connect(self.status_update.emit)
        if on_new_files:
            self._scanner.new_files_found.connect(on_new_files)
        if on_done:
            self._scanner.finished.connect(on_done)
        self._scanner.start()

    # ── Community Auto-Upload ────────────────────────────────────────
    def community_upload(self, new_sites, callback=None):
        """
        Upload newly discovered sites to the shared GitHub repo
        so future OyNIx users benefit from a growing database.

        Args:
            new_sites: list of dicts with url, title, description, category
            callback: optional (success, message) callback
        """
        if not self.is_configured() or not new_sites:
            return

        self._community_thread = CommunityUploader(
            self.config['token'], self.config['repo'], new_sites,
        )
        self._community_thread.progress.connect(self.status_update.emit)
        if callback:
            self._community_thread.finished.connect(callback)
        self._community_thread.start()


class CommunityUploader(QThread):
    """Batch-upload discovered sites to shared GitHub database."""
    progress = pyqtSignal(str)
    finished = pyqtSignal(bool, str)

    TARGET = "database/community_sites.json"

    def __init__(self, token, repo, sites):
        super().__init__()
        self.token = token
        self.repo = repo
        self.sites = sites

    def run(self):
        if not HAS_REQUESTS:
            self.finished.emit(False, "requests not installed")
            return
        try:
            import base64
            headers = {
                'Authorization': f'token {self.token}',
                'Accept': 'application/vnd.github.v3+json',
            }
            api_url = (f"https://api.github.com/repos/{self.repo}"
                       f"/contents/{self.TARGET}")

            # Fetch existing community file
            self.progress.emit("Fetching community database...")
            existing = []
            sha = None
            resp = requests.get(api_url, headers=headers, timeout=10)
            if resp.status_code == 200:
                sha = resp.json().get('sha')
                content = base64.b64decode(
                    resp.json()['content']).decode()
                existing = json.loads(content)

            # Merge: only add URLs not already present
            known = {s.get('url', '') for s in existing}
            added = 0
            for s in self.sites:
                if s.get('url') and s['url'] not in known:
                    existing.append({
                        'url': s['url'],
                        'title': s.get('title', ''),
                        'description': s.get('description', ''),
                        'category': s.get('category', 'General'),
                        'source': 'community',
                    })
                    known.add(s['url'])
                    added += 1

            if added == 0:
                self.finished.emit(True, "No new sites to upload")
                return

            # Upload merged file
            self.progress.emit(f"Uploading {added} new sites...")
            encoded = base64.b64encode(
                json.dumps(existing, indent=2).encode()).decode()
            data = {
                'message': f'Community: +{added} sites - {time.strftime("%Y-%m-%d")}',
                'content': encoded,
            }
            if sha:
                data['sha'] = sha

            resp = requests.put(api_url, headers=headers,
                                json=data, timeout=30)
            if resp.status_code in (200, 201):
                self.finished.emit(True, f"Uploaded {added} sites to community DB")
            else:
                self.finished.emit(False, f"GitHub API: {resp.status_code}")

        except Exception as e:
            self.finished.emit(False, f"Community upload error: {e}")


class CommunityFolderScanner(QThread):
    """
    Scans a GitHub repo's database/community/ folder for new DB files.
    Users drop exported JSON database files into this folder.
    The browser checks for NEW files it hasn't seen before, parses them,
    and offers to merge new sites (skipping duplicates).
    """
    progress = pyqtSignal(str)
    new_files_found = pyqtSignal(list)  # list of {filename, url, sites: [...]}
    finished = pyqtSignal(bool, str)

    FOLDER = "database/community"

    def __init__(self, repo, token=None):
        super().__init__()
        self.repo = repo
        self.token = token
        self._seen_file = os.path.join(SYNC_DIR, "seen_community_files.json")

    def _load_seen(self):
        try:
            with open(self._seen_file) as f:
                return set(json.load(f))
        except (FileNotFoundError, json.JSONDecodeError):
            return set()

    def _save_seen(self, seen):
        os.makedirs(os.path.dirname(self._seen_file), exist_ok=True)
        with open(self._seen_file, 'w') as f:
            json.dump(list(seen), f)

    def run(self):
        if not HAS_REQUESTS:
            self.finished.emit(False, "requests library not installed")
            return
        try:
            headers = {'Accept': 'application/vnd.github.v3+json'}
            if self.token:
                headers['Authorization'] = f'token {self.token}'

            # List files in community folder
            api_url = f"https://api.github.com/repos/{self.repo}/contents/{self.FOLDER}"
            self.progress.emit("Checking community database folder...")
            resp = requests.get(api_url, headers=headers, timeout=15)

            if resp.status_code != 200:
                self.finished.emit(False, f"Could not access {self.FOLDER}: {resp.status_code}")
                return

            files = resp.json()
            if not isinstance(files, list):
                self.finished.emit(False, "Unexpected response from GitHub")
                return

            seen = self._load_seen()
            json_files = [f for f in files
                          if f.get('name', '').endswith('.json') and f['name'] not in seen]

            if not json_files:
                self.finished.emit(True, "No new community database files found")
                return

            self.progress.emit(f"Found {len(json_files)} new database file(s)")
            new_entries = []

            for file_info in json_files:
                fname = file_info['name']
                download_url = file_info.get('download_url', '')
                if not download_url:
                    continue

                self.progress.emit(f"Downloading {fname}...")
                file_resp = requests.get(download_url, headers=headers, timeout=15)
                if file_resp.status_code != 200:
                    continue

                try:
                    data = file_resp.json()
                    sites = self._extract_sites(data)
                    if sites:
                        new_entries.append({
                            'filename': fname,
                            'url': download_url,
                            'sites': sites,
                            'count': len(sites),
                        })
                except (json.JSONDecodeError, ValueError):
                    continue

                seen.add(fname)

            self._save_seen(seen)

            if new_entries:
                self.new_files_found.emit(new_entries)
                total = sum(e['count'] for e in new_entries)
                self.finished.emit(True, f"Found {total} sites in {len(new_entries)} new file(s)")
            else:
                self.finished.emit(True, "No valid sites found in new files")

        except Exception as e:
            self.finished.emit(False, f"Scan error: {e}")

    def _extract_sites(self, data):
        """Extract site entries from various JSON formats."""
        sites = []
        if isinstance(data, list):
            # Flat list of site dicts
            for item in data:
                if isinstance(item, dict) and item.get('url'):
                    sites.append({
                        'url': item['url'],
                        'title': item.get('title', ''),
                        'description': item.get('description', item.get('desc', '')),
                        'category': item.get('category', 'community'),
                    })
        elif isinstance(data, dict):
            # OyNIx export format: {curated: {cat: [...]}, discovered: {cat: [...]}}
            for section in ('curated', 'discovered', 'sites'):
                if section in data and isinstance(data[section], dict):
                    for cat, entries in data[section].items():
                        if isinstance(entries, list):
                            for item in entries:
                                if isinstance(item, dict) and item.get('url'):
                                    sites.append({
                                        'url': item['url'],
                                        'title': item.get('title', ''),
                                        'description': item.get('description', item.get('desc', '')),
                                        'category': cat,
                                    })
            # Also handle flat sites list inside a dict
            if 'sites' in data and isinstance(data['sites'], list):
                for item in data['sites']:
                    if isinstance(item, dict) and item.get('url'):
                        sites.append(item)
        return sites


# Lazy singleton - must not create QObject before QApplication exists
_github_sync = None

def _get_github_sync():
    global _github_sync
    if _github_sync is None:
        _github_sync = GitHubSync()
    return _github_sync

class _GitHubSyncProxy:
    """Proxy that delays GitHubSync creation until first access."""
    def __getattr__(self, name):
        return getattr(_get_github_sync(), name)

github_sync = _GitHubSyncProxy()

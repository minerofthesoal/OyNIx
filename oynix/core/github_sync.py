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

from PyQt6.QtCore import QThread, pyqtSignal, QObject

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

    def configure(self, token, repo, auto_sync=False, interval=60):
        """Configure GitHub sync settings."""
        self.config.update({
            'token': token,
            'repo': repo,
            'auto_sync': auto_sync,
            'sync_interval_minutes': interval,
        })
        self.save_config()

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


# Singleton
github_sync = GitHubSync()

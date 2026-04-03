"""
OyNIx Browser - Multi-Profile Manager
Each profile has its own isolated data directory with:
  - config, history, bookmarks, credentials, search index
  - auto-login rules (domain → username, auto-fill on page load)
  - profile picture, display name
Profiles are saved/restored via .nydta archives.
"""
import os
import json
import time
import shutil
import hashlib
import base64
from pathlib import Path

BASE_DIR = os.path.expanduser("~/.config/oynix")
PROFILES_DIR = os.path.join(BASE_DIR, "profiles")
PROFILES_INDEX = os.path.join(PROFILES_DIR, "profiles.json")
ACTIVE_PROFILE_FILE = os.path.join(BASE_DIR, "active_profile")

# Default profile name
DEFAULT_PROFILE = "default"


def _xor_crypt(data: bytes, key: bytes) -> bytes:
    """XOR encrypt/decrypt for local credential storage."""
    expanded = key * ((len(data) // len(key)) + 1)
    return bytes(a ^ b for a, b in zip(data, expanded[:len(data)]))


def _derive_key(master: str, salt: str = "oynix_profile_v1") -> bytes:
    return hashlib.pbkdf2_hmac('sha256', master.encode(), salt.encode(), 100000)


class ProfileCredentials:
    """Per-profile credential store with encrypted passwords and auto-login rules."""

    def __init__(self, profile_dir: str):
        self._dir = os.path.join(profile_dir, "credentials")
        os.makedirs(self._dir, exist_ok=True)
        self._cred_file = os.path.join(self._dir, "passwords.enc")
        self._autologin_file = os.path.join(self._dir, "autologin.json")
        self._master_file = os.path.join(self._dir, ".master_hash")
        self._master_key = None
        self._unlocked = False
        self._credentials = {}   # domain -> [{username, password, saved_at, notes}]
        self._autologin = {}     # domain -> {username, enabled, fill_only}
        self._load_autologin()

    @property
    def is_unlocked(self):
        return self._unlocked

    def has_master_password(self):
        return os.path.isfile(self._master_file)

    def set_master_password(self, password: str):
        os.makedirs(self._dir, exist_ok=True)
        pw_hash = hashlib.sha256(password.encode()).hexdigest()
        with open(self._master_file, 'w') as f:
            f.write(pw_hash)
        self._master_key = _derive_key(password)
        self._unlocked = True
        self._save_credentials()

    def unlock(self, password: str) -> bool:
        if not self.has_master_password():
            self.set_master_password(password)
            return True
        with open(self._master_file) as f:
            expected = f.read().strip()
        actual = hashlib.sha256(password.encode()).hexdigest()
        if actual == expected:
            self._master_key = _derive_key(password)
            self._unlocked = True
            self._load_credentials()
            return True
        return False

    def lock(self):
        self._master_key = None
        self._credentials = {}
        self._unlocked = False

    # ── Password CRUD ────────────────────────────────────────────
    def save_credential(self, domain: str, username: str, password: str, notes: str = ""):
        domain = domain.lower().strip()
        if domain not in self._credentials:
            self._credentials[domain] = []
        for cred in self._credentials[domain]:
            if cred['username'] == username:
                cred['password'] = password
                cred['notes'] = notes
                cred['saved_at'] = time.strftime('%Y-%m-%d %H:%M')
                self._save_credentials()
                return
        self._credentials[domain].append({
            'username': username,
            'password': password,
            'notes': notes,
            'saved_at': time.strftime('%Y-%m-%d %H:%M'),
        })
        self._save_credentials()

    def get_credentials(self, domain: str):
        return self._credentials.get(domain.lower().strip(), [])

    def get_all_credentials(self):
        return dict(self._credentials)

    def delete_credential(self, domain: str, username: str):
        domain = domain.lower().strip()
        if domain in self._credentials:
            self._credentials[domain] = [
                c for c in self._credentials[domain] if c['username'] != username
            ]
            if not self._credentials[domain]:
                del self._credentials[domain]
            self._save_credentials()
            # Remove auto-login if this was the auto-login user
            if domain in self._autologin and self._autologin[domain].get('username') == username:
                del self._autologin[domain]
                self._save_autologin()

    def get_all_domains(self):
        return list(self._credentials.keys())

    # ── Auto-Login Rules ─────────────────────────────────────────
    def set_autologin(self, domain: str, username: str, enabled: bool = True, fill_only: bool = False):
        """Set auto-login rule: auto-fill (and optionally auto-submit) for a domain."""
        domain = domain.lower().strip()
        self._autologin[domain] = {
            'username': username,
            'enabled': enabled,
            'fill_only': fill_only,  # True = fill fields but don't submit
        }
        self._save_autologin()

    def get_autologin(self, domain: str):
        """Get auto-login rule for domain. Returns None if not set."""
        return self._autologin.get(domain.lower().strip())

    def get_all_autologins(self):
        return dict(self._autologin)

    def remove_autologin(self, domain: str):
        domain = domain.lower().strip()
        if domain in self._autologin:
            del self._autologin[domain]
            self._save_autologin()

    # ── Persistence ──────────────────────────────────────────────
    def _save_credentials(self):
        if not self._master_key:
            return
        data = json.dumps(self._credentials).encode()
        encrypted = _xor_crypt(data, self._master_key)
        with open(self._cred_file, 'wb') as f:
            f.write(base64.b64encode(encrypted))

    def _load_credentials(self):
        if not self._master_key or not os.path.isfile(self._cred_file):
            return
        try:
            with open(self._cred_file, 'rb') as f:
                encrypted = base64.b64decode(f.read())
            decrypted = _xor_crypt(encrypted, self._master_key)
            self._credentials = json.loads(decrypted.decode())
        except Exception:
            self._credentials = {}

    def _save_autologin(self):
        with open(self._autologin_file, 'w') as f:
            json.dump(self._autologin, f, indent=2)

    def _load_autologin(self):
        try:
            with open(self._autologin_file) as f:
                self._autologin = json.load(f)
        except (FileNotFoundError, json.JSONDecodeError):
            self._autologin = {}

    def export_dict(self) -> dict:
        """Export credentials + auto-login for .nydta (encrypted blob + autologin rules)."""
        result = {'autologin': self._autologin}
        if self._master_key and os.path.isfile(self._cred_file):
            with open(self._cred_file, 'rb') as f:
                result['passwords_enc'] = f.read().decode('ascii')
            with open(self._master_file) as f:
                result['master_hash'] = f.read().strip()
        return result

    def import_dict(self, data: dict):
        """Import credentials + auto-login from .nydta."""
        if 'autologin' in data:
            self._autologin.update(data['autologin'])
            self._save_autologin()
        if 'passwords_enc' in data and 'master_hash' in data:
            with open(self._cred_file, 'wb') as f:
                f.write(data['passwords_enc'].encode('ascii'))
            with open(self._master_file, 'w') as f:
                f.write(data['master_hash'])


class Profile:
    """A single browser profile with its own data directory."""

    def __init__(self, name: str, display_name: str = "", avatar_path: str = ""):
        self.name = name
        self.display_name = display_name or name.replace('_', ' ').title()
        self.avatar_path = avatar_path
        self.created_at = time.strftime('%Y-%m-%d %H:%M')
        self.last_used = self.created_at
        self.data_dir = os.path.join(PROFILES_DIR, name)
        os.makedirs(self.data_dir, exist_ok=True)
        self.credentials = ProfileCredentials(self.data_dir)

    @property
    def config_path(self):
        return os.path.join(self.data_dir, "browser_config.json")

    @property
    def history_path(self):
        return os.path.join(self.data_dir, "history.json")

    @property
    def bookmarks_path(self):
        return os.path.join(self.data_dir, "bookmarks.json")

    @property
    def profile_pic_path(self):
        return os.path.join(self.data_dir, "profile.png")

    def to_dict(self) -> dict:
        return {
            'name': self.name,
            'display_name': self.display_name,
            'avatar_path': self.avatar_path,
            'created_at': self.created_at,
            'last_used': self.last_used,
        }

    @classmethod
    def from_dict(cls, d: dict) -> 'Profile':
        p = cls(d['name'], d.get('display_name', ''), d.get('avatar_path', ''))
        p.created_at = d.get('created_at', p.created_at)
        p.last_used = d.get('last_used', p.last_used)
        return p


class ProfileManager:
    """Manages multiple browser profiles."""

    def __init__(self):
        os.makedirs(PROFILES_DIR, exist_ok=True)
        self._profiles = {}  # name -> Profile
        self._active_name = DEFAULT_PROFILE
        self._load_index()
        self._ensure_default()
        self._load_active()

    def _ensure_default(self):
        """Ensure the default profile exists, migrating legacy data if needed."""
        if DEFAULT_PROFILE not in self._profiles:
            p = Profile(DEFAULT_PROFILE, "Default")
            self._profiles[DEFAULT_PROFILE] = p

            # Migrate legacy data from ~/.config/oynix/ to default profile
            legacy_files = {
                'browser_config.json': p.config_path,
                'history.json': p.history_path,
                'bookmarks.json': p.bookmarks_path,
                'profile.png': p.profile_pic_path,
            }
            for src_name, dest in legacy_files.items():
                src = os.path.join(BASE_DIR, src_name)
                if os.path.isfile(src) and not os.path.isfile(dest):
                    shutil.copy2(src, dest)

            # Migrate legacy credentials
            legacy_cred_dir = os.path.join(BASE_DIR, "credentials")
            profile_cred_dir = os.path.join(p.data_dir, "credentials")
            if os.path.isdir(legacy_cred_dir):
                for fname in os.listdir(legacy_cred_dir):
                    src = os.path.join(legacy_cred_dir, fname)
                    dest = os.path.join(profile_cred_dir, fname)
                    if os.path.isfile(src) and not os.path.isfile(dest):
                        shutil.copy2(src, dest)

            self._save_index()

    def _load_index(self):
        try:
            with open(PROFILES_INDEX) as f:
                data = json.load(f)
            for d in data.get('profiles', []):
                p = Profile.from_dict(d)
                self._profiles[p.name] = p
        except (FileNotFoundError, json.JSONDecodeError):
            pass

    def _save_index(self):
        data = {
            'profiles': [p.to_dict() for p in self._profiles.values()],
            'active': self._active_name,
        }
        with open(PROFILES_INDEX, 'w') as f:
            json.dump(data, f, indent=2)

    def _load_active(self):
        try:
            with open(ACTIVE_PROFILE_FILE) as f:
                name = f.read().strip()
            if name in self._profiles:
                self._active_name = name
        except FileNotFoundError:
            pass

    def _save_active(self):
        with open(ACTIVE_PROFILE_FILE, 'w') as f:
            f.write(self._active_name)

    # ── Public API ───────────────────────────────────────────────
    @property
    def active(self) -> Profile:
        return self._profiles[self._active_name]

    @property
    def active_name(self) -> str:
        return self._active_name

    def list_profiles(self) -> list:
        return sorted(self._profiles.values(), key=lambda p: p.name)

    def get_profile(self, name: str) -> Profile:
        return self._profiles.get(name)

    def create_profile(self, name: str, display_name: str = "", avatar_path: str = "") -> Profile:
        """Create a new profile. Returns the profile or raises ValueError."""
        safe_name = name.lower().replace(' ', '_').strip()
        if not safe_name:
            raise ValueError("Profile name cannot be empty")
        if safe_name in self._profiles:
            raise ValueError(f"Profile '{safe_name}' already exists")

        p = Profile(safe_name, display_name or name, avatar_path)

        # Copy default config from current active profile as starting point
        active = self.active
        if os.path.isfile(active.config_path):
            shutil.copy2(active.config_path, p.config_path)

        self._profiles[safe_name] = p
        self._save_index()
        return p

    def switch_profile(self, name: str) -> Profile:
        """Switch to a different profile. Returns the profile."""
        if name not in self._profiles:
            raise ValueError(f"Profile '{name}' not found")
        # Mark current as last used
        self.active.last_used = time.strftime('%Y-%m-%d %H:%M')
        self._active_name = name
        self.active.last_used = time.strftime('%Y-%m-%d %H:%M')
        self._save_active()
        self._save_index()
        return self.active

    def delete_profile(self, name: str):
        """Delete a profile (cannot delete default or active)."""
        if name == DEFAULT_PROFILE:
            raise ValueError("Cannot delete the default profile")
        if name == self._active_name:
            raise ValueError("Cannot delete the active profile — switch first")
        if name not in self._profiles:
            raise ValueError(f"Profile '{name}' not found")

        p = self._profiles[name]
        if os.path.isdir(p.data_dir):
            shutil.rmtree(p.data_dir)
        del self._profiles[name]
        self._save_index()

    def rename_profile(self, old_name: str, new_display_name: str):
        """Rename a profile's display name."""
        if old_name not in self._profiles:
            raise ValueError(f"Profile '{old_name}' not found")
        self._profiles[old_name].display_name = new_display_name
        self._save_index()

    def duplicate_profile(self, source_name: str, new_name: str) -> Profile:
        """Duplicate a profile with all its data."""
        if source_name not in self._profiles:
            raise ValueError(f"Profile '{source_name}' not found")
        safe_name = new_name.lower().replace(' ', '_').strip()
        if safe_name in self._profiles:
            raise ValueError(f"Profile '{safe_name}' already exists")

        source = self._profiles[source_name]
        new_profile = Profile(safe_name, new_name)
        # Copy all data
        if os.path.isdir(source.data_dir):
            shutil.copytree(source.data_dir, new_profile.data_dir, dirs_exist_ok=True)
        self._profiles[safe_name] = new_profile
        self._save_index()
        return new_profile


# ── Auto-Login JavaScript ────────────────────────────────────────
def get_autologin_js(domain: str, username: str, password: str, fill_only: bool = False) -> str:
    """Generate JavaScript to auto-fill (and optionally submit) login forms."""
    # Escape for JS string embedding
    u = username.replace('\\', '\\\\').replace("'", "\\'")
    p = password.replace('\\', '\\\\').replace("'", "\\'")

    submit_block = ""
    if not fill_only:
        submit_block = """
        // Auto-submit after a short delay
        setTimeout(function() {
            var submit = form.querySelector('button[type="submit"], input[type="submit"]');
            if (!submit) submit = form.querySelector('button:not([type="button"])');
            if (submit) submit.click();
        }, 500);
"""

    return f"""
(function() {{
    function tryFill() {{
        var inputs = document.querySelectorAll('input');
        var userField = null, passField = null, form = null;
        for (var i = 0; i < inputs.length; i++) {{
            var inp = inputs[i];
            var t = (inp.type || '').toLowerCase();
            var n = (inp.name || '').toLowerCase();
            var id = (inp.id || '').toLowerCase();
            var ac = (inp.autocomplete || '').toLowerCase();
            if (t === 'password' || ac === 'current-password' || ac === 'new-password') {{
                passField = inp;
                form = inp.closest('form');
            }} else if (t === 'email' || t === 'text' || ac === 'username' || ac === 'email' ||
                       n.includes('user') || n.includes('email') || n.includes('login') ||
                       id.includes('user') || id.includes('email') || id.includes('login')) {{
                userField = inp;
            }}
        }}
        if (passField) {{
            if (userField) {{
                var nativeSet = Object.getOwnPropertyDescriptor(
                    window.HTMLInputElement.prototype, 'value').set;
                nativeSet.call(userField, '{u}');
                userField.dispatchEvent(new Event('input', {{bubbles: true}}));
                userField.dispatchEvent(new Event('change', {{bubbles: true}}));
            }}
            var nativeSet2 = Object.getOwnPropertyDescriptor(
                window.HTMLInputElement.prototype, 'value').set;
            nativeSet2.call(passField, '{p}');
            passField.dispatchEvent(new Event('input', {{bubbles: true}}));
            passField.dispatchEvent(new Event('change', {{bubbles: true}}));
            {submit_block}
            return true;
        }}
        return false;
    }}
    // Try immediately, then retry after dynamic content loads
    if (!tryFill()) {{
        setTimeout(tryFill, 800);
        setTimeout(tryFill, 2000);
    }}
}})();
"""


# ── Singleton ────────────────────────────────────────────────────
profile_manager = ProfileManager()

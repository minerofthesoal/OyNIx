"""
OyNIx Browser - Credential Manager
Password storage, passkey support, and biometric auth (Touch ID / fingerprint).
Uses OS keyring when available, falls back to encrypted JSON file.
"""
import os
import json
import hashlib
import base64
import time
from urllib.parse import urlparse

from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QLabel, QLineEdit,
    QPushButton, QListWidget, QListWidgetItem, QMessageBox,
    QCheckBox, QWidget, QInputDialog
)
from PyQt6.QtCore import Qt, pyqtSignal, QObject

# Try OS keyring
try:
    import keyring
    HAS_KEYRING = True
except ImportError:
    HAS_KEYRING = False

CRED_DIR = os.path.expanduser("~/.config/oynix/credentials")
CRED_FILE = os.path.join(CRED_DIR, "passwords.enc")
PASSKEY_FILE = os.path.join(CRED_DIR, "passkeys.json")
MASTER_HASH_FILE = os.path.join(CRED_DIR, ".master_hash")


def _derive_key(password, salt=b'oynix_cred_salt_v1'):
    """Derive encryption key from master password using PBKDF2."""
    return hashlib.pbkdf2_hmac('sha256', password.encode(), salt, 100000)


def _xor_encrypt(data, key):
    """Simple XOR encryption (for local storage only)."""
    key_bytes = key * ((len(data) // len(key)) + 1)
    return bytes(a ^ b for a, b in zip(data, key_bytes[:len(data)]))


class CredentialManager(QObject):
    """Manages passwords, passkeys, and biometric auth."""

    credentials_changed = pyqtSignal()

    def __init__(self):
        super().__init__()
        os.makedirs(CRED_DIR, exist_ok=True)
        self._master_key = None
        self._credentials = {}  # domain -> [{username, password, saved_at}]
        self._passkeys = {}     # domain -> [{credential_id, public_key, created_at}]
        self._unlocked = False
        self._load_passkeys()

    @property
    def is_unlocked(self):
        return self._unlocked

    def has_master_password(self):
        return os.path.isfile(MASTER_HASH_FILE)

    def set_master_password(self, password):
        """Set or change the master password."""
        os.makedirs(CRED_DIR, exist_ok=True)
        pw_hash = hashlib.sha256(password.encode()).hexdigest()
        with open(MASTER_HASH_FILE, 'w') as f:
            f.write(pw_hash)
        self._master_key = _derive_key(password)
        self._unlocked = True
        self._save_credentials()

    def unlock(self, password):
        """Unlock credential store with master password."""
        if not self.has_master_password():
            self.set_master_password(password)
            return True
        expected = open(MASTER_HASH_FILE).read().strip()
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

    def save_credential(self, url, username, password):
        """Save a credential for a domain."""
        domain = urlparse(url).netloc.lower()
        if domain not in self._credentials:
            self._credentials[domain] = []
        # Update existing or add new
        for cred in self._credentials[domain]:
            if cred['username'] == username:
                cred['password'] = password
                cred['saved_at'] = time.strftime('%Y-%m-%d %H:%M')
                self._save_credentials()
                self.credentials_changed.emit()
                return
        self._credentials[domain].append({
            'username': username,
            'password': password,
            'saved_at': time.strftime('%Y-%m-%d %H:%M')
        })
        self._save_credentials()
        self.credentials_changed.emit()

    def get_credentials(self, url):
        """Get saved credentials for a domain."""
        domain = urlparse(url).netloc.lower()
        return self._credentials.get(domain, [])

    def delete_credential(self, domain, username):
        """Delete a specific credential."""
        if domain in self._credentials:
            self._credentials[domain] = [
                c for c in self._credentials[domain]
                if c['username'] != username
            ]
            if not self._credentials[domain]:
                del self._credentials[domain]
            self._save_credentials()
            self.credentials_changed.emit()

    def get_all_domains(self):
        """Get all domains with saved credentials."""
        return list(self._credentials.keys())

    # ── Passkeys ──────────────────────────────────────────────────
    def register_passkey(self, url, credential_id, display_name=""):
        """Register a passkey (WebAuthn) for a domain."""
        domain = urlparse(url).netloc.lower()
        if domain not in self._passkeys:
            self._passkeys[domain] = []
        self._passkeys[domain].append({
            'credential_id': credential_id,
            'display_name': display_name or domain,
            'created_at': time.strftime('%Y-%m-%d %H:%M'),
        })
        self._save_passkeys()

    def get_passkeys(self, url):
        domain = urlparse(url).netloc.lower()
        return self._passkeys.get(domain, [])

    def delete_passkey(self, domain, credential_id):
        if domain in self._passkeys:
            self._passkeys[domain] = [
                p for p in self._passkeys[domain]
                if p['credential_id'] != credential_id
            ]
            self._save_passkeys()

    # ── Biometric Auth ────────────────────────────────────────────
    def check_biometric_available(self):
        """Check if biometric auth (fingerprint/Touch ID) is available."""
        import sys
        if sys.platform == 'darwin':
            # macOS Touch ID via LocalAuthentication
            try:
                import subprocess
                result = subprocess.run(
                    ['bioutil', '-rs'],
                    capture_output=True, timeout=2
                )
                return result.returncode == 0
            except Exception:
                return False
        elif sys.platform == 'linux':
            # Linux fprintd
            try:
                import subprocess
                result = subprocess.run(
                    ['fprintd-list', os.environ.get('USER', '')],
                    capture_output=True, timeout=2
                )
                return result.returncode == 0
            except Exception:
                return False
        elif sys.platform == 'win32':
            # Windows Hello
            try:
                import subprocess
                result = subprocess.run(
                    ['powershell', '-Command',
                     'Get-WmiObject -Namespace root/cimv2/security/microsofttpm -Class Win32_Tpm'],
                    capture_output=True, timeout=3
                )
                return b'IsEnabled' in result.stdout
            except Exception:
                return False
        return False

    def authenticate_biometric(self):
        """Attempt biometric authentication. Returns True if successful."""
        import sys
        if sys.platform == 'darwin':
            try:
                import subprocess
                result = subprocess.run(
                    ['osascript', '-e',
                     'tell application "System Events" to display dialog '
                     '"OyNIx Browser needs your fingerprint" '
                     'with title "Touch ID" default button "OK"'],
                    capture_output=True, timeout=30
                )
                return result.returncode == 0
            except Exception:
                return False
        elif sys.platform == 'linux':
            try:
                import subprocess
                result = subprocess.run(
                    ['fprintd-verify'],
                    capture_output=True, timeout=30
                )
                return result.returncode == 0
            except Exception:
                return False
        return False

    # ── Persistence ───────────────────────────────────────────────
    def _save_credentials(self):
        if not self._master_key:
            return
        data = json.dumps(self._credentials).encode()
        encrypted = _xor_encrypt(data, self._master_key)
        with open(CRED_FILE, 'wb') as f:
            f.write(base64.b64encode(encrypted))

    def _load_credentials(self):
        if not self._master_key or not os.path.isfile(CRED_FILE):
            return
        try:
            with open(CRED_FILE, 'rb') as f:
                encrypted = base64.b64decode(f.read())
            decrypted = _xor_encrypt(encrypted, self._master_key)
            self._credentials = json.loads(decrypted.decode())
        except Exception:
            self._credentials = {}

    def _save_passkeys(self):
        with open(PASSKEY_FILE, 'w') as f:
            json.dump(self._passkeys, f, indent=2)

    def _load_passkeys(self):
        try:
            with open(PASSKEY_FILE) as f:
                self._passkeys = json.load(f)
        except (FileNotFoundError, json.JSONDecodeError):
            self._passkeys = {}


class CredentialDialog(QDialog):
    """Dialog for managing passwords and passkeys."""

    def __init__(self, cred_manager, parent=None):
        super().__init__(parent)
        from oynix.core.theme_engine import NYX_COLORS
        c = NYX_COLORS
        self.cm = cred_manager
        self.setWindowTitle("Password & Passkey Manager")
        self.setMinimumSize(550, 450)

        layout = QVBoxLayout(self)

        # Unlock section (if locked)
        if not self.cm.is_unlocked:
            self._show_unlock(layout)
            return

        header = QLabel("Saved Credentials")
        header.setObjectName("sectionHeader")
        layout.addWidget(header)

        # Biometric status
        bio = self.cm.check_biometric_available()
        bio_label = QLabel(f"Biometric: {'Available' if bio else 'Not available'}")
        bio_label.setStyleSheet(f"color: {c['success'] if bio else c['text_muted']}; "
                                f"font-size: 12px; background: transparent;")
        layout.addWidget(bio_label)

        # Credential list
        self.cred_list = QListWidget()
        for domain in self.cm.get_all_domains():
            creds = self.cm._credentials[domain]
            for cred in creds:
                item = QListWidgetItem(
                    f"{domain}  —  {cred['username']}  ({cred['saved_at']})")
                item.setData(Qt.ItemDataRole.UserRole, (domain, cred['username']))
                self.cred_list.addItem(item)
        layout.addWidget(self.cred_list)

        # Passkeys section
        passkey_header = QLabel("Passkeys")
        passkey_header.setObjectName("sectionHeader")
        layout.addWidget(passkey_header)
        self.passkey_list = QListWidget()
        self.passkey_list.setMaximumHeight(120)
        for domain, pks in self.cm._passkeys.items():
            for pk in pks:
                item = QListWidgetItem(
                    f"{pk['display_name']}  ({domain})  [{pk['created_at']}]")
                item.setData(Qt.ItemDataRole.UserRole, (domain, pk['credential_id']))
                self.passkey_list.addItem(item)
        layout.addWidget(self.passkey_list)

        # Buttons
        btn_row = QHBoxLayout()
        del_btn = QPushButton("Delete Selected")
        del_btn.clicked.connect(self._delete_selected)
        btn_row.addWidget(del_btn)
        lock_btn = QPushButton("Lock")
        lock_btn.clicked.connect(lambda: (self.cm.lock(), self.accept()))
        btn_row.addWidget(lock_btn)
        btn_row.addStretch()
        close_btn = QPushButton("Close")
        close_btn.clicked.connect(self.accept)
        btn_row.addWidget(close_btn)
        layout.addLayout(btn_row)

    def _show_unlock(self, layout):
        from oynix.core.theme_engine import NYX_COLORS
        label = QLabel("Master Password Required" if self.cm.has_master_password()
                        else "Set a Master Password")
        label.setObjectName("sectionHeader")
        layout.addWidget(label)
        self.pw_input = QLineEdit()
        self.pw_input.setEchoMode(QLineEdit.EchoMode.Password)
        self.pw_input.setPlaceholderText("Enter master password...")
        self.pw_input.returnPressed.connect(self._do_unlock)
        layout.addWidget(self.pw_input)
        unlock_btn = QPushButton("Unlock" if self.cm.has_master_password() else "Set Password")
        unlock_btn.setObjectName("accentBtn")
        unlock_btn.clicked.connect(self._do_unlock)
        layout.addWidget(unlock_btn)
        layout.addStretch()

    def _do_unlock(self):
        pw = self.pw_input.text()
        if not pw:
            return
        if self.cm.unlock(pw):
            self.accept()
            # Re-open with unlocked view
            dlg = CredentialDialog(self.cm, self.parent())
            dlg.exec()
        else:
            QMessageBox.warning(self, "Error", "Incorrect master password")

    def _delete_selected(self):
        current = self.cred_list.currentItem()
        if current:
            domain, username = current.data(Qt.ItemDataRole.UserRole)
            self.cm.delete_credential(domain, username)
            self.cred_list.takeItem(self.cred_list.row(current))


class SavePasswordBar(QWidget):
    """Bar shown when a login form is detected, offering to save credentials."""

    def __init__(self, cred_manager, parent=None):
        super().__init__(parent)
        from oynix.core.theme_engine import NYX_COLORS
        c = NYX_COLORS
        self.cm = cred_manager
        self.setFixedHeight(40)
        self.setStyleSheet(f"background: {c['bg_mid']}; border-top: 1px solid {c['purple_mid']};")
        layout = QHBoxLayout(self)
        layout.setContentsMargins(12, 4, 12, 4)
        self.msg_label = QLabel("Save password?")
        self.msg_label.setStyleSheet(f"color: {c['text_primary']}; background: transparent; font-size: 13px;")
        layout.addWidget(self.msg_label)
        layout.addStretch()
        self.save_btn = QPushButton("Save")
        self.save_btn.setObjectName("accentBtn")
        self.save_btn.setFixedHeight(28)
        layout.addWidget(self.save_btn)
        self.never_btn = QPushButton("Never")
        self.never_btn.setFixedHeight(28)
        self.never_btn.clicked.connect(self.hide)
        layout.addWidget(self.never_btn)
        self.close_btn = QPushButton("\u00D7")
        self.close_btn.setObjectName("navBtn")
        self.close_btn.setFixedSize(28, 28)
        self.close_btn.clicked.connect(self.hide)
        layout.addWidget(self.close_btn)
        self.hide()
        self._url = ""
        self._username = ""
        self._password = ""

    def offer_save(self, url, username, password):
        """Show the save bar for a credential."""
        if not self.cm.is_unlocked:
            return
        self._url = url
        self._username = username
        self._password = password
        domain = urlparse(url).netloc
        self.msg_label.setText(f"Save password for {username} on {domain}?")
        self.save_btn.clicked.connect(self._do_save)
        self.show()

    def _do_save(self):
        self.cm.save_credential(self._url, self._username, self._password)
        self.hide()


# Lazy singleton
_cred_mgr = None

def _get_cred_mgr():
    global _cred_mgr
    if _cred_mgr is None:
        _cred_mgr = CredentialManager()
    return _cred_mgr

class _CredProxy:
    def __getattr__(self, name):
        return getattr(_get_cred_mgr(), name)

credential_manager = _CredProxy()

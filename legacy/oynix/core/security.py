"""
OyNIx Browser v1.0.0 - Security Module
Detects login pages and shows Nyx-themed security prompts.
"""

from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QLabel, QPushButton, QCheckBox
)
from PyQt6.QtCore import QUrl

from oynix.core.theme_engine import NYX_COLORS


class SecurityPromptDialog(QDialog):
    """Nyx-themed security prompt for sensitive pages."""

    def __init__(self, site_name, url, security_level="medium", parent=None):
        super().__init__(parent)
        self.setWindowTitle("OyNIx Security")
        self.setMinimumWidth(480)

        c = NYX_COLORS
        self.setStyleSheet(
            f"QDialog {{ background: {c['bg_dark']}; color: {c['text_primary']}; }}"
        )
        self._build(site_name, url, security_level, c)

    def _build(self, site_name, url, level, c):
        layout = QVBoxLayout(self)

        icons = {"high": "\U0001F534", "medium": "\U0001F7E1", "low": "\U0001F7E2"}
        icon = icons.get(level, "\U0001F535")

        title = QLabel(f"{icon} Security Check: {site_name}")
        title.setStyleSheet(
            f"font-size:16px; font-weight:bold; padding:10px; "
            f"color:{c['purple_light']}; background:transparent;"
        )
        layout.addWidget(title)

        url_label = QLabel(f"URL: {url}")
        url_label.setStyleSheet(
            f"color:{c['text_muted']}; padding:4px; background:transparent;"
        )
        layout.addWidget(url_label)

        if level == "high":
            text = (
                "This appears to be a login page!\n\n"
                "Before entering credentials:\n"
                "- Verify the URL is correct\n"
                "- Check for HTTPS (secure connection)\n"
                "- Be wary of phishing attempts"
            )
        elif level == "medium":
            text = (
                "This page may collect sensitive information.\n\n"
                "Please verify:\n"
                "- The URL matches the expected domain\n"
                "- Connection is secure (HTTPS)"
            )
        else:
            text = "This appears to be a safe page."

        warning = QLabel(text)
        warning.setWordWrap(True)
        warning.setStyleSheet(
            f"padding:14px; background:{c['bg_mid']}; "
            f"border-radius:8px; border:1px solid {c['border']}; "
            f"color:{c['text_primary']};"
        )
        layout.addWidget(warning)

        self.remember_choice = QCheckBox("Don't show again for this site")
        self.remember_choice.setStyleSheet(f"color:{c['text_secondary']};")
        layout.addWidget(self.remember_choice)

        proceed = QPushButton("Proceed to Site")
        proceed.clicked.connect(self.accept)
        proceed.setStyleSheet(
            f"background:{c['success']}; color:{c['bg_darkest']}; "
            f"padding:10px; border-radius:8px; font-weight:bold;"
        )
        layout.addWidget(proceed)

        cancel = QPushButton("Go Back")
        cancel.clicked.connect(self.reject)
        cancel.setStyleSheet(
            f"background:{c['error']}; color:{c['bg_darkest']}; "
            f"padding:10px; border-radius:8px; font-weight:bold;"
        )
        layout.addWidget(cancel)


class OynixSecurity:
    """Security manager for OyNIx Browser."""

    def __init__(self):
        self.trusted_domains = set()
        self.blocked_domains = set()
        self.security_checks_enabled = True

        self.login_patterns = {
            'microsoft': [
                'login.microsoftonline.com', 'login.live.com',
                'account.microsoft.com'
            ],
            'google': [
                'accounts.google.com', 'myaccount.google.com'
            ],
            'github': [
                'github.com/login', 'github.com/session'
            ],
            'facebook': [
                'facebook.com/login', 'm.facebook.com/login'
            ],
            'banking': [
                'bankofamerica.com', 'chase.com',
                'wellsfargo.com', 'citibank.com'
            ],
        }

    def check_url_security(self, url, parent_widget=None):
        if not self.security_checks_enabled:
            return (True, None)

        url_str = url.toString()
        domain = url.host()

        if domain in self.trusted_domains:
            return (True, None)
        if domain in self.blocked_domains:
            return (False, "Blocked domain")

        site_type, level = self._detect_sensitive(url_str, domain)

        if site_type:
            dialog = SecurityPromptDialog(
                site_type.replace('_', ' ').title(),
                url_str, level, parent_widget
            )
            result = dialog.exec()

            if dialog.remember_choice.isChecked():
                if result == QDialog.DialogCode.Accepted:
                    self.trusted_domains.add(domain)
                else:
                    self.blocked_domains.add(domain)

            return (result == QDialog.DialogCode.Accepted, site_type)

        return (True, None)

    def _detect_sensitive(self, url_str, domain):
        url_lower = url_str.lower()

        for pattern in self.login_patterns['microsoft']:
            if pattern in url_lower:
                return ("microsoft_login", "high")

        for pattern in self.login_patterns['google']:
            if pattern in url_lower:
                return ("google_login", "high")

        for pattern in self.login_patterns['banking']:
            if pattern in domain.lower():
                return ("banking", "high")

        if any(w in url_lower for w in ['login', 'signin', 'authenticate', 'password']):
            if not url_str.startswith('https://'):
                return ("insecure_login", "high")
            return ("generic_login", "medium")

        if any(w in url_lower for w in ['checkout', 'payment', 'billing', 'paypal']):
            return ("payment", "medium")

        return (None, "low")

    def add_trusted_domain(self, domain):
        self.trusted_domains.add(domain)

    def remove_trusted_domain(self, domain):
        self.trusted_domains.discard(domain)

    def enable_security_checks(self, enabled=True):
        self.security_checks_enabled = enabled


# Singleton
security_manager = OynixSecurity()

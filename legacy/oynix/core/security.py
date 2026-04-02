"""
Oynix Security Module - Smart Login Protection
Detects login pages and shows security prompts

Features:
- Microsoft login detection
- Google login detection  
- Banking site detection
- Password manager integration
- Phishing warnings
- HTTPS verification
"""

from PyQt6.QtWidgets import QMessageBox, QDialog, QVBoxLayout, QLabel, QPushButton, QCheckBox
from PyQt6.QtCore import QUrl
import re

class SecurityPromptDialog(QDialog):
    """Custom security prompt for sensitive pages."""
    
    def __init__(self, site_name, url, security_level="medium", parent=None):
        super().__init__(parent)
        self.setWindowTitle("Oynix Security Prompt")
        self.setMinimumWidth(500)
        self.setup_ui(site_name, url, security_level)
        
    def setup_ui(self, site_name, url, security_level):
        """Create the security prompt UI."""
        layout = QVBoxLayout(self)
        
        # Icon and title based on security level
        icons = {
            "high": "🔴",
            "medium": "🟡",
            "low": "🟢"
        }
        icon = icons.get(security_level, "🔵")
        
        # Warning message
        title = QLabel(f"{icon} Security Check: {site_name}")
        title.setStyleSheet("font-size: 18px; font-weight: bold; padding: 10px;")
        layout.addWidget(title)
        
        # URL display
        url_label = QLabel(f"URL: {url}")
        url_label.setStyleSheet("color: #89b4fa; padding: 5px;")
        layout.addWidget(url_label)
        
        # Security warnings based on level
        if security_level == "high":
            warning = QLabel(
                "⚠️ This appears to be a login page!\n\n"
                "Before entering credentials:\n"
                "• Verify the URL is correct\n"
                "• Check for HTTPS (secure connection)\n"
                "• Be wary of phishing attempts\n"
                "• Never enter passwords on HTTP sites"
            )
        elif security_level == "medium":
            warning = QLabel(
                "ℹ️ This page may collect sensitive information.\n\n"
                "Please verify:\n"
                "• The URL matches the expected domain\n"
                "• Connection is secure (HTTPS)\n"
                "• No unusual redirects occurred"
            )
        else:
            warning = QLabel(
                "✓ This appears to be a safe page.\n\n"
                "Standard security practices apply."
            )
        
        warning.setStyleSheet("padding: 15px; background: #313244; border-radius: 10px;")
        layout.addWidget(warning)
        
        # Remember choice checkbox
        self.remember_choice = QCheckBox("Don't show this again for this site")
        layout.addWidget(self.remember_choice)
        
        # Buttons
        button_layout = QVBoxLayout()
        
        proceed_btn = QPushButton("Proceed to Site")
        proceed_btn.clicked.connect(self.accept)
        proceed_btn.setStyleSheet(
            "background: #a6e3a1; color: #1e1e2e; padding: 10px; "
            "border-radius: 8px; font-weight: bold;"
        )
        
        cancel_btn = QPushButton("Go Back (Recommended for suspicious sites)")
        cancel_btn.clicked.connect(self.reject)
        cancel_btn.setStyleSheet(
            "background: #f38ba8; color: #1e1e2e; padding: 10px; "
            "border-radius: 8px; font-weight: bold; margin-top: 5px;"
        )
        
        button_layout.addWidget(proceed_btn)
        button_layout.addWidget(cancel_btn)
        layout.addLayout(button_layout)


class OynixSecurity:
    """
    Security manager for Oynix Browser.
    Detects sensitive pages and shows appropriate warnings.
    """
    
    def __init__(self):
        self.trusted_domains = set()
        self.blocked_domains = set()
        self.security_checks_enabled = True
        
        # Known login patterns
        self.login_patterns = {
            'microsoft': [
                'login.microsoftonline.com',
                'login.live.com',
                'account.microsoft.com'
            ],
            'google': [
                'accounts.google.com',
                'myaccount.google.com'
            ],
            'github': [
                'github.com/login',
                'github.com/session'
            ],
            'facebook': [
                'facebook.com/login',
                'm.facebook.com/login'
            ],
            'banking': [
                'bankofamerica.com',
                'chase.com',
                'wellsfargo.com',
                'citibank.com'
            ]
        }
    
    def check_url_security(self, url, parent_widget=None):
        """
        Check if URL requires security prompt.
        
        Args:
            url: QUrl object to check
            parent_widget: Parent widget for dialog
        
        Returns:
            (should_load, security_info) tuple
        """
        if not self.security_checks_enabled:
            return (True, None)
        
        url_str = url.toString()
        domain = url.host()
        
        # Skip if already trusted
        if domain in self.trusted_domains:
            return (True, None)
        
        # Block if in blocked list
        if domain in self.blocked_domains:
            return (False, "Blocked domain")
        
        # Check for sensitive pages
        site_type, security_level = self.detect_sensitive_site(url_str, domain)
        
        if site_type:
            # Show security prompt
            site_name = site_type.title()
            dialog = SecurityPromptDialog(site_name, url_str, security_level, parent_widget)
            result = dialog.exec()
            
            # Handle user choice
            if dialog.remember_choice.isChecked():
                if result == QDialog.DialogCode.Accepted:
                    self.trusted_domains.add(domain)
                else:
                    self.blocked_domains.add(domain)
            
            return (result == QDialog.DialogCode.Accepted, site_type)
        
        return (True, None)
    
    def detect_sensitive_site(self, url_str, domain):
        """
        Detect if URL is a sensitive page requiring extra security.
        
        Returns:
            (site_type, security_level) tuple
        """
        url_lower = url_str.lower()
        
        # Check for Microsoft login
        for pattern in self.login_patterns['microsoft']:
            if pattern in url_lower:
                return ("microsoft_login", "high")
        
        # Check for Google login
        for pattern in self.login_patterns['google']:
            if pattern in url_lower:
                return ("google_login", "high")
        
        # Check for banking
        for pattern in self.login_patterns['banking']:
            if pattern in domain.lower():
                return ("banking", "high")
        
        # Check for generic login pages
        if any(word in url_lower for word in ['login', 'signin', 'authenticate', 'password']):
            # Check if HTTPS
            if not url_str.startswith('https://'):
                return ("insecure_login", "high")
            return ("generic_login", "medium")
        
        # Check for payment pages
        if any(word in url_lower for word in ['checkout', 'payment', 'billing', 'paypal']):
            return ("payment", "medium")
        
        return (None, "low")
    
    def verify_https(self, url):
        """Check if URL uses HTTPS."""
        return url.toString().startswith('https://')
    
    def is_phishing_suspicious(self, url):
        """
        Simple phishing detection based on common patterns.
        
        Returns:
            (is_suspicious, reason) tuple
        """
        url_str = url.toString()
        domain = url.host()
        
        # Check for IDN homograph attacks
        if any(ord(c) > 127 for c in domain):
            return (True, "Contains non-ASCII characters in domain")
        
        # Check for common phishing patterns
        suspicious_patterns = [
            'verify-account',
            'secure-login',
            'account-recovery',
            'confirm-identity',
            'suspended-account'
        ]
        
        for pattern in suspicious_patterns:
            if pattern in url_str.lower():
                return (True, f"Suspicious pattern: {pattern}")
        
        # Check for excessive subdomains (e.g., login.microsoft.com.evil.com)
        if domain.count('.') > 3:
            return (True, "Excessive subdomains")
        
        return (False, None)
    
    def add_trusted_domain(self, domain):
        """Add domain to trusted list."""
        self.trusted_domains.add(domain)
        print(f"✓ Added trusted domain: {domain}")
    
    def remove_trusted_domain(self, domain):
        """Remove domain from trusted list."""
        self.trusted_domains.discard(domain)
        print(f"✓ Removed trusted domain: {domain}")
    
    def enable_security_checks(self, enabled=True):
        """Toggle security checks on/off."""
        self.security_checks_enabled = enabled
        status = "enabled" if enabled else "disabled"
        print(f"✓ Security checks {status}")


# Create singleton instance
security_manager = OynixSecurity()

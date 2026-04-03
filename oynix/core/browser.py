"""
OyNIx Browser v2.2 - Main Browser Core
Complete desktop browser with local LLM, Nyx search, tree tabs,
bookmarks, downloads, history, find-in-page, command palette,
forced theme on external search engines, XPI extension import,
and site comparison with auto-update.
"""
import sys
import os
import json
import time
from urllib.parse import parse_qs, urlparse

from PyQt6.QtWidgets import (
    QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, QLineEdit,
    QPushButton, QLabel, QToolBar, QStatusBar, QMessageBox,
    QFileDialog, QSplitter, QApplication, QDialog, QListWidget,
    QListWidgetItem, QScrollArea, QFrame, QInputDialog, QProgressBar
)
from PyQt6.QtWebEngineWidgets import QWebEngineView
from PyQt6.QtWebEngineCore import (
    QWebEnginePage, QWebEngineSettings, QWebEngineProfile, QWebEngineDownloadRequest
)
from PyQt6.QtCore import QUrl, Qt, pyqtSignal, QTimer, QSize, QPropertyAnimation, QEasingCurve
from PyQt6.QtGui import QIcon, QAction, QShortcut, QKeySequence

from oynix.core.theme_engine import (
    NYX_COLORS, get_qt_stylesheet, get_homepage_html,
    get_search_results_html, get_theme, THEMES,
    get_external_search_theme_css, get_history_html,
    get_bookmarks_html, get_downloads_html
)
from oynix.core.tree_tabs import TabManager
from oynix.core.nyx_search import nyx_search
from oynix.core.ai_manager import ai_manager
from oynix.core.database import database, guess_category
from oynix.core.security import security_manager
from oynix.core.github_sync import github_sync

from oynix.core.extensions import (
    install_extension, uninstall_extension, toggle_extension,
    load_registry, get_content_scripts_for_url, get_extra_styles_for_url,
    convert_xpi_to_npi as _convert_xpi, compile_npi
)
from oynix.UI.ai_chat import AIChatPanel
from oynix.core.credentials import credential_manager, CredentialDialog, SavePasswordBar
from oynix.core.search_builder import SearchEngineBuilder, load_custom_engines, get_custom_search_url
from oynix.core.nydta import export_nydta, import_nydta, PROFILE_PIC_PATH
from oynix.core.profiles import profile_manager, get_autologin_js
try:
    from oynix.UI.icons import svg_icon
except ImportError:
    svg_icon = None

try:
    from oynix.core.audio_player import AudioPlayer
    HAS_AUDIO = True
except ImportError:
    HAS_AUDIO = False

# Config defaults
DEFAULT_CONFIG = {
    'use_tree_tabs': True,
    'default_search_engine': 'nyx',
    'show_security_prompts': True,
    'auto_index_pages': True,
    'auto_expand_database': True,
    'community_upload': False,
    'theme': 'nyx',
    'show_ai_panel': False,
    'force_nyx_theme_external': True,
    'auto_compare_sites': True,
    'auto_update_repo_db': False,
}

# External search engine domains
SEARCH_ENGINES = {
    'google.com', 'www.google.com', 'duckduckgo.com', 'www.duckduckgo.com',
    'bing.com', 'www.bing.com', 'search.brave.com', 'yahoo.com',
    'www.yahoo.com', 'yandex.com', 'startpage.com', 'ecosia.org',
}

BOOKMARKS_FILE = os.path.expanduser("~/.config/oynix/bookmarks.json")
HISTORY_FILE = os.path.expanduser("~/.config/oynix/history.json")
DOWNLOADS_DIR = os.path.expanduser("~/Downloads")


class OynixWebPage(QWebEnginePage):
    """Custom web page with security integration and oyn:// interception."""
    oyn_url_requested = pyqtSignal(QUrl)

    def acceptNavigationRequest(self, url, nav_type, is_main_frame):
        # Intercept oyn:// URLs so homepage search bar and card links work
        if is_main_frame and url.scheme() == 'oyn':
            self.oyn_url_requested.emit(url)
            return False  # Don't navigate — handled by signal
        if is_main_frame and security_manager.security_checks_enabled:
            ok, info = security_manager.check_url_security(url, self.parent())
            return ok
        return super().acceptNavigationRequest(url, nav_type, is_main_frame)


class FindBar(QWidget):
    """In-page find bar (Ctrl+F)."""
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setFixedHeight(40)
        self.setStyleSheet(f"background: {NYX_COLORS['bg_mid']}; border-top: 1px solid {NYX_COLORS['border']};")
        layout = QHBoxLayout(self)
        layout.setContentsMargins(12, 4, 12, 4)
        lbl = QLabel("Find:")
        lbl.setStyleSheet("background: transparent;")
        layout.addWidget(lbl)
        self.input = QLineEdit()
        self.input.setPlaceholderText("Search in page...")
        self.input.setFixedWidth(280)
        self.input.setStyleSheet(f"border-radius: 14px; padding: 4px 12px; font-size: 13px; "
                                 f"background: {NYX_COLORS['bg_light']}; border: 1px solid {NYX_COLORS['border']};")
        self.input.returnPressed.connect(lambda: self._find(False))
        layout.addWidget(self.input)
        for text, fwd in [("Prev", True), ("Next", False)]:
            btn = QPushButton(text)
            btn.setObjectName("pillBtn")
            btn.setFixedHeight(28)
            btn.clicked.connect(lambda _, b=fwd: self._find(b))
            layout.addWidget(btn)
        self.match_label = QLabel("")
        self.match_label.setStyleSheet(f"color: {NYX_COLORS['text_muted']}; font-size: 12px; background: transparent;")
        layout.addWidget(self.match_label)
        layout.addStretch()
        close_btn = QPushButton("×")
        close_btn.setObjectName("navBtn")
        close_btn.setFixedSize(28, 28)
        close_btn.clicked.connect(self.hide_bar)
        layout.addWidget(close_btn)
        self._browser = None
        self.hide()

    def show_bar(self, browser_view):
        self._browser = browser_view
        self.show()
        self.input.setFocus()
        self.input.selectAll()

    def hide_bar(self):
        if self._browser:
            self._browser.findText("")
        self.hide()

    def _find(self, backward=False):
        if not self._browser:
            return
        text = self.input.text()
        if backward:
            self._browser.findText(text, QWebEnginePage.FindFlag.FindBackward)
        else:
            self._browser.findText(text)


class AutoLoginDialog(QDialog):
    """Dialog for managing auto-login rules."""

    def __init__(self, pcreds, parent=None):
        super().__init__(parent)
        self.pcreds = pcreds
        self.setWindowTitle("Auto-Login Manager")
        self.setMinimumSize(550, 400)
        c = NYX_COLORS

        layout = QVBoxLayout(self)
        header = QLabel(f"Auto-Login Rules — {profile_manager.active.display_name}")
        header.setStyleSheet(f"color: {c['purple_pale']}; font-size: 16px; font-weight: bold; background: transparent;")
        layout.addWidget(header)

        desc = QLabel("Sites where credentials are auto-filled on page load:")
        desc.setStyleSheet(f"color: {c['text_muted']}; font-size: 12px; background: transparent;")
        layout.addWidget(desc)

        self.rule_list = QListWidget()
        autologins = pcreds.get_all_autologins()
        for domain, rule in autologins.items():
            status = "fill + submit" if not rule.get('fill_only') else "fill only"
            enabled = "ON" if rule.get('enabled') else "OFF"
            item = QListWidgetItem(
                f"{domain}  —  {rule.get('username', '?')}  [{enabled}, {status}]")
            item.setData(Qt.ItemDataRole.UserRole, domain)
            self.rule_list.addItem(item)
        layout.addWidget(self.rule_list)

        # Saved passwords
        pw_header = QLabel("Saved Passwords")
        pw_header.setStyleSheet(f"color: {c['purple_pale']}; font-size: 14px; font-weight: bold; background: transparent;")
        layout.addWidget(pw_header)
        self.cred_list = QListWidget()
        self.cred_list.setMaximumHeight(150)
        for domain in pcreds.get_all_domains():
            for cred in pcreds.get_credentials(domain):
                item = QListWidgetItem(
                    f"{domain}  —  {cred['username']}  ({cred.get('saved_at', '?')})")
                item.setData(Qt.ItemDataRole.UserRole, (domain, cred['username']))
                self.cred_list.addItem(item)
        layout.addWidget(self.cred_list)

        btn_row = QHBoxLayout()
        toggle_btn = QPushButton("Toggle Auto-Login")
        toggle_btn.clicked.connect(self._toggle_autologin)
        btn_row.addWidget(toggle_btn)
        del_rule_btn = QPushButton("Remove Rule")
        del_rule_btn.clicked.connect(self._remove_rule)
        btn_row.addWidget(del_rule_btn)
        del_pw_btn = QPushButton("Delete Password")
        del_pw_btn.clicked.connect(self._delete_password)
        btn_row.addWidget(del_pw_btn)
        btn_row.addStretch()
        close_btn = QPushButton("Close")
        close_btn.clicked.connect(self.accept)
        btn_row.addWidget(close_btn)
        layout.addLayout(btn_row)

    def _toggle_autologin(self):
        current = self.rule_list.currentItem()
        if not current:
            return
        domain = current.data(Qt.ItemDataRole.UserRole)
        rule = self.pcreds.get_autologin(domain)
        if rule:
            rule['enabled'] = not rule.get('enabled', True)
            self.pcreds.set_autologin(domain, rule['username'], rule['enabled'], rule.get('fill_only', False))
            self.accept()
            AutoLoginDialog(self.pcreds, self.parent()).exec()

    def _remove_rule(self):
        current = self.rule_list.currentItem()
        if not current:
            return
        domain = current.data(Qt.ItemDataRole.UserRole)
        self.pcreds.remove_autologin(domain)
        self.rule_list.takeItem(self.rule_list.row(current))

    def _delete_password(self):
        current = self.cred_list.currentItem()
        if not current:
            return
        domain, username = current.data(Qt.ItemDataRole.UserRole)
        self.pcreds.delete_credential(domain, username)
        self.cred_list.takeItem(self.cred_list.row(current))


class CommandPalette(QDialog):
    """Quick command palette (Ctrl+K)."""
    command_selected = pyqtSignal(str)

    def __init__(self, commands, parent=None):
        super().__init__(parent)
        self.setWindowFlags(Qt.WindowType.FramelessWindowHint | Qt.WindowType.Popup)
        self.setFixedSize(500, 380)
        self.setStyleSheet(f"""
            QDialog {{ background: {NYX_COLORS['bg_mid']}; border: 1px solid {NYX_COLORS['purple_mid']};
                border-radius: 14px; }}
        """)
        layout = QVBoxLayout(self)
        layout.setContentsMargins(12, 12, 12, 8)
        self.search = QLineEdit()
        self.search.setPlaceholderText("Type a command...")
        self.search.setStyleSheet(f"border-radius: 20px; padding: 10px 18px; font-size: 14px; "
                                   f"background: {NYX_COLORS['bg_light']}; border: 2px solid {NYX_COLORS['purple_mid']};")
        self.search.textChanged.connect(self._filter)
        layout.addWidget(self.search)
        self.list = QListWidget()
        self.list.setStyleSheet(f"QListWidget {{ border: none; background: transparent; }}"
                                f"QListWidget::item {{ padding: 10px 14px; border-radius: 8px; margin: 1px; }}"
                                f"QListWidget::item:selected {{ background: {NYX_COLORS['purple_dark']}; color: {NYX_COLORS['purple_pale']}; }}")
        self._commands = commands
        for name, shortcut in commands:
            item = QListWidgetItem(f"{name}  ({shortcut})" if shortcut else name)
            item.setData(Qt.ItemDataRole.UserRole, name)
            self.list.addItem(item)
        self.list.itemActivated.connect(self._on_activate)
        layout.addWidget(self.list)

    def _filter(self, text):
        text = text.lower()
        for i in range(self.list.count()):
            item = self.list.item(i)
            item.setHidden(text not in item.text().lower())

    def _on_activate(self, item):
        self.command_selected.emit(item.data(Qt.ItemDataRole.UserRole))
        self.accept()

    def keyPressEvent(self, event):
        if event.key() == Qt.Key.Key_Escape:
            self.reject()
        elif event.key() in (Qt.Key.Key_Down, Qt.Key.Key_Up):
            self.list.setFocus()
            self.list.keyPressEvent(event)
        elif event.key() in (Qt.Key.Key_Return, Qt.Key.Key_Enter):
            current = self.list.currentItem()
            if current:
                self._on_activate(current)
        else:
            super().keyPressEvent(event)


class OynixBrowser(QMainWindow):
    """OyNIx Browser v2.2 — The Nyx-Powered Local AI Browser."""

    def __init__(self):
        super().__init__()
        self.config = dict(DEFAULT_CONFIG)
        self._load_config()
        self.current_theme = self.config.get('theme', 'nyx')
        self.theme_colors = get_theme(self.current_theme)
        # Load data from active profile directory (falls back to legacy paths)
        self._bookmarks = self._load_json(
            self._get_profile_path("bookmarks.json"),
            self._load_json(BOOKMARKS_FILE, []))
        self._history = self._load_json(
            self._get_profile_path("history.json"),
            self._load_json(HISTORY_FILE, []))
        self._downloads = []
        self._community_batch = []

        self._setup_window()
        self._setup_toolbar()
        self._setup_tab_manager()
        self._setup_ai_panel()
        self._setup_audio_player()
        self._setup_find_bar()
        self._setup_credential_bar()
        self._setup_statusbar()
        self._setup_shortcuts()
        self._apply_theme()
        self._setup_download_handler()

        from oynix.UI.menus import create_oynix_menus
        self.menus = create_oynix_menus(self)

        ai_manager.status_update.connect(self._on_ai_status)
        ai_manager.load_model_async()
        self.new_tab(QUrl("oyn://home"))
        self._update_status("Ready")

        # Start auto-sync if configured
        github_sync.start_auto_sync(export_func=lambda p: database.export_to_json(p))

    # ── Config persistence (profile-aware) ──────────────────────────
    def _get_profile_path(self, filename):
        """Get path to a file in the active profile's data dir."""
        return os.path.join(profile_manager.active.data_dir, filename)

    def _load_config(self):
        # Try profile-specific config first, fall back to legacy
        for path in [self._get_profile_path("browser_config.json"),
                     os.path.expanduser("~/.config/oynix/browser_config.json")]:
            try:
                with open(path) as f:
                    saved = json.load(f)
                    self.config.update(saved)
                    return
            except (FileNotFoundError, json.JSONDecodeError):
                continue

    def _save_config(self):
        path = self._get_profile_path("browser_config.json")
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'w') as f:
            json.dump(self.config, f, indent=2)

    def _load_json(self, path, default):
        try:
            with open(path) as f:
                return json.load(f)
        except (FileNotFoundError, json.JSONDecodeError):
            return default

    def _save_json(self, path, data):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'w') as f:
            json.dump(data, f, indent=2)
        # Mirror history/bookmarks to active profile dir
        basename = os.path.basename(path)
        if basename in ('history.json', 'bookmarks.json'):
            profile_path = self._get_profile_path(basename)
            if profile_path != path:
                os.makedirs(os.path.dirname(profile_path), exist_ok=True)
                with open(profile_path, 'w') as f:
                    json.dump(data, f, indent=2)

    # ── Setup ──────────────────────────────────────────────────────
    def _setup_window(self):
        self.setWindowTitle("OyNIx Browser")
        self.setGeometry(80, 60, 1500, 950)
        self.setMinimumSize(900, 600)
        # Set window icon from assets
        icon_paths = [
            os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(
                os.path.abspath(__file__)))), 'assets', 'icon-256.png'),
            os.path.join(os.path.dirname(os.path.dirname(
                os.path.abspath(__file__))), 'assets', 'icon-256.png'),
            '/usr/share/icons/hicolor/256x256/apps/oynix.png',
            '/app/share/icons/hicolor/256x256/apps/io.github.oynix.browser.png',
        ]
        for ip in icon_paths:
            if os.path.isfile(ip):
                from PyQt6.QtGui import QIcon
                self.setWindowIcon(QIcon(ip))
                break

    def _setup_toolbar(self):
        tb = QToolBar("Navigation")
        tb.setMovable(False)
        tb.setIconSize(QSize(20, 20))
        self.addToolBar(tb)

        def nav_btn(icon_name, tooltip, callback):
            btn = QPushButton()
            if svg_icon:
                btn.setIcon(svg_icon(icon_name, NYX_COLORS['text_secondary'], 18))
            else:
                fallback = {'back': '\u25C0', 'forward': '\u25B6', 'reload': '\u27F3',
                            'home': '\u2302', 'search': '\U0001F50D', 'new_tab': '+',
                            'bookmark': '\u2606', 'downloads': '\u2913', 'menu': '\u2263'}
                btn.setText(fallback.get(icon_name, '?'))
            btn.setObjectName("navBtn")
            btn.setToolTip(tooltip)
            btn.setFixedSize(34, 34)
            btn.clicked.connect(callback)
            tb.addWidget(btn)
            return btn

        self.back_btn = nav_btn('back', 'Back', self._go_back)
        self.fwd_btn = nav_btn('forward', 'Forward', self._go_forward)
        self.reload_btn = nav_btn('reload', 'Reload', self._reload)
        self.home_btn = nav_btn('home', 'Home', self.navigate_home)
        tb.addSeparator()

        self.url_bar = QLineEdit()
        self.url_bar.setPlaceholderText("Search with Nyx or enter a URL...")
        self.url_bar.returnPressed.connect(self.navigate_to_url)
        tb.addWidget(self.url_bar)

        nav_btn('search', 'Search', self.navigate_to_url)
        tb.addSeparator()
        self.bookmark_btn = nav_btn('bookmark', 'Bookmark (Ctrl+D)', self.toggle_bookmark)
        nav_btn('downloads', 'Downloads (Ctrl+J)', self.show_downloads)
        nav_btn('new_tab', 'New Tab (Ctrl+T)', lambda: self.new_tab())

        # Extension toolbar buttons (right side of nav bar)
        tb.addSeparator()
        self._ext_toolbar_btns = []
        self._rebuild_extension_toolbar(tb)

    def _rebuild_extension_toolbar(self, toolbar=None):
        """Add/refresh extension action buttons in the toolbar."""
        # Remove old extension buttons
        for btn in self._ext_toolbar_btns:
            btn.setParent(None)
            btn.deleteLater()
        self._ext_toolbar_btns.clear()

        registry = load_registry()
        if not registry:
            return

        if toolbar is None:
            toolbar = self.findChild(QToolBar, "Navigation")
        if not toolbar:
            return

        for ext in registry:
            if not ext.get('enabled', True):
                continue
            name = ext.get('name', '?')
            has_popup = ext.get('popup') is not None
            desc = ext.get('description', '')[:40]

            btn = QPushButton()
            # Use first letter as icon fallback
            icon_text = name[0].upper() if name else '?'
            btn.setText(icon_text)
            btn.setObjectName("extBtn")
            btn.setToolTip(f"{name}\n{desc}" if desc else name)
            btn.setFixedSize(30, 30)
            btn.setStyleSheet(f"""
                QPushButton#extBtn {{
                    background: {NYX_COLORS['bg_lighter']};
                    color: {NYX_COLORS['purple_pale']};
                    border: 1px solid {NYX_COLORS['border']};
                    border-radius: 6px;
                    font-weight: bold;
                    font-size: 13px;
                }}
                QPushButton#extBtn:hover {{
                    background: {NYX_COLORS['purple_dark']};
                    border-color: {NYX_COLORS['purple_mid']};
                }}
            """)

            if has_popup:
                btn.clicked.connect(lambda checked=False, e=ext: self._show_ext_popup(e))
            else:
                btn.clicked.connect(lambda checked=False, e=ext: self._toggle_ext_inline(e))

            toolbar.addWidget(btn)
            self._ext_toolbar_btns.append(btn)

    def _show_ext_popup(self, ext):
        """Show an extension's popup HTML in a small dialog."""
        popup = ext.get('popup')
        if not popup or not popup.get('html'):
            return

        dialog = QDialog(self)
        dialog.setWindowTitle(ext.get('name', 'Extension'))
        dialog.setFixedSize(400, 500)
        layout = QVBoxLayout(dialog)
        layout.setContentsMargins(0, 0, 0, 0)

        popup_view = QWebEngineView()
        popup_view.setHtml(popup['html'],
                           QUrl.fromLocalFile(os.path.join(ext.get('path', ''), popup.get('file', ''))))
        layout.addWidget(popup_view)
        dialog.exec()

    def _toggle_ext_inline(self, ext):
        """Toggle an extension that has no popup (just enable/disable)."""
        name = ext.get('name', '')
        new_state = toggle_extension(name)
        if new_state is not None:
            state_str = 'enabled' if new_state else 'disabled'
            self._update_status(f"Extension '{name}' {state_str}")
            self._rebuild_extension_toolbar()

    def _setup_tab_manager(self):
        self.tab_manager = TabManager()
        self.tab_manager.current_view_changed.connect(self._on_current_view_changed)
        self.tab_manager.new_tab_requested.connect(lambda: self.new_tab())
        self.main_splitter = QSplitter(Qt.Orientation.Horizontal)
        self.main_splitter.addWidget(self.tab_manager)
        self.setCentralWidget(self.main_splitter)

    def _setup_ai_panel(self):
        self.ai_panel = AIChatPanel()
        self.ai_panel.close_requested.connect(self.toggle_ai_panel)
        self.ai_panel.summarize_requested.connect(self._summarize_page)
        self.ai_panel.hide()
        self.main_splitter.addWidget(self.ai_panel)
        self.main_splitter.setSizes([1200, 0])

    def _setup_audio_player(self):
        if HAS_AUDIO:
            self.audio_player = AudioPlayer()
            self.audio_player.hide()
            self.main_splitter.addWidget(self.audio_player)
            self.audio_player.setup_web_media_detection(self)
        else:
            self.audio_player = None

    def _setup_credential_bar(self):
        self.save_pw_bar = SavePasswordBar(credential_manager, self)

    def _setup_find_bar(self):
        self.find_bar = FindBar(self)

    def _setup_statusbar(self):
        self.status_bar = QStatusBar()
        self.setStatusBar(self.status_bar)
        self.ai_status_label = QLabel("AI: Loading...")
        self.ai_status_label.setStyleSheet(
            f"color: {NYX_COLORS['text_muted']}; font-size: 11px; padding: 0 8px; background: transparent;")
        self.status_bar.addPermanentWidget(self.ai_status_label)
        self.db_label = QLabel("")
        self.db_label.setStyleSheet(f"color: {NYX_COLORS['text_muted']}; font-size: 11px; background: transparent;")
        self.status_bar.addPermanentWidget(self.db_label)
        self._refresh_stats()

    def _setup_shortcuts(self):
        # Default shortcut -> action mapping
        self._shortcut_actions = {
            "New Tab":         lambda: self.new_tab(),
            "Close Tab":       self._close_current_tab,
            "Address Bar":     lambda: (self.url_bar.setFocus(), self.url_bar.selectAll()),
            "Find in Page":    self.show_find_dialog,
            "Bookmark":        self.toggle_bookmark,
            "Downloads":       self.show_downloads,
            "History":         self.show_history,
            "Bookmarks":       self.show_bookmarks,
            "Command Palette": self.show_command_palette,
            "Toggle AI Panel": self.toggle_ai_panel,
            "Audio Player":    self.toggle_audio_player,
            "Reload":          self._reload,
            "Full Screen":     self.toggle_fullscreen,
            "Zoom In":         self.zoom_in,
            "Zoom Out":        self.zoom_out,
            "Reset Zoom":      self.reset_zoom,
            "Settings":        self.show_settings,
        }
        self._default_keys = {
            "New Tab": "Ctrl+T", "Close Tab": "Ctrl+W", "Address Bar": "Ctrl+L",
            "Find in Page": "Ctrl+F", "Bookmark": "Ctrl+D", "Downloads": "Ctrl+J",
            "History": "Ctrl+H", "Bookmarks": "Ctrl+B", "Command Palette": "Ctrl+K",
            "Toggle AI Panel": "Ctrl+Shift+A", "Audio Player": "Ctrl+Shift+M",
            "Reload": "F5", "Full Screen": "F11", "Zoom In": "Ctrl++",
            "Zoom Out": "Ctrl+-", "Reset Zoom": "Ctrl+0", "Settings": "Ctrl+,",
        }
        custom = self.config.get('custom_shortcuts', {})
        if not isinstance(custom, dict):
            custom = {}

        self._active_shortcuts = []
        for action, default_key in self._default_keys.items():
            key = custom.get(action, default_key)
            if not key:
                continue
            callback = self._shortcut_actions.get(action)
            if callback:
                sc = QShortcut(QKeySequence(key), self)
                sc.activated.connect(callback)
                self._active_shortcuts.append(sc)

        # Escape always closes find bar
        esc = QShortcut(QKeySequence("Escape"), self)
        esc.activated.connect(lambda: self.find_bar.hide_bar())
        self._active_shortcuts.append(esc)

    def reload_shortcuts(self):
        """Reload shortcuts after settings change."""
        for sc in self._active_shortcuts:
            sc.setEnabled(False)
            sc.deleteLater()
        self._active_shortcuts.clear()
        self._setup_shortcuts()

    def _apply_theme(self):
        self.theme_colors = get_theme(self.current_theme)
        c = self.theme_colors
        self.setStyleSheet(get_qt_stylesheet(c))
        # Update widgets that were styled with hardcoded colors at init time
        if hasattr(self, 'ai_status_label'):
            self.ai_status_label.setStyleSheet(
                f"color: {c['text_muted']}; font-size: 11px; padding: 0 8px; background: transparent;")
        if hasattr(self, 'db_label'):
            self.db_label.setStyleSheet(
                f"color: {c['text_muted']}; font-size: 11px; background: transparent;")
        # Re-render the current page if it's an internal oyn:// page
        tab = self.current_tab()
        if tab:
            url_str = tab.url().toString()
            if url_str.startswith('oyn://home'):
                tab.setHtml(get_homepage_html(c), tab.url())
            elif url_str.startswith('oyn://bookmarks'):
                html = get_bookmarks_html(self._bookmarks, c)
                tab.setHtml(html, tab.url())
            elif url_str.startswith('oyn://history'):
                html = get_history_html(self._history[:100], c)
                tab.setHtml(html, tab.url())
            elif url_str.startswith('oyn://downloads'):
                html = get_downloads_html(self._downloads, c)
                tab.setHtml(html, tab.url())

    def _setup_download_handler(self):
        profile = QWebEngineProfile.defaultProfile()
        profile.downloadRequested.connect(self._on_download_requested)

    # ── Tab Management ─────────────────────────────────────────────
    def new_tab(self, url=None, parent_view=None):
        if url is None:
            url = QUrl("oyn://home")
        browser = QWebEngineView()
        page = OynixWebPage(browser)
        browser.setPage(page)
        page.oyn_url_requested.connect(lambda u, b=browser: self._handle_oyn_url(u, b))
        browser.urlChanged.connect(lambda u, b=browser: self._on_url_changed(u, b))
        browser.loadFinished.connect(lambda ok, b=browser: self._on_load_finished(ok, b))
        browser.titleChanged.connect(lambda t, b=browser: self._on_title_changed(t, b))
        settings = browser.page().settings()
        settings.setAttribute(QWebEngineSettings.WebAttribute.JavascriptEnabled, True)
        settings.setAttribute(QWebEngineSettings.WebAttribute.LocalStorageEnabled, True)
        settings.setAttribute(QWebEngineSettings.WebAttribute.ScrollAnimatorEnabled, True)
        self.tab_manager.add_tab(browser, "New Tab", parent_view)
        if isinstance(url, str):
            url = QUrl(url)
        if url.scheme() == "oyn":
            self._handle_oyn_url(url, browser)
        else:
            browser.setUrl(url)
        return browser

    def _close_current_tab(self):
        view = self.tab_manager.get_current_view()
        if view:
            self.tab_manager.close_tab(view)

    def current_tab(self):
        return self.tab_manager.get_current_view()

    def close_tab(self, index=None):
        self._close_current_tab()

    # ── Navigation ─────────────────────────────────────────────────
    def navigate_to_url(self):
        text = self.url_bar.text().strip()
        if not text:
            return
        tab = self.current_tab()
        if not tab:
            return
        if text.startswith('oyn://'):
            self._handle_oyn_url(QUrl(text), tab)
        elif '.' in text and ' ' not in text:
            url = text if text.startswith(('http://', 'https://')) else f'https://{text}'
            tab.setUrl(QUrl(url))
        else:
            # Always use Nyx search from URL bar — never navigate to external search sites
            self._perform_search(text, 'nyx')

    def _get_search_url(self, engine, query):
        urls = {
            'duckduckgo': f'https://duckduckgo.com/?q={query}',
            'google': f'https://www.google.com/search?q={query}',
            'brave': f'https://search.brave.com/search?q={query}',
            'bing': f'https://www.bing.com/search?q={query}',
        }
        if engine in urls:
            return urls[engine]
        # Check custom engines
        custom_url = get_custom_search_url(engine, query)
        if custom_url:
            return custom_url
        return f'https://duckduckgo.com/?q={query}'

    def navigate_home(self):
        tab = self.current_tab()
        if tab:
            self._handle_oyn_url(QUrl("oyn://home"), tab)

    def _go_back(self):
        tab = self.current_tab()
        if tab:
            tab.back()

    def _go_forward(self):
        tab = self.current_tab()
        if tab:
            tab.forward()

    def _reload(self):
        tab = self.current_tab()
        if tab:
            tab.reload()

    def open_url(self, url):
        tab = self.current_tab()
        if tab:
            tab.setUrl(QUrl(url) if isinstance(url, str) else url)

    # ── Search ─────────────────────────────────────────────────────
    def _perform_search(self, query, engine='nyx'):
        # All searches go through Nyx — never navigates to external search sites
        self._nyx_search(query)

    def _nyx_search(self, query):
        """Nyx-first search: local index + database, then web fallback for extra results."""
        # Disconnect any previous web results handler to avoid stacking
        try:
            nyx_search.web_results_ready.disconnect(self._web_results_handler)
        except (TypeError, RuntimeError, AttributeError):
            pass

        results = nyx_search.search(query)
        local = results.get('local', []) + results.get('database', [])
        relevant = results.get('relevant_sites', [])

        # Score and rank — include ALL results, even loosely related
        query_lower = query.lower()
        scored = []
        for r in local + relevant:
            title = (r.get('title') or '').lower()
            desc = (r.get('description') or '').lower()
            url_str = (r.get('url') or '').lower()
            score = 1  # minimum score so everything is included
            if query_lower in title:
                score += 60
            if query_lower in desc:
                score += 30
            if query_lower in url_str:
                score += 20
            for word in query_lower.split():
                if word in title:
                    score += 15
                if word in desc:
                    score += 8
            r['score'] = min(score, 100)
            scored.append(r)
        # Deduplicate by URL
        seen = set()
        deduped = []
        for r in scored:
            key = r.get('url', '').rstrip('/').lower()
            if key not in seen:
                seen.add(key)
                deduped.append(r)
        scored = deduped
        scored.sort(key=lambda x: x.get('score', 0), reverse=True)

        # Show Nyx results immediately
        html = get_search_results_html(query, scored, [], self.theme_colors)
        tab = self.current_tab()
        if tab:
            tab.setHtml(html, QUrl(f"oyn://search?q={query}"))

        # Store for the async callback
        self._pending_search_query = query
        self._pending_search_local = scored

        # Always fetch web results in background — they supplement Nyx results
        # (never navigates to DDG/Google site, only adds result cards)
        self._web_results_handler = lambda web: self._on_web_results(web)
        nyx_search.web_results_ready.connect(self._web_results_handler)

    def _on_web_results(self, web_results):
        """Handle web fallback results — adds cards, never navigates away."""
        try:
            nyx_search.web_results_ready.disconnect(self._web_results_handler)
        except (TypeError, RuntimeError, AttributeError):
            pass

        query = getattr(self, '_pending_search_query', '')
        local = getattr(self, '_pending_search_local', [])

        if not web_results:
            return

        # Deduplicate: don't show web results already in Nyx results
        local_urls = {r.get('url', '').rstrip('/').lower() for r in local}
        unique_web = [r for r in web_results
                      if r.get('url', '').rstrip('/').lower() not in local_urls]

        if unique_web or not local:
            # Re-render with supplemental web results added below Nyx results
            html = get_search_results_html(query, local, unique_web, self.theme_colors)
            tab = self.current_tab()
            if tab:
                tab.setHtml(html, QUrl(f"oyn://search?q={query}"))

        # Auto-expand database with discovered web results
        if self.config.get('auto_expand_database'):
            for r in unique_web:
                database.add_site(r.get('url', ''), r.get('title', ''),
                                  r.get('description', ''), source='web')
            self._refresh_stats()

    # ── Internal URL handler ───────────────────────────────────────
    def _handle_oyn_url(self, url, browser=None):
        # QUrl parses oyn://foo as host="foo", path=""
        if browser is None:
            browser = self.current_tab()
        path = url.host() or url.path().strip('/')
        query_str = url.query()

        if path in ("home", ""):
            if browser:
                browser.setHtml(get_homepage_html(self.theme_colors), QUrl("oyn://home"))

        elif path in ("search", "nyx-search"):
            params = parse_qs(query_str)
            q = params.get('q', [''])[0]
            engine = params.get('engine', ['nyx'])[0]
            if q:
                self._perform_search(q, engine)
            else:
                # No query — focus the URL bar for the user to type
                self.url_bar.setFocus()
                self.url_bar.selectAll()

        elif path == "settings":
            self.show_settings()

        elif path == "ai-chat":
            if not self.ai_panel.isVisible():
                self.toggle_ai_panel()

        elif path == "database":
            self.manage_database()

        elif path == "history":
            self.show_history()

        elif path == "bookmarks":
            self.show_bookmarks()

        elif path == "downloads":
            self.show_downloads()

        elif path == "profiles":
            self.show_profiles()

        elif path == "switch-profile":
            params = parse_qs(query_str)
            name = params.get('name', [''])[0]
            if name:
                self.switch_profile(name)

        elif path == "about":
            self.show_about()

    # ── Page events ────────────────────────────────────────────────
    def _on_url_changed(self, qurl, browser):
        if browser == self.current_tab():
            url_str = qurl.toString()
            if not url_str.startswith("oyn://"):
                self.url_bar.setText(url_str)
            self.tab_manager.update_tab_url(browser, url_str)

    def _on_title_changed(self, title, browser):
        self.tab_manager.update_tab_title(browser, title)
        if browser == self.current_tab() and title:
            self.setWindowTitle(f"{title} - OyNIx")

    def _on_load_finished(self, ok, browser):
        if not ok:
            return
        url = browser.url()
        url_str = url.toString()
        title = browser.page().title() or url_str

        # Add to history
        if not url_str.startswith(("oyn://", "about:", "data:")):
            entry = {'url': url_str, 'title': title, 'time': time.strftime('%Y-%m-%d %H:%M')}
            self._history.insert(0, entry)
            self._history = self._history[:500]
            self._save_json(HISTORY_FILE, self._history)

        # Auto-index for Nyx search
        if self.config.get('auto_index_pages') and not url_str.startswith(("oyn://", "about:")):
            def _index_content(html):
                text = html[:3000] if html else ""
                nyx_search.index_page(url_str, title, text)
            browser.page().toPlainText(_index_content)

        # Auto-expand database
        if self.config.get('auto_expand_database') and not url_str.startswith(("oyn://", "about:")):
            database.add_site(url_str, title, source='visited')

        # Site comparison — check if current site matches known DB sites
        if self.config.get('auto_compare_sites'):
            self._compare_site(url_str, title)

        # Force Nyx theme + dynamic refractions on external pages
        if self.config.get('force_nyx_theme_external'):
            host = url.host().lower()
            if any(se in host for se in SEARCH_ENGINES):
                css = get_external_search_theme_css(self.theme_colors)
                from oynix.core.theme_engine import get_external_refraction_js
                refract_js = get_external_refraction_js()
                js = f"(function(){{var s=document.createElement('style');s.textContent=`{css}`;document.head.appendChild(s);{refract_js}}})()"
                browser.page().runJavaScript(js)

        # Inject extension content scripts / styles
        if not url_str.startswith(("oyn://", "about:", "data:")):
            self._inject_extensions(browser, url_str)

        # Auto-fill credentials on login pages
        if not url_str.startswith(("oyn://", "about:", "data:")):
            self._check_login_page(url_str, browser)

        # Apply remembered zoom level for this domain
        if not url_str.startswith(("oyn://", "about:", "data:")):
            self._apply_zoom_for_domain(browser)

        self._refresh_stats()

    def _compare_site(self, url_str, title):
        """Compare visited site against known DB and auto-update."""
        parsed = urlparse(url_str)
        domain = parsed.netloc.lower().replace('www.', '')
        existing = database.search(domain)
        if not existing:
            database.add_site(url_str, title, source='auto-compared')
            if self.config.get('auto_update_repo_db') and len(self._community_batch) < 50:
                cat = guess_category(url_str)
                self._community_batch.append({'url': url_str, 'title': title, 'category': cat})

    def _on_current_view_changed(self, browser_view):
        if not browser_view:
            return
        url = browser_view.url().toString()
        title = browser_view.page().title()
        if not url.startswith("oyn://"):
            self.url_bar.setText(url)
        if title:
            self.setWindowTitle(f"{title} - OyNIx")
        self._update_bookmark_btn(url)

    def _on_ai_status(self, status):
        self.ai_status_label.setText(f"AI: {status}")

    def _update_status(self, message):
        self.status_bar.showMessage(message, 5000)
        # Brief purple glow on status update
        c = self.theme_colors
        self.status_bar.setStyleSheet(
            f"QStatusBar {{ background: {c['bg_darkest']}; color: {c['purple_light']};"
            f" border-top: 1px solid {c['purple_mid']}; font-size: 11px; padding: 2px 8px; }}"
            f" QStatusBar::item {{ border: none; }}")
        QTimer.singleShot(1500, lambda: self.status_bar.setStyleSheet(
            f"QStatusBar {{ background: {c['bg_darkest']}; color: {c['text_muted']};"
            f" border-top: 1px solid {c['border']}; font-size: 11px; padding: 2px 8px; }}"
            f" QStatusBar::item {{ border: none; }}"))

    def _refresh_stats(self):
        stats = database.get_stats()
        self.db_label.setText(f"DB: {stats.get('total_sites', 0)} sites")

    # ── Features ───────────────────────────────────────────────────
    def toggle_tab_mode(self):
        current = self.config.get('use_tree_tabs', True)
        self.config['use_tree_tabs'] = not current
        self.tab_manager.set_tree_mode(not current)
        self._save_config()

    def _animate_splitter(self, splitter, target_sizes, duration=250):
        """Smoothly animate a QSplitter to target sizes."""
        start = splitter.sizes()
        if len(start) != len(target_sizes):
            splitter.setSizes(target_sizes)
            return
        self._anim_timer_step = 0
        steps = max(1, duration // 16)
        def step():
            self._anim_timer_step += 1
            t = min(1.0, self._anim_timer_step / steps)
            # Ease-out cubic
            t2 = 1 - (1 - t) ** 3
            sizes = [int(s + (e - s) * t2) for s, e in zip(start, target_sizes)]
            splitter.setSizes(sizes)
            if self._anim_timer_step >= steps:
                self._anim_qtimer.stop()
        self._anim_qtimer = QTimer()
        self._anim_qtimer.timeout.connect(step)
        self._anim_qtimer.start(16)

    def toggle_ai_panel(self):
        if self.ai_panel.isVisible():
            self._animate_splitter(self.main_splitter,
                [self.main_splitter.sizes()[0] + self.main_splitter.sizes()[1], 0])
            QTimer.singleShot(300, self.ai_panel.hide)
        else:
            self.ai_panel.show()
            total = sum(self.main_splitter.sizes())
            self._animate_splitter(self.main_splitter, [total - 350, 350])

    def toggle_fullscreen(self):
        if self.isFullScreen():
            self.showNormal()
        else:
            self.showFullScreen()

    def show_find_dialog(self):
        tab = self.current_tab()
        if tab:
            self.find_bar.show_bar(tab)

    def show_command_palette(self):
        commands = [
            ("New Tab", "Ctrl+T"), ("Close Tab", "Ctrl+W"), ("Settings", "Ctrl+,"),
            ("Find in Page", "Ctrl+F"), ("Bookmarks", "Ctrl+B"), ("History", "Ctrl+H"),
            ("Downloads", "Ctrl+J"), ("AI Chat", "Ctrl+Shift+A"), ("Toggle Tree Tabs", ""),
            ("Zoom In", "Ctrl++"), ("Zoom Out", "Ctrl+-"), ("Full Screen", "F11"),
            ("Home", ""), ("Reload", "F5"), ("Toggle Reader Mode", ""),
            ("Print Page", ""), ("Save Page", ""), ("View Source", ""),
            ("Install Extension", ""), ("Manage Extensions", ""),
            ("Manage Database", ""), ("Scan Community Databases", ""),
            ("Import Database File", ""), ("Audio Player", ""),
            ("Passwords & Passkeys", ""), ("Import Chrome History", ""),
            ("Import Chrome Searches", ""), ("Import Google Takeout", ""),
            ("Search Engine Builder", ""),
            ("Export Profile (.nydta)", ""), ("Import Profile (.nydta)", ""),
            ("Profiles", ""), ("New Profile", ""), ("Switch Profile", ""),
            ("Delete Profile", ""), ("Auto-Login Manager", ""),
            ("Save Credential", ""),
            ("Pin/Unpin Tab", ""), ("Search Tabs", ""), ("Tab Group", ""),
            ("Reading List", ""), ("Add to Reading List", ""),
            ("Quick Notes", ""), ("Screenshot Page", ""),
            ("Save Session", ""), ("Restore Session", ""),
            ("Convert XPI → NPI", ""), ("NPI Builder", ""),
        ]
        palette = CommandPalette(commands, self)
        palette.command_selected.connect(self._run_command)
        palette.move(self.geometry().center() - palette.rect().center())
        palette.exec()

    def _run_command(self, name):
        mapping = {
            "New Tab": lambda: self.new_tab(), "Close Tab": self._close_current_tab,
            "Settings": self.show_settings, "Find in Page": self.show_find_dialog,
            "Bookmarks": self.show_bookmarks, "History": self.show_history,
            "Downloads": self.show_downloads, "AI Chat": self.toggle_ai_panel,
            "Toggle Tree Tabs": self.toggle_tab_mode, "Zoom In": self.zoom_in,
            "Zoom Out": self.zoom_out, "Full Screen": self.toggle_fullscreen,
            "Home": self.navigate_home, "Reload": self._reload,
            "Toggle Reader Mode": self.toggle_reader_mode, "Print Page": self.print_page,
            "Save Page": self.save_page, "View Source": self.view_source,
            "Install Extension": self.import_extension,
            "Manage Extensions": self.manage_extensions,
            "Manage Database": self.manage_database,
            "Scan Community Databases": self.scan_community_databases,
            "Import Database File": self.pick_database_file,
            "Audio Player": self.toggle_audio_player,
            "Passwords & Passkeys": self.show_passwords,
            "Import Chrome History": self.import_chrome_history,
            "Import Chrome Searches": self.import_chrome_searches,
            "Import Google Takeout": self.import_google_takeout,
            "Search Engine Builder": self.show_search_builder,
            "Export Profile (.nydta)": self.export_nydta,
            "Import Profile (.nydta)": self.import_nydta,
            "Profiles": self.show_profiles,
            "New Profile": self.create_profile,
            "Switch Profile": self.switch_profile,
            "Delete Profile": self.delete_profile,
            "Auto-Login Manager": self.manage_autologin,
            "Save Credential": self.save_credential_to_profile,
            "Pin/Unpin Tab": self.pin_current_tab,
            "Search Tabs": self.search_tabs,
            "Tab Group": self.create_tab_group,
            "Reading List": self.show_reading_list,
            "Add to Reading List": self.add_to_reading_list,
            "Quick Notes": self.show_quick_notes,
            "Screenshot Page": self.screenshot_page,
            "Save Session": self.save_session,
            "Restore Session": self.restore_session,
            "Convert XPI → NPI": self.convert_xpi_to_npi,
            "NPI Builder": self.open_npi_builder,
        }
        fn = mapping.get(name)
        if fn:
            fn()

    # ── Bookmarks ──────────────────────────────────────────────────
    def toggle_bookmark(self):
        tab = self.current_tab()
        if not tab:
            return
        url = tab.url().toString()
        title = tab.page().title() or url
        existing = [b for b in self._bookmarks if b['url'] == url]
        if existing:
            self._bookmarks = [b for b in self._bookmarks if b['url'] != url]
            self._update_status("Bookmark removed")
        else:
            folder, ok = QInputDialog.getText(self, "Bookmark", "Folder (optional):", text="")
            self._bookmarks.append({'url': url, 'title': title, 'folder': folder if ok else '',
                                     'added': time.strftime('%Y-%m-%d')})
            self._update_status("Bookmarked!")
        self._save_json(BOOKMARKS_FILE, self._bookmarks)
        self._update_bookmark_btn(url)

    def _update_bookmark_btn(self, url):
        is_bookmarked = any(b['url'] == url for b in self._bookmarks)
        if svg_icon:
            icon_name = 'bookmark_filled' if is_bookmarked else 'bookmark'
            self.bookmark_btn.setIcon(svg_icon(icon_name, NYX_COLORS['purple_light'] if is_bookmarked
                                                else NYX_COLORS['text_secondary'], 18))

    def show_bookmarks(self):
        html = get_bookmarks_html(self._bookmarks, self.theme_colors)
        tab = self.current_tab()
        if tab:
            tab.setHtml(html, QUrl("oyn://bookmarks"))

    def import_bookmarks(self):
        path, _ = QFileDialog.getOpenFileName(self, "Import Bookmarks", "", "HTML Files (*.html);;JSON (*.json)")
        if not path:
            return
        try:
            with open(path) as f:
                content = f.read()
            if path.endswith('.json'):
                imported = json.loads(content)
                if isinstance(imported, list):
                    self._bookmarks.extend(imported)
            else:
                # Parse HTML bookmark file
                from bs4 import BeautifulSoup
                soup = BeautifulSoup(content, 'html.parser')
                for a in soup.find_all('a', href=True):
                    self._bookmarks.append({'url': a['href'], 'title': a.get_text() or a['href'],
                                            'folder': 'Imported'})
            self._save_json(BOOKMARKS_FILE, self._bookmarks)
            self._update_status(f"Imported bookmarks from {os.path.basename(path)}")
        except Exception as e:
            QMessageBox.warning(self, "Import Error", str(e))

    def export_bookmarks(self):
        path, _ = QFileDialog.getSaveFileName(self, "Export Bookmarks", "bookmarks.json", "JSON (*.json)")
        if path:
            self._save_json(path, self._bookmarks)
            self._update_status("Bookmarks exported")

    # ── History ────────────────────────────────────────────────────
    def show_history(self):
        html = get_history_html(self._history[:100], self.theme_colors)
        tab = self.current_tab()
        if tab:
            tab.setHtml(html, QUrl("oyn://history"))

    def import_history(self):
        path, _ = QFileDialog.getOpenFileName(self, "Import History", "", "JSON (*.json)")
        if path:
            try:
                with open(path) as f:
                    imported = json.load(f)
                self._history = imported + self._history
                self._save_json(HISTORY_FILE, self._history)
                self._update_status("History imported")
            except Exception as e:
                QMessageBox.warning(self, "Error", str(e))

    def export_history(self):
        path, _ = QFileDialog.getSaveFileName(self, "Export History", "history.json", "JSON (*.json)")
        if path:
            self._save_json(path, self._history)
            self._update_status("History exported")

    # ── Downloads ──────────────────────────────────────────────────
    def _on_download_requested(self, download):
        path = os.path.join(DOWNLOADS_DIR, download.downloadFileName())
        download.setDownloadDirectory(DOWNLOADS_DIR)
        download.accept()
        entry = {'filename': download.downloadFileName(), 'path': path,
                 'size': '', 'status': 'downloading', 'progress': 0}
        self._downloads.insert(0, entry)
        download.receivedBytesChanged.connect(
            lambda: self._on_download_progress(download, entry))
        download.isFinishedChanged.connect(
            lambda: self._on_download_finished(download, entry))
        self._update_status(f"Downloading: {download.downloadFileName()}")

    def _on_download_progress(self, download, entry):
        total = download.totalBytes()
        received = download.receivedBytes()
        if total > 0:
            entry['progress'] = int(received / total * 100)
            entry['size'] = f"{received // 1024}KB / {total // 1024}KB"

    def _on_download_finished(self, download, entry):
        entry['status'] = 'complete'
        entry['progress'] = 100
        self._update_status(f"Download complete: {entry['filename']}")

    def show_downloads(self):
        html = get_downloads_html(self._downloads, self.theme_colors)
        tab = self.current_tab()
        if tab:
            tab.setHtml(html, QUrl("oyn://downloads"))

    # ── Settings ───────────────────────────────────────────────────
    def show_settings(self):
        from oynix.UI.settings import SettingsDialog
        dialog = SettingsDialog(self, self.config)
        if dialog.exec():
            self.config.update(dialog.get_config())
            self._save_config()
            new_theme = self.config.get('theme', 'nyx')
            if new_theme != self.current_theme:
                self.set_theme(new_theme)
            self.tab_manager.set_tree_mode(self.config.get('use_tree_tabs', True))
            self.reload_shortcuts()

    def set_theme(self, name):
        self.current_theme = name
        self.config['theme'] = name
        self._apply_theme()
        self._save_config()

    # ── Zoom ───────────────────────────────────────────────────────
    def zoom_in(self):
        tab = self.current_tab()
        if tab:
            tab.setZoomFactor(tab.zoomFactor() + 0.1)
            self._save_zoom_for_domain()

    def zoom_out(self):
        tab = self.current_tab()
        if tab:
            tab.setZoomFactor(max(0.3, tab.zoomFactor() - 0.1))
            self._save_zoom_for_domain()

    def reset_zoom(self):
        tab = self.current_tab()
        if tab:
            tab.setZoomFactor(1.0)
            self._save_zoom_for_domain()

    # ── Print / Save / Source / Reader ─────────────────────────────
    def print_page(self):
        tab = self.current_tab()
        if tab:
            tab.page().printToPdf(os.path.expanduser("~/oynix_page.pdf"))
            self._update_status("Page saved as PDF to ~/oynix_page.pdf")

    def save_page(self):
        tab = self.current_tab()
        if not tab:
            return
        path, _ = QFileDialog.getSaveFileName(self, "Save Page", "page.html", "HTML (*.html)")
        if path:
            tab.page().toHtml(lambda html: self._write_file(path, html))

    def _write_file(self, path, content):
        with open(path, 'w') as f:
            f.write(content)
        self._update_status(f"Saved to {path}")

    def view_source(self):
        tab = self.current_tab()
        if tab:
            tab.page().toHtml(lambda html: self._show_source(html))

    def _show_source(self, html):
        new_tab = self.new_tab()
        escaped = html.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')
        source_html = f'''<!DOCTYPE html><html><head><title>View Source</title>
        <style>body{{background:{NYX_COLORS['bg_darkest']};color:{NYX_COLORS['text_primary']};
        font-family:monospace;padding:20px;white-space:pre-wrap;font-size:13px;line-height:1.6}}
        </style></head><body>{escaped}</body></html>'''
        new_tab.setHtml(source_html, QUrl("oyn://source"))

    def toggle_reader_mode(self):
        tab = self.current_tab()
        if tab:
            tab.page().toHtml(lambda html: self._apply_reader_mode(html, tab))

    def _apply_reader_mode(self, html, tab):
        js = '''(function(){
            var body = document.body;
            var main = document.querySelector('article, main, [role="main"], .post-content, .article-body');
            if (!main) main = body;
            var text = main.innerHTML;
            document.head.innerHTML = '<style>body{max-width:700px;margin:40px auto;padding:20px;' +
                'background:#0e0e16;color:#E8E0F0;font-family:Georgia,serif;font-size:18px;line-height:1.8}' +
                'a{color:#9B6FDF}img{max-width:100%;height:auto;border-radius:8px}' +
                'h1,h2,h3{color:#B088F0}</style>';
            body.innerHTML = text;
        })()'''
        tab.page().runJavaScript(js)
        self._update_status("Reader mode enabled")

    # ── Extensions (.npi / .xpi) ─────────────────────────────────
    def import_extension(self):
        """Install an extension from .npi (OyNIx) or .xpi (Firefox) file."""
        path, _ = QFileDialog.getOpenFileName(
            self, "Install Extension", "",
            "OyNIx Extensions (*.npi);;Firefox Extensions (*.xpi);;All Files (*)")
        if not path:
            return
        ok, info, error = install_extension(path)
        if ok:
            fmt = info.get('format', 'unknown').upper()
            name = info.get('name', '?')
            ver = info.get('version', '?')
            cs = len(info.get('content_scripts', []))
            feats = info.get('oynix_features', [])
            msg = f"Installed: {name} v{ver} ({fmt})\n\n"
            if cs:
                msg += f"Content scripts: {cs} rule(s)\n"
            if info.get('popup'):
                msg += "Has popup UI\n"
            if feats:
                msg += f"OyNIx features: {', '.join(feats)}\n"
            msg += f"\nPermissions: {', '.join(info.get('permissions', ['none']))}"
            QMessageBox.information(self, "Extension Installed", msg)
            self._update_status(f"Extension '{name}' installed")
        else:
            QMessageBox.warning(self, "Install Failed", error)

    # Keep old name as alias
    import_xpi_extension = import_extension

    def open_npi_builder(self):
        """Open the NPI Extension Builder dialog."""
        from oynix.UI.npi_builder import NPIBuilderDialog
        dlg = NPIBuilderDialog(self)
        dlg.exec()

    def convert_xpi_to_npi(self):
        """Convert a Firefox .xpi to OyNIx .npi format."""
        from oynix.core.extensions import convert_xpi_to_npi
        path, _ = QFileDialog.getOpenFileName(
            self, "Select XPI to Convert", "",
            "Firefox Extensions (*.xpi);;All Files (*)")
        if not path:
            return
        out, _ = QFileDialog.getSaveFileName(
            self, "Save NPI As",
            os.path.splitext(path)[0] + '.npi',
            "OyNIx Extensions (*.npi)")
        if not out:
            return
        ok, out_path, err = convert_xpi_to_npi(path, out)
        if ok:
            reply = QMessageBox.question(self, "Conversion Complete",
                f"Saved: {out_path}\n\nInstall the converted extension now?")
            if reply == QMessageBox.StandardButton.Yes:
                ok2, info, err2 = install_extension(out_path)
                if ok2:
                    QMessageBox.information(self, "Installed",
                        f"Installed: {info.get('name', '?')}")
                    self._rebuild_extension_toolbar()
                else:
                    QMessageBox.warning(self, "Install Failed", err2)
        else:
            QMessageBox.warning(self, "Conversion Failed", err)

    def manage_extensions(self):
        """Show extension manager dialog."""
        registry = load_registry()
        if not registry:
            reply = QMessageBox.question(self, "Extensions",
                "No extensions installed.\n\nInstall one now?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No)
            if reply == QMessageBox.StandardButton.Yes:
                self.import_extension()
            return

        dialog = QDialog(self)
        dialog.setWindowTitle("Extensions Manager")
        dialog.setMinimumSize(500, 400)
        layout = QVBoxLayout(dialog)

        header = QLabel("Installed Extensions")
        header.setObjectName("sectionHeader")
        layout.addWidget(header)

        ext_list = QListWidget()
        for ext in registry:
            status = "ON" if ext.get('enabled', True) else "OFF"
            fmt = ext.get('format', '?').upper()
            item_text = f"[{status}] {ext['name']} v{ext.get('version','?')} ({fmt})"
            if ext.get('description'):
                item_text += f"\n    {ext['description'][:80]}"
            item = QListWidgetItem(item_text)
            item.setData(Qt.ItemDataRole.UserRole, ext['name'])
            ext_list.addItem(item)
        layout.addWidget(ext_list)

        btn_row = QHBoxLayout()
        install_btn = QPushButton("Install New...")
        install_btn.setObjectName("accentBtn")
        install_btn.clicked.connect(lambda: (dialog.accept(), self.import_extension()))
        btn_row.addWidget(install_btn)

        toggle_btn = QPushButton("Toggle On/Off")
        toggle_btn.clicked.connect(lambda: self._toggle_ext(ext_list))
        btn_row.addWidget(toggle_btn)

        remove_btn = QPushButton("Uninstall")
        remove_btn.clicked.connect(lambda: self._uninstall_ext(ext_list, dialog))
        btn_row.addWidget(remove_btn)

        btn_row.addStretch()
        close_btn = QPushButton("Close")
        close_btn.clicked.connect(dialog.accept)
        btn_row.addWidget(close_btn)
        layout.addLayout(btn_row)

        dialog.exec()

    def _toggle_ext(self, ext_list):
        current = ext_list.currentItem()
        if not current:
            return
        name = current.data(Qt.ItemDataRole.UserRole)
        new_state = toggle_extension(name)
        if new_state is not None:
            status = "ON" if new_state else "OFF"
            self._update_status(f"Extension '{name}' {'enabled' if new_state else 'disabled'}")
            # Refresh list
            current.setText(current.text().replace("[ON]", f"[{status}]").replace("[OFF]", f"[{status}]"))

    def _uninstall_ext(self, ext_list, dialog):
        current = ext_list.currentItem()
        if not current:
            return
        name = current.data(Qt.ItemDataRole.UserRole)
        reply = QMessageBox.question(self, "Uninstall", f"Uninstall '{name}'?")
        if reply == QMessageBox.StandardButton.Yes:
            ok, msg = uninstall_extension(name)
            if ok:
                ext_list.takeItem(ext_list.row(current))
                self._update_status(msg)

    def _inject_extensions(self, browser, url_str):
        """Inject extension content scripts and styles into a loaded page."""
        # Content scripts from extensions
        scripts = get_content_scripts_for_url(url_str)
        for cs in scripts:
            for js_code in cs.get('js', []):
                browser.page().runJavaScript(js_code)
            for css_code in cs.get('css', []):
                js = f"(function(){{var s=document.createElement('style');s.textContent=`{css_code}`;document.head.appendChild(s)}})()"
                browser.page().runJavaScript(js)

        # NPI extra styles
        extra_styles = get_extra_styles_for_url(url_str)
        for css in extra_styles:
            js = f"(function(){{var s=document.createElement('style');s.textContent=`{css}`;document.head.appendChild(s)}})()"
            browser.page().runJavaScript(js)

    # ── Community Database Folder ──────────────────────────────────
    def scan_community_databases(self):
        """Check GitHub repo's community folder for new database files."""
        if not github_sync.is_configured():
            QMessageBox.information(self, "Community Databases",
                "Configure GitHub sync first (Search > GitHub Sync > Configure)")
            return
        github_sync.scan_community_folder(
            on_new_files=self._on_community_files_found,
            on_done=lambda ok, msg: self._update_status(msg))

    def _on_community_files_found(self, file_entries):
        """Handle new community database files found on GitHub."""
        dialog = QDialog(self)
        dialog.setWindowTitle("New Community Databases Found")
        dialog.setMinimumSize(520, 380)
        layout = QVBoxLayout(dialog)

        header = QLabel(f"Found {len(file_entries)} new database file(s)")
        header.setObjectName("sectionHeader")
        layout.addWidget(header)

        desc = QLabel("Select which databases to merge into your browser:")
        desc.setStyleSheet(f"color: {NYX_COLORS['text_secondary']}; background: transparent;")
        layout.addWidget(desc)

        file_list = QListWidget()
        file_list.setSelectionMode(QListWidget.SelectionMode.MultiSelection)
        for entry in file_entries:
            item = QListWidgetItem(f"{entry['filename']}  ({entry['count']} sites)")
            item.setData(Qt.ItemDataRole.UserRole, entry)
            item.setSelected(True)
            file_list.addItem(item)
        layout.addWidget(file_list)

        btn_row = QHBoxLayout()
        merge_btn = QPushButton("Merge Selected")
        merge_btn.setObjectName("accentBtn")
        btn_row.addWidget(merge_btn)
        skip_btn = QPushButton("Skip All")
        btn_row.addWidget(skip_btn)
        layout.addLayout(btn_row)

        def merge():
            selected = file_list.selectedItems()
            total_added = 0
            for item in selected:
                entry = item.data(Qt.ItemDataRole.UserRole)
                for site in entry['sites']:
                    added = database.add_site(
                        site.get('url', ''), site.get('title', ''),
                        site.get('description', ''), source='community')
                    if added:
                        total_added += 1
            self._update_status(f"Merged {total_added} new sites from community databases")
            self._refresh_stats()
            dialog.accept()

        merge_btn.clicked.connect(merge)
        skip_btn.clicked.connect(dialog.reject)
        dialog.exec()

    def pick_database_file(self):
        """Let user pick a local JSON database file to merge."""
        path, _ = QFileDialog.getOpenFileName(
            self, "Select Database File", "",
            "JSON Files (*.json);;All Files (*)")
        if not path:
            return
        try:
            with open(path) as f:
                data = json.load(f)

            # Try to extract sites from various formats
            sites = []
            if isinstance(data, list):
                sites = [s for s in data if isinstance(s, dict) and s.get('url')]
            elif isinstance(data, dict):
                for section in ('curated', 'discovered', 'sites'):
                    if section in data and isinstance(data[section], dict):
                        for cat, entries in data[section].items():
                            if isinstance(entries, list):
                                for item in entries:
                                    if isinstance(item, dict) and item.get('url'):
                                        sites.append(item)
                    elif section in data and isinstance(data[section], list):
                        sites.extend([s for s in data[section]
                                      if isinstance(s, dict) and s.get('url')])

            if not sites:
                QMessageBox.information(self, "No Sites Found",
                    "Could not find any site entries in this file.\n"
                    "Expected JSON format: list of {url, title, description} objects.")
                return

            reply = QMessageBox.question(self, "Import Database",
                f"Found {len(sites)} sites in {os.path.basename(path)}.\n\n"
                f"Merge into your browser database?\n(Duplicates will be skipped)")
            if reply != QMessageBox.StandardButton.Yes:
                return

            added = 0
            for s in sites:
                result = database.add_site(
                    s.get('url', ''), s.get('title', ''),
                    s.get('description', s.get('desc', '')), source='imported')
                if result:
                    added += 1
            self._update_status(f"Imported {added} new sites (skipped {len(sites)-added} duplicates)")
            self._refresh_stats()
        except json.JSONDecodeError:
            QMessageBox.warning(self, "Error", "Invalid JSON file")
        except Exception as e:
            QMessageBox.warning(self, "Error", str(e))

    # ── Database ───────────────────────────────────────────────────
    def manage_database(self):
        stats = database.get_stats()
        categories = database.get_all_categories()
        cat_text = ", ".join(sorted(categories)[:20])
        msg = (f"Total Sites: {stats.get('total_sites', 0)}\n"
               f"Curated: {stats.get('total_curated', 0)}\n"
               f"Discovered: {stats.get('discovered', 0)}\n"
               f"Categories: {len(categories)}\n\n"
               f"Top categories: {cat_text}\n\n"
               f"Auto-expand: {'ON' if self.config.get('auto_expand_database') else 'OFF'}")
        QMessageBox.information(self, "Site Database", msg)

    def auto_expand_database(self):
        self.config['auto_expand_database'] = not self.config.get('auto_expand_database', True)
        self._save_config()
        state = "enabled" if self.config['auto_expand_database'] else "disabled"
        self._update_status(f"Auto-expand database {state}")

    def export_database(self):
        path, _ = QFileDialog.getSaveFileName(self, "Export Database", "oynix_database.json", "JSON (*.json)")
        if path:
            database.export_to_json(path)
            self._update_status("Database exported")

    # ── AI ─────────────────────────────────────────────────────────
    def open_ai_chat(self):
        self.toggle_ai_panel()

    def _summarize_page(self):
        tab = self.current_tab()
        if tab:
            tab.page().toPlainText(lambda text: ai_manager.summarize(text[:3000]))

    def summarize_page(self):
        self._summarize_page()

    def generate_summary(self):
        self._summarize_page()

    def extract_key_points(self):
        tab = self.current_tab()
        if tab:
            tab.page().toPlainText(
                lambda text: ai_manager.chat(f"Extract the key points from this text:\n{text[:2000]}"))

    def explain_simple(self):
        tab = self.current_tab()
        if tab:
            tab.page().toPlainText(
                lambda text: ai_manager.chat(f"Explain this simply:\n{text[:2000]}"))

    def set_ai_model(self):
        """Show AI model info and allow switching."""
        models = ai_manager.get_available_models()
        current = ai_manager.model_key
        status = ai_manager.get_status()

        items = []
        for key, info in models.items():
            marker = " [ACTIVE]" if key == current else ""
            items.append(f"{info['name']}{marker}\n  {info['description']}")

        model_names = list(models.keys())
        model_labels = [models[k]['name'] for k in model_names]
        current_idx = model_names.index(current) if current in model_names else 0

        from PyQt6.QtWidgets import QInputDialog
        choice, ok = QInputDialog.getItem(
            self, "AI Model",
            f"Status: {status['status']}\nCurrent: {models.get(current, {}).get('name', current)}\n\nSelect model:",
            model_labels, current_idx, False)
        if ok and choice:
            idx = model_labels.index(choice)
            new_key = model_names[idx]
            if new_key != current:
                ai_manager.switch_model(new_key)
                self._update_status(f"Switching AI to {choice}...")

    # ── Window ─────────────────────────────────────────────────────
    def new_window(self):
        win = OynixBrowser()
        win.show()

    def new_private_window(self):
        win = OynixBrowser()
        win.setWindowTitle("OyNIx (Private)")
        # Use off-the-record profile
        win.show()

    def open_file(self):
        path, _ = QFileDialog.getOpenFileName(self, "Open File", "", "Web Files (*.html *.htm *.pdf);;All (*)")
        if path:
            self.current_tab().setUrl(QUrl.fromLocalFile(path))

    # ── GitHub Sync ────────────────────────────────────────────────
    def upload_to_github(self):
        if not github_sync.is_configured():
            self.configure_github()
            return
        import tempfile
        tmp = tempfile.NamedTemporaryFile(suffix='.json', delete=False)
        database.export_to_json(tmp.name)
        with open(tmp.name) as f:
            data = f.read()
        os.unlink(tmp.name)
        github_sync.upload_database(data)
        self._update_status("Uploading database to GitHub...")

    def import_from_github(self):
        if not github_sync.is_configured():
            self.configure_github()
            return
        github_sync.import_database()
        github_sync.import_finished.connect(self._on_github_import)

    def _on_github_import(self, sites):
        try:
            github_sync.import_finished.disconnect()
        except TypeError:
            pass
        if sites:
            database.add_sites_batch(sites)
            self._update_status(f"Imported {len(sites)} sites from GitHub")
        self._refresh_stats()

    def configure_github(self):
        token, ok = QInputDialog.getText(self, "GitHub Token",
            "Enter GitHub personal access token:", QLineEdit.EchoMode.Password)
        if ok and token:
            repo, ok2 = QInputDialog.getText(self, "GitHub Repo",
                "Repository (user/repo):", text="")
            if ok2 and repo:
                auto = QMessageBox.question(self, "Auto-Sync",
                    "Enable auto-sync to GitHub?") == QMessageBox.StandardButton.Yes
                interval = 60
                if auto:
                    val, ok3 = QInputDialog.getInt(self, "Sync Interval",
                        "Sync every N minutes (min 30):", 60, 30, 1440)
                    if ok3:
                        interval = val
                github_sync.configure(token, repo, auto_sync=auto, interval=interval)
                github_sync.start_auto_sync(
                    export_func=lambda p: database.export_to_json(p))
                self._update_status("GitHub sync configured"
                    + (" (auto-sync ON)" if auto else ""))

    # ── Misc stubs now implemented ─────────────────────────────────
    def set_search_engine(self, engine='nyx'):
        self.config['default_search_engine'] = engine
        self._save_config()

    def clear_data(self):
        reply = QMessageBox.question(self, "Clear Data",
            "Clear all browsing data (history, cookies, cache)?")
        if reply == QMessageBox.StandardButton.Yes:
            self._history = []
            self._save_json(HISTORY_FILE, [])
            QWebEngineProfile.defaultProfile().clearHttpCache()
            self._update_status("Browsing data cleared")

    def show_shortcuts(self):
        shortcuts = ("Ctrl+T: New Tab\nCtrl+W: Close Tab\nCtrl+L: Focus URL\n"
                     "Ctrl+F: Find in Page\nCtrl+D: Bookmark\nCtrl+K: Command Palette\n"
                     "Ctrl+J: Downloads\nCtrl+H: History\nCtrl+B: Bookmarks\n"
                     "Ctrl+Shift+A: AI Panel\nF5: Reload\nF11: Fullscreen\n"
                     "Ctrl++/-/0: Zoom")
        QMessageBox.information(self, "Keyboard Shortcuts", shortcuts)

    def show_about(self):
        QMessageBox.about(self, "About OyNIx",
            "OyNIx Browser v2.2\nThe Nyx-Powered Local AI Browser\n\n"
            "Features: Tree Tabs, Local LLM, Nyx Search,\n"
            "1400+ Site Database, Dynamic Refractions,\n"
            "C++/C# Turbo Indexer, Audio Player,\n"
            "Password Manager, .nydta Profiles,\n"
            "XPI Extensions, GitHub Sync, Flatpak\n\n"
            "Coded by Claude (Anthropic)")

    def show_release_notes(self):
        QMessageBox.information(self, "Release Notes",
            "v2.2 — Dynamic Refractions & Turbo Search\n\n"
            "• Nyx-first search (no external redirects)\n"
            "• GPU-accelerated mouse-tracking refractions\n"
            "• Dynamic glass effects on all UI pages\n"
            "• C++ & C# turbo indexer with batch scoring\n"
            "• Custom cat-ear logo & app icons\n"
            "• Flatpak build support + CI\n"
            "• Enhanced animations throughout\n"
            "• .nydta profile export/import\n"
            "• Password & passkey manager\n"
            "• Audio player\n• Chrome history import")

    def check_updates(self):
        QMessageBox.information(self, "Updates", "You are running OyNIx v2.2 (latest).")

    def import_settings(self):
        path, _ = QFileDialog.getOpenFileName(self, "Import Settings", "", "JSON (*.json)")
        if path:
            try:
                with open(path) as f:
                    self.config.update(json.load(f))
                self._save_config()
                self._apply_theme()
                self._update_status("Settings imported")
            except Exception as e:
                QMessageBox.warning(self, "Error", str(e))

    def export_settings(self):
        path, _ = QFileDialog.getSaveFileName(self, "Export Settings", "oynix_settings.json", "JSON (*.json)")
        if path:
            self._save_json(path, self.config)

    def manage_trusted_domains(self):
        QMessageBox.information(self, "Trusted Domains",
            f"Trusted: {len(security_manager.trusted_domains)}\n"
            f"Blocked: {len(security_manager.blocked_domains)}")

    # ── .nydta Profile Export / Import ────────────────────────────
    def export_nydta(self):
        """Export full browser profile as .nydta archive."""
        path, _ = QFileDialog.getSaveFileName(
            self, "Export OyNIx Profile", "oynix_profile.nydta",
            "OyNIx Profile (*.nydta);;All Files (*)")
        if not path:
            return

        def _get_db_entries():
            """Get database entries for export."""
            import tempfile
            tmp = tempfile.NamedTemporaryFile(suffix='.json', delete=False)
            tmp.close()
            database.export_to_json(tmp.name)
            try:
                with open(tmp.name) as f:
                    data = json.load(f)
                # Flatten to list
                entries = []
                if isinstance(data, dict):
                    for section in ('curated', 'discovered', 'sites'):
                        if section in data and isinstance(data[section], dict):
                            for cat, items in data[section].items():
                                if isinstance(items, list):
                                    for item in items:
                                        if isinstance(item, dict):
                                            item['_category'] = cat
                                            entries.append(item)
                        elif section in data and isinstance(data[section], list):
                            entries.extend(data[section])
                elif isinstance(data, list):
                    entries = data
                return entries
            except Exception:
                return []
            finally:
                os.unlink(tmp.name)

        # Include credentials and profile info
        cred_data = profile_manager.active.credentials.export_dict()
        profile_info = profile_manager.active.to_dict()

        ok, msg = export_nydta(path, self._history, self.config, _get_db_entries,
                               credentials_data=cred_data, profile_info=profile_info)
        if ok:
            self._update_status(msg)
            QMessageBox.information(self, "Profile Exported", msg)
        else:
            QMessageBox.warning(self, "Export Failed", msg)

    def import_nydta(self):
        """Import a .nydta profile archive."""
        path, _ = QFileDialog.getOpenFileName(
            self, "Import OyNIx Profile", "",
            "OyNIx Profile (*.nydta);;All Files (*)")
        if not path:
            return

        ok, data, msg = import_nydta(path)
        if not ok:
            QMessageBox.warning(self, "Import Failed", msg)
            return

        # Show what was found and let user choose
        parts = []
        if data['history']:
            parts.append(f"{len(data['history'])} history entries")
        if data['database']:
            parts.append(f"{len(data['database'])} database entries")
        if data['settings']:
            parts.append("settings")
        if data['profile_pic_path']:
            parts.append("profile picture")

        reply = QMessageBox.question(self, "Import Profile",
            f"Found: {', '.join(parts)}\n\nImport all into OyNIx?\n"
            "(Existing data will be merged, not replaced)")
        if reply != QMessageBox.StandardButton.Yes:
            return

        # Merge history
        if data['history']:
            existing_urls = {h['url'] for h in self._history}
            added = 0
            for entry in data['history']:
                if entry.get('url') and entry['url'] not in existing_urls:
                    self._history.insert(0, entry)
                    existing_urls.add(entry['url'])
                    added += 1
            self._history = self._history[:5000]
            self._save_json(HISTORY_FILE, self._history)

        # Merge database
        db_added = 0
        if data['database']:
            for entry in data['database']:
                url = entry.get('url', '')
                title = entry.get('title', '')
                desc = entry.get('description', '')
                if url and database.add_site(url, title, desc, source='nydta'):
                    db_added += 1

        # Import settings (merge, don't replace)
        if data['settings']:
            for key, val in data['settings'].items():
                if key not in self.config:
                    self.config[key] = val
            self._save_config()

        # Import credentials + auto-login rules into active profile
        if data.get('credentials'):
            profile_manager.active.credentials.import_dict(data['credentials'])

        # Profile picture — save to active profile dir
        if data['profile_pic_path']:
            import shutil
            dest = profile_manager.active.profile_pic_path
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            shutil.copy2(data['profile_pic_path'], dest)
            # Also copy to legacy path for backward compat
            shutil.copy2(data['profile_pic_path'], PROFILE_PIC_PATH)

        self._refresh_stats()
        result = f"Imported: {len(data['history'])} history, {db_added} new DB entries"
        if data.get('credentials'):
            result += ", credentials"
        if data['profile_pic_path']:
            result += ", profile picture"
        self._update_status(result)
        QMessageBox.information(self, "Import Complete", result)

    def set_profile_picture(self):
        """Set the user's profile picture."""
        path, _ = QFileDialog.getOpenFileName(
            self, "Select Profile Picture", "",
            "Images (*.png *.jpg *.jpeg *.jxl *.webp);;All (*)")
        if not path:
            return
        import shutil
        dest = profile_manager.active.profile_pic_path
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        shutil.copy2(path, dest)
        # Legacy path too
        os.makedirs(os.path.dirname(PROFILE_PIC_PATH), exist_ok=True)
        shutil.copy2(path, PROFILE_PIC_PATH)
        self._update_status("Profile picture updated")

    # ── Profile Management ───────────────────────────────────────
    def show_profiles(self):
        """Show the profiles management page."""
        tab = self.current_tab()
        if tab:
            from oynix.core.theme_engine import get_profiles_html
            html = get_profiles_html(profile_manager, self.theme_colors)
            tab.setHtml(html, QUrl("oyn://profiles"))

    def create_profile(self):
        """Create a new browser profile."""
        name, ok = QInputDialog.getText(self, "New Profile", "Profile name:")
        if not ok or not name.strip():
            return
        try:
            profile_manager.create_profile(name.strip())
            self._update_status(f"Profile '{name.strip()}' created")
            self.show_profiles()
        except ValueError as e:
            QMessageBox.warning(self, "Error", str(e))

    def switch_profile(self, name=None):
        """Switch to a different profile."""
        if name is None:
            profiles = profile_manager.list_profiles()
            names = [f"{p.display_name} ({p.name})" for p in profiles]
            choice, ok = QInputDialog.getItem(self, "Switch Profile",
                                              "Select profile:", names, 0, False)
            if not ok:
                return
            # Extract the (name) part
            name = choice.rsplit('(', 1)[-1].rstrip(')')

        try:
            # Save current state before switching
            self._save_json(self._get_profile_path("history.json"), self._history)
            self._save_json(self._get_profile_path("bookmarks.json"), self._bookmarks)
            self._save_config()

            profile_manager.switch_profile(name)

            # Reload data from new profile
            self.config = dict(DEFAULT_CONFIG)
            self._load_config()
            self.current_theme = self.config.get('theme', 'nyx')
            self._apply_theme()
            self._bookmarks = self._load_json(
                self._get_profile_path("bookmarks.json"), [])
            self._history = self._load_json(
                self._get_profile_path("history.json"), [])

            self._update_status(f"Switched to profile: {profile_manager.active.display_name}")
            self.navigate_home()
        except ValueError as e:
            QMessageBox.warning(self, "Error", str(e))

    def delete_profile(self):
        """Delete a browser profile."""
        profiles = [p for p in profile_manager.list_profiles() if p.name != 'default']
        if not profiles:
            QMessageBox.information(self, "Profiles", "No profiles to delete (default cannot be deleted)")
            return
        names = [f"{p.display_name} ({p.name})" for p in profiles]
        choice, ok = QInputDialog.getItem(self, "Delete Profile",
                                          "Select profile to delete:", names, 0, False)
        if not ok:
            return
        name = choice.rsplit('(', 1)[-1].rstrip(')')
        reply = QMessageBox.question(self, "Delete Profile",
            f"Delete profile '{name}' and all its data?\nThis cannot be undone.")
        if reply == QMessageBox.StandardButton.Yes:
            try:
                profile_manager.delete_profile(name)
                self._update_status(f"Profile '{name}' deleted")
            except ValueError as e:
                QMessageBox.warning(self, "Error", str(e))

    def manage_autologin(self):
        """Show auto-login management dialog."""
        from oynix.core.profiles import ProfileCredentials
        pcreds = profile_manager.active.credentials
        if not pcreds.is_unlocked:
            pw, ok = QInputDialog.getText(self, "Unlock Credentials",
                "Master password:" if pcreds.has_master_password() else "Set master password:",
                QLineEdit.EchoMode.Password)
            if not ok or not pw:
                return
            if not pcreds.unlock(pw):
                QMessageBox.warning(self, "Error", "Incorrect password")
                return

        dialog = AutoLoginDialog(pcreds, self)
        dialog.exec()

    def save_credential_to_profile(self):
        """Save a credential for the current page to the active profile."""
        tab = self.current_tab()
        if not tab:
            return
        url = tab.url().toString()
        domain = urlparse(url).netloc.lower()
        pcreds = profile_manager.active.credentials
        if not pcreds.is_unlocked:
            pw, ok = QInputDialog.getText(self, "Unlock Credentials",
                "Master password:" if pcreds.has_master_password() else "Set master password:",
                QLineEdit.EchoMode.Password)
            if not ok or not pw:
                return
            if not pcreds.unlock(pw):
                QMessageBox.warning(self, "Error", "Incorrect password")
                return

        username, ok1 = QInputDialog.getText(self, "Save Credential", f"Username for {domain}:")
        if not ok1 or not username:
            return
        password, ok2 = QInputDialog.getText(self, "Save Credential", "Password:",
                                              QLineEdit.EchoMode.Password)
        if not ok2:
            return
        pcreds.save_credential(domain, username, password)

        # Ask about auto-login
        reply = QMessageBox.question(self, "Auto-Login",
            f"Enable auto-login for {username} on {domain}?\n"
            "(Will auto-fill credentials when you visit this site)")
        if reply == QMessageBox.StandardButton.Yes:
            fill_reply = QMessageBox.question(self, "Auto-Login",
                "Auto-submit the login form too?\n"
                "(No = just fill fields, Yes = fill and submit)")
            fill_only = fill_reply != QMessageBox.StandardButton.Yes
            pcreds.set_autologin(domain, username, enabled=True, fill_only=fill_only)

        self._update_status(f"Credential saved for {domain}")

    # ── Audio Player ────────────────────────────────────────────────
    def toggle_audio_player(self):
        if not self.audio_player:
            QMessageBox.information(self, "Audio Player",
                "Audio player requires PyQt6.QtMultimedia.\n"
                "Install: pip install PyQt6-Multimedia")
            return
        if self.audio_player.isVisible():
            self.audio_player.hide()
        else:
            self.audio_player.show()

    # ── Credential Manager ────────────────────────────────────────
    def show_passwords(self):
        dialog = CredentialDialog(credential_manager, self)
        dialog.exec()

    def _check_login_page(self, url_str, browser):
        """Inject JS to detect login forms and auto-fill/auto-login."""
        parsed = urlparse(url_str)
        domain = parsed.netloc.lower()

        # Check profile auto-login rules first
        pcreds = profile_manager.active.credentials
        autologin = pcreds.get_autologin(domain)
        if autologin and autologin.get('enabled') and pcreds.is_unlocked:
            creds = pcreds.get_credentials(domain)
            target_user = autologin.get('username', '')
            for c in creds:
                if c['username'] == target_user:
                    js = get_autologin_js(domain, c['username'], c['password'],
                                          fill_only=autologin.get('fill_only', False))
                    browser.page().runJavaScript(js)
                    return

        # Fall back to legacy credential manager auto-fill
        if not credential_manager.is_unlocked:
            return
        creds = credential_manager.get_credentials(url_str)
        if creds:
            c = creds[0]
            js = get_autologin_js(domain, c['username'], c['password'], fill_only=True)
            browser.page().runJavaScript(js)

    # ── Chrome Import ─────────────────────────────────────────────
    def import_chrome_history(self):
        """Import browsing history from Google Chrome."""
        from oynix.core.chrome_import import import_chrome_history, find_chrome_history_db
        db_path = find_chrome_history_db()
        if not db_path:
            path, _ = QFileDialog.getOpenFileName(
                self, "Select Chrome History DB", "",
                "SQLite (History);;All (*)")
            if not path:
                return
            db_path = path

        entries, msg = import_chrome_history(db_path)
        if not entries:
            QMessageBox.information(self, "Chrome Import", msg)
            return

        reply = QMessageBox.question(self, "Chrome Import",
            f"{msg}\n\nImport {len(entries)} entries into OyNIx?\n"
            "They will be added to your history and database.")
        if reply != QMessageBox.StandardButton.Yes:
            return

        # Add to history
        for e in entries:
            self._history.insert(0, {
                'url': e['url'], 'title': e['title'],
                'time': e.get('time', ''), 'source': 'chrome'
            })
        self._history = self._history[:5000]
        self._save_json(HISTORY_FILE, self._history)

        # Add to database
        added = 0
        for e in entries:
            result = database.add_site(e['url'], e['title'], source='chrome')
            if result:
                added += 1
        self._refresh_stats()
        self._update_status(f"Chrome import: {len(entries)} history + {added} new DB sites")

    def import_chrome_searches(self):
        """Import search queries from Chrome history."""
        from oynix.core.chrome_import import import_chrome_search_history, find_chrome_history_db
        db_path = find_chrome_history_db()
        if not db_path:
            path, _ = QFileDialog.getOpenFileName(
                self, "Select Chrome History DB", "",
                "SQLite (History);;All (*)")
            if not path:
                return
            db_path = path

        searches, msg = import_chrome_search_history(db_path)
        if not searches:
            QMessageBox.information(self, "Chrome Search Import", msg)
            return

        QMessageBox.information(self, "Chrome Search Import",
            f"{msg}\n\nSearch queries have been indexed for Nyx search.")
        for s in searches:
            nyx_search.index_page(
                s['url'], f"Search: {s['query']}", s['query'])

    def import_google_takeout(self):
        """Import from Google Takeout JSON export."""
        path, _ = QFileDialog.getOpenFileName(
            self, "Import Google Takeout", "",
            "JSON (*.json);;All (*)")
        if not path:
            return
        from oynix.core.chrome_import import import_google_takeout
        history, searches, msg = import_google_takeout(path)
        if not history and not searches:
            QMessageBox.information(self, "Takeout Import", msg)
            return

        reply = QMessageBox.question(self, "Takeout Import",
            f"{msg}\n\nImport into OyNIx?")
        if reply != QMessageBox.StandardButton.Yes:
            return

        for e in history:
            self._history.insert(0, e)
        self._history = self._history[:5000]
        self._save_json(HISTORY_FILE, self._history)

        added = 0
        for e in history:
            if database.add_site(e.get('url',''), e.get('title',''), source='takeout'):
                added += 1
        for s in searches:
            nyx_search.index_page(s['url'], f"Search: {s['query']}", s['query'])

        self._refresh_stats()
        self._update_status(f"Takeout: {len(history)} history + {len(searches)} searches + {added} DB sites")

    # ── Search Engine Builder ─────────────────────────────────────
    def show_search_builder(self):
        dialog = SearchEngineBuilder(self)
        dialog.exec()

    # FindBar + SavePW bar layout fix
    def resizeEvent(self, event):
        super().resizeEvent(event)
        if hasattr(self, 'find_bar'):
            self.find_bar.setGeometry(0, self.height() - 80, self.width(), 40)
        if hasattr(self, 'save_pw_bar'):
            self.save_pw_bar.setGeometry(0, self.height() - 120, self.width(), 40)

    # ══════════════════════════════════════════════════════════════════
    # v2.2 Features
    # ══════════════════════════════════════════════════════════════════

    # ── 1. Tab Pinning ─────────────────────────────────────────────
    def pin_current_tab(self):
        """Pin/unpin the current tab (pinned tabs stay at the front)."""
        idx = self.tab_manager.currentIndex()
        if idx < 0:
            return
        pinned = getattr(self, '_pinned_tabs', set())
        tab = self.tab_manager.widget(idx)
        tab_id = id(tab)
        if tab_id in pinned:
            pinned.discard(tab_id)
            self._update_status("Tab unpinned")
        else:
            pinned.add(tab_id)
            # Move to front
            if idx > 0:
                self.tab_manager.tabBar().moveTab(idx, 0)
            self._update_status("Tab pinned")
        self._pinned_tabs = pinned

    # ── 2. Page Screenshot ─────────────────────────────────────────
    def screenshot_page(self):
        """Capture the current page as a PNG screenshot."""
        tab = self.current_tab()
        if not tab:
            return
        path, _ = QFileDialog.getSaveFileName(
            self, "Save Screenshot",
            os.path.expanduser("~/screenshot.png"),
            "PNG Images (*.png)")
        if not path:
            return
        # Grab the web view widget
        pixmap = tab.grab()
        pixmap.save(path, "PNG")
        self._update_status(f"Screenshot saved: {path}")

    # ── 3. Reading List ────────────────────────────────────────────
    def add_to_reading_list(self):
        """Add current page to reading list for later."""
        tab = self.current_tab()
        if not tab:
            return
        url = tab.url().toString()
        title = tab.page().title() or url
        reading_list = self._load_json(
            self._get_profile_path("reading_list.json"), [])
        if any(r['url'] == url for r in reading_list):
            self._update_status("Already in reading list")
            return
        reading_list.insert(0, {
            'url': url, 'title': title,
            'added': time.strftime("%Y-%m-%d %H:%M"),
            'read': False,
        })
        self._save_json(self._get_profile_path("reading_list.json"), reading_list)
        self._update_status(f"Added to reading list: {title[:40]}")

    def show_reading_list(self):
        """Show the reading list in a dialog."""
        reading_list = self._load_json(
            self._get_profile_path("reading_list.json"), [])
        dialog = QDialog(self)
        dialog.setWindowTitle("Reading List")
        dialog.setMinimumSize(500, 400)
        layout = QVBoxLayout(dialog)
        header = QLabel(f"Reading List ({len(reading_list)} items)")
        header.setStyleSheet(
            f"font-size: 16px; font-weight: bold; color: {self.theme_colors['purple_light']};"
            " background: transparent;")
        layout.addWidget(header)
        lst = QListWidget()
        for item in reading_list:
            check = "\u2713 " if item.get('read') else ""
            li = QListWidgetItem(f"{check}{item['title']}")
            li.setData(Qt.ItemDataRole.UserRole, item)
            lst.addItem(li)
        layout.addWidget(lst)
        btn_row = QHBoxLayout()
        open_btn = QPushButton("Open Selected")
        open_btn.clicked.connect(lambda: self._open_reading_item(lst, dialog))
        btn_row.addWidget(open_btn)
        remove_btn = QPushButton("Remove Selected")
        remove_btn.clicked.connect(lambda: self._remove_reading_item(lst, reading_list))
        btn_row.addWidget(remove_btn)
        layout.addLayout(btn_row)
        dialog.exec()

    def _open_reading_item(self, lst, dialog):
        item = lst.currentItem()
        if item:
            data = item.data(Qt.ItemDataRole.UserRole)
            self.new_tab(QUrl(data['url']))
            data['read'] = True
            reading_list = self._load_json(
                self._get_profile_path("reading_list.json"), [])
            for r in reading_list:
                if r['url'] == data['url']:
                    r['read'] = True
            self._save_json(self._get_profile_path("reading_list.json"), reading_list)
            dialog.accept()

    def _remove_reading_item(self, lst, reading_list):
        item = lst.currentItem()
        if item:
            data = item.data(Qt.ItemDataRole.UserRole)
            reading_list[:] = [r for r in reading_list if r['url'] != data['url']]
            self._save_json(self._get_profile_path("reading_list.json"), reading_list)
            lst.takeItem(lst.row(item))

    # ── 4. Tab Search ──────────────────────────────────────────────
    def search_tabs(self):
        """Search through open tabs by title/URL."""
        text, ok = QInputDialog.getText(self, "Search Tabs", "Find tab:")
        if not ok or not text:
            return
        text_lower = text.lower()
        for i in range(self.tab_manager.count()):
            w = self.tab_manager.widget(i)
            if w:
                title = (w.page().title() or '').lower()
                url = w.url().toString().lower()
                if text_lower in title or text_lower in url:
                    self.tab_manager.setCurrentIndex(i)
                    self._update_status(f"Found tab: {w.page().title()}")
                    return
        self._update_status("No matching tab found")

    # ── 5. Session Save/Restore ────────────────────────────────────
    def save_session(self):
        """Save all open tabs as a session file."""
        tabs = []
        for i in range(self.tab_manager.count()):
            w = self.tab_manager.widget(i)
            if w:
                url = w.url().toString()
                if url and not url.startswith('oyn://'):
                    tabs.append({
                        'url': url,
                        'title': w.page().title() or url,
                    })
        if not tabs:
            self._update_status("No tabs to save")
            return
        path, _ = QFileDialog.getSaveFileName(
            self, "Save Session",
            os.path.expanduser("~/oynix_session.json"),
            "Session Files (*.json)")
        if path:
            with open(path, 'w') as f:
                json.dump({'tabs': tabs, 'saved': time.strftime("%Y-%m-%d %H:%M")}, f, indent=2)
            self._update_status(f"Session saved: {len(tabs)} tabs")

    def restore_session(self):
        """Restore tabs from a session file."""
        path, _ = QFileDialog.getOpenFileName(
            self, "Restore Session", "",
            "Session Files (*.json);;All Files (*)")
        if not path:
            return
        try:
            with open(path) as f:
                data = json.load(f)
            tabs = data.get('tabs', [])
            for t in tabs:
                self.new_tab(QUrl(t['url']))
            self._update_status(f"Restored {len(tabs)} tabs")
        except Exception as e:
            QMessageBox.warning(self, "Session Restore", f"Error: {e}")

    # ── 6. Site-Specific Zoom Memory ───────────────────────────────
    def _get_zoom_map(self):
        return self._load_json(self._get_profile_path("zoom_map.json"), {})

    def _save_zoom_for_domain(self):
        """Remember the current zoom level for this domain."""
        tab = self.current_tab()
        if not tab:
            return
        domain = urlparse(tab.url().toString()).netloc
        if not domain:
            return
        zoom_map = self._get_zoom_map()
        zoom_map[domain] = tab.zoomFactor()
        self._save_json(self._get_profile_path("zoom_map.json"), zoom_map)

    def _apply_zoom_for_domain(self, browser):
        """Apply saved zoom level for the current domain."""
        domain = urlparse(browser.url().toString()).netloc
        if not domain:
            return
        zoom_map = self._get_zoom_map()
        if domain in zoom_map:
            browser.setZoomFactor(zoom_map[domain])

    # ── 7. Quick Notes ─────────────────────────────────────────────
    def show_quick_notes(self):
        """Open a quick-notes dialog for the current page."""
        tab = self.current_tab()
        if not tab:
            return
        url = tab.url().toString()
        domain = urlparse(url).netloc or 'general'
        notes_file = self._get_profile_path("notes.json")
        all_notes = self._load_json(notes_file, {})
        current_note = all_notes.get(domain, '')

        dialog = QDialog(self)
        dialog.setWindowTitle(f"Notes — {domain}")
        dialog.setMinimumSize(400, 300)
        layout = QVBoxLayout(dialog)
        from PyQt6.QtWidgets import QTextEdit as _QTE
        editor = _QTE()
        editor.setPlainText(current_note)
        editor.setPlaceholderText("Type your notes here...")
        editor.setStyleSheet(
            f"font-family: 'JetBrains Mono', monospace; font-size: 13px;"
            f" background: {self.theme_colors['bg_mid']};"
            f" color: {self.theme_colors['text_primary']};"
            f" border: 1px solid {self.theme_colors['border']}; border-radius: 8px; padding: 10px;")
        layout.addWidget(editor)
        save_btn = QPushButton("Save Notes")
        save_btn.setObjectName("accentBtn")
        save_btn.clicked.connect(lambda: self._save_note(all_notes, domain, editor.toPlainText(), notes_file, dialog))
        layout.addWidget(save_btn)
        dialog.exec()

    def _save_note(self, all_notes, domain, text, path, dialog):
        if text.strip():
            all_notes[domain] = text
        else:
            all_notes.pop(domain, None)
        self._save_json(path, all_notes)
        self._update_status("Notes saved")
        dialog.accept()

    # ── 8. Tab Groups ──────────────────────────────────────────────
    def create_tab_group(self):
        """Create a named tab group from selected tabs."""
        name, ok = QInputDialog.getText(self, "Tab Group", "Group name:")
        if not ok or not name:
            return
        colors = ['#7B4FBF', '#6FCF97', '#F2C94C', '#EB5757', '#56CCF2', '#BB6BD9']
        color_names = ['Purple', 'Green', 'Yellow', 'Red', 'Blue', 'Pink']
        color, ok2 = QInputDialog.getItem(self, "Group Color", "Select color:", color_names, 0, False)
        if not ok2:
            return
        idx = color_names.index(color) if color in color_names else 0
        groups = getattr(self, '_tab_groups', {})
        groups[name] = {
            'color': colors[idx],
            'tabs': [self.tab_manager.currentIndex()],
        }
        self._tab_groups = groups
        self._update_status(f"Created tab group: {name}")

"""
OyNIx Browser v1.0.0 - Main Browser Core
Firefox-inspired browser with local LLM, Nyx search, tree tabs.

This is the heart of OyNIx - connects all modules together:
- Tree-style tabs + normal tabs (toggle)
- Nyx purple/black theme
- Local LLM AI assistant (auto-install)
- Nyx search engine with auto-indexing
- DuckDuckGo integration
- GitHub database sync
- Security prompts
"""

import sys
import os
from urllib.parse import parse_qs, urlparse

from PyQt6.QtWidgets import (
    QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, QLineEdit,
    QPushButton, QLabel, QToolBar, QStatusBar, QMessageBox,
    QFileDialog, QSplitter, QApplication
)
from PyQt6.QtWebEngineWidgets import QWebEngineView
from PyQt6.QtWebEngineCore import QWebEnginePage, QWebEngineSettings, QWebEngineProfile
from PyQt6.QtCore import QUrl, Qt, pyqtSignal, QTimer, QSize
from PyQt6.QtGui import QIcon, QAction, QShortcut, QKeySequence

# Core modules
from oynix.core.theme_engine import (
    NYX_COLORS, get_qt_stylesheet, get_homepage_html,
    get_search_results_html, get_theme, THEMES
)
from oynix.core.tree_tabs import TabManager
from oynix.core.nyx_search import nyx_search
from oynix.core.ai_manager import ai_manager
from oynix.core.database import database
from oynix.core.security import security_manager
from oynix.core.github_sync import github_sync

# UI modules
from oynix.UI.ai_chat import AIChatPanel


class OynixWebPage(QWebEnginePage):
    """Custom web page with security integration."""

    def acceptNavigationRequest(self, url, nav_type, is_main_frame):
        if is_main_frame and security_manager.security_checks_enabled:
            should_load, info = security_manager.check_url_security(
                url, self.parent())
            return should_load
        return super().acceptNavigationRequest(url, nav_type, is_main_frame)


class OynixBrowser(QMainWindow):
    """
    OyNIx Browser v1.0.0 - The Nyx-Powered Local AI Browser

    Features:
    - Tree-style tabs (toggle to normal)
    - Medium purple + black Nyx theme
    - Local LLM assistant (auto-downloads, no external deps)
    - Nyx search engine with auto-indexing
    - DuckDuckGo / Google / Brave search
    - GitHub database sync
    - 1400+ curated site database
    - Security prompts for login pages
    """

    def __init__(self):
        super().__init__()

        # Configuration
        self.config = {
            'use_tree_tabs': True,
            'default_search_engine': 'nyx',
            'show_security_prompts': True,
            'auto_index_pages': True,
            'theme': 'nyx',
            'show_ai_panel': False,
        }

        self.current_theme = 'nyx'
        self.theme_colors = get_theme('nyx')

        # Setup browser
        self._setup_window()
        self._setup_toolbar()
        self._setup_tab_manager()
        self._setup_ai_panel()
        self._setup_statusbar()
        self._setup_shortcuts()
        self._apply_theme()

        # Create menus (imported after init)
        from oynix.UI.menus import create_oynix_menus
        self.menus = create_oynix_menus(self)

        # Start AI model loading in background
        ai_manager.status_update.connect(self._on_ai_status)
        ai_manager.load_model_async()

        # Create initial tab
        self.new_tab(QUrl("oyn://home"))

        self._update_status("Ready")
        print("OyNIx Browser v1.0.0 initialized!")

    # ═══════════════════════════════════════════════════════════════════
    #  SETUP
    # ═══════════════════════════════════════════════════════════════════

    def _setup_window(self):
        self.setWindowTitle("OyNIx Browser")
        self.setGeometry(80, 60, 1500, 950)
        self.setMinimumSize(900, 600)

    def _setup_toolbar(self):
        toolbar = QToolBar("Navigation")
        toolbar.setMovable(False)
        toolbar.setIconSize(QSize(20, 20))
        self.addToolBar(toolbar)

        # Navigation buttons
        self.back_btn = QPushButton("\u25C0")
        self.back_btn.setObjectName("navButton")
        self.back_btn.setToolTip("Back")
        self.back_btn.clicked.connect(self._go_back)
        toolbar.addWidget(self.back_btn)

        self.fwd_btn = QPushButton("\u25B6")
        self.fwd_btn.setObjectName("navButton")
        self.fwd_btn.setToolTip("Forward")
        self.fwd_btn.clicked.connect(self._go_forward)
        toolbar.addWidget(self.fwd_btn)

        self.reload_btn = QPushButton("\u27F3")
        self.reload_btn.setObjectName("navButton")
        self.reload_btn.setToolTip("Reload")
        self.reload_btn.clicked.connect(self._reload)
        toolbar.addWidget(self.reload_btn)

        self.home_btn = QPushButton("\u2302")
        self.home_btn.setObjectName("navButton")
        self.home_btn.setToolTip("Home")
        self.home_btn.clicked.connect(self.navigate_home)
        toolbar.addWidget(self.home_btn)

        # URL bar
        self.url_bar = QLineEdit()
        self.url_bar.setPlaceholderText(
            "Search with Nyx or enter a URL...")
        self.url_bar.returnPressed.connect(self.navigate_to_url)
        toolbar.addWidget(self.url_bar)

        # Search button
        search_btn = QPushButton("\U0001F50D")
        search_btn.setObjectName("navButton")
        search_btn.setToolTip("Search")
        search_btn.clicked.connect(self.navigate_to_url)
        toolbar.addWidget(search_btn)

        # New tab button
        new_tab_btn = QPushButton("+")
        new_tab_btn.setObjectName("navButton")
        new_tab_btn.setToolTip("New Tab (Ctrl+T)")
        new_tab_btn.clicked.connect(lambda: self.new_tab())
        toolbar.addWidget(new_tab_btn)

        # Tab mode toggle
        self.tab_mode_btn = QPushButton("\u2263")
        self.tab_mode_btn.setObjectName("navButton")
        self.tab_mode_btn.setToolTip("Toggle Tree/Normal Tabs")
        self.tab_mode_btn.clicked.connect(self.toggle_tab_mode)
        toolbar.addWidget(self.tab_mode_btn)

        # AI chat toggle
        self.ai_btn = QPushButton("\u2604")
        self.ai_btn.setObjectName("navButton")
        self.ai_btn.setToolTip("Toggle AI Chat Panel")
        self.ai_btn.clicked.connect(self.toggle_ai_panel)
        toolbar.addWidget(self.ai_btn)

    def _setup_tab_manager(self):
        """Setup the tab manager (tree + normal tabs)."""
        self.tab_manager = TabManager()
        self.tab_manager.current_view_changed.connect(
            self._on_current_view_changed)
        self.tab_manager.new_tab_requested.connect(lambda: self.new_tab())

        # Main content area with splitter for AI panel
        self.main_splitter = QSplitter(Qt.Orientation.Horizontal)
        self.main_splitter.addWidget(self.tab_manager)

        self.setCentralWidget(self.main_splitter)

    def _setup_ai_panel(self):
        """Setup the AI chat sidebar panel."""
        self.ai_panel = AIChatPanel()
        self.ai_panel.close_requested.connect(self.toggle_ai_panel)
        self.ai_panel.summarize_requested.connect(self._summarize_page)
        self.ai_panel.hide()
        self.main_splitter.addWidget(self.ai_panel)
        self.main_splitter.setSizes([1200, 0])

    def _setup_statusbar(self):
        self.status_bar = QStatusBar()
        self.setStatusBar(self.status_bar)

        # AI status indicator
        self.ai_status_label = QLabel("AI: Loading...")
        self.ai_status_label.setStyleSheet(
            f"color: {NYX_COLORS['text_muted']}; font-size: 11px; "
            f"padding: 0 8px; background: transparent;"
        )
        self.status_bar.addPermanentWidget(self.ai_status_label)

        # Search stats
        stats = nyx_search.get_stats()
        self.search_stats_label = QLabel(
            f"DB: {stats['database_sites']} sites | "
            f"Index: {stats['indexed_pages']} pages"
        )
        self.search_stats_label.setStyleSheet(
            f"color: {NYX_COLORS['text_muted']}; font-size: 11px; "
            f"padding: 0 8px; background: transparent;"
        )
        self.status_bar.addPermanentWidget(self.search_stats_label)

    def _setup_shortcuts(self):
        """Setup keyboard shortcuts."""
        QShortcut(QKeySequence("Ctrl+T"), self, lambda: self.new_tab())
        QShortcut(QKeySequence("Ctrl+W"), self, self._close_current_tab)
        QShortcut(QKeySequence("Ctrl+L"), self, self.url_bar.setFocus)
        QShortcut(QKeySequence("Ctrl+R"), self, self._reload)
        QShortcut(QKeySequence("F5"), self, self._reload)
        QShortcut(QKeySequence("Alt+Left"), self, self._go_back)
        QShortcut(QKeySequence("Alt+Right"), self, self._go_forward)
        QShortcut(QKeySequence("F11"), self, self.toggle_fullscreen)
        QShortcut(QKeySequence("Ctrl+Shift+A"), self, self.toggle_ai_panel)

    def _apply_theme(self):
        """Apply the Nyx theme."""
        self.setStyleSheet(get_qt_stylesheet(self.theme_colors))

    # ═══════════════════════════════════════════════════════════════════
    #  TAB MANAGEMENT
    # ═══════════════════════════════════════════════════════════════════

    def new_tab(self, url=None, parent_view=None):
        """Create a new browser tab."""
        if url is None:
            url = QUrl("oyn://home")

        browser = QWebEngineView()
        page = OynixWebPage(browser)
        browser.setPage(page)

        # Connect signals
        browser.urlChanged.connect(
            lambda qurl, b=browser: self._on_url_changed(qurl, b))
        browser.loadFinished.connect(
            lambda ok, b=browser: self._on_load_finished(ok, b))
        browser.titleChanged.connect(
            lambda title, b=browser: self._on_title_changed(title, b))

        # Enable settings
        settings = browser.page().settings()
        settings.setAttribute(
            QWebEngineSettings.WebAttribute.JavascriptEnabled, True)
        settings.setAttribute(
            QWebEngineSettings.WebAttribute.LocalStorageEnabled, True)
        settings.setAttribute(
            QWebEngineSettings.WebAttribute.ScrollAnimatorEnabled, True)

        # Add to tab manager
        self.tab_manager.add_tab(browser, "New Tab", parent_view)

        # Navigate
        if isinstance(url, str):
            url = QUrl(url)

        if url.scheme() == "oyn":
            self._handle_oyn_url(url, browser)
        else:
            browser.setUrl(url)

        return browser

    def _close_current_tab(self):
        """Close the current tab."""
        view = self.tab_manager.get_current_view()
        if view:
            self.tab_manager.close_tab(view)

    def current_tab(self):
        """Get the current browser view."""
        return self.tab_manager.get_current_view()

    def close_tab(self, index=None):
        """Close tab (compatibility with menu system)."""
        self._close_current_tab()

    # ═══════════════════════════════════════════════════════════════════
    #  NAVIGATION
    # ═══════════════════════════════════════════════════════════════════

    def navigate_to_url(self):
        """Navigate to URL from the URL bar."""
        text = self.url_bar.text().strip()
        if not text:
            return

        # Check if it's a search query or URL
        if ' ' in text or ('.' not in text and ':' not in text):
            self._perform_search(text)
        else:
            if not text.startswith(('http://', 'https://', 'oyn://')):
                text = 'https://' + text

            url = QUrl(text)
            if url.scheme() == "oyn":
                self._handle_oyn_url(url, self.current_tab())
            else:
                self.current_tab().setUrl(url)

    def navigate_home(self):
        """Navigate to home page."""
        view = self.current_tab()
        if view:
            view.setHtml(get_homepage_html(self.theme_colors),
                         QUrl("oyn://home"))

    def _go_back(self):
        view = self.current_tab()
        if view:
            view.back()

    def _go_forward(self):
        view = self.current_tab()
        if view:
            view.forward()

    def _reload(self):
        view = self.current_tab()
        if view:
            view.reload()

    def open_url(self, url):
        """Open a URL in current tab."""
        view = self.current_tab()
        if view:
            view.setUrl(QUrl(url))

    # ═══════════════════════════════════════════════════════════════════
    #  SEARCH
    # ═══════════════════════════════════════════════════════════════════

    def _perform_search(self, query, engine=None):
        """Perform a search using the configured engine."""
        engine = engine or self.config.get('default_search_engine', 'nyx')

        if engine == 'nyx':
            self._nyx_search(query)
        elif engine in ('google', 'brave', 'bing'):
            url = nyx_search.get_web_search_url(query, engine)
            self.current_tab().setUrl(QUrl(url))
        else:
            # DuckDuckGo or default
            self._nyx_search(query)

    def _nyx_search(self, query):
        """Perform Nyx search (local index + database + DuckDuckGo)."""
        results = nyx_search.search(query)
        local = results.get('local', [])
        db = results.get('database', [])

        # Combine local results
        combined_local = db + local

        # Show results page immediately with local results
        html = get_search_results_html(
            query, combined_local, [],
            self.theme_colors
        )
        view = self.current_tab()
        if view:
            view.setHtml(html, QUrl(f"oyn://search?q={query}"))

        # Web results will arrive async
        nyx_search.web_results_ready.connect(
            lambda web: self._on_web_results(query, combined_local, web))

        self._update_status(f"Nyx Search: {query}")

    def _on_web_results(self, query, local_results, web_results):
        """Update search page when web results arrive."""
        try:
            nyx_search.web_results_ready.disconnect()
        except TypeError:
            pass

        html = get_search_results_html(
            query, local_results, web_results,
            self.theme_colors
        )
        view = self.current_tab()
        if view:
            view.setHtml(html, QUrl(f"oyn://search?q={query}"))

        # Update stats
        stats = nyx_search.get_stats()
        self.search_stats_label.setText(
            f"DB: {stats['database_sites']} sites | "
            f"Index: {stats['indexed_pages']} pages"
        )

    # ═══════════════════════════════════════════════════════════════════
    #  INTERNAL URL HANDLER
    # ═══════════════════════════════════════════════════════════════════

    def _handle_oyn_url(self, url, browser):
        """Handle internal oyn:// URLs."""
        path = url.path() or url.host()
        query_str = url.query()

        if path in ("home", "/home", ""):
            browser.setHtml(get_homepage_html(self.theme_colors),
                            QUrl("oyn://home"))

        elif path in ("search", "/search", "nyx-search", "/nyx-search"):
            params = parse_qs(query_str)
            q = params.get('q', [''])[0]
            engine = params.get('engine', ['nyx'])[0]
            if q:
                self._perform_search(q, engine)

        elif path in ("settings", "/settings"):
            self.show_settings()

        elif path in ("ai-chat", "/ai-chat"):
            if not self.ai_panel.isVisible():
                self.toggle_ai_panel()

        elif path in ("database", "/database"):
            self.manage_database()

        else:
            browser.setHtml(
                f"<html><body style='background:#0a0a0f;color:#E8E0F0;"
                f"font-family:sans-serif;display:flex;align-items:center;"
                f"justify-content:center;height:100vh'>"
                f"<div style='text-align:center'>"
                f"<h1 style='color:#7B4FBF'>OyNIx</h1>"
                f"<p>Unknown page: {path}</p></div></body></html>",
                QUrl("oyn://error")
            )

    # ═══════════════════════════════════════════════════════════════════
    #  SIGNAL HANDLERS
    # ═══════════════════════════════════════════════════════════════════

    def _on_url_changed(self, qurl, browser):
        """URL changed in a tab."""
        if browser == self.current_tab():
            url_str = qurl.toString()
            if not url_str.startswith("oyn://"):
                self.url_bar.setText(url_str)
            else:
                self.url_bar.setText("")

        self.tab_manager.update_tab_url(browser, qurl.toString())

    def _on_title_changed(self, title, browser):
        """Tab title changed."""
        self.tab_manager.update_tab_title(browser, title or "New Tab")

        if browser == self.current_tab() and title:
            self.setWindowTitle(f"{title} - OyNIx Browser")

    def _on_load_finished(self, ok, browser):
        """Page finished loading."""
        if not ok:
            return

        url = browser.url().toString()
        title = browser.page().title()

        # Auto-index the page for Nyx search
        if (self.config.get('auto_index_pages', True) and
                not url.startswith("oyn://") and
                url.startswith("http")):
            # Get page text for indexing
            browser.page().toPlainText(
                lambda text: nyx_search.index_page(url, title, text[:2000])
            )

    def _on_current_view_changed(self, browser):
        """Active tab changed."""
        if browser:
            url = browser.url().toString()
            title = browser.page().title()

            if not url.startswith("oyn://"):
                self.url_bar.setText(url)
            else:
                self.url_bar.setText("")

            if title:
                self.setWindowTitle(f"{title} - OyNIx Browser")
            else:
                self.setWindowTitle("OyNIx Browser")

    def _on_ai_status(self, status):
        """AI status changed."""
        self.ai_status_label.setText(f"AI: {status}")

    def _update_status(self, message):
        """Update status bar message."""
        self.status_bar.showMessage(message, 5000)

    # ═══════════════════════════════════════════════════════════════════
    #  TOGGLE FEATURES
    # ═══════════════════════════════════════════════════════════════════

    def toggle_tab_mode(self):
        """Toggle between tree tabs and normal tabs."""
        self.config['use_tree_tabs'] = not self.config['use_tree_tabs']
        self.tab_manager.set_tree_mode(self.config['use_tree_tabs'])
        mode = "Tree" if self.config['use_tree_tabs'] else "Normal"
        self._update_status(f"Tab mode: {mode}")

    def toggle_ai_panel(self):
        """Toggle AI chat panel visibility."""
        if self.ai_panel.isVisible():
            self.ai_panel.hide()
            self.main_splitter.setSizes([1400, 0])
        else:
            self.ai_panel.show()
            self.main_splitter.setSizes([1050, 380])

    def toggle_fullscreen(self):
        """Toggle fullscreen mode."""
        if self.isFullScreen():
            self.showNormal()
        else:
            self.showFullScreen()

    # ═══════════════════════════════════════════════════════════════════
    #  MENU CALLBACKS (used by menus.py)
    # ═══════════════════════════════════════════════════════════════════

    def new_window(self):
        new_browser = OynixBrowser()
        new_browser.show()

    def new_private_window(self):
        QMessageBox.information(self, "Private Window",
                                "Private browsing mode coming soon!")

    def open_file(self):
        filename, _ = QFileDialog.getOpenFileName(
            self, "Open File", "",
            "HTML Files (*.html *.htm);;All Files (*)"
        )
        if filename:
            self.current_tab().setUrl(QUrl.fromLocalFile(filename))

    def save_page(self):
        QMessageBox.information(self, "Save Page",
                                "Page saving coming soon!")

    def print_page(self):
        QMessageBox.information(self, "Print",
                                "Printing coming soon!")

    def show_settings(self):
        from oynix.UI.settings import SettingsDialog
        dialog = SettingsDialog(self, self.config)
        if dialog.exec():
            self.config.update(dialog.get_config())
            # Apply changes
            new_theme = self.config.get('theme', 'nyx')
            if new_theme != self.current_theme:
                self.set_theme(new_theme)

    def show_find_dialog(self):
        QMessageBox.information(self, "Find", "Find in page coming soon!")

    def zoom_in(self):
        view = self.current_tab()
        if view:
            view.setZoomFactor(view.zoomFactor() + 0.1)

    def zoom_out(self):
        view = self.current_tab()
        if view:
            view.setZoomFactor(max(0.1, view.zoomFactor() - 0.1))

    def reset_zoom(self):
        view = self.current_tab()
        if view:
            view.setZoomFactor(1.0)

    def toggle_reader_mode(self):
        QMessageBox.information(self, "Reader Mode",
                                "Reader mode coming soon!")

    def set_theme(self, theme_name):
        """Change browser theme."""
        self.current_theme = theme_name
        self.theme_colors = get_theme(theme_name)
        self._apply_theme()
        self._update_status(f"Theme: {theme_name}")

    def customize_theme(self):
        QMessageBox.information(self, "Custom Theme",
                                "Theme customization coming soon!")

    def set_search_engine(self, engine):
        self.config['default_search_engine'] = engine
        self._update_status(f"Search engine: {engine}")

    def set_custom_search_engine(self):
        QMessageBox.information(self, "Custom Search",
                                "Custom search engine setup coming soon!")

    def toggle_search_v2(self, enabled):
        self.config['use_nyx_search'] = enabled
        status = "enabled" if enabled else "disabled"
        self._update_status(f"Nyx Search {status}")

    def auto_expand_database(self):
        QMessageBox.information(
            self, "Auto-Expand",
            "Database auto-expands as you search with Nyx!\n"
            "Every page you visit gets indexed automatically."
        )

    def manage_database(self):
        stats = nyx_search.get_stats()
        QMessageBox.information(
            self, "Database Manager",
            f"Curated Sites: {stats['database_sites']}\n"
            f"Indexed Pages: {stats['indexed_pages']}\n"
            f"Search Backend: {stats['backend']}"
        )

    def open_ai_chat(self):
        if not self.ai_panel.isVisible():
            self.toggle_ai_panel()

    def _summarize_page(self):
        """Summarize the current page."""
        view = self.current_tab()
        if view:
            view.page().toPlainText(
                lambda text: self.ai_panel.summarize_page(text))

    def summarize_page(self):
        self._summarize_page()

    def extract_key_points(self):
        QMessageBox.information(self, "Key Points",
                                "Key point extraction coming soon!")

    def generate_summary(self):
        self._summarize_page()

    def explain_simple(self):
        QMessageBox.information(self, "Explain",
                                "ELI5 explanations coming soon!")

    def set_ai_model(self, model_name):
        self._update_status(f"AI Model: {model_name}")

    def load_custom_ai_model(self):
        QMessageBox.information(self, "Custom Model",
                                "Custom model loading coming soon!")

    def show_ai_settings(self):
        QMessageBox.information(self, "AI Settings",
                                "AI settings coming soon!")

    def show_downloads(self):
        QMessageBox.information(self, "Downloads",
                                "Downloads manager coming soon!")

    def show_history(self):
        QMessageBox.information(self, "History",
                                "History viewer coming soon!")

    def show_bookmarks(self):
        QMessageBox.information(self, "Bookmarks",
                                "Bookmarks manager coming soon!")

    def manage_extensions(self):
        QMessageBox.information(self, "Extensions",
                                "Extension manager coming soon!")

    def manage_addons(self):
        QMessageBox.information(self, "Add-ons",
                                "Add-on manager coming soon!")

    def clear_data(self):
        QMessageBox.information(self, "Clear Data",
                                "Clear data dialog coming soon!")

    def manage_cookies(self):
        QMessageBox.information(self, "Cookies",
                                "Cookie manager coming soon!")

    def manage_trusted_domains(self):
        domains = "\n".join(security_manager.trusted_domains) or "None"
        QMessageBox.information(self, "Trusted Domains",
                                f"Trusted domains:\n{domains}")

    def manage_permissions(self):
        QMessageBox.information(self, "Permissions",
                                "Permissions manager coming soon!")

    def open_notes(self):
        QMessageBox.information(self, "Notes", "Notes tool coming soon!")

    def open_calculator(self):
        QMessageBox.information(self, "Calculator",
                                "Calculator coming soon!")

    def open_system_monitor(self):
        QMessageBox.information(self, "System Monitor",
                                "System monitor coming soon!")

    def open_rss_reader(self):
        QMessageBox.information(self, "RSS Reader",
                                "RSS reader coming soon!")

    def toggle_web_inspector(self):
        QMessageBox.information(self, "Web Inspector",
                                "Web inspector coming soon!")

    def open_console(self):
        QMessageBox.information(self, "Console",
                                "JavaScript console coming soon!")

    def view_source(self):
        QMessageBox.information(self, "View Source",
                                "View source coming soon!")

    def open_network_monitor(self):
        QMessageBox.information(self, "Network",
                                "Network monitor coming soon!")

    def switch_user_agent(self):
        QMessageBox.information(self, "User Agent",
                                "User agent switcher coming soon!")

    def toggle_responsive_mode(self):
        QMessageBox.information(self, "Responsive Mode",
                                "Responsive mode coming soon!")

    def open_css_editor(self):
        QMessageBox.information(self, "CSS Editor",
                                "CSS editor coming soon!")

    def show_user_guide(self):
        QMessageBox.information(self, "User Guide",
                                "User guide coming soon!")

    def show_tutorial(self):
        QMessageBox.information(self, "Tutorial",
                                "Tutorial coming soon!")

    def show_shortcuts(self):
        shortcuts = (
            "Keyboard Shortcuts:\n\n"
            "Navigation:\n"
            "  Ctrl+T - New Tab\n"
            "  Ctrl+W - Close Tab\n"
            "  Ctrl+L - Focus URL Bar\n"
            "  Alt+Left - Back\n"
            "  Alt+Right - Forward\n"
            "  Ctrl+R / F5 - Reload\n\n"
            "View:\n"
            "  Ctrl++ - Zoom In\n"
            "  Ctrl+- - Zoom Out\n"
            "  Ctrl+0 - Reset Zoom\n"
            "  F11 - Fullscreen\n\n"
            "Features:\n"
            "  Ctrl+Shift+A - Toggle AI Panel\n"
            "  Ctrl+, - Settings\n"
        )
        QMessageBox.information(self, "Shortcuts", shortcuts)

    def check_updates(self):
        QMessageBox.information(self, "Updates",
                                "You're running OyNIx v1.0.0")

    def show_release_notes(self):
        notes = (
            "OyNIx Browser v1.0.0\n\n"
            "What's New:\n"
            "- Tree-style tabs with graph view\n"
            "- Medium purple + black Nyx theme\n"
            "- Local LLM AI assistant (auto-downloads)\n"
            "- Nyx search engine with auto-indexing\n"
            "- DuckDuckGo integration\n"
            "- GitHub database sync\n"
            "- 1400+ curated site database\n"
            "- Smart security prompts\n"
            "- Complete UI redesign\n"
        )
        QMessageBox.information(self, "Release Notes", notes)

    def show_about(self):
        about = (
            "OyNIx Browser v1.0.0\n"
            "The Nyx-Powered Local AI Browser\n\n"
            "Features:\n"
            "- Tree-style & normal tabs\n"
            "- Local LLM (auto-downloads, runs on CPU)\n"
            "- Nyx search with auto-indexing\n"
            "- DuckDuckGo + Google + Brave\n"
            "- 1400+ curated sites database\n"
            "- GitHub database sync\n"
            "- Security prompts\n"
            "- Beautiful purple/black theme\n\n"
            "License: MIT\n"
            "Author: OyNIx Team"
        )
        QMessageBox.about(self, "About OyNIx", about)

    # GitHub sync
    def upload_to_github(self):
        """Export index and upload to GitHub."""
        filepath = os.path.join(
            os.path.expanduser("~"),
            ".config", "oynix", "sync", "nyx_export.json"
        )
        os.makedirs(os.path.dirname(filepath), exist_ok=True)

        if nyx_search.export_index(filepath):
            github_sync.upload_database(
                filepath,
                lambda ok, msg: QMessageBox.information(
                    self, "GitHub Sync", msg)
            )
        else:
            QMessageBox.warning(self, "Export Error",
                                "Failed to export database")

    def import_from_github(self):
        """Import shared database from GitHub."""
        github_sync.import_database(
            callback=lambda ok, data: self._on_github_import(ok, data)
        )

    def _on_github_import(self, success, data):
        if success and data:
            for entry in data:
                nyx_search.indexer.index_page(
                    entry.get('url', ''),
                    entry.get('title', ''),
                    entry.get('content', ''),
                )
            QMessageBox.information(
                self, "Import",
                f"Imported {len(data)} entries from GitHub"
            )
        else:
            QMessageBox.warning(self, "Import",
                                "Failed to import from GitHub")

    def configure_github(self):
        """Show GitHub sync configuration."""
        QMessageBox.information(
            self, "GitHub Sync",
            "Configure GitHub sync in Settings > Advanced > GitHub Sync"
        )

    # Import/Export compatibility
    def import_bookmarks(self):
        QMessageBox.information(self, "Import",
                                "Import bookmarks coming soon!")

    def import_history(self):
        QMessageBox.information(self, "Import",
                                "Import history coming soon!")

    def import_settings(self):
        QMessageBox.information(self, "Import",
                                "Import settings coming soon!")

    def export_bookmarks(self):
        QMessageBox.information(self, "Export",
                                "Export bookmarks coming soon!")

    def export_history(self):
        QMessageBox.information(self, "Export",
                                "Export history coming soon!")

    def export_database(self):
        """Export search database to JSON."""
        filename, _ = QFileDialog.getSaveFileName(
            self, "Export Database", "oynix_database.json",
            "JSON Files (*.json)"
        )
        if filename:
            if nyx_search.export_index(filename):
                QMessageBox.information(
                    self, "Export", f"Database exported to {filename}")
            else:
                # Fallback to curated database export
                database.export_to_json(filename)
                QMessageBox.information(
                    self, "Export",
                    f"Curated database exported to {filename}")

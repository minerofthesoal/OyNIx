"""
OyNIx Browser v1.0.0 - Menu System
Nyx-themed menus with all browser features.
"""

from PyQt6.QtWidgets import QMenu, QMenuBar
from PyQt6.QtGui import QAction, QKeySequence


class OynixMenuBar:
    """Menu bar for OyNIx Browser."""

    def __init__(self, browser_window):
        self.browser = browser_window
        self.menubar = browser_window.menuBar()
        self._create_all()

    def _create_all(self):
        self._file_menu()
        self._edit_menu()
        self._view_menu()
        self._search_menu()
        self._tools_menu()
        self._ai_menu()
        self._developer_menu()
        self._help_menu()

    def _file_menu(self):
        m = self.menubar.addMenu("File")

        self._action(m, "New Tab", "Ctrl+T", self.browser.new_tab)
        self._action(m, "New Window", "Ctrl+N", self.browser.new_window)
        self._action(m, "New Private Window", "Ctrl+Shift+P",
                     self.browser.new_private_window)
        m.addSeparator()
        self._action(m, "Open File...", "Ctrl+O", self.browser.open_file)
        self._action(m, "Save Page As...", "Ctrl+S", self.browser.save_page)
        self._action(m, "Print...", "Ctrl+P", self.browser.print_page)
        m.addSeparator()

        imp = m.addMenu("Import")
        imp.addAction("Import Bookmarks", self.browser.import_bookmarks)
        imp.addAction("Import History", self.browser.import_history)
        imp.addAction("Import Settings", self.browser.import_settings)

        exp = m.addMenu("Export")
        exp.addAction("Export Bookmarks", self.browser.export_bookmarks)
        exp.addAction("Export History", self.browser.export_history)
        exp.addAction("Export Database", self.browser.export_database)

        m.addSeparator()
        self._action(m, "Close Tab", "Ctrl+W", self.browser.close_tab)
        self._action(m, "Quit", "Ctrl+Q", self.browser.close)

    def _edit_menu(self):
        m = self.menubar.addMenu("Edit")
        self._action(m, "Undo", "Ctrl+Z")
        self._action(m, "Redo", "Ctrl+Y")
        m.addSeparator()
        self._action(m, "Cut", "Ctrl+X")
        self._action(m, "Copy", "Ctrl+C")
        self._action(m, "Paste", "Ctrl+V")
        m.addSeparator()
        self._action(m, "Find in Page...", "Ctrl+F",
                     self.browser.show_find_dialog)
        m.addSeparator()
        self._action(m, "Settings", "Ctrl+,", self.browser.show_settings)

    def _view_menu(self):
        m = self.menubar.addMenu("View")
        self._action(m, "Zoom In", "Ctrl++", self.browser.zoom_in)
        self._action(m, "Zoom Out", "Ctrl+-", self.browser.zoom_out)
        self._action(m, "Reset Zoom", "Ctrl+0", self.browser.reset_zoom)
        m.addSeparator()
        self._action(m, "Full Screen", "F11", self.browser.toggle_fullscreen)
        self._action(m, "Reader Mode", "F9", self.browser.toggle_reader_mode)
        m.addSeparator()
        self._action(m, "Toggle Tree Tabs", None,
                     self.browser.toggle_tab_mode)
        self._action(m, "Toggle AI Panel", "Ctrl+Shift+A",
                     self.browser.toggle_ai_panel)
        m.addSeparator()

        themes = m.addMenu("Themes")
        themes.addAction("Nyx (Default)",
                         lambda: self.browser.set_theme("nyx"))
        themes.addAction("Nyx Midnight",
                         lambda: self.browser.set_theme("nyx_midnight"))
        themes.addAction("Nyx Violet",
                         lambda: self.browser.set_theme("nyx_violet"))
        themes.addAction("Nyx Amethyst",
                         lambda: self.browser.set_theme("nyx_amethyst"))
        themes.addSeparator()
        themes.addAction("Custom Theme...", self.browser.customize_theme)

    def _search_menu(self):
        m = self.menubar.addMenu("Search")

        self._action(m, "Nyx Search", None,
                     lambda: self.browser.set_search_engine("nyx"))
        m.addSeparator()

        engines = m.addMenu("Search Engines")
        engines.addAction("Nyx (Local + DuckDuckGo)",
                          lambda: self.browser.set_search_engine("nyx"))
        engines.addAction("DuckDuckGo",
                          lambda: self.browser.set_search_engine("duckduckgo"))
        engines.addAction("Google",
                          lambda: self.browser.set_search_engine("google"))
        engines.addAction("Brave",
                          lambda: self.browser.set_search_engine("brave"))
        engines.addSeparator()
        engines.addAction("Custom...", self.browser.set_custom_search_engine)

        m.addSeparator()
        m.addAction("Auto-Expand Database", self.browser.auto_expand_database)
        m.addAction("Manage Database", self.browser.manage_database)

        m.addSeparator()
        gh = m.addMenu("GitHub Sync")
        gh.addAction("Upload Database to GitHub",
                     self.browser.upload_to_github)
        gh.addAction("Import from GitHub",
                     self.browser.import_from_github)
        gh.addAction("Configure GitHub...",
                     self.browser.configure_github)

    def _tools_menu(self):
        m = self.menubar.addMenu("Tools")

        self._action(m, "Downloads", "Ctrl+J", self.browser.show_downloads)
        self._action(m, "History", "Ctrl+H", self.browser.show_history)
        self._action(m, "Bookmarks", "Ctrl+B", self.browser.show_bookmarks)
        m.addSeparator()
        m.addAction("Extensions", self.browser.manage_extensions)
        m.addAction("Add-ons Manager", self.browser.manage_addons)
        m.addSeparator()

        privacy = m.addMenu("Privacy & Security")
        privacy.addAction("Clear Browsing Data...", self.browser.clear_data)
        privacy.addAction("Manage Cookies...", self.browser.manage_cookies)
        privacy.addAction("Trusted Domains...",
                          self.browser.manage_trusted_domains)
        privacy.addAction("Site Permissions...",
                          self.browser.manage_permissions)

        m.addSeparator()
        m.addAction("Notes", self.browser.open_notes)
        m.addAction("Calculator", self.browser.open_calculator)
        m.addAction("System Monitor", self.browser.open_system_monitor)
        m.addAction("RSS Reader", self.browser.open_rss_reader)

    def _ai_menu(self):
        m = self.menubar.addMenu("AI")

        self._action(m, "Chat with Nyx AI", "Ctrl+Shift+A",
                     self.browser.open_ai_chat)
        m.addSeparator()
        m.addAction("Summarize Page", self.browser.summarize_page)
        m.addAction("Extract Key Points", self.browser.extract_key_points)
        m.addAction("Explain Simply", self.browser.explain_simple)
        m.addSeparator()

        models = m.addMenu("AI Models")
        models.addAction(
            "TinyLlama 1.1B (Default)",
            lambda: self.browser.set_ai_model("tinyllama"))
        models.addAction("Custom Model...",
                         self.browser.load_custom_ai_model)
        m.addSeparator()
        m.addAction("AI Settings", self.browser.show_ai_settings)

    def _developer_menu(self):
        m = self.menubar.addMenu("Developer")

        self._action(m, "Web Inspector", "F12",
                     self.browser.toggle_web_inspector)
        self._action(m, "JavaScript Console", "Ctrl+Shift+J",
                     self.browser.open_console)
        m.addSeparator()
        self._action(m, "View Page Source", "Ctrl+U",
                     self.browser.view_source)
        m.addAction("Network Monitor", self.browser.open_network_monitor)
        m.addSeparator()
        m.addAction("User Agent Switcher", self.browser.switch_user_agent)
        m.addAction("Responsive Design Mode",
                    self.browser.toggle_responsive_mode)
        m.addAction("CSS Editor", self.browser.open_css_editor)

    def _help_menu(self):
        m = self.menubar.addMenu("Help")
        m.addAction("User Guide", self.browser.show_user_guide)
        m.addAction("Quick Start Tutorial", self.browser.show_tutorial)
        m.addAction("Keyboard Shortcuts", self.browser.show_shortcuts)
        m.addSeparator()
        m.addAction("Check for Updates", self.browser.check_updates)
        m.addAction("Release Notes", self.browser.show_release_notes)
        m.addSeparator()
        m.addAction("About OyNIx", self.browser.show_about)

    def _action(self, menu, text, shortcut=None, callback=None):
        """Helper to add a menu action."""
        action = QAction(text, self.browser)
        if shortcut:
            action.setShortcut(QKeySequence(shortcut))
        if callback:
            action.triggered.connect(callback)
        menu.addAction(action)
        return action


def create_oynix_menus(browser_window):
    """Create all OyNIx menus."""
    return OynixMenuBar(browser_window)

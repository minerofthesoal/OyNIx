"""
OyNIx Browser v2.0 - Menu System
Complete menu bar with all features including XPI import.
"""
from PyQt6.QtWidgets import QMenu
from PyQt6.QtGui import QAction, QKeySequence


class OynixMenuBar:
    def __init__(self, browser_window):
        self.b = browser_window
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

    def _a(self, menu, text, callback, shortcut=None):
        action = QAction(text, self.b)
        if shortcut:
            action.setShortcut(QKeySequence(shortcut))
        action.triggered.connect(callback)
        menu.addAction(action)
        return action

    def _file_menu(self):
        m = self.menubar.addMenu("File")
        self._a(m, "New Tab", lambda: self.b.new_tab(), "Ctrl+T")
        self._a(m, "New Window", self.b.new_window, "Ctrl+N")
        self._a(m, "New Private Window", self.b.new_private_window, "Ctrl+Shift+P")
        m.addSeparator()
        self._a(m, "Open File...", self.b.open_file, "Ctrl+O")
        self._a(m, "Save Page...", self.b.save_page, "Ctrl+S")
        self._a(m, "Print to PDF", self.b.print_page, "Ctrl+P")
        m.addSeparator()
        imp = m.addMenu("Import")
        self._a(imp, "Bookmarks...", self.b.import_bookmarks)
        self._a(imp, "History...", self.b.import_history)
        self._a(imp, "Settings...", self.b.import_settings)
        self._a(imp, "XPI Extension...", self.b.import_xpi_extension)
        exp = m.addMenu("Export")
        self._a(exp, "Bookmarks...", self.b.export_bookmarks)
        self._a(exp, "History...", self.b.export_history)
        self._a(exp, "Database...", self.b.export_database)
        self._a(exp, "Settings...", self.b.export_settings)
        m.addSeparator()
        self._a(m, "Close Tab", self.b.close_tab, "Ctrl+W")
        self._a(m, "Quit", lambda: self.b.close(), "Ctrl+Q")

    def _edit_menu(self):
        m = self.menubar.addMenu("Edit")
        self._a(m, "Find in Page", self.b.show_find_dialog, "Ctrl+F")
        m.addSeparator()
        self._a(m, "Settings", self.b.show_settings, "Ctrl+,")

    def _view_menu(self):
        m = self.menubar.addMenu("View")
        self._a(m, "Zoom In", self.b.zoom_in, "Ctrl++")
        self._a(m, "Zoom Out", self.b.zoom_out, "Ctrl+-")
        self._a(m, "Reset Zoom", self.b.reset_zoom, "Ctrl+0")
        m.addSeparator()
        self._a(m, "Full Screen", self.b.toggle_fullscreen, "F11")
        self._a(m, "Reader Mode", self.b.toggle_reader_mode)
        m.addSeparator()
        self._a(m, "Toggle Tree Tabs", self.b.toggle_tab_mode)
        self._a(m, "Toggle AI Panel", self.b.toggle_ai_panel, "Ctrl+Shift+A")
        m.addSeparator()
        themes = m.addMenu("Themes")
        for name in ['nyx', 'nyx_midnight', 'nyx_violet', 'nyx_amethyst', 'nyx_ember']:
            self._a(themes, name.replace('_', ' ').title(), lambda n=name: self.b.set_theme(n))

    def _search_menu(self):
        m = self.menubar.addMenu("Search")
        engines = m.addMenu("Search Engine")
        for eng in ['nyx', 'duckduckgo', 'google', 'brave', 'bing']:
            self._a(engines, eng.title(), lambda e=eng: self.b.set_search_engine(e))
        m.addSeparator()
        self._a(m, "Auto-Expand Database", self.b.auto_expand_database)
        self._a(m, "Manage Database", self.b.manage_database)
        m.addSeparator()
        gh = m.addMenu("GitHub Sync")
        self._a(gh, "Upload Database", self.b.upload_to_github)
        self._a(gh, "Import from GitHub", self.b.import_from_github)
        self._a(gh, "Configure...", self.b.configure_github)

    def _tools_menu(self):
        m = self.menubar.addMenu("Tools")
        self._a(m, "Downloads", self.b.show_downloads, "Ctrl+J")
        self._a(m, "History", self.b.show_history, "Ctrl+H")
        self._a(m, "Bookmarks", self.b.show_bookmarks, "Ctrl+B")
        self._a(m, "Command Palette", self.b.show_command_palette, "Ctrl+K")
        m.addSeparator()
        self._a(m, "Extensions", self.b.manage_extensions)
        self._a(m, "Import XPI...", self.b.import_xpi_extension)
        m.addSeparator()
        priv = m.addMenu("Privacy & Security")
        self._a(priv, "Clear Browsing Data", self.b.clear_data)
        self._a(priv, "Trusted Domains", self.b.manage_trusted_domains)

    def _ai_menu(self):
        m = self.menubar.addMenu("AI")
        self._a(m, "Chat with Nyx AI", self.b.toggle_ai_panel, "Ctrl+Shift+A")
        self._a(m, "Summarize Page", self.b.summarize_page)
        self._a(m, "Extract Key Points", self.b.extract_key_points)
        self._a(m, "Explain Simply", self.b.explain_simple)
        m.addSeparator()
        self._a(m, "AI Model Info", self.b.set_ai_model)

    def _developer_menu(self):
        m = self.menubar.addMenu("Developer")
        self._a(m, "View Page Source", self.b.view_source, "Ctrl+U")
        self._a(m, "Toggle Reader Mode", self.b.toggle_reader_mode)

    def _help_menu(self):
        m = self.menubar.addMenu("Help")
        self._a(m, "Keyboard Shortcuts", self.b.show_shortcuts)
        self._a(m, "Check for Updates", self.b.check_updates)
        self._a(m, "Release Notes", self.b.show_release_notes)
        m.addSeparator()
        self._a(m, "About OyNIx", self.b.show_about)


def create_oynix_menus(browser_window):
    return OynixMenuBar(browser_window)

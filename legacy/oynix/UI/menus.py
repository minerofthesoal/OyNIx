"""
Oynix Browser v0.6.11.1 - Completely Remade Menus
Modern, comprehensive menu system with beautiful UI

All menus completely redesigned:
- File Menu (New, improved options)
- Edit Menu (Enhanced)
- View Menu (New display options)
- Tools Menu (Expanded)
- AI Menu (Brand new!)
- Help Menu (Comprehensive)
"""

from PyQt6.QtWidgets import (QMenu, QMenuBar, QDialog, QVBoxLayout, QHBoxLayout,
                              QLabel, QPushButton, QTabWidget, QWidget, QScrollArea,
                              QGroupBox, QGridLayout, QCheckBox, QSpinBox, QComboBox,
                              QLineEdit, QTextEdit, QSlider, QFrame)
from PyQt6.QtGui import QAction, QIcon, QKeySequence
from PyQt6.QtCore import Qt, pyqtSignal

class OynixMenuBar:
    """
    Completely remade menu bar for Oynix v0.6.11.1.
    Modern, organized, feature-rich.
    """
    
    def __init__(self, browser_window):
        self.browser = browser_window
        self.menubar = browser_window.menuBar()
        self.create_all_menus()
    
    def create_all_menus(self):
        """Create all menu categories."""
        self.create_file_menu()
        self.create_edit_menu()
        self.create_view_menu()
        self.create_search_menu()
        self.create_tools_menu()
        self.create_ai_menu()
        self.create_developer_menu()
        self.create_help_menu()
    
    def create_file_menu(self):
        """File menu - Complete remake."""
        file_menu = self.menubar.addMenu("📁 File")
        
        # New Tab
        new_tab = QAction("🆕 New Tab", self.browser)
        new_tab.setShortcut(QKeySequence("Ctrl+T"))
        new_tab.triggered.connect(self.browser.new_tab)
        file_menu.addAction(new_tab)
        
        # New Window
        new_window = QAction("🪟 New Window", self.browser)
        new_window.setShortcut(QKeySequence("Ctrl+N"))
        new_window.triggered.connect(self.browser.new_window)
        file_menu.addAction(new_window)
        
        # New Private Window
        private_window = QAction("🕵️ New Private Window", self.browser)
        private_window.setShortcut(QKeySequence("Ctrl+Shift+P"))
        private_window.triggered.connect(self.browser.new_private_window)
        file_menu.addAction(private_window)
        
        file_menu.addSeparator()
        
        # Open File
        open_file = QAction("📂 Open File...", self.browser)
        open_file.setShortcut(QKeySequence("Ctrl+O"))
        open_file.triggered.connect(self.browser.open_file)
        file_menu.addAction(open_file)
        
        # Save Page
        save_page = QAction("💾 Save Page As...", self.browser)
        save_page.setShortcut(QKeySequence("Ctrl+S"))
        save_page.triggered.connect(self.browser.save_page)
        file_menu.addAction(save_page)
        
        # Print
        print_page = QAction("🖨️ Print...", self.browser)
        print_page.setShortcut(QKeySequence("Ctrl+P"))
        print_page.triggered.connect(self.browser.print_page)
        file_menu.addAction(print_page)
        
        file_menu.addSeparator()
        
        # Import/Export
        import_menu = file_menu.addMenu("📥 Import")
        import_menu.addAction("Import Bookmarks", self.browser.import_bookmarks)
        import_menu.addAction("Import History", self.browser.import_history)
        import_menu.addAction("Import Settings", self.browser.import_settings)
        
        export_menu = file_menu.addMenu("📤 Export")
        export_menu.addAction("Export Bookmarks", self.browser.export_bookmarks)
        export_menu.addAction("Export History", self.browser.export_history)
        export_menu.addAction("Export Database", self.browser.export_database)
        
        file_menu.addSeparator()
        
        # Close Tab
        close_tab = QAction("❌ Close Tab", self.browser)
        close_tab.setShortcut(QKeySequence("Ctrl+W"))
        close_tab.triggered.connect(self.browser.close_tab)
        file_menu.addAction(close_tab)
        
        # Quit
        quit_action = QAction("🚪 Quit Oynix", self.browser)
        quit_action.setShortcut(QKeySequence("Ctrl+Q"))
        quit_action.triggered.connect(self.browser.close)
        file_menu.addAction(quit_action)
    
    def create_edit_menu(self):
        """Edit menu - Enhanced."""
        edit_menu = self.menubar.addMenu("✏️ Edit")
        
        # Basic editing
        undo = QAction("↶ Undo", self.browser)
        undo.setShortcut(QKeySequence("Ctrl+Z"))
        edit_menu.addAction(undo)
        
        redo = QAction("↷ Redo", self.browser)
        redo.setShortcut(QKeySequence("Ctrl+Y"))
        edit_menu.addAction(redo)
        
        edit_menu.addSeparator()
        
        cut = QAction("✂️ Cut", self.browser)
        cut.setShortcut(QKeySequence("Ctrl+X"))
        edit_menu.addAction(cut)
        
        copy = QAction("📋 Copy", self.browser)
        copy.setShortcut(QKeySequence("Ctrl+C"))
        edit_menu.addAction(copy)
        
        paste = QAction("📌 Paste", self.browser)
        paste.setShortcut(QKeySequence("Ctrl+V"))
        edit_menu.addAction(paste)
        
        edit_menu.addSeparator()
        
        # Find
        find = QAction("🔍 Find in Page...", self.browser)
        find.setShortcut(QKeySequence("Ctrl+F"))
        find.triggered.connect(self.browser.show_find_dialog)
        edit_menu.addAction(find)
        
        find_next = QAction("➡️ Find Next", self.browser)
        find_next.setShortcut(QKeySequence("F3"))
        edit_menu.addAction(find_next)
        
        edit_menu.addSeparator()
        
        # Settings
        settings = QAction("⚙️ Settings", self.browser)
        settings.setShortcut(QKeySequence("Ctrl+,"))
        settings.triggered.connect(self.browser.show_settings)
        edit_menu.addAction(settings)
    
    def create_view_menu(self):
        """View menu - New display options."""
        view_menu = self.menubar.addMenu("👁️ View")
        
        # Zoom
        zoom_in = QAction("🔍+ Zoom In", self.browser)
        zoom_in.setShortcut(QKeySequence("Ctrl++"))
        zoom_in.triggered.connect(self.browser.zoom_in)
        view_menu.addAction(zoom_in)
        
        zoom_out = QAction("🔍- Zoom Out", self.browser)
        zoom_out.setShortcut(QKeySequence("Ctrl+-"))
        zoom_out.triggered.connect(self.browser.zoom_out)
        view_menu.addAction(zoom_out)
        
        reset_zoom = QAction("↺ Reset Zoom", self.browser)
        reset_zoom.setShortcut(QKeySequence("Ctrl+0"))
        reset_zoom.triggered.connect(self.browser.reset_zoom)
        view_menu.addAction(reset_zoom)
        
        view_menu.addSeparator()
        
        # View modes
        fullscreen = QAction("⛶ Full Screen", self.browser)
        fullscreen.setShortcut(QKeySequence("F11"))
        fullscreen.triggered.connect(self.browser.toggle_fullscreen)
        view_menu.addAction(fullscreen)
        
        reader_mode = QAction("📖 Reader Mode", self.browser)
        reader_mode.setShortcut(QKeySequence("F9"))
        reader_mode.triggered.connect(self.browser.toggle_reader_mode)
        view_menu.addAction(reader_mode)
        
        view_menu.addSeparator()
        
        # UI elements
        show_bookmarks = QAction("⭐ Show Bookmarks Bar", self.browser)
        show_bookmarks.setCheckable(True)
        show_bookmarks.setChecked(True)
        view_menu.addAction(show_bookmarks)
        
        show_status = QAction("📊 Show Status Bar", self.browser)
        show_status.setCheckable(True)
        show_status.setChecked(True)
        view_menu.addAction(show_status)
        
        view_menu.addSeparator()
        
        # Themes submenu
        theme_menu = view_menu.addMenu("🎨 Themes")
        theme_menu.addAction("Default Theme", lambda: self.browser.set_theme("default"))
        theme_menu.addAction("Dark Mode", lambda: self.browser.set_theme("dark"))
        theme_menu.addAction("Catppuccin Mocha", lambda: self.browser.set_theme("catppuccin"))
        theme_menu.addAction("Nord", lambda: self.browser.set_theme("nord"))
        theme_menu.addAction("Dracula", lambda: self.browser.set_theme("dracula"))
        theme_menu.addAction("Custom Theme...", self.browser.customize_theme)
    
    def create_search_menu(self):
        """Search menu - NEW in v0.6.11.1."""
        search_menu = self.menubar.addMenu("🔎 Search")
        
        # Search options
        use_v2 = QAction("⚡ Enable V2 Search Engine", self.browser)
        use_v2.setCheckable(True)
        use_v2.setChecked(False)  # Disabled by default
        use_v2.triggered.connect(self.browser.toggle_search_v2)
        search_menu.addAction(use_v2)
        
        search_menu.addSeparator()
        
        # Database actions
        expand_db = QAction("🚀 Auto-Expand Database", self.browser)
        expand_db.triggered.connect(self.browser.auto_expand_database)
        search_menu.addAction(expand_db)
        
        manage_db = QAction("🗄️ Manage Database", self.browser)
        manage_db.triggered.connect(self.browser.manage_database)
        search_menu.addAction(manage_db)
        
        search_menu.addSeparator()
        
        # Search engines
        engines_menu = search_menu.addMenu("🌐 Search Engines")
        engines_menu.addAction("Google", lambda: self.browser.set_search_engine("google"))
        engines_menu.addAction("DuckDuckGo", lambda: self.browser.set_search_engine("duckduckgo"))
        engines_menu.addAction("Brave", lambda: self.browser.set_search_engine("brave"))
        engines_menu.addAction("Bing", lambda: self.browser.set_search_engine("bing"))
        engines_menu.addSeparator()
        engines_menu.addAction("Custom...", self.browser.set_custom_search_engine)
    
    def create_tools_menu(self):
        """Tools menu - Expanded."""
        tools_menu = self.menubar.addMenu("🔧 Tools")
        
        # Downloads
        downloads = QAction("⬇️ Downloads", self.browser)
        downloads.setShortcut(QKeySequence("Ctrl+J"))
        downloads.triggered.connect(self.browser.show_downloads)
        tools_menu.addAction(downloads)
        
        # History
        history = QAction("🕒 History", self.browser)
        history.setShortcut(QKeySequence("Ctrl+H"))
        history.triggered.connect(self.browser.show_history)
        tools_menu.addAction(history)
        
        # Bookmarks
        bookmarks = QAction("⭐ Bookmarks", self.browser)
        bookmarks.setShortcut(QKeySequence("Ctrl+B"))
        bookmarks.triggered.connect(self.browser.show_bookmarks)
        tools_menu.addAction(bookmarks)
        
        tools_menu.addSeparator()
        
        # Extensions
        extensions = QAction("🧩 Extensions", self.browser)
        extensions.triggered.connect(self.browser.manage_extensions)
        tools_menu.addAction(extensions)
        
        # Add-ons
        addons = QAction("➕ Add-ons Manager", self.browser)
        addons.triggered.connect(self.browser.manage_addons)
        tools_menu.addAction(addons)
        
        tools_menu.addSeparator()
        
        # Privacy tools
        privacy_menu = tools_menu.addMenu("🔒 Privacy & Security")
        privacy_menu.addAction("Clear Browsing Data...", self.browser.clear_data)
        privacy_menu.addAction("Manage Cookies...", self.browser.manage_cookies)
        privacy_menu.addAction("Trusted Domains...", self.browser.manage_trusted_domains)
        privacy_menu.addAction("Site Permissions...", self.browser.manage_permissions)
        
        tools_menu.addSeparator()
        
        # Utilities
        tools_menu.addAction("📝 Notes", self.browser.open_notes)
        tools_menu.addAction("🧮 Calculator", self.browser.open_calculator)
        tools_menu.addAction("📊 System Monitor", self.browser.open_system_monitor)
        tools_menu.addAction("📡 RSS Reader", self.browser.open_rss_reader)
    
    def create_ai_menu(self):
        """AI menu - Brand new in v0.6.11.1!"""
        ai_menu = self.menubar.addMenu("🤖 AI")
        
        # AI Chat
        ai_chat = QAction("💬 Chat with AI", self.browser)
        ai_chat.setShortcut(QKeySequence("Ctrl+Alt+A"))
        ai_chat.triggered.connect(self.browser.open_ai_chat)
        ai_menu.addAction(ai_chat)
        
        # AI Tools
        ai_menu.addSeparator()
        ai_menu.addAction("✨ Summarize Page", self.browser.summarize_page)
        ai_menu.addAction("🔍 Extract Key Points", self.browser.extract_key_points)
        ai_menu.addAction("📝 Generate Summary", self.browser.generate_summary)
        ai_menu.addAction("💡 Explain Like I'm 5", self.browser.explain_simple)
        
        ai_menu.addSeparator()
        
        # AI Models
        models_menu = ai_menu.addMenu("🧠 AI Models")
        models_menu.addAction("Nano-mini-6.99-v1 (Default)", 
                             lambda: self.browser.set_ai_model("nano-mini"))
        models_menu.addAction("Custom Model...", self.browser.load_custom_ai_model)
        
        ai_menu.addSeparator()
        
        # AI Settings
        ai_menu.addAction("⚙️ AI Settings", self.browser.show_ai_settings)
    
    def create_developer_menu(self):
        """Developer menu - For power users."""
        dev_menu = self.menubar.addMenu("👨‍💻 Developer")
        
        # Inspector
        inspector = QAction("🔍 Web Inspector", self.browser)
        inspector.setShortcut(QKeySequence("F12"))
        inspector.triggered.connect(self.browser.toggle_web_inspector)
        dev_menu.addAction(inspector)
        
        # Console
        console = QAction("🖥️ JavaScript Console", self.browser)
        console.setShortcut(QKeySequence("Ctrl+Shift+J"))
        console.triggered.connect(self.browser.open_console)
        dev_menu.addAction(console)
        
        dev_menu.addSeparator()
        
        # View source
        view_source = QAction("📄 View Page Source", self.browser)
        view_source.setShortcut(QKeySequence("Ctrl+U"))
        view_source.triggered.connect(self.browser.view_source)
        dev_menu.addAction(view_source)
        
        # Network monitor
        network = QAction("📡 Network Monitor", self.browser)
        network.triggered.connect(self.browser.open_network_monitor)
        dev_menu.addAction(network)
        
        dev_menu.addSeparator()
        
        # Developer tools
        dev_menu.addAction("🔧 User Agent Switcher", self.browser.switch_user_agent)
        dev_menu.addAction("📱 Responsive Design Mode", self.browser.toggle_responsive_mode)
        dev_menu.addAction("🎨 CSS Editor", self.browser.open_css_editor)
    
    def create_help_menu(self):
        """Help menu - Comprehensive."""
        help_menu = self.menubar.addMenu("❓ Help")
        
        # Help topics
        help_menu.addAction("📖 User Guide", self.browser.show_user_guide)
        help_menu.addAction("🚀 Quick Start Tutorial", self.browser.show_tutorial)
        help_menu.addAction("⌨️ Keyboard Shortcuts", self.browser.show_shortcuts)
        
        help_menu.addSeparator()
        
        # Online help
        help_menu.addAction("🌐 Online Documentation", 
                           lambda: self.browser.open_url("https://github.com/oynix/docs"))
        help_menu.addAction("💬 Community Forum", 
                           lambda: self.browser.open_url("https://github.com/oynix/discussions"))
        help_menu.addAction("🐛 Report a Bug", 
                           lambda: self.browser.open_url("https://github.com/oynix/issues"))
        
        help_menu.addSeparator()
        
        # Updates
        help_menu.addAction("🔄 Check for Updates", self.browser.check_updates)
        help_menu.addAction("📋 Release Notes", self.browser.show_release_notes)
        
        help_menu.addSeparator()
        
        # About
        about = QAction("ℹ️ About Oynix", self.browser)
        about.triggered.connect(self.browser.show_about)
        help_menu.addAction(about)


# Create modern menu system
def create_oynix_menus(browser_window):
    """
    Create all Oynix menus for a browser window.
    
    Args:
        browser_window: Main browser window instance
    
    Returns:
        OynixMenuBar instance
    """
    return OynixMenuBar(browser_window)

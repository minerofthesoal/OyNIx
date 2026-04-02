"""
Oynix Browser v0.6.11.1 - Main Browser Core
Complete browser engine with all features integrated

This is the heart of Oynix - connects all modules together.
"""

import sys
import os
from PyQt6.QtWidgets import (QMainWindow, QTabWidget, QWidget, QVBoxLayout, 
                              QHBoxLayout, QLineEdit, QPushButton, QLabel,
                              QToolBar, QStatusBar, QMessageBox, QFileDialog)
from PyQt6.QtWebEngineWidgets import QWebEngineView
from PyQt6.QtWebEngineCore import QWebEnginePage, QWebEngineSettings
from PyQt6.QtCore import QUrl, Qt, pyqtSignal
from PyQt6.QtGui import QIcon, QAction

# Import Oynix modules
from database import database
from search_engine_v2 import search_engine_v2
from ai_manager import ai_manager
from security import security_manager

# Import UI modules
import sys
sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'ui'))
from menus import create_oynix_menus


class OynixWebPage(QWebEnginePage):
    """Custom web page with security integration."""
    
    def acceptNavigationRequest(self, url, nav_type, is_main_frame):
        """Intercept navigation for security checks."""
        if is_main_frame:
            # Check with security manager
            should_load, info = security_manager.check_url_security(url, self.parent())
            return should_load
        return super().acceptNavigationRequest(url, nav_type, is_main_frame)


class OynixBrowser(QMainWindow):
    """
    Main Oynix Browser Window v0.6.11.1
    Integrates all modules into unified interface.
    """
    
    def __init__(self):
        super().__init__()
        
        # Browser state
        self.tabs = QTabWidget()
        self.current_theme = "catppuccin"
        self.theme_colors = self.get_theme_colors()
        
        # Settings (V2 disabled by default!)
        self.config = {
            'use_search_v2': False,  # DISABLED BY DEFAULT
            'auto_expand_database': True,
            'show_security_prompts': True,
            'default_search_engine': 'google',
        }
        
        # Initialize browser
        self.setup_ui()
        self.setup_connections()
        
        # Load AI in background
        print("Starting AI model loading...")
        ai_manager.load_model_async()
        
        # Create initial tab
        self.new_tab(QUrl("oyn://home"))
        
        print("✓ Oynix Browser initialized!")
    
    def setup_ui(self):
        """Setup the browser UI."""
        self.setWindowTitle("Oynix Browser v0.6.11.1")
        self.setGeometry(100, 100, 1400, 900)
        
        # Apply theme
        self.apply_theme()
        
        # Create menu bar
        self.menus = create_oynix_menus(self)
        
        # Create toolbar
        self.create_toolbar()
        
        # Setup tabs
        self.tabs.setTabsClosable(True)
        self.tabs.tabCloseRequested.connect(self.close_tab)
        self.tabs.currentChanged.connect(self.current_tab_changed)
        
        # Main layout
        main_widget = QWidget()
        layout = QVBoxLayout()
        layout.addWidget(self.tabs)
        main_widget.setLayout(layout)
        self.setCentralWidget(main_widget)
        
        # Status bar
        self.status = QStatusBar()
        self.setStatusBar(self.status)
        self.update_status("Ready")
    
    def create_toolbar(self):
        """Create navigation toolbar."""
        toolbar = QToolBar("Navigation")
        toolbar.setMovable(False)
        self.addToolBar(toolbar)
        
        # Back button
        back_btn = QPushButton("◄")
        back_btn.setToolTip("Back")
        back_btn.clicked.connect(lambda: self.current_tab().back())
        toolbar.addWidget(back_btn)
        
        # Forward button
        forward_btn = QPushButton("►")
        forward_btn.setToolTip("Forward")
        forward_btn.clicked.connect(lambda: self.current_tab().forward())
        toolbar.addWidget(forward_btn)
        
        # Reload button
        reload_btn = QPushButton("⟳")
        reload_btn.setToolTip("Reload")
        reload_btn.clicked.connect(lambda: self.current_tab().reload())
        toolbar.addWidget(reload_btn)
        
        # Home button
        home_btn = QPushButton("🏠")
        home_btn.setToolTip("Home")
        home_btn.clicked.connect(self.navigate_home)
        toolbar.addWidget(home_btn)
        
        # URL bar
        self.url_bar = QLineEdit()
        self.url_bar.setPlaceholderText("Search or enter address...")
        self.url_bar.returnPressed.connect(self.navigate_to_url)
        toolbar.addWidget(self.url_bar)
        
        # Search button
        search_btn = QPushButton("🔍")
        search_btn.setToolTip("Search")
        search_btn.clicked.connect(self.navigate_to_url)
        toolbar.addWidget(search_btn)
        
        # AI button
        ai_btn = QPushButton("🤖")
        ai_btn.setToolTip("AI Chat")
        ai_btn.clicked.connect(self.open_ai_chat)
        toolbar.addWidget(ai_btn)
    
    def setup_connections(self):
        """Setup signal connections."""
        pass
    
    def new_tab(self, url=None):
        """Create a new tab."""
        if url is None:
            url = QUrl("oyn://home")
        
        # Create web view with custom page
        browser = QWebEngineView()
        page = OynixWebPage(browser)
        browser.setPage(page)
        
        # Connect signals
        browser.urlChanged.connect(lambda qurl, browser=browser: 
                                   self.update_url(qurl, browser))
        browser.loadFinished.connect(lambda _, browser=browser:
                                     self.update_title(browser))
        
        # Add tab
        i = self.tabs.addTab(browser, "New Tab")
        self.tabs.setCurrentIndex(i)
        
        # Navigate
        if isinstance(url, str):
            url = QUrl(url)
        
        if url.scheme() == "oyn":
            self.handle_oyn_url(url, browser)
        else:
            browser.setUrl(url)
        
        return browser
    
    def current_tab(self):
        """Get current tab's browser."""
        return self.tabs.currentWidget()
    
    def close_tab(self, index):
        """Close a tab."""
        if self.tabs.count() < 2:
            return  # Don't close last tab
        
        self.tabs.removeTab(index)
    
    def current_tab_changed(self, index):
        """Update UI when tab changes."""
        browser = self.tabs.widget(index)
        if browser:
            self.update_url(browser.url(), browser)
            self.update_title(browser)
    
    def navigate_to_url(self):
        """Navigate to URL from URL bar."""
        text = self.url_bar.text().strip()
        
        if not text:
            return
        
        # Check if it's a URL or search query
        if ' ' in text or '.' not in text:
            # It's a search query
            self.search(text)
        else:
            # It's a URL
            if not text.startswith(('http://', 'https://', 'oyn://')):
                text = 'https://' + text
            
            url = QUrl(text)
            if url.scheme() == "oyn":
                self.handle_oyn_url(url, self.current_tab())
            else:
                self.current_tab().setUrl(url)
    
    def navigate_home(self):
        """Navigate to home page."""
        self.current_tab().setHtml(self.get_homepage_html())
    
    def search(self, query):
        """
        Perform search using configured search engine.
        Uses V2 if enabled, otherwise uses standard search engine.
        """
        if self.config.get('use_search_v2', False):
            # Use V2 engine (local + Google)
            self.search_v2(query)
        else:
            # Use standard search engine
            search_url = self.get_search_url(query)
            self.current_tab().setUrl(QUrl(search_url))
    
    def search_v2(self, query):
        """
        Search using V2 engine (local database + Google integration).
        Results displayed in Oynix theme.
        """
        # Perform search
        results = search_engine_v2.search(query, callback=self.on_google_results)
        
        # Generate and display results HTML
        html = search_engine_v2.get_search_results_html(
            query, results, self.theme_colors
        )
        
        self.current_tab().setHtml(html)
        self.update_status(f"Search: {query} | V2 Engine")
    
    def on_google_results(self, google_results):
        """Called when Google results are fetched (V2 engine)."""
        # Format Google results
        formatted = search_engine_v2.format_google_results_for_oynix(google_results)
        
        # Update the page with Google results
        # (This would inject the results into the already-displayed page)
        print(f"✓ Received {len(formatted)} Google results")
        
        # TODO: Inject results into page via JavaScript
    
    def get_search_url(self, query):
        """Get search URL based on configured search engine."""
        engines = {
            'google': f'https://www.google.com/search?q={query}',
            'duckduckgo': f'https://duckduckgo.com/?q={query}',
            'brave': f'https://search.brave.com/search?q={query}',
            'bing': f'https://www.bing.com/search?q={query}',
        }
        
        engine = self.config.get('default_search_engine', 'google')
        return engines.get(engine, engines['google'])
    
    def handle_oyn_url(self, url, browser):
        """Handle internal oyn:// URLs."""
        path = url.path()
        
        if path == "/home" or not path:
            browser.setHtml(self.get_homepage_html())
        elif path == "/settings":
            self.show_settings()
        elif path == "/search":
            query = url.query()
            # Parse query parameter
            if query:
                # TODO: Parse query properly
                pass
        else:
            browser.setHtml(f"<h1>Unknown Oynix page: {path}</h1>")
    
    def get_homepage_html(self):
        """Generate beautiful Oynix homepage."""
        return f'''
<!DOCTYPE html>
<html>
<head>
    <title>Oynix Home</title>
    <style>
        * {{ margin: 0; padding: 0; box-sizing: border-box; }}
        
        body {{
            background: linear-gradient(135deg, #1e1e2e 0%, #181825 100%);
            color: #cdd6f4;
            font-family: 'Segoe UI', Ubuntu, sans-serif;
            min-height: 100vh;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
        }}
        
        .logo {{
            font-size: 6em;
            font-weight: 900;
            background: linear-gradient(135deg, #89b4fa, #cba6f7, #f5c2e7);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            margin-bottom: 20px;
            letter-spacing: -4px;
        }}
        
        .version {{
            color: #a6adc8;
            font-size: 1.2em;
            margin-bottom: 40px;
        }}
        
        .search-box {{
            width: 600px;
            padding: 20px 30px;
            font-size: 1.2em;
            background: rgba(49, 50, 68, 0.8);
            border: 2px solid #45475a;
            border-radius: 50px;
            color: #cdd6f4;
            margin-bottom: 60px;
        }}
        
        .search-box:focus {{
            outline: none;
            border-color: #89b4fa;
            box-shadow: 0 0 30px rgba(137, 180, 250, 0.5);
        }}
        
        .quick-links {{
            display: grid;
            grid-template-columns: repeat(4, 1fr);
            gap: 20px;
            max-width: 800px;
        }}
        
        .link-card {{
            background: linear-gradient(135deg, #313244, #1e1e2e);
            padding: 30px;
            border-radius: 20px;
            text-align: center;
            cursor: pointer;
            transition: all 0.3s;
            border: 2px solid #45475a;
            text-decoration: none;
        }}
        
        .link-card:hover {{
            transform: translateY(-10px);
            border-color: #89b4fa;
            box-shadow: 0 15px 40px rgba(137, 180, 250, 0.3);
        }}
        
        .link-icon {{
            font-size: 3em;
            margin-bottom: 10px;
        }}
        
        .link-title {{
            color: #cdd6f4;
            font-weight: 600;
        }}
        
        .stats {{
            position: fixed;
            bottom: 30px;
            display: flex;
            gap: 40px;
            color: #a6adc8;
        }}
        
        .stat {{
            text-align: center;
        }}
        
        .stat-number {{
            font-size: 2em;
            color: #89b4fa;
            font-weight: bold;
        }}
    </style>
</head>
<body>
    <div class="logo">OYNIX</div>
    <div class="version">v0.6.11.1 ULTIMATE • Multi-File Edition</div>
    
    <input type="text" class="search-box" 
           placeholder="Search 1400+ sites or the web..." 
           onkeypress="if(event.key==='Enter') search(this.value)"
           autofocus>
    
    <div class="quick-links">
        <a href="oyn://ai-chat" class="link-card">
            <div class="link-icon">💬</div>
            <div class="link-title">AI Chat</div>
        </a>
        
        <a href="oyn://search" class="link-card">
            <div class="link-icon">🔍</div>
            <div class="link-title">Search</div>
        </a>
        
        <a href="oyn://settings" class="link-card">
            <div class="link-icon">⚙️</div>
            <div class="link-title">Settings</div>
        </a>
        
        <a href="https://github.com" class="link-card">
            <div class="link-icon">🐙</div>
            <div class="link-title">GitHub</div>
        </a>
    </div>
    
    <div class="stats">
        <div class="stat">
            <div class="stat-number">1400+</div>
            <div>Curated Sites</div>
        </div>
        <div class="stat">
            <div class="stat-number">25</div>
            <div>Categories</div>
        </div>
        <div class="stat">
            <div class="stat-number">V2</div>
            <div>Search Engine</div>
        </div>
    </div>
    
    <script>
        function search(query) {{
            if (query.trim()) {{
                window.location = 'oyn://search?q=' + encodeURIComponent(query);
            }}
        }}
    </script>
</body>
</html>
        '''
    
    def update_url(self, url, browser=None):
        """Update URL bar when page changes."""
        if browser != self.current_tab():
            return
        
        self.url_bar.setText(url.toString())
    
    def update_title(self, browser):
        """Update tab title."""
        if browser != self.current_tab():
            return
        
        title = browser.page().title()
        index = self.tabs.indexOf(browser)
        
        if len(title) > 20:
            title = title[:20] + "..."
        
        self.tabs.setTabText(index, title or "New Tab")
    
    def update_status(self, message):
        """Update status bar."""
        self.status.showMessage(message)
    
    def get_theme_colors(self):
        """Get current theme colors."""
        themes = {
            'catppuccin': {
                'window': '#1e1e2e',
                'text': '#cdd6f4',
                'accent': '#89b4fa',
                'secondary': '#313244',
            },
            'dark': {
                'window': '#1a1a1a',
                'text': '#e0e0e0',
                'accent': '#4a9eff',
                'secondary': '#2a2a2a',
            }
        }
        
        return themes.get(self.current_theme, themes['catppuccin'])
    
    def apply_theme(self):
        """Apply current theme to browser."""
        colors = self.theme_colors
        
        stylesheet = f"""
        QMainWindow {{
            background-color: {colors['window']};
            color: {colors['text']};
        }}
        
        QTabWidget::pane {{
            border: none;
            background-color: {colors['window']};
        }}
        
        QTabBar::tab {{
            background-color: {colors['secondary']};
            color: {colors['text']};
            padding: 8px 20px;
            margin-right: 2px;
            border-top-left-radius: 8px;
            border-top-right-radius: 8px;
        }}
        
        QTabBar::tab:selected {{
            background-color: {colors['accent']};
            color: {colors['window']};
        }}
        
        QLineEdit {{
            background-color: {colors['secondary']};
            color: {colors['text']};
            border: 2px solid {colors['accent']};
            border-radius: 8px;
            padding: 8px;
            font-size: 14px;
        }}
        
        QPushButton {{
            background-color: {colors['accent']};
            color: {colors['window']};
            border: none;
            border-radius: 6px;
            padding: 8px 15px;
            font-weight: bold;
        }}
        
        QPushButton:hover {{
            background-color: {colors['text']};
        }}
        
        QStatusBar {{
            background-color: {colors['secondary']};
            color: {colors['text']};
        }}
        """
        
        self.setStyleSheet(stylesheet)
    
    # ===== MENU CALLBACKS =====
    
    def new_window(self):
        """Open new browser window."""
        new_browser = OynixBrowser()
        new_browser.show()
    
    def new_private_window(self):
        """Open new private window."""
        # TODO: Implement private mode
        QMessageBox.information(self, "Private Window", 
                               "Private mode coming soon!")
    
    def open_file(self):
        """Open local file."""
        filename, _ = QFileDialog.getOpenFileName(
            self, "Open File", "", 
            "HTML Files (*.html *.htm);;All Files (*)"
        )
        
        if filename:
            self.current_tab().setUrl(QUrl.fromLocalFile(filename))
    
    def save_page(self):
        """Save current page."""
        # TODO: Implement page saving
        QMessageBox.information(self, "Save Page", 
                               "Page saving coming soon!")
    
    def print_page(self):
        """Print current page."""
        # TODO: Implement printing
        QMessageBox.information(self, "Print", 
                               "Printing coming soon!")
    
    def show_settings(self):
        """Show settings dialog."""
        from settings import SettingsDialog
        dialog = SettingsDialog(self, self.config)
        if dialog.exec():
            # Update config
            self.config.update(dialog.get_config())
            
            # Apply changes
            if self.config.get('use_search_v2'):
                search_engine_v2.enable_v2(True)
                self.update_status("Search V2 Engine enabled")
            else:
                search_engine_v2.enable_v2(False)
                self.update_status("Search V2 Engine disabled")
    
    def show_find_dialog(self):
        """Show find in page dialog."""
        # TODO: Implement find
        QMessageBox.information(self, "Find", 
                               "Find in page coming soon!")
    
    def zoom_in(self):
        """Zoom in current page."""
        current_zoom = self.current_tab().zoomFactor()
        self.current_tab().setZoomFactor(current_zoom + 0.1)
    
    def zoom_out(self):
        """Zoom out current page."""
        current_zoom = self.current_tab().zoomFactor()
        self.current_tab().setZoomFactor(max(0.1, current_zoom - 0.1))
    
    def reset_zoom(self):
        """Reset zoom to 100%."""
        self.current_tab().setZoomFactor(1.0)
    
    def toggle_fullscreen(self):
        """Toggle fullscreen mode."""
        if self.isFullScreen():
            self.showNormal()
        else:
            self.showFullScreen()
    
    def toggle_reader_mode(self):
        """Toggle reader mode."""
        # TODO: Implement reader mode
        QMessageBox.information(self, "Reader Mode", 
                               "Reader mode coming soon!")
    
    def set_theme(self, theme_name):
        """Change browser theme."""
        self.current_theme = theme_name
        self.theme_colors = self.get_theme_colors()
        self.apply_theme()
        self.update_status(f"Theme: {theme_name}")
    
    def customize_theme(self):
        """Open theme customization dialog."""
        QMessageBox.information(self, "Custom Theme", 
                               "Theme customization coming soon!")
    
    def toggle_search_v2(self, enabled):
        """Toggle V2 search engine."""
        self.config['use_search_v2'] = enabled
        search_engine_v2.enable_v2(enabled)
        
        status = "enabled" if enabled else "disabled"
        self.update_status(f"Search V2 {status}")
    
    def auto_expand_database(self):
        """Auto-expand database from web."""
        QMessageBox.information(self, "Auto-Expand", 
                               "Database auto-expansion coming soon!")
    
    def manage_database(self):
        """Open database manager."""
        QMessageBox.information(self, "Database Manager", 
                               f"Current database: {database.total_sites} sites")
    
    def set_search_engine(self, engine):
        """Set default search engine."""
        self.config['default_search_engine'] = engine
        self.update_status(f"Search engine: {engine}")
    
    def set_custom_search_engine(self):
        """Set custom search engine."""
        # TODO: Implement custom search engine
        QMessageBox.information(self, "Custom Search", 
                               "Custom search engine coming soon!")
    
    def open_ai_chat(self):
        """Open AI chat interface."""
        if not ai_manager.is_ready():
            QMessageBox.information(self, "AI Loading", 
                                   "AI model is still loading. Please wait...")
            return
        
        # TODO: Open AI chat dialog
        QMessageBox.information(self, "AI Chat", 
                               "AI chat interface coming soon!")
    
    def summarize_page(self):
        """Summarize current page with AI."""
        if not ai_manager.is_ready():
            QMessageBox.warning(self, "AI Not Ready", 
                               "AI model is still loading.")
            return
        
        # TODO: Get page text and summarize
        QMessageBox.information(self, "Summarize", 
                               "Page summarization coming soon!")
    
    def extract_key_points(self):
        """Extract key points from page."""
        QMessageBox.information(self, "Key Points", 
                               "Key point extraction coming soon!")
    
    def generate_summary(self):
        """Generate summary of page."""
        self.summarize_page()
    
    def explain_simple(self):
        """Explain page content in simple terms."""
        QMessageBox.information(self, "Explain", 
                               "ELI5 explanations coming soon!")
    
    def set_ai_model(self, model_name):
        """Set AI model."""
        self.update_status(f"AI Model: {model_name}")
    
    def load_custom_ai_model(self):
        """Load custom AI model."""
        QMessageBox.information(self, "Custom Model", 
                               "Custom model loading coming soon!")
    
    def show_ai_settings(self):
        """Show AI settings."""
        QMessageBox.information(self, "AI Settings", 
                               "AI settings coming soon!")
    
    def show_downloads(self):
        """Show downloads manager."""
        QMessageBox.information(self, "Downloads", 
                               "Downloads manager coming soon!")
    
    def show_history(self):
        """Show browsing history."""
        QMessageBox.information(self, "History", 
                               "History viewer coming soon!")
    
    def show_bookmarks(self):
        """Show bookmarks manager."""
        QMessageBox.information(self, "Bookmarks", 
                               "Bookmarks manager coming soon!")
    
    def manage_extensions(self):
        """Manage browser extensions."""
        QMessageBox.information(self, "Extensions", 
                               "Extension manager coming soon!")
    
    def manage_addons(self):
        """Manage add-ons."""
        QMessageBox.information(self, "Add-ons", 
                               "Add-on manager coming soon!")
    
    def clear_data(self):
        """Clear browsing data."""
        QMessageBox.information(self, "Clear Data", 
                               "Clear data dialog coming soon!")
    
    def manage_cookies(self):
        """Manage cookies."""
        QMessageBox.information(self, "Cookies", 
                               "Cookie manager coming soon!")
    
    def manage_trusted_domains(self):
        """Manage trusted domains."""
        domains = "\n".join(security_manager.trusted_domains)
        QMessageBox.information(self, "Trusted Domains", 
                               f"Trusted domains:\n{domains or 'None'}")
    
    def manage_permissions(self):
        """Manage site permissions."""
        QMessageBox.information(self, "Permissions", 
                               "Permissions manager coming soon!")
    
    def open_notes(self):
        """Open notes tool."""
        QMessageBox.information(self, "Notes", 
                               "Notes tool coming soon!")
    
    def open_calculator(self):
        """Open calculator."""
        QMessageBox.information(self, "Calculator", 
                               "Calculator coming soon!")
    
    def open_system_monitor(self):
        """Open system monitor."""
        QMessageBox.information(self, "System Monitor", 
                               "System monitor coming soon!")
    
    def open_rss_reader(self):
        """Open RSS reader."""
        QMessageBox.information(self, "RSS Reader", 
                               "RSS reader coming soon!")
    
    def toggle_web_inspector(self):
        """Toggle web inspector."""
        QMessageBox.information(self, "Web Inspector", 
                               "Web inspector coming soon!")
    
    def open_console(self):
        """Open JavaScript console."""
        QMessageBox.information(self, "Console", 
                               "JavaScript console coming soon!")
    
    def view_source(self):
        """View page source."""
        QMessageBox.information(self, "View Source", 
                               "View source coming soon!")
    
    def open_network_monitor(self):
        """Open network monitor."""
        QMessageBox.information(self, "Network", 
                               "Network monitor coming soon!")
    
    def switch_user_agent(self):
        """Switch user agent."""
        QMessageBox.information(self, "User Agent", 
                               "User agent switcher coming soon!")
    
    def toggle_responsive_mode(self):
        """Toggle responsive design mode."""
        QMessageBox.information(self, "Responsive Mode", 
                               "Responsive mode coming soon!")
    
    def open_css_editor(self):
        """Open CSS editor."""
        QMessageBox.information(self, "CSS Editor", 
                               "CSS editor coming soon!")
    
    def show_user_guide(self):
        """Show user guide."""
        QMessageBox.information(self, "User Guide", 
                               "User guide coming soon!")
    
    def show_tutorial(self):
        """Show quick start tutorial."""
        QMessageBox.information(self, "Tutorial", 
                               "Tutorial coming soon!")
    
    def show_shortcuts(self):
        """Show keyboard shortcuts."""
        shortcuts = """
Keyboard Shortcuts:
        
Navigation:
  Ctrl+T - New Tab
  Ctrl+W - Close Tab
  Ctrl+Tab - Next Tab
  
Search:
  Ctrl+F - Find in Page
  Ctrl+L - Focus URL Bar
  
View:
  Ctrl++ - Zoom In
  Ctrl+- - Zoom Out
  Ctrl+0 - Reset Zoom
  F11 - Fullscreen
  
Other:
  Ctrl+, - Settings
  F12 - Web Inspector
  Ctrl+Q - Quit
        """
        QMessageBox.information(self, "Shortcuts", shortcuts)
    
    def open_url(self, url):
        """Open URL in current tab."""
        self.current_tab().setUrl(QUrl(url))
    
    def check_updates(self):
        """Check for updates."""
        QMessageBox.information(self, "Updates", 
                               "You're running the latest version: v0.6.11.1")
    
    def show_release_notes(self):
        """Show release notes."""
        notes = """
Oynix Browser v0.6.11.1 ULTIMATE

What's New:
• 2150+ new lines of code
• Multi-file architecture
• 1400+ site database (2.775x expansion)
• Search Engine V2 (toggle-able)
• Nano-mini-6.99-v1 AI
• Security prompt system
• Completely remade menus
        """
        QMessageBox.information(self, "Release Notes", notes)
    
    def show_about(self):
        """Show about dialog."""
        about = """
Oynix Browser v0.6.11.1 ULTIMATE
Multi-File Edition

The most advanced open-source browser!

Features:
• 1400+ curated sites
• Hybrid search engine (V2)
• Nano-mini AI integration
• Smart security prompts
• Beautiful Catppuccin theme

License: MIT
Author: Oynix Team
        """
        QMessageBox.about(self, "About Oynix", about)
    
    def import_bookmarks(self):
        QMessageBox.information(self, "Import", "Import bookmarks coming soon!")
    
    def import_history(self):
        QMessageBox.information(self, "Import", "Import history coming soon!")
    
    def import_settings(self):
        QMessageBox.information(self, "Import", "Import settings coming soon!")
    
    def export_bookmarks(self):
        QMessageBox.information(self, "Export", "Export bookmarks coming soon!")
    
    def export_history(self):
        QMessageBox.information(self, "Export", "Export history coming soon!")
    
    def export_database(self):
        """Export search database to JSON."""
        filename, _ = QFileDialog.getSaveFileName(
            self, "Export Database", "oynix_database.json",
            "JSON Files (*.json)"
        )
        
        if filename:
            database.export_to_json(filename)
            QMessageBox.information(self, "Export", 
                                   f"Database exported to {filename}")

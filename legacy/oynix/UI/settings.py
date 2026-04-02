"""
Oynix Browser v0.6.11.1 - Revamped Settings Dialog
Modern, comprehensive settings interface with tabs

Features:
- General settings
- Search settings (V2 toggle here!)
- Privacy & Security
- AI settings
- Appearance
- Advanced
"""

from PyQt6.QtWidgets import (QDialog, QVBoxLayout, QHBoxLayout, QTabWidget,
                              QWidget, QLabel, QCheckBox, QSpinBox, QComboBox,
                              QPushButton, QGroupBox, QGridLayout, QLineEdit,
                              QSlider, QColorDialog, QFontDialog)
from PyQt6.QtCore import Qt


class SettingsDialog(QDialog):
    """
    Comprehensive settings dialog for Oynix v0.6.11.1
    """
    
    def __init__(self, parent=None, config=None):
        super().__init__(parent)
        
        self.config = config or {}
        self.temp_config = self.config.copy()
        
        self.setWindowTitle("Oynix Settings")
        self.setMinimumSize(800, 600)
        
        self.setup_ui()
    
    def setup_ui(self):
        """Setup the settings UI."""
        layout = QVBoxLayout(self)
        
        # Create tab widget
        tabs = QTabWidget()
        tabs.addTab(self.create_general_tab(), "⚙️ General")
        tabs.addTab(self.create_search_tab(), "🔍 Search")
        tabs.addTab(self.create_privacy_tab(), "🔒 Privacy")
        tabs.addTab(self.create_ai_tab(), "🤖 AI")
        tabs.addTab(self.create_appearance_tab(), "🎨 Appearance")
        tabs.addTab(self.create_advanced_tab(), "🔧 Advanced")
        
        layout.addWidget(tabs)
        
        # Buttons
        button_layout = QHBoxLayout()
        button_layout.addStretch()
        
        save_btn = QPushButton("💾 Save")
        save_btn.clicked.connect(self.accept)
        
        cancel_btn = QPushButton("❌ Cancel")
        cancel_btn.clicked.connect(self.reject)
        
        button_layout.addWidget(save_btn)
        button_layout.addWidget(cancel_btn)
        
        layout.addLayout(button_layout)
    
    def create_general_tab(self):
        """General settings tab."""
        widget = QWidget()
        layout = QVBoxLayout(widget)
        
        # Startup group
        startup_group = QGroupBox("Startup")
        startup_layout = QVBoxLayout()
        
        restore_session = QCheckBox("Restore previous session on startup")
        restore_session.setChecked(self.config.get('restore_session', True))
        startup_layout.addWidget(restore_session)
        
        show_home = QCheckBox("Show home page on new tab")
        show_home.setChecked(self.config.get('show_home_on_new_tab', True))
        startup_layout.addWidget(show_home)
        
        startup_group.setLayout(startup_layout)
        layout.addWidget(startup_group)
        
        # Downloads group
        downloads_group = QGroupBox("Downloads")
        downloads_layout = QVBoxLayout()
        
        ask_download = QCheckBox("Ask where to save each file")
        ask_download.setChecked(self.config.get('ask_download_location', True))
        downloads_layout.addWidget(ask_download)
        
        auto_open = QCheckBox("Automatically open downloaded files")
        auto_open.setChecked(self.config.get('auto_open_downloads', False))
        downloads_layout.addWidget(auto_open)
        
        downloads_group.setLayout(downloads_layout)
        layout.addWidget(downloads_group)
        
        # Tabs group
        tabs_group = QGroupBox("Tabs")
        tabs_layout = QVBoxLayout()
        
        warn_close = QCheckBox("Warn when closing multiple tabs")
        warn_close.setChecked(self.config.get('warn_close_tabs', True))
        tabs_layout.addWidget(warn_close)
        
        show_tab_preview = QCheckBox("Show tab previews on hover")
        show_tab_preview.setChecked(self.config.get('show_tab_preview', True))
        tabs_layout.addWidget(show_tab_preview)
        
        tabs_group.setLayout(tabs_layout)
        layout.addWidget(tabs_group)
        
        layout.addStretch()
        return widget
    
    def create_search_tab(self):
        """Search settings tab - V2 TOGGLE HERE!"""
        widget = QWidget()
        layout = QVBoxLayout(widget)
        
        # Search Engine V2 group - MOST IMPORTANT!
        v2_group = QGroupBox("🚀 Search Engine V2 (New!)")
        v2_layout = QVBoxLayout()
        
        use_v2 = QCheckBox("Enable Search Engine V2")
        use_v2.setChecked(self.config.get('use_search_v2', False))  # Disabled by default!
        use_v2.stateChanged.connect(lambda state: 
                                    self.temp_config.update({'use_search_v2': state == Qt.CheckState.Checked}))
        v2_layout.addWidget(use_v2)
        
        v2_desc = QLabel(
            "V2 Engine combines local 1400+ site database with live Google results.\n"
            "Results are displayed in unified Oynix theme.\n"
            "⚠️ Requires internet connection for Google integration."
        )
        v2_desc.setWordWrap(True)
        v2_desc.setStyleSheet("color: #a6adc8; padding: 10px; background: #313244; border-radius: 8px;")
        v2_layout.addWidget(v2_desc)
        
        auto_expand = QCheckBox("Auto-expand database from Google results")
        auto_expand.setChecked(self.config.get('auto_expand_database', True))
        auto_expand.stateChanged.connect(lambda state:
                                         self.temp_config.update({'auto_expand_database': state == Qt.CheckState.Checked}))
        v2_layout.addWidget(auto_expand)
        
        v2_group.setLayout(v2_layout)
        layout.addWidget(v2_group)
        
        # Default search engine
        engine_group = QGroupBox("Default Search Engine")
        engine_layout = QGridLayout()
        
        engine_layout.addWidget(QLabel("Search Engine:"), 0, 0)
        
        engine_combo = QComboBox()
        engine_combo.addItems(["Google", "DuckDuckGo", "Brave", "Bing"])
        current_engine = self.config.get('default_search_engine', 'google')
        engine_combo.setCurrentText(current_engine.title())
        engine_combo.currentTextChanged.connect(lambda text:
                                                self.temp_config.update({'default_search_engine': text.lower()}))
        engine_layout.addWidget(engine_combo, 0, 1)
        
        engine_group.setLayout(engine_layout)
        layout.addWidget(engine_group)
        
        # Search suggestions
        suggestions_group = QGroupBox("Search Suggestions")
        suggestions_layout = QVBoxLayout()
        
        show_suggestions = QCheckBox("Show search suggestions")
        show_suggestions.setChecked(self.config.get('show_suggestions', True))
        suggestions_layout.addWidget(show_suggestions)
        
        local_suggestions = QCheckBox("Include local database in suggestions")
        local_suggestions.setChecked(self.config.get('local_suggestions', True))
        suggestions_layout.addWidget(local_suggestions)
        
        suggestions_group.setLayout(suggestions_layout)
        layout.addWidget(suggestions_group)
        
        layout.addStretch()
        return widget
    
    def create_privacy_tab(self):
        """Privacy & Security settings tab."""
        widget = QWidget()
        layout = QVBoxLayout(widget)
        
        # Security prompts group
        security_group = QGroupBox("🔒 Security Prompts")
        security_layout = QVBoxLayout()
        
        enable_security = QCheckBox("Enable security prompts for login pages")
        enable_security.setChecked(self.config.get('show_security_prompts', True))
        enable_security.stateChanged.connect(lambda state:
                                             self.temp_config.update({'show_security_prompts': state == Qt.CheckState.Checked}))
        security_layout.addWidget(enable_security)
        
        security_desc = QLabel(
            "Shows warnings when visiting login pages like Microsoft, Google, banking sites.\n"
            "Helps protect against phishing and unsafe sites."
        )
        security_desc.setWordWrap(True)
        security_desc.setStyleSheet("color: #a6adc8; padding: 10px; background: #313244; border-radius: 8px;")
        security_layout.addWidget(security_desc)
        
        warn_http = QCheckBox("Warn when submitting data over HTTP")
        warn_http.setChecked(self.config.get('warn_http', True))
        security_layout.addWidget(warn_http)
        
        security_group.setLayout(security_layout)
        layout.addWidget(security_group)
        
        # Privacy group
        privacy_group = QGroupBox("Privacy")
        privacy_layout = QVBoxLayout()
        
        do_not_track = QCheckBox("Send 'Do Not Track' requests")
        do_not_track.setChecked(self.config.get('do_not_track', True))
        privacy_layout.addWidget(do_not_track)
        
        block_third_party = QCheckBox("Block third-party cookies")
        block_third_party.setChecked(self.config.get('block_third_party_cookies', True))
        privacy_layout.addWidget(block_third_party)
        
        clear_on_exit = QCheckBox("Clear history on browser exit")
        clear_on_exit.setChecked(self.config.get('clear_on_exit', False))
        privacy_layout.addWidget(clear_on_exit)
        
        privacy_group.setLayout(privacy_layout)
        layout.addWidget(privacy_group)
        
        # Content blocking
        blocking_group = QGroupBox("Content Blocking")
        blocking_layout = QVBoxLayout()
        
        block_ads = QCheckBox("Block advertisements")
        block_ads.setChecked(self.config.get('block_ads', True))
        blocking_layout.addWidget(block_ads)
        
        block_trackers = QCheckBox("Block trackers")
        block_trackers.setChecked(self.config.get('block_trackers', True))
        blocking_layout.addWidget(block_trackers)
        
        block_popups = QCheckBox("Block pop-ups")
        block_popups.setChecked(self.config.get('block_popups', True))
        blocking_layout.addWidget(block_popups)
        
        blocking_group.setLayout(blocking_layout)
        layout.addWidget(blocking_group)
        
        layout.addStretch()
        return widget
    
    def create_ai_tab(self):
        """AI settings tab."""
        widget = QWidget()
        layout = QVBoxLayout(widget)
        
        # AI Model group
        model_group = QGroupBox("🤖 AI Model")
        model_layout = QGridLayout()
        
        model_layout.addWidget(QLabel("Model:"), 0, 0)
        
        model_combo = QComboBox()
        model_combo.addItems(["Nano-mini-6.99-v1 (Default)", "Custom..."])
        model_layout.addWidget(model_combo, 0, 1)
        
        model_info = QLabel(
            "Current: ray0rf1re/Nano-mini-6.99-v1\n"
            "Size: ~1.4GB | Parameters: 6.99M\n"
            "Custom config and tokenizer support"
        )
        model_info.setWordWrap(True)
        model_info.setStyleSheet("color: #a6adc8; padding: 10px; background: #313244; border-radius: 8px;")
        model_layout.addWidget(model_info, 1, 0, 1, 2)
        
        model_group.setLayout(model_layout)
        layout.addWidget(model_group)
        
        # AI Features group
        features_group = QGroupBox("AI Features")
        features_layout = QVBoxLayout()
        
        auto_summarize = QCheckBox("Auto-summarize long articles")
        auto_summarize.setChecked(self.config.get('auto_summarize', False))
        features_layout.addWidget(auto_summarize)
        
        show_ai_suggestions = QCheckBox("Show AI-powered suggestions")
        show_ai_suggestions.setChecked(self.config.get('ai_suggestions', True))
        features_layout.addWidget(show_ai_suggestions)
        
        context_aware = QCheckBox("Enable context-aware responses")
        context_aware.setChecked(self.config.get('context_aware', True))
        features_layout.addWidget(context_aware)
        
        features_group.setLayout(features_layout)
        layout.addWidget(features_group)
        
        # Performance group
        perf_group = QGroupBox("Performance")
        perf_layout = QGridLayout()
        
        perf_layout.addWidget(QLabel("Max Response Length:"), 0, 0)
        
        max_length = QSpinBox()
        max_length.setRange(50, 1000)
        max_length.setValue(self.config.get('ai_max_length', 200))
        max_length.setSuffix(" tokens")
        perf_layout.addWidget(max_length, 0, 1)
        
        perf_layout.addWidget(QLabel("Temperature:"), 1, 0)
        
        temperature = QSlider(Qt.Orientation.Horizontal)
        temperature.setRange(1, 20)
        temperature.setValue(int(self.config.get('ai_temperature', 0.7) * 10))
        temperature.setTickPosition(QSlider.TickPosition.TicksBelow)
        perf_layout.addWidget(temperature, 1, 1)
        
        perf_group.setLayout(perf_layout)
        layout.addWidget(perf_group)
        
        layout.addStretch()
        return widget
    
    def create_appearance_tab(self):
        """Appearance settings tab."""
        widget = QWidget()
        layout = QVBoxLayout(widget)
        
        # Theme group
        theme_group = QGroupBox("🎨 Theme")
        theme_layout = QGridLayout()
        
        theme_layout.addWidget(QLabel("Theme:"), 0, 0)
        
        theme_combo = QComboBox()
        theme_combo.addItems([
            "Catppuccin Mocha",
            "Dark Mode",
            "Nord",
            "Dracula",
            "Light Mode",
            "Custom..."
        ])
        current_theme = self.config.get('theme', 'catppuccin')
        theme_combo.setCurrentText(current_theme.replace('_', ' ').title())
        theme_layout.addWidget(theme_combo, 0, 1)
        
        theme_group.setLayout(theme_layout)
        layout.addWidget(theme_group)
        
        # Font group
        font_group = QGroupBox("Font")
        font_layout = QGridLayout()
        
        font_layout.addWidget(QLabel("Font Family:"), 0, 0)
        
        font_combo = QComboBox()
        font_combo.addItems(["Segoe UI", "Ubuntu", "Arial", "Helvetica", "Custom..."])
        font_layout.addWidget(font_combo, 0, 1)
        
        font_layout.addWidget(QLabel("Font Size:"), 1, 0)
        
        font_size = QSpinBox()
        font_size.setRange(8, 24)
        font_size.setValue(self.config.get('font_size', 14))
        font_size.setSuffix(" pt")
        font_layout.addWidget(font_size, 1, 1)
        
        font_group.setLayout(font_layout)
        layout.addWidget(font_group)
        
        # Display group
        display_group = QGroupBox("Display")
        display_layout = QVBoxLayout()
        
        show_bookmarks_bar = QCheckBox("Show bookmarks bar")
        show_bookmarks_bar.setChecked(self.config.get('show_bookmarks_bar', True))
        display_layout.addWidget(show_bookmarks_bar)
        
        show_status_bar = QCheckBox("Show status bar")
        show_status_bar.setChecked(self.config.get('show_status_bar', True))
        display_layout.addWidget(show_status_bar)
        
        smooth_scrolling = QCheckBox("Enable smooth scrolling")
        smooth_scrolling.setChecked(self.config.get('smooth_scrolling', True))
        display_layout.addWidget(smooth_scrolling)
        
        display_group.setLayout(display_layout)
        layout.addWidget(display_group)
        
        layout.addStretch()
        return widget
    
    def create_advanced_tab(self):
        """Advanced settings tab."""
        widget = QWidget()
        layout = QVBoxLayout(widget)
        
        # Performance group
        perf_group = QGroupBox("⚡ Performance")
        perf_layout = QVBoxLayout()
        
        hardware_accel = QCheckBox("Use hardware acceleration")
        hardware_accel.setChecked(self.config.get('hardware_acceleration', True))
        perf_layout.addWidget(hardware_accel)
        
        lazy_load = QCheckBox("Lazy load images")
        lazy_load.setChecked(self.config.get('lazy_load_images', True))
        perf_layout.addWidget(lazy_load)
        
        preload_links = QCheckBox("Preload links on hover")
        preload_links.setChecked(self.config.get('preload_links', False))
        perf_layout.addWidget(preload_links)
        
        perf_group.setLayout(perf_layout)
        layout.addWidget(perf_group)
        
        # Network group
        network_group = QGroupBox("Network")
        network_layout = QGridLayout()
        
        network_layout.addWidget(QLabel("Proxy:"), 0, 0)
        
        proxy_combo = QComboBox()
        proxy_combo.addItems(["No proxy", "System proxy", "Custom..."])
        network_layout.addWidget(proxy_combo, 0, 1)
        
        network_layout.addWidget(QLabel("DNS:"), 1, 0)
        
        dns_combo = QComboBox()
        dns_combo.addItems(["System DNS", "Cloudflare (1.1.1.1)", "Google (8.8.8.8)", "Custom..."])
        network_layout.addWidget(dns_combo, 1, 1)
        
        network_group.setLayout(network_layout)
        layout.addWidget(network_group)
        
        # Developer group
        dev_group = QGroupBox("👨‍💻 Developer")
        dev_layout = QVBoxLayout()
        
        enable_dev_tools = QCheckBox("Enable developer tools")
        enable_dev_tools.setChecked(self.config.get('enable_dev_tools', True))
        dev_layout.addWidget(enable_dev_tools)
        
        show_console = QCheckBox("Show JavaScript errors in console")
        show_console.setChecked(self.config.get('show_js_console', False))
        dev_layout.addWidget(show_console)
        
        dev_group.setLayout(dev_layout)
        layout.addWidget(dev_group)
        
        # Reset group
        reset_group = QGroupBox("⚠️ Reset")
        reset_layout = QVBoxLayout()
        
        reset_btn = QPushButton("Reset All Settings to Default")
        reset_btn.clicked.connect(self.reset_settings)
        reset_layout.addWidget(reset_btn)
        
        reset_group.setLayout(reset_layout)
        layout.addWidget(reset_group)
        
        layout.addStretch()
        return widget
    
    def reset_settings(self):
        """Reset all settings to default."""
        from PyQt6.QtWidgets import QMessageBox
        
        reply = QMessageBox.question(
            self, "Reset Settings",
            "Are you sure you want to reset all settings to default?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )
        
        if reply == QMessageBox.StandardButton.Yes:
            self.temp_config = {
                'use_search_v2': False,  # V2 disabled by default
                'auto_expand_database': True,
                'show_security_prompts': True,
                'default_search_engine': 'google',
            }
            QMessageBox.information(self, "Reset", "Settings reset to default!")
    
    def get_config(self):
        """Get current configuration."""
        return self.temp_config

"""
OyNIx Browser v1.0.0 - Settings Dialog
Nyx-themed comprehensive settings interface.
"""

from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QTabWidget, QWidget, QLabel,
    QCheckBox, QSpinBox, QComboBox, QPushButton, QGroupBox, QGridLayout,
    QLineEdit, QSlider
)
from PyQt6.QtCore import Qt


class SettingsDialog(QDialog):
    """Settings dialog for OyNIx Browser."""

    def __init__(self, parent=None, config=None):
        super().__init__(parent)
        self.config = config or {}
        self.temp_config = self.config.copy()

        self.setWindowTitle("OyNIx Settings")
        self.setMinimumSize(750, 550)
        self._setup_ui()

    def _setup_ui(self):
        layout = QVBoxLayout(self)

        tabs = QTabWidget()
        tabs.addTab(self._general_tab(), "General")
        tabs.addTab(self._search_tab(), "Search")
        tabs.addTab(self._privacy_tab(), "Privacy")
        tabs.addTab(self._ai_tab(), "AI")
        tabs.addTab(self._appearance_tab(), "Appearance")
        tabs.addTab(self._advanced_tab(), "Advanced")
        layout.addWidget(tabs)

        # Buttons
        btn_layout = QHBoxLayout()
        btn_layout.addStretch()

        save_btn = QPushButton("Save")
        save_btn.setObjectName("accentButton")
        save_btn.clicked.connect(self.accept)
        btn_layout.addWidget(save_btn)

        cancel_btn = QPushButton("Cancel")
        cancel_btn.clicked.connect(self.reject)
        btn_layout.addWidget(cancel_btn)

        layout.addLayout(btn_layout)

    def _general_tab(self):
        w = QWidget()
        layout = QVBoxLayout(w)

        # Tabs
        g = QGroupBox("Tabs")
        gl = QVBoxLayout()

        tree_tabs = QCheckBox("Use tree-style tabs (sidebar)")
        tree_tabs.setChecked(self.config.get('use_tree_tabs', True))
        tree_tabs.stateChanged.connect(
            lambda s: self.temp_config.update(
                {'use_tree_tabs': s == Qt.CheckState.Checked.value}))
        gl.addWidget(tree_tabs)

        warn_close = QCheckBox("Warn when closing multiple tabs")
        warn_close.setChecked(self.config.get('warn_close_tabs', True))
        gl.addWidget(warn_close)

        g.setLayout(gl)
        layout.addWidget(g)

        # Startup
        g2 = QGroupBox("Startup")
        g2l = QVBoxLayout()

        restore = QCheckBox("Restore previous session on startup")
        restore.setChecked(self.config.get('restore_session', True))
        g2l.addWidget(restore)

        show_home = QCheckBox("Show home page on new tab")
        show_home.setChecked(self.config.get('show_home_on_new_tab', True))
        g2l.addWidget(show_home)

        g2.setLayout(g2l)
        layout.addWidget(g2)

        layout.addStretch()
        return w

    def _search_tab(self):
        w = QWidget()
        layout = QVBoxLayout(w)

        # Nyx Search
        g = QGroupBox("Nyx Search Engine")
        gl = QVBoxLayout()

        auto_index = QCheckBox("Auto-index pages as you browse")
        auto_index.setChecked(self.config.get('auto_index_pages', True))
        auto_index.stateChanged.connect(
            lambda s: self.temp_config.update(
                {'auto_index_pages': s == Qt.CheckState.Checked.value}))
        gl.addWidget(auto_index)

        desc = QLabel(
            "Nyx Search combines your browsing index, "
            "1400+ curated sites, and DuckDuckGo web results.\n"
            "Pages you visit are automatically indexed for future searches."
        )
        desc.setWordWrap(True)
        desc.setStyleSheet(
            "color: #706880; padding: 8px; "
            "background: #1a1a24; border-radius: 6px;")
        gl.addWidget(desc)

        g.setLayout(gl)
        layout.addWidget(g)

        # Default engine
        g2 = QGroupBox("Default Search Engine")
        g2l = QGridLayout()

        g2l.addWidget(QLabel("Engine:"), 0, 0)
        engine = QComboBox()
        engine.addItems(["Nyx", "DuckDuckGo", "Google", "Brave"])
        current = self.config.get('default_search_engine', 'nyx')
        engine.setCurrentText(current.title() if current != 'nyx' else 'Nyx')
        engine.currentTextChanged.connect(
            lambda t: self.temp_config.update(
                {'default_search_engine': t.lower()}))
        g2l.addWidget(engine, 0, 1)

        g2.setLayout(g2l)
        layout.addWidget(g2)

        # GitHub Sync
        g3 = QGroupBox("GitHub Database Sync")
        g3l = QGridLayout()

        g3l.addWidget(QLabel("Repository:"), 0, 0)
        self.gh_repo = QLineEdit()
        self.gh_repo.setPlaceholderText("user/repo")
        g3l.addWidget(self.gh_repo, 0, 1)

        g3l.addWidget(QLabel("Token:"), 1, 0)
        self.gh_token = QLineEdit()
        self.gh_token.setPlaceholderText("ghp_...")
        self.gh_token.setEchoMode(QLineEdit.EchoMode.Password)
        g3l.addWidget(self.gh_token, 1, 1)

        g3.setLayout(g3l)
        layout.addWidget(g3)

        layout.addStretch()
        return w

    def _privacy_tab(self):
        w = QWidget()
        layout = QVBoxLayout(w)

        g = QGroupBox("Security Prompts")
        gl = QVBoxLayout()

        security = QCheckBox("Enable security prompts for login pages")
        security.setChecked(self.config.get('show_security_prompts', True))
        security.stateChanged.connect(
            lambda s: self.temp_config.update(
                {'show_security_prompts': s == Qt.CheckState.Checked.value}))
        gl.addWidget(security)

        warn_http = QCheckBox("Warn when submitting data over HTTP")
        warn_http.setChecked(self.config.get('warn_http', True))
        gl.addWidget(warn_http)

        g.setLayout(gl)
        layout.addWidget(g)

        g2 = QGroupBox("Privacy")
        g2l = QVBoxLayout()

        dnt = QCheckBox("Send 'Do Not Track' requests")
        dnt.setChecked(self.config.get('do_not_track', True))
        g2l.addWidget(dnt)

        block_3p = QCheckBox("Block third-party cookies")
        block_3p.setChecked(self.config.get('block_third_party_cookies', True))
        g2l.addWidget(block_3p)

        clear_exit = QCheckBox("Clear history on browser exit")
        clear_exit.setChecked(self.config.get('clear_on_exit', False))
        g2l.addWidget(clear_exit)

        g2.setLayout(g2l)
        layout.addWidget(g2)

        layout.addStretch()
        return w

    def _ai_tab(self):
        w = QWidget()
        layout = QVBoxLayout(w)

        g = QGroupBox("Local AI Model")
        gl = QVBoxLayout()

        info = QLabel(
            "Model: TinyLlama 1.1B Chat (GGUF Q4)\n"
            "Auto-downloads on first launch (~700MB)\n"
            "Runs locally on CPU - no GPU or internet needed after download"
        )
        info.setWordWrap(True)
        info.setStyleSheet(
            "color: #706880; padding: 8px; "
            "background: #1a1a24; border-radius: 6px;")
        gl.addWidget(info)

        g.setLayout(gl)
        layout.addWidget(g)

        g2 = QGroupBox("AI Features")
        g2l = QVBoxLayout()

        auto_sum = QCheckBox("Auto-summarize long articles")
        auto_sum.setChecked(self.config.get('auto_summarize', False))
        g2l.addWidget(auto_sum)

        suggestions = QCheckBox("Show AI-powered search suggestions")
        suggestions.setChecked(self.config.get('ai_suggestions', True))
        g2l.addWidget(suggestions)

        g2.setLayout(g2l)
        layout.addWidget(g2)

        g3 = QGroupBox("Performance")
        g3l = QGridLayout()

        g3l.addWidget(QLabel("Max Response Length:"), 0, 0)
        max_len = QSpinBox()
        max_len.setRange(50, 1000)
        max_len.setValue(self.config.get('ai_max_length', 200))
        max_len.setSuffix(" tokens")
        g3l.addWidget(max_len, 0, 1)

        g3l.addWidget(QLabel("Temperature:"), 1, 0)
        temp = QSlider(Qt.Orientation.Horizontal)
        temp.setRange(1, 20)
        temp.setValue(int(self.config.get('ai_temperature', 0.7) * 10))
        g3l.addWidget(temp, 1, 1)

        g3.setLayout(g3l)
        layout.addWidget(g3)

        layout.addStretch()
        return w

    def _appearance_tab(self):
        w = QWidget()
        layout = QVBoxLayout(w)

        g = QGroupBox("Theme")
        gl = QGridLayout()

        gl.addWidget(QLabel("Theme:"), 0, 0)
        theme = QComboBox()
        theme.addItems([
            "Nyx (Purple/Black)",
            "Nyx Midnight",
            "Nyx Violet",
            "Nyx Amethyst",
        ])
        current = self.config.get('theme', 'nyx')
        theme_map = {
            'nyx': 0, 'nyx_midnight': 1,
            'nyx_violet': 2, 'nyx_amethyst': 3,
        }
        theme.setCurrentIndex(theme_map.get(current, 0))
        reverse_map = {
            0: 'nyx', 1: 'nyx_midnight',
            2: 'nyx_violet', 3: 'nyx_amethyst',
        }
        theme.currentIndexChanged.connect(
            lambda i: self.temp_config.update(
                {'theme': reverse_map.get(i, 'nyx')}))
        gl.addWidget(theme, 0, 1)

        g.setLayout(gl)
        layout.addWidget(g)

        g2 = QGroupBox("Font")
        g2l = QGridLayout()

        g2l.addWidget(QLabel("Font Family:"), 0, 0)
        font = QComboBox()
        font.addItems(["Segoe UI", "Ubuntu", "Cantarell", "Arial"])
        g2l.addWidget(font, 0, 1)

        g2l.addWidget(QLabel("Font Size:"), 1, 0)
        size = QSpinBox()
        size.setRange(8, 24)
        size.setValue(self.config.get('font_size', 13))
        size.setSuffix(" pt")
        g2l.addWidget(size, 1, 1)

        g2.setLayout(g2l)
        layout.addWidget(g2)

        g3 = QGroupBox("Display")
        g3l = QVBoxLayout()

        smooth = QCheckBox("Enable smooth scrolling")
        smooth.setChecked(self.config.get('smooth_scrolling', True))
        g3l.addWidget(smooth)

        g3.setLayout(g3l)
        layout.addWidget(g3)

        layout.addStretch()
        return w

    def _advanced_tab(self):
        w = QWidget()
        layout = QVBoxLayout(w)

        g = QGroupBox("Performance")
        gl = QVBoxLayout()

        hw = QCheckBox("Use hardware acceleration")
        hw.setChecked(self.config.get('hardware_acceleration', True))
        gl.addWidget(hw)

        lazy = QCheckBox("Lazy load images")
        lazy.setChecked(self.config.get('lazy_load_images', True))
        gl.addWidget(lazy)

        g.setLayout(gl)
        layout.addWidget(g)

        g2 = QGroupBox("Network")
        g2l = QGridLayout()

        g2l.addWidget(QLabel("DNS:"), 0, 0)
        dns = QComboBox()
        dns.addItems(["System DNS", "Cloudflare (1.1.1.1)",
                       "Google (8.8.8.8)", "Custom..."])
        g2l.addWidget(dns, 0, 1)

        g2.setLayout(g2l)
        layout.addWidget(g2)

        g3 = QGroupBox("Reset")
        g3l = QVBoxLayout()

        reset_btn = QPushButton("Reset All Settings to Default")
        reset_btn.clicked.connect(self._reset_settings)
        g3l.addWidget(reset_btn)

        g3.setLayout(g3l)
        layout.addWidget(g3)

        layout.addStretch()
        return w

    def _reset_settings(self):
        from PyQt6.QtWidgets import QMessageBox
        reply = QMessageBox.question(
            self, "Reset Settings",
            "Reset all settings to default?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )
        if reply == QMessageBox.StandardButton.Yes:
            self.temp_config = {
                'use_tree_tabs': True,
                'default_search_engine': 'nyx',
                'show_security_prompts': True,
                'auto_index_pages': True,
                'theme': 'nyx',
            }

    def get_config(self):
        return self.temp_config

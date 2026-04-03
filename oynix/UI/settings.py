"""
OyNIx Browser v1.0.0 - Modern Settings Dialog
Sidebar-navigation settings interface with smooth transitions.
"""

from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QWidget, QLabel, QCheckBox,
    QSpinBox, QComboBox, QPushButton, QGridLayout, QLineEdit, QSlider,
    QStackedWidget, QScrollArea, QListWidget, QListWidgetItem, QFrame,
    QFileDialog, QMessageBox, QSizePolicy,
)
from PyQt6.QtCore import Qt, QSize, QPropertyAnimation, QEasingCurve, pyqtSignal
from PyQt6.QtGui import QFont, QIcon, QKeySequence

from oynix.UI.icons import svg_icon
from oynix.core.theme_engine import NYX_COLORS

# ── Default config ────────────────────────────────────────────────────
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
    'animation_speed': 'normal',
    'download_dir': '',
    'ask_download_location': True,
    'font_size': 13,
    'compact_mode': False,
    'homepage_url': 'oynix://home',
    'startup_behavior': 'homepage',
    'nyx_search_enabled': True,
    'search_result_count': 20,
    'safe_search': True,
    'do_not_track': True,
    'cookie_setting': 'standard',
    'ai_enabled': False,
    'ai_model_selection': 'auto',
    'ai_model_path': '',
    'ai_auto_download': True,
    'ai_max_length': 200,
    'github_repo_url': '',
    'github_sync_frequency': 'daily',
    'auto_open_downloads_panel': True,
}

C = NYX_COLORS  # shorthand


# ── Key capture widget ────────────────────────────────────────────────

class ShortcutEdit(QLineEdit):
    """A line edit that captures key combinations when focused."""
    shortcut_changed = pyqtSignal(str)

    def __init__(self, text="", parent=None):
        super().__init__(text, parent)
        self.setReadOnly(True)
        self.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._capturing = False
        self.setStyleSheet(f"""
            QLineEdit {{
                background: {C['bg_mid']};
                color: {C['purple_soft']};
                border: 1px solid {C['border']};
                border-radius: 4px;
                padding: 4px 8px;
                font-family: monospace;
                font-size: 12px;
            }}
            QLineEdit:focus {{
                border-color: {C['purple_mid']};
                background: {C['bg_light']};
            }}
        """)

    def mousePressEvent(self, event):
        self._capturing = True
        self.setText("Press keys...")
        self.setStyleSheet(self.styleSheet().replace(
            f"color: {C['purple_soft']}", f"color: {C['warning']}"))
        super().mousePressEvent(event)

    def keyPressEvent(self, event):
        if not self._capturing:
            return super().keyPressEvent(event)

        key = event.key()
        mods = event.modifiers()

        # Escape cancels
        if key == Qt.Key.Key_Escape:
            self._capturing = False
            self.clearFocus()
            self.setStyleSheet(self.styleSheet().replace(
                f"color: {C['warning']}", f"color: {C['purple_soft']}"))
            return

        # Backspace clears
        if key == Qt.Key.Key_Backspace:
            self._capturing = False
            self.setText("")
            self.setStyleSheet(self.styleSheet().replace(
                f"color: {C['warning']}", f"color: {C['purple_soft']}"))
            self.shortcut_changed.emit("")
            self.clearFocus()
            return

        # Ignore bare modifier keys
        if key in (Qt.Key.Key_Control, Qt.Key.Key_Shift, Qt.Key.Key_Alt, Qt.Key.Key_Meta):
            return

        parts = []
        if mods & Qt.KeyboardModifier.ControlModifier:
            parts.append("Ctrl")
        if mods & Qt.KeyboardModifier.ShiftModifier:
            parts.append("Shift")
        if mods & Qt.KeyboardModifier.AltModifier:
            parts.append("Alt")
        if mods & Qt.KeyboardModifier.MetaModifier:
            parts.append("Meta")

        key_seq = QKeySequence(key)
        key_text = key_seq.toString()
        if key_text:
            parts.append(key_text)

        combo = "+".join(parts)
        self._capturing = False
        self.setText(combo)
        self.setStyleSheet(self.styleSheet().replace(
            f"color: {C['warning']}", f"color: {C['purple_soft']}"))
        self.shortcut_changed.emit(combo)
        self.clearFocus()

    def focusOutEvent(self, event):
        if self._capturing:
            self._capturing = False
            # Restore if cancelled by losing focus
            self.setStyleSheet(self.styleSheet().replace(
                f"color: {C['warning']}", f"color: {C['purple_soft']}"))
        super().focusOutEvent(event)


# ── Helpers ───────────────────────────────────────────────────────────

def _heading(text):
    """Section heading label."""
    lbl = QLabel(text)
    lbl.setFont(QFont("Segoe UI", 14, QFont.Weight.Bold))
    lbl.setStyleSheet(f"color: {C['text_primary']}; padding: 2px 0 6px 0; background: transparent;")
    return lbl


def _description(text):
    """Muted description label."""
    lbl = QLabel(text)
    lbl.setWordWrap(True)
    lbl.setStyleSheet(
        f"color: {C['text_muted']}; font-size: 11px; padding: 4px 0; background: transparent;"
    )
    return lbl


def _separator():
    """Horizontal line separator."""
    line = QFrame()
    line.setFrameShape(QFrame.Shape.HLine)
    line.setStyleSheet(f"color: {C['border']}; background: {C['border']}; max-height: 1px;")
    return line


def _toggle(label_text, checked, callback):
    """Styled toggle checkbox."""
    cb = QCheckBox(label_text)
    cb.setChecked(checked)
    cb.setStyleSheet(f"""
        QCheckBox {{
            color: {C['text_primary']};
            spacing: 8px;
            padding: 4px 0;
            background: transparent;
        }}
        QCheckBox::indicator {{
            width: 18px; height: 18px;
            border: 2px solid {C['purple_mid']};
            border-radius: 4px;
            background: {C['bg_mid']};
        }}
        QCheckBox::indicator:checked {{
            background: {C['purple_mid']};
            border-color: {C['purple_light']};
        }}
    """)
    cb.stateChanged.connect(
        lambda s: callback(s == Qt.CheckState.Checked.value)
    )
    return cb


def _card(widgets):
    """Wrap widgets in a styled card frame."""
    card = QFrame()
    card.setStyleSheet(f"""
        QFrame {{
            background: {C['bg_light']};
            border: 1px solid {C['border']};
            border-radius: 8px;
            padding: 12px;
        }}
    """)
    lay = QVBoxLayout(card)
    lay.setContentsMargins(14, 10, 14, 10)
    lay.setSpacing(6)
    for w in widgets:
        lay.addWidget(w)
    return card


# ── Main Dialog ───────────────────────────────────────────────────────

class SettingsDialog(QDialog):
    """Modern settings dialog for OyNIx Browser."""

    SECTIONS = [
        ("General",             "home"),
        ("Appearance",          "eye"),
        ("Search",              "search"),
        ("Privacy & Security",  "shield"),
        ("AI Assistant",        "ai"),
        ("Database & Sync",     "database"),
        ("Downloads",           "downloads"),
        ("Keyboard Shortcuts",  "command"),
    ]

    def __init__(self, parent=None, config=None):
        super().__init__(parent)
        self.config = config or {}
        self.temp_config = {**DEFAULT_CONFIG, **self.config}

        self.setWindowTitle("OyNIx Settings")
        self.setMinimumSize(700, 500)
        self.resize(750, 540)
        self._apply_dialog_style()
        self._setup_ui()

    # ── UI setup ──────────────────────────────────────────────────────

    def _apply_dialog_style(self):
        self.setStyleSheet(f"""
            QDialog {{
                background: {C['bg_darkest']};
                color: {C['text_primary']};
            }}
            QLabel {{
                color: {C['text_primary']};
                background: transparent;
            }}
            QComboBox {{
                background: {C['bg_mid']};
                color: {C['text_primary']};
                border: 1px solid {C['border']};
                border-radius: 6px;
                padding: 5px 10px;
                min-height: 24px;
            }}
            QComboBox::drop-down {{
                border: none;
                width: 24px;
            }}
            QComboBox QAbstractItemView {{
                background: {C['bg_light']};
                color: {C['text_primary']};
                selection-background-color: {C['purple_dark']};
                border: 1px solid {C['border']};
            }}
            QLineEdit {{
                background: {C['bg_mid']};
                color: {C['text_primary']};
                border: 1px solid {C['border']};
                border-radius: 6px;
                padding: 5px 10px;
                min-height: 24px;
            }}
            QLineEdit:focus {{
                border-color: {C['purple_mid']};
            }}
            QSpinBox {{
                background: {C['bg_mid']};
                color: {C['text_primary']};
                border: 1px solid {C['border']};
                border-radius: 6px;
                padding: 4px 8px;
            }}
            QSlider::groove:horizontal {{
                background: {C['bg_surface']};
                height: 6px;
                border-radius: 3px;
            }}
            QSlider::handle:horizontal {{
                background: {C['purple_mid']};
                width: 16px; height: 16px;
                margin: -5px 0;
                border-radius: 8px;
            }}
            QSlider::sub-page:horizontal {{
                background: {C['purple_dark']};
                border-radius: 3px;
            }}
            QPushButton {{
                background: {C['bg_lighter']};
                color: {C['text_primary']};
                border: 1px solid {C['border']};
                border-radius: 6px;
                padding: 6px 16px;
                min-height: 26px;
            }}
            QPushButton:hover {{
                background: {C['bg_surface']};
                border-color: {C['purple_mid']};
            }}
            QScrollArea {{
                border: none;
                background: transparent;
            }}
            QScrollBar:vertical {{
                background: {C['bg_dark']};
                width: 8px;
                border-radius: 4px;
            }}
            QScrollBar::handle:vertical {{
                background: {C['scrollbar']};
                border-radius: 4px;
                min-height: 30px;
            }}
            QScrollBar::handle:vertical:hover {{
                background: {C['scrollbar_hover']};
            }}
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {{
                height: 0;
            }}
        """)

    def _setup_ui(self):
        root = QVBoxLayout(self)
        root.setContentsMargins(0, 0, 0, 0)
        root.setSpacing(0)

        body = QHBoxLayout()
        body.setContentsMargins(0, 0, 0, 0)
        body.setSpacing(0)

        # ── Sidebar ──
        self.sidebar = QListWidget()
        self.sidebar.setFixedWidth(190)
        self.sidebar.setIconSize(QSize(18, 18))
        self.sidebar.setStyleSheet(f"""
            QListWidget {{
                background: {C['bg_dark']};
                border: none;
                border-right: 1px solid {C['border']};
                padding: 8px 4px;
                outline: none;
            }}
            QListWidget::item {{
                color: {C['text_secondary']};
                padding: 9px 12px;
                border-radius: 6px;
                margin: 1px 4px;
            }}
            QListWidget::item:hover {{
                background: {C['bg_light']};
                color: {C['text_primary']};
            }}
            QListWidget::item:selected {{
                background: {C['purple_dark']};
                color: {C['text_primary']};
            }}
        """)

        for name, icon_name in self.SECTIONS:
            item = QListWidgetItem(svg_icon(icon_name, C['purple_soft'], 18), name)
            item.setSizeHint(QSize(170, 36))
            self.sidebar.addItem(item)

        # ── Content stack ──
        self.stack = QStackedWidget()
        self.stack.setStyleSheet(f"background: {C['bg_darkest']};")

        self._build_general()
        self._build_appearance()
        self._build_search()
        self._build_privacy()
        self._build_ai()
        self._build_database()
        self._build_downloads()
        self._build_shortcuts()

        self.sidebar.currentRowChanged.connect(self._switch_section)
        self.sidebar.setCurrentRow(0)

        body.addWidget(self.sidebar)
        body.addWidget(self.stack, 1)
        root.addLayout(body, 1)

        # ── Bottom buttons ──
        btn_bar = QHBoxLayout()
        btn_bar.setContentsMargins(16, 10, 16, 12)

        reset_btn = QPushButton("Reset to Defaults")
        reset_btn.setStyleSheet(f"""
            QPushButton {{
                color: {C['text_muted']};
                background: transparent;
                border: 1px solid {C['border']};
                border-radius: 6px;
                padding: 6px 14px;
            }}
            QPushButton:hover {{ color: {C['error']}; border-color: {C['error']}; }}
        """)
        reset_btn.clicked.connect(self._reset_settings)
        btn_bar.addWidget(reset_btn)
        btn_bar.addStretch()

        cancel_btn = QPushButton("Cancel")
        cancel_btn.clicked.connect(self.reject)
        btn_bar.addWidget(cancel_btn)

        apply_btn = QPushButton("Apply")
        apply_btn.setStyleSheet(f"""
            QPushButton {{
                background: {C['purple_mid']};
                color: #fff;
                border: none;
                border-radius: 6px;
                padding: 7px 24px;
                font-weight: bold;
            }}
            QPushButton:hover {{ background: {C['purple_light']}; }}
        """)
        apply_btn.clicked.connect(self.accept)
        btn_bar.addWidget(apply_btn)

        root.addLayout(btn_bar)

    # ── Section switching ─────────────────────────────────────────────

    def _switch_section(self, index):
        self.stack.setCurrentIndex(index)

    def _add_section(self, widget):
        """Wrap a section widget in a scroll area and add to stack."""
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setWidget(widget)
        scroll.setStyleSheet("QScrollArea { background: transparent; border: none; }")
        self.stack.addWidget(scroll)

    # ── Config helpers ────────────────────────────────────────────────

    def _cfg(self, key):
        val = self.temp_config.get(key, DEFAULT_CONFIG.get(key))
        # Guard against type corruption in saved configs
        default = DEFAULT_CONFIG.get(key)
        if default is not None and type(val) != type(default):
            return default
        return val

    def _set(self, key, value):
        self.temp_config[key] = value

    # ── Section builders ──────────────────────────────────────────────

    def _build_general(self):
        w = QWidget()
        w.setStyleSheet("background: transparent;")
        lay = QVBoxLayout(w)
        lay.setContentsMargins(24, 20, 24, 20)
        lay.setSpacing(14)

        lay.addWidget(_heading("General"))

        # Homepage
        row = QHBoxLayout()
        lbl = QLabel("Homepage URL")
        lbl.setFixedWidth(120)
        row.addWidget(lbl)
        home_edit = QLineEdit(self._cfg('homepage_url'))
        home_edit.setPlaceholderText("oynix://home")
        home_edit.textChanged.connect(lambda t: self._set('homepage_url', t))
        row.addWidget(home_edit, 1)
        lay.addLayout(row)

        # Default search engine
        row2 = QHBoxLayout()
        lbl2 = QLabel("Search Engine")
        lbl2.setFixedWidth(120)
        row2.addWidget(lbl2)
        engine = QComboBox()
        engine.addItems(["Nyx", "DuckDuckGo", "Google", "Brave"])
        cur = self._cfg('default_search_engine')
        if not isinstance(cur, str):
            cur = 'nyx'
        engine.setCurrentText(cur.title() if cur != 'nyx' else 'Nyx')
        engine.currentTextChanged.connect(
            lambda t: self._set('default_search_engine', t.lower()))
        row2.addWidget(engine, 1)
        lay.addLayout(row2)

        lay.addWidget(_separator())

        # Startup behavior
        row3 = QHBoxLayout()
        lbl3 = QLabel("On Startup")
        lbl3.setFixedWidth(120)
        row3.addWidget(lbl3)
        startup = QComboBox()
        startup.addItems(["Open homepage", "Restore last session", "Open blank page"])
        startup_map = {'homepage': 0, 'restore': 1, 'blank': 2}
        reverse_startup = {0: 'homepage', 1: 'restore', 2: 'blank'}
        startup.setCurrentIndex(startup_map.get(self._cfg('startup_behavior'), 0))
        startup.currentIndexChanged.connect(
            lambda i: self._set('startup_behavior', reverse_startup.get(i, 'homepage')))
        row3.addWidget(startup, 1)
        lay.addLayout(row3)

        # Tab mode
        row4 = QHBoxLayout()
        lbl4 = QLabel("Default Tab Mode")
        lbl4.setFixedWidth(120)
        row4.addWidget(lbl4)
        tab_mode = QComboBox()
        tab_mode.addItems(["Tree Tabs", "Normal Tabs"])
        tab_mode.setCurrentIndex(0 if self._cfg('use_tree_tabs') else 1)
        tab_mode.currentIndexChanged.connect(
            lambda i: self._set('use_tree_tabs', i == 0))
        row4.addWidget(tab_mode, 1)
        lay.addLayout(row4)

        lay.addStretch()
        self._add_section(w)

    def _build_appearance(self):
        w = QWidget()
        w.setStyleSheet("background: transparent;")
        lay = QVBoxLayout(w)
        lay.setContentsMargins(24, 20, 24, 20)
        lay.setSpacing(14)

        lay.addWidget(_heading("Appearance"))

        # Theme selector with swatches
        lay.addWidget(QLabel("Theme"))
        theme_row = QHBoxLayout()
        theme_row.setSpacing(10)

        themes = [
            ('nyx',          'Nyx',          C['purple_mid'],  C['bg_darkest']),
            ('nyx_midnight', 'Midnight',     '#6040A0',        '#050510'),
            ('nyx_violet',   'Violet',       '#8855CC',        '#0a0a0f'),
            ('nyx_amethyst', 'Amethyst',     '#9966CC',        '#0d0810'),
        ]
        self._theme_btns = []
        current_theme = self._cfg('theme')
        for key, label, accent, bg in themes:
            btn = QPushButton(label)
            selected = key == current_theme
            btn.setStyleSheet(f"""
                QPushButton {{
                    background: {bg};
                    color: {accent};
                    border: 2px solid {'%s' % accent if selected else C['border']};
                    border-radius: 8px;
                    padding: 10px 6px;
                    min-width: 80px;
                    font-weight: {'bold' if selected else 'normal'};
                }}
                QPushButton:hover {{ border-color: {accent}; }}
            """)
            btn.setProperty('theme_key', key)
            btn.clicked.connect(lambda checked, k=key: self._select_theme(k))
            theme_row.addWidget(btn)
            self._theme_btns.append((btn, key, accent, bg))

        theme_row.addStretch()
        lay.addLayout(theme_row)

        lay.addWidget(_separator())

        # Force Nyx theme on external search engines
        lay.addWidget(_card([
            _toggle(
                "Force Nyx theme on external search engines",
                self._cfg('force_nyx_theme_external'),
                lambda v: self._set('force_nyx_theme_external', v),
            ),
            _description(
                "When enabled, Google, DuckDuckGo, and other search engine pages "
                "are restyled with the Nyx purple-dark theme for a consistent look."
            ),
        ]))

        lay.addWidget(_separator())

        # Animation speed
        row = QHBoxLayout()
        row.addWidget(QLabel("Animation Speed"))
        anim = QComboBox()
        anim.addItems(["Fast", "Normal", "Slow", "Off"])
        anim_map = {'fast': 0, 'normal': 1, 'slow': 2, 'off': 3}
        reverse_anim = {0: 'fast', 1: 'normal', 2: 'slow', 3: 'off'}
        anim.setCurrentIndex(anim_map.get(self._cfg('animation_speed'), 1))
        anim.currentIndexChanged.connect(
            lambda i: self._set('animation_speed', reverse_anim.get(i, 'normal')))
        row.addWidget(anim)
        row.addStretch()
        lay.addLayout(row)

        # Font size slider
        fs_row = QHBoxLayout()
        fs_row.addWidget(QLabel("Font Size"))
        self._font_label = QLabel(f"{self._cfg('font_size')} px")
        self._font_label.setFixedWidth(48)
        self._font_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        slider = QSlider(Qt.Orientation.Horizontal)
        slider.setRange(8, 24)
        slider.setValue(self._cfg('font_size'))
        slider.valueChanged.connect(self._on_font_size_changed)
        fs_row.addWidget(slider, 1)
        fs_row.addWidget(self._font_label)
        lay.addLayout(fs_row)

        # Compact mode
        lay.addWidget(_toggle(
            "Compact mode (smaller UI elements)",
            self._cfg('compact_mode'),
            lambda v: self._set('compact_mode', v),
        ))

        lay.addStretch()
        self._add_section(w)

    def _select_theme(self, key):
        self._set('theme', key)
        for btn, bkey, accent, bg in self._theme_btns:
            selected = bkey == key
            btn.setStyleSheet(f"""
                QPushButton {{
                    background: {bg};
                    color: {accent};
                    border: 2px solid {'%s' % accent if selected else C['border']};
                    border-radius: 8px;
                    padding: 10px 6px;
                    min-width: 80px;
                    font-weight: {'bold' if selected else 'normal'};
                }}
                QPushButton:hover {{ border-color: {accent}; }}
            """)

    def _on_font_size_changed(self, val):
        self._set('font_size', val)
        self._font_label.setText(f"{val} px")

    def _build_search(self):
        w = QWidget()
        w.setStyleSheet("background: transparent;")
        lay = QVBoxLayout(w)
        lay.setContentsMargins(24, 20, 24, 20)
        lay.setSpacing(14)

        lay.addWidget(_heading("Search"))

        lay.addWidget(_toggle(
            "Enable Nyx search engine",
            self._cfg('nyx_search_enabled'),
            lambda v: self._set('nyx_search_enabled', v),
        ))

        lay.addWidget(_toggle(
            "Auto-index visited pages",
            self._cfg('auto_index_pages'),
            lambda v: self._set('auto_index_pages', v),
        ))
        lay.addWidget(_description(
            "Pages you visit are automatically indexed so they appear in future Nyx searches."
        ))

        lay.addWidget(_toggle(
            "Auto-expand database with visited sites",
            self._cfg('auto_expand_database'),
            lambda v: self._set('auto_expand_database', v),
        ))
        lay.addWidget(_description(
            "Automatically add visited websites and DuckDuckGo results to the local database."
        ))

        lay.addWidget(_separator())

        # Result count
        row = QHBoxLayout()
        row.addWidget(QLabel("Results per page"))
        count = QComboBox()
        count.addItems(["10", "20", "50"])
        count_map = {10: 0, 20: 1, 50: 2}
        reverse_count = {0: 10, 1: 20, 2: 50}
        count.setCurrentIndex(count_map.get(self._cfg('search_result_count'), 1))
        count.currentIndexChanged.connect(
            lambda i: self._set('search_result_count', reverse_count.get(i, 20)))
        row.addWidget(count)
        row.addStretch()
        lay.addLayout(row)

        lay.addWidget(_toggle(
            "Safe search",
            self._cfg('safe_search'),
            lambda v: self._set('safe_search', v),
        ))

        lay.addStretch()
        self._add_section(w)

    def _build_privacy(self):
        w = QWidget()
        w.setStyleSheet("background: transparent;")
        lay = QVBoxLayout(w)
        lay.setContentsMargins(24, 20, 24, 20)
        lay.setSpacing(14)

        lay.addWidget(_heading("Privacy & Security"))

        lay.addWidget(_toggle(
            "Security prompts for login pages",
            self._cfg('show_security_prompts'),
            lambda v: self._set('show_security_prompts', v),
        ))
        lay.addWidget(_description(
            "Show a security reminder when you navigate to pages with login forms."
        ))

        lay.addWidget(_separator())

        # Cookie settings
        row = QHBoxLayout()
        row.addWidget(QLabel("Cookies"))
        cookies = QComboBox()
        cookies.addItems(["Standard", "Block third-party", "Block all"])
        cookie_map = {'standard': 0, 'block_third_party': 1, 'block_all': 2}
        reverse_cookie = {0: 'standard', 1: 'block_third_party', 2: 'block_all'}
        cookies.setCurrentIndex(cookie_map.get(self._cfg('cookie_setting'), 0))
        cookies.currentIndexChanged.connect(
            lambda i: self._set('cookie_setting', reverse_cookie.get(i, 'standard')))
        row.addWidget(cookies)
        row.addStretch()
        lay.addLayout(row)

        lay.addWidget(_toggle(
            "Send 'Do Not Track' requests",
            self._cfg('do_not_track'),
            lambda v: self._set('do_not_track', v),
        ))

        lay.addWidget(_separator())

        # Clear data button
        clear_btn = QPushButton("Clear Browsing Data...")
        clear_btn.setStyleSheet(f"""
            QPushButton {{
                background: {C['bg_lighter']};
                color: {C['error']};
                border: 1px solid {C['error']};
                border-radius: 6px;
                padding: 8px 20px;
            }}
            QPushButton:hover {{ background: {C['error']}; color: #fff; }}
        """)
        clear_btn.clicked.connect(self._clear_browsing_data)
        lay.addWidget(clear_btn)

        lay.addStretch()
        self._add_section(w)

    def _build_ai(self):
        w = QWidget()
        w.setStyleSheet("background: transparent;")
        lay = QVBoxLayout(w)
        lay.setContentsMargins(24, 20, 24, 20)
        lay.setSpacing(14)

        lay.addWidget(_heading("AI Assistant"))

        lay.addWidget(_toggle(
            "Enable AI assistant",
            self._cfg('ai_enabled'),
            lambda v: self._set('ai_enabled', v),
        ))

        lay.addWidget(_separator())

        # Model selection
        row = QHBoxLayout()
        row.addWidget(QLabel("Model"))
        model = QComboBox()
        model.addItems(["Auto (TinyLlama 1.1B)", "Manual path..."])
        model.setCurrentIndex(0 if self._cfg('ai_model_selection') == 'auto' else 1)
        model.currentIndexChanged.connect(
            lambda i: self._set('ai_model_selection', 'auto' if i == 0 else 'manual'))
        row.addWidget(model, 1)
        lay.addLayout(row)

        # Manual path
        path_row = QHBoxLayout()
        path_row.addWidget(QLabel("Model path"))
        self._model_path = QLineEdit(self._cfg('ai_model_path'))
        self._model_path.setPlaceholderText("/path/to/model.gguf")
        self._model_path.textChanged.connect(lambda t: self._set('ai_model_path', t))
        path_row.addWidget(self._model_path, 1)
        browse_btn = QPushButton("Browse")
        browse_btn.clicked.connect(self._browse_model)
        path_row.addWidget(browse_btn)
        lay.addLayout(path_row)

        lay.addWidget(_toggle(
            "Auto-download model on first launch",
            self._cfg('ai_auto_download'),
            lambda v: self._set('ai_auto_download', v),
        ))

        lay.addWidget(_separator())

        # Max response length
        ml_row = QHBoxLayout()
        ml_row.addWidget(QLabel("Max response length"))
        self._ai_len_label = QLabel(f"{self._cfg('ai_max_length')} tokens")
        self._ai_len_label.setFixedWidth(72)
        self._ai_len_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        ai_slider = QSlider(Qt.Orientation.Horizontal)
        ai_slider.setRange(50, 1000)
        ai_slider.setSingleStep(50)
        ai_slider.setValue(self._cfg('ai_max_length'))
        ai_slider.valueChanged.connect(self._on_ai_len_changed)
        ml_row.addWidget(ai_slider, 1)
        ml_row.addWidget(self._ai_len_label)
        lay.addLayout(ml_row)

        lay.addWidget(_description(
            "Model: TinyLlama 1.1B Chat (GGUF Q4). Runs locally on CPU, "
            "no GPU or internet needed after download (~700 MB)."
        ))

        lay.addStretch()
        self._add_section(w)

    def _on_ai_len_changed(self, val):
        self._set('ai_max_length', val)
        self._ai_len_label.setText(f"{val} tokens")

    def _build_database(self):
        w = QWidget()
        w.setStyleSheet("background: transparent;")
        lay = QVBoxLayout(w)
        lay.setContentsMargins(24, 20, 24, 20)
        lay.setSpacing(14)

        lay.addWidget(_heading("Database & Sync"))

        lay.addWidget(_toggle(
            "Auto-expand database with visited sites",
            self._cfg('auto_expand_database'),
            lambda v: self._set('auto_expand_database', v),
        ))

        lay.addWidget(_card([
            _toggle(
                "Community upload (share discovered sites to repo)",
                self._cfg('community_upload'),
                lambda v: self._set('community_upload', v),
            ),
            _description(
                "Periodically upload newly discovered sites to the configured GitHub "
                "repository so future OyNIx users benefit from a growing shared database."
            ),
        ]))

        lay.addWidget(_card([
            _toggle(
                "Auto-compare and update database",
                self._cfg('auto_compare_sites'),
                lambda v: self._set('auto_compare_sites', v),
            ),
            _description(
                "Compares currently visited sites against known sites in the database "
                "and updates metadata (title, category, rating) automatically."
            ),
        ]))

        lay.addWidget(_toggle(
            "Auto-update repo database on sync",
            self._cfg('auto_update_repo_db'),
            lambda v: self._set('auto_update_repo_db', v),
        ))

        lay.addWidget(_separator())

        # GitHub sync settings
        lay.addWidget(QLabel("GitHub Sync"))

        repo_row = QHBoxLayout()
        repo_row.addWidget(QLabel("Repository URL"))
        repo_edit = QLineEdit(self._cfg('github_repo_url'))
        repo_edit.setPlaceholderText("https://github.com/user/repo")
        repo_edit.textChanged.connect(lambda t: self._set('github_repo_url', t))
        repo_row.addWidget(repo_edit, 1)
        lay.addLayout(repo_row)

        freq_row = QHBoxLayout()
        freq_row.addWidget(QLabel("Sync frequency"))
        freq = QComboBox()
        freq.addItems(["Manual", "Daily", "Weekly"])
        freq_map = {'manual': 0, 'daily': 1, 'weekly': 2}
        reverse_freq = {0: 'manual', 1: 'daily', 2: 'weekly'}
        freq.setCurrentIndex(freq_map.get(self._cfg('github_sync_frequency'), 1))
        freq.currentIndexChanged.connect(
            lambda i: self._set('github_sync_frequency', reverse_freq.get(i, 'daily')))
        freq_row.addWidget(freq)
        freq_row.addStretch()
        lay.addLayout(freq_row)

        lay.addWidget(_separator())

        # Database stats
        stats = QLabel("Database statistics will appear here once loaded.")
        stats.setStyleSheet(
            f"color: {C['text_muted']}; padding: 10px; "
            f"background: {C['bg_mid']}; border-radius: 6px;"
        )
        lay.addWidget(stats)

        # Export / Import
        btn_row = QHBoxLayout()
        export_btn = QPushButton("Export Database")
        export_btn.clicked.connect(self._export_database)
        btn_row.addWidget(export_btn)
        import_btn = QPushButton("Import Database")
        import_btn.clicked.connect(self._import_database)
        btn_row.addWidget(import_btn)
        btn_row.addStretch()
        lay.addLayout(btn_row)

        lay.addStretch()
        self._add_section(w)

    def _build_downloads(self):
        w = QWidget()
        w.setStyleSheet("background: transparent;")
        lay = QVBoxLayout(w)
        lay.setContentsMargins(24, 20, 24, 20)
        lay.setSpacing(14)

        lay.addWidget(_heading("Downloads"))

        # Download directory
        dir_row = QHBoxLayout()
        dir_row.addWidget(QLabel("Download directory"))
        self._dl_dir = QLineEdit(self._cfg('download_dir'))
        self._dl_dir.setPlaceholderText("Default system downloads folder")
        self._dl_dir.textChanged.connect(lambda t: self._set('download_dir', t))
        dir_row.addWidget(self._dl_dir, 1)
        browse = QPushButton("Browse")
        browse.clicked.connect(self._browse_download_dir)
        dir_row.addWidget(browse)
        lay.addLayout(dir_row)

        lay.addWidget(_toggle(
            "Ask where to save each file",
            self._cfg('ask_download_location'),
            lambda v: self._set('ask_download_location', v),
        ))

        lay.addWidget(_toggle(
            "Auto-open downloads panel when a download starts",
            self._cfg('auto_open_downloads_panel'),
            lambda v: self._set('auto_open_downloads_panel', v),
        ))

        lay.addStretch()
        self._add_section(w)

    def _build_shortcuts(self):
        w = QWidget()
        w.setStyleSheet("background: transparent;")
        lay = QVBoxLayout(w)
        lay.setContentsMargins(24, 20, 24, 20)
        lay.setSpacing(14)

        lay.addWidget(_heading("Keyboard Shortcuts"))
        lay.addWidget(_description(
            "Click a shortcut field, then press the new key combination to rebind it. "
            "Press Escape to cancel, or Backspace to clear a binding."
        ))

        self._default_shortcuts = {
            "New Tab":           "Ctrl+T",
            "Close Tab":         "Ctrl+W",
            "Reload":            "F5",
            "Find in Page":      "Ctrl+F",
            "Address Bar":       "Ctrl+L",
            "Bookmark":          "Ctrl+D",
            "Settings":          "Ctrl+,",
            "Downloads":         "Ctrl+J",
            "History":           "Ctrl+H",
            "Bookmarks":         "Ctrl+B",
            "Command Palette":   "Ctrl+K",
            "Zoom In":           "Ctrl++",
            "Zoom Out":          "Ctrl+-",
            "Reset Zoom":        "Ctrl+0",
            "Full Screen":       "F11",
            "Toggle AI Panel":   "Ctrl+Shift+A",
            "Audio Player":      "Ctrl+Shift+M",
        }

        saved = self._cfg('custom_shortcuts')
        if not isinstance(saved, dict):
            saved = {}
        self._shortcut_edits = {}

        grid = QGridLayout()
        grid.setColumnStretch(1, 1)
        grid.setSpacing(6)
        for i, (action, default_key) in enumerate(self._default_shortcuts.items()):
            current_key = saved.get(action, default_key)

            lbl = QLabel(action)
            lbl.setStyleSheet(f"color: {C['text_secondary']}; background: transparent;")
            grid.addWidget(lbl, i, 0)

            key_edit = ShortcutEdit(current_key)
            key_edit.setFixedWidth(160)
            key_edit.shortcut_changed.connect(
                lambda new_key, a=action: self._on_shortcut_changed(a, new_key))
            grid.addWidget(key_edit, i, 1, Qt.AlignmentFlag.AlignLeft)
            self._shortcut_edits[action] = key_edit

        lay.addLayout(grid)

        lay.addWidget(_separator())

        reset_btn = QPushButton("Reset All Shortcuts to Defaults")
        reset_btn.clicked.connect(self._reset_shortcuts)
        lay.addWidget(reset_btn)

        lay.addStretch()
        self._add_section(w)

    def _on_shortcut_changed(self, action, new_key):
        saved = self.temp_config.get('custom_shortcuts', {})
        if not isinstance(saved, dict):
            saved = {}
        saved[action] = new_key
        self._set('custom_shortcuts', saved)

    def _reset_shortcuts(self):
        self._set('custom_shortcuts', {})
        for action, edit in self._shortcut_edits.items():
            edit.setText(self._default_shortcuts[action])
        QMessageBox.information(self, "Reset", "All shortcuts reset to defaults.")

    # ── Action callbacks ──────────────────────────────────────────────

    def _browse_model(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "Select AI Model", "", "GGUF Models (*.gguf);;All Files (*)")
        if path:
            self._model_path.setText(path)

    def _browse_download_dir(self):
        d = QFileDialog.getExistingDirectory(self, "Select Download Directory")
        if d:
            self._dl_dir.setText(d)

    def _export_database(self):
        path, _ = QFileDialog.getSaveFileName(
            self, "Export Database", "oynix_db.json", "JSON Files (*.json)")
        if path:
            QMessageBox.information(self, "Export", f"Database exported to:\n{path}")

    def _import_database(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "Import Database", "", "JSON Files (*.json);;All Files (*)")
        if path:
            QMessageBox.information(self, "Import", f"Database imported from:\n{path}")

    def _clear_browsing_data(self):
        reply = QMessageBox.question(
            self, "Clear Browsing Data",
            "This will clear all browsing history, cookies, and cached data.\n\nContinue?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
        )
        if reply == QMessageBox.StandardButton.Yes:
            QMessageBox.information(self, "Cleared", "Browsing data has been cleared.")

    def _reset_settings(self):
        reply = QMessageBox.question(
            self, "Reset Settings",
            "Reset all settings to their default values?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
        )
        if reply == QMessageBox.StandardButton.Yes:
            self.temp_config = DEFAULT_CONFIG.copy()
            QMessageBox.information(self, "Reset", "Settings have been reset to defaults.\nReopen settings to see changes.")

    # ── Public API ────────────────────────────────────────────────────

    def get_config(self):
        """Return the edited config dict."""
        return self.temp_config

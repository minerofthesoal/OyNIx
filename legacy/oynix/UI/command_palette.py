"""
OyNIx Browser - Command Palette (Ctrl+K)
VS Code-style quick command launcher for fast navigation and actions.
"""

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLineEdit, QLabel,
    QListWidget, QListWidgetItem, QFrame, QGraphicsOpacityEffect
)
from PyQt6.QtCore import (
    Qt, pyqtSignal, QPropertyAnimation, QEasingCurve, QSize, QTimer
)
from PyQt6.QtGui import QKeySequence, QShortcut

from oynix.UI.icons import svg_icon
from oynix.core.theme_engine import NYX_COLORS

c = NYX_COLORS


class CommandItem:
    """A command palette entry."""
    def __init__(self, name, description, icon_name, action, shortcut="", category=""):
        self.name = name
        self.description = description
        self.icon_name = icon_name
        self.action = action
        self.shortcut = shortcut
        self.category = category


class CommandPalette(QWidget):
    """Floating command palette overlay."""

    command_executed = pyqtSignal(str)  # command name
    closed = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.commands = []
        self._setup_ui()
        self.hide()

    def _setup_ui(self):
        self.setObjectName("commandPalette")
        self.setFixedWidth(580)
        self.setMaximumHeight(420)

        # Semi-transparent backdrop effect
        self.setStyleSheet(f"""
            QWidget#commandPalette {{
                background: {c['bg_mid']};
                border: 1px solid {c['purple_dark']};
                border-radius: 12px;
            }}
        """)

        # Opacity effect for fade animation
        self._opacity = QGraphicsOpacityEffect(self)
        self._opacity.setOpacity(0.0)
        self.setGraphicsEffect(self._opacity)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        # Search input
        input_frame = QFrame()
        input_frame.setStyleSheet(f"""
            QFrame {{
                background: {c['bg_light']};
                border-bottom: 1px solid {c['border']};
                border-top-left-radius: 12px;
                border-top-right-radius: 12px;
            }}
        """)
        input_layout = QHBoxLayout(input_frame)
        input_layout.setContentsMargins(14, 10, 14, 10)
        input_layout.setSpacing(10)

        search_icon = QLabel()
        search_icon.setPixmap(svg_icon('command', c['purple_light'], 20).pixmap(20, 20))
        search_icon.setStyleSheet("background: transparent;")
        input_layout.addWidget(search_icon)

        self.input = QLineEdit()
        self.input.setPlaceholderText("Type a command, URL, or search...")
        self.input.setStyleSheet(f"""
            QLineEdit {{
                background: transparent;
                color: {c['text_primary']};
                border: none;
                font-size: 15px;
                padding: 4px 0;
            }}
        """)
        self.input.textChanged.connect(self._filter)
        self.input.returnPressed.connect(self._execute_selected)
        input_layout.addWidget(self.input, 1)

        esc_label = QLabel("ESC")
        esc_label.setStyleSheet(f"""
            background: {c['bg_lighter']}; color: {c['text_muted']};
            border-radius: 4px; padding: 2px 6px;
            font-size: 10px; font-weight: bold;
        """)
        input_layout.addWidget(esc_label)
        layout.addWidget(input_frame)

        # Results list
        self.results = QListWidget()
        self.results.setStyleSheet(f"""
            QListWidget {{
                background: {c['bg_mid']};
                border: none;
                outline: none;
                padding: 4px;
            }}
            QListWidget::item {{
                padding: 8px 12px;
                border-radius: 8px;
                margin: 1px 4px;
                color: {c['text_primary']};
            }}
            QListWidget::item:selected {{
                background: {c['purple_dark']};
                color: {c['purple_pale']};
            }}
            QListWidget::item:hover:!selected {{
                background: {c['bg_lighter']};
            }}
        """)
        self.results.setVerticalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        self.results.itemActivated.connect(self._on_item_activated)
        layout.addWidget(self.results)

        # Footer hint
        footer = QLabel("  Tab to autocomplete  ·  Enter to execute  ·  Esc to close")
        footer.setStyleSheet(f"""
            color: {c['text_muted']}; font-size: 10px;
            padding: 6px 14px; background: {c['bg_dark']};
            border-top: 1px solid {c['border']};
            border-bottom-left-radius: 12px;
            border-bottom-right-radius: 12px;
        """)
        layout.addWidget(footer)

    def register_commands(self, commands):
        """Register a list of CommandItem objects."""
        self.commands = commands
        self._populate_all()

    def _populate_all(self):
        self.results.clear()
        for cmd in self.commands:
            self._add_result_item(cmd)

    def _add_result_item(self, cmd):
        item = QListWidgetItem()
        item.setData(Qt.ItemDataRole.UserRole, cmd)
        item.setSizeHint(QSize(0, 40))

        widget = QWidget()
        layout = QHBoxLayout(widget)
        layout.setContentsMargins(4, 2, 4, 2)
        layout.setSpacing(10)

        icon = QLabel()
        icon.setPixmap(svg_icon(cmd.icon_name, c['purple_light'], 18).pixmap(18, 18))
        icon.setFixedSize(22, 22)
        icon.setStyleSheet("background: transparent;")
        layout.addWidget(icon)

        text_layout = QVBoxLayout()
        text_layout.setSpacing(0)
        text_layout.setContentsMargins(0, 0, 0, 0)

        name = QLabel(cmd.name)
        name.setStyleSheet(f"color: {c['text_primary']}; font-size: 13px; font-weight: 500; background: transparent;")
        text_layout.addWidget(name)

        if cmd.description:
            desc = QLabel(cmd.description)
            desc.setStyleSheet(f"color: {c['text_muted']}; font-size: 10px; background: transparent;")
            text_layout.addWidget(desc)
        layout.addLayout(text_layout, 1)

        if cmd.category:
            cat = QLabel(cmd.category)
            cat.setStyleSheet(f"""
                color: {c['purple_light']}; font-size: 10px;
                background: {c['purple_dark']}; border-radius: 4px;
                padding: 1px 6px;
            """)
            layout.addWidget(cat)

        if cmd.shortcut:
            sc = QLabel(cmd.shortcut)
            sc.setStyleSheet(f"""
                color: {c['text_muted']}; font-size: 10px;
                background: {c['bg_lighter']}; border-radius: 4px;
                padding: 2px 6px;
            """)
            layout.addWidget(sc)

        self.results.addItem(item)
        self.results.setItemWidget(item, widget)

    def _filter(self, text):
        text = text.lower().strip()
        self.results.clear()
        if not text:
            self._populate_all()
            return
        matches = []
        for cmd in self.commands:
            score = 0
            name_lower = cmd.name.lower()
            if text in name_lower:
                score = 100 - name_lower.index(text)
            elif text in cmd.description.lower():
                score = 50
            elif text in cmd.category.lower():
                score = 30
            if score > 0:
                matches.append((score, cmd))
        matches.sort(key=lambda x: x[0], reverse=True)
        for _, cmd in matches[:15]:
            self._add_result_item(cmd)
        if self.results.count() > 0:
            self.results.setCurrentRow(0)

    def _execute_selected(self):
        item = self.results.currentItem()
        if item:
            self._on_item_activated(item)
        elif self.input.text().strip():
            # Treat as URL or search query
            self.command_executed.emit(self.input.text().strip())
            self.hide_palette()

    def _on_item_activated(self, item):
        cmd = item.data(Qt.ItemDataRole.UserRole)
        if cmd and callable(cmd.action):
            cmd.action()
            self.command_executed.emit(cmd.name)
        self.hide_palette()

    def show_palette(self):
        """Show with fade-in animation."""
        self.input.clear()
        self._populate_all()
        self.show()
        self.raise_()
        self.input.setFocus()

        # Center in parent
        if self.parent():
            pw = self.parent().width()
            x = (pw - self.width()) // 2
            self.move(x, 60)

        # Fade in
        anim = QPropertyAnimation(self._opacity, b"opacity")
        anim.setDuration(150)
        anim.setStartValue(0.0)
        anim.setEndValue(1.0)
        anim.setEasingCurve(QEasingCurve.Type.OutCubic)
        anim.start()
        self._fade_anim = anim  # prevent GC

        if self.results.count() > 0:
            self.results.setCurrentRow(0)

    def hide_palette(self):
        """Hide with fade-out animation."""
        anim = QPropertyAnimation(self._opacity, b"opacity")
        anim.setDuration(100)
        anim.setStartValue(1.0)
        anim.setEndValue(0.0)
        anim.setEasingCurve(QEasingCurve.Type.InCubic)
        anim.finished.connect(self.hide)
        anim.finished.connect(self.closed.emit)
        anim.start()
        self._fade_anim = anim

    def keyPressEvent(self, event):
        if event.key() == Qt.Key.Key_Escape:
            self.hide_palette()
        elif event.key() == Qt.Key.Key_Down:
            row = self.results.currentRow()
            if row < self.results.count() - 1:
                self.results.setCurrentRow(row + 1)
        elif event.key() == Qt.Key.Key_Up:
            row = self.results.currentRow()
            if row > 0:
                self.results.setCurrentRow(row - 1)
        elif event.key() == Qt.Key.Key_Tab:
            # Autocomplete from selected item
            item = self.results.currentItem()
            if item:
                cmd = item.data(Qt.ItemDataRole.UserRole)
                if cmd:
                    self.input.setText(cmd.name)
        else:
            super().keyPressEvent(event)

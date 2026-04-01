"""
OyNIx Browser - AI Chat Panel
Sidebar panel for chatting with the local Nyx AI assistant.

Features:
- Chat with local LLM
- Summarize current page
- AI search assistance
- Conversation history
- Nyx purple/black theme
"""

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QTextEdit, QLineEdit,
    QPushButton, QLabel, QScrollArea, QFrame, QSizePolicy
)
from PyQt6.QtCore import Qt, pyqtSignal, QTimer
from PyQt6.QtGui import QFont, QTextCursor

from oynix.core.ai_manager import ai_manager
from oynix.core.theme_engine import NYX_COLORS


class ChatMessage(QFrame):
    """A single chat message bubble."""

    def __init__(self, text, is_user=True, parent=None):
        super().__init__(parent)
        self.setFrameShape(QFrame.Shape.NoFrame)

        c = NYX_COLORS
        layout = QVBoxLayout(self)
        layout.setContentsMargins(8, 4, 8, 4)

        # Sender label
        sender = QLabel("You" if is_user else "Nyx AI")
        sender.setStyleSheet(
            f"font-size:11px; font-weight:bold; background:transparent; "
            f"color: {c['purple_light'] if not is_user else c['text_muted']};"
        )
        layout.addWidget(sender)

        # Message text
        msg = QLabel(text)
        msg.setWordWrap(True)
        msg.setTextInteractionFlags(
            Qt.TextInteractionFlag.TextSelectableByMouse)

        if is_user:
            msg.setStyleSheet(
                f"background: {c['bg_lighter']}; "
                f"color: {c['text_primary']}; "
                f"border-radius: 12px; padding: 10px 14px; "
                f"border: 1px solid {c['border']};"
            )
        else:
            msg.setStyleSheet(
                f"background: {c['purple_dark']}; "
                f"color: {c['purple_pale']}; "
                f"border-radius: 12px; padding: 10px 14px; "
                f"border: 1px solid {c['purple_mid']};"
            )
        layout.addWidget(msg)

        self.setStyleSheet("background: transparent;")


class AIChatPanel(QWidget):
    """
    Side panel for AI chat interaction.
    Docks to the right side of the browser.
    """

    summarize_requested = pyqtSignal()
    close_requested = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumWidth(320)
        self.setMaximumWidth(450)
        self.setup_ui()

        # Connect AI status updates
        ai_manager.status_update.connect(self._on_ai_status)

    def setup_ui(self):
        c = NYX_COLORS
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        # ── Header ────────────────────────────────
        header = QWidget()
        header.setFixedHeight(48)
        header.setStyleSheet(
            f"background: {c['bg_darkest']}; "
            f"border-bottom: 1px solid {c['border']};"
        )
        header_layout = QHBoxLayout(header)
        header_layout.setContentsMargins(14, 0, 8, 0)

        title = QLabel("Nyx AI")
        title.setStyleSheet(
            f"font-size: 15px; font-weight: bold; "
            f"color: {c['purple_light']}; background: transparent;"
        )
        header_layout.addWidget(title)

        self.status_label = QLabel("Loading...")
        self.status_label.setStyleSheet(
            f"font-size: 11px; color: {c['text_muted']}; "
            f"background: transparent;"
        )
        header_layout.addWidget(self.status_label)
        header_layout.addStretch()

        close_btn = QPushButton("x")
        close_btn.setFixedSize(28, 28)
        close_btn.setObjectName("navButton")
        close_btn.clicked.connect(self.close_requested.emit)
        header_layout.addWidget(close_btn)

        layout.addWidget(header)

        # ── Chat Area ─────────────────────────────
        self.chat_scroll = QScrollArea()
        self.chat_scroll.setWidgetResizable(True)
        self.chat_scroll.setHorizontalScrollBarPolicy(
            Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        self.chat_scroll.setStyleSheet(
            f"QScrollArea {{ background: {c['bg_dark']}; border: none; }}")

        self.chat_container = QWidget()
        self.chat_layout = QVBoxLayout(self.chat_container)
        self.chat_layout.setAlignment(Qt.AlignmentFlag.AlignTop)
        self.chat_layout.setSpacing(4)
        self.chat_layout.setContentsMargins(4, 8, 4, 8)

        self.chat_scroll.setWidget(self.chat_container)
        layout.addWidget(self.chat_scroll, 1)

        # Welcome message
        self._add_ai_message(
            "Hello! I'm Nyx, your local AI assistant.\n\n"
            "I can help you:\n"
            "- Search the web intelligently\n"
            "- Summarize pages you're reading\n"
            "- Answer your questions\n"
            "- Chat about anything\n\n"
            "Type a message below to get started!"
        )

        # ── Quick Actions ─────────────────────────
        actions = QWidget()
        actions.setFixedHeight(40)
        actions.setStyleSheet(
            f"background: {c['bg_darkest']}; "
            f"border-top: 1px solid {c['border']};"
        )
        actions_layout = QHBoxLayout(actions)
        actions_layout.setContentsMargins(8, 4, 8, 4)
        actions_layout.setSpacing(6)

        summarize_btn = QPushButton("Summarize Page")
        summarize_btn.setObjectName("accentButton")
        summarize_btn.setFixedHeight(28)
        summarize_btn.clicked.connect(self.summarize_requested.emit)
        actions_layout.addWidget(summarize_btn)

        clear_btn = QPushButton("Clear Chat")
        clear_btn.setFixedHeight(28)
        clear_btn.clicked.connect(self.clear_chat)
        actions_layout.addWidget(clear_btn)

        layout.addWidget(actions)

        # ── Input Area ────────────────────────────
        input_area = QWidget()
        input_area.setFixedHeight(52)
        input_area.setStyleSheet(
            f"background: {c['bg_darkest']}; "
            f"border-top: 1px solid {c['border']};"
        )
        input_layout = QHBoxLayout(input_area)
        input_layout.setContentsMargins(8, 8, 8, 8)
        input_layout.setSpacing(8)

        self.input_field = QLineEdit()
        self.input_field.setPlaceholderText("Ask Nyx anything...")
        self.input_field.returnPressed.connect(self.send_message)
        input_layout.addWidget(self.input_field)

        send_btn = QPushButton("Send")
        send_btn.setObjectName("accentButton")
        send_btn.setFixedWidth(60)
        send_btn.clicked.connect(self.send_message)
        input_layout.addWidget(send_btn)

        layout.addWidget(input_area)

    def send_message(self):
        """Send a user message and get AI response."""
        text = self.input_field.text().strip()
        if not text:
            return

        self.input_field.clear()

        # Add user message
        self._add_user_message(text)

        # Show typing indicator
        self._add_ai_message("Thinking...")
        typing_widget = self.chat_layout.itemAt(
            self.chat_layout.count() - 1).widget()

        # Generate response (in a timer to not block UI)
        QTimer.singleShot(50, lambda: self._generate_response(
            text, typing_widget))

    def _generate_response(self, text, typing_widget):
        """Generate AI response and replace typing indicator."""
        response = ai_manager.chat(text)

        # Remove typing indicator
        self.chat_layout.removeWidget(typing_widget)
        typing_widget.deleteLater()

        # Add actual response
        self._add_ai_message(response)

    def summarize_page(self, page_text):
        """Summarize page content."""
        self._add_user_message("[Summarize current page]")

        summary = ai_manager.summarize(page_text)
        self._add_ai_message(f"Page Summary:\n\n{summary}")

    def _add_user_message(self, text):
        """Add a user message to the chat."""
        msg = ChatMessage(text, is_user=True)
        self.chat_layout.addWidget(msg)
        self._scroll_to_bottom()

    def _add_ai_message(self, text):
        """Add an AI message to the chat."""
        msg = ChatMessage(text, is_user=False)
        self.chat_layout.addWidget(msg)
        self._scroll_to_bottom()

    def _scroll_to_bottom(self):
        """Scroll chat to bottom."""
        QTimer.singleShot(50, lambda: self.chat_scroll.verticalScrollBar()
                          .setValue(
                              self.chat_scroll.verticalScrollBar().maximum()))

    def _on_ai_status(self, status):
        """Update AI status label."""
        if "ready" in status.lower():
            self.status_label.setText("Ready")
            self.status_label.setStyleSheet(
                f"font-size:11px; color:{NYX_COLORS['success']}; "
                f"background:transparent;"
            )
        elif "download" in status.lower():
            self.status_label.setText("Downloading...")
            self.status_label.setStyleSheet(
                f"font-size:11px; color:{NYX_COLORS['warning']}; "
                f"background:transparent;"
            )
        elif "loading" in status.lower():
            self.status_label.setText("Loading...")
            self.status_label.setStyleSheet(
                f"font-size:11px; color:{NYX_COLORS['warning']}; "
                f"background:transparent;"
            )
        else:
            self.status_label.setText("Fallback Mode")

    def clear_chat(self):
        """Clear all chat messages."""
        while self.chat_layout.count():
            item = self.chat_layout.takeAt(0)
            if item.widget():
                item.widget().deleteLater()

        ai_manager.clear_history()

        self._add_ai_message("Chat cleared. How can I help you?")

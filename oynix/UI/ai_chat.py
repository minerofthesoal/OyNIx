"""
OyNIx Browser - AI Chat Panel (Modern Design)
Sidebar panel for chatting with the local Nyx AI assistant.

Features:
- Modern chat bubble UI with avatars and timestamps
- Message slide/fade animations
- Typing indicator with bouncing dots
- Auto-growing multi-line input
- Quick action pills
- Frosted glass aesthetic
- Custom scrollbar
"""

from datetime import datetime

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QTextEdit,
    QPushButton, QLabel, QScrollArea, QFrame, QSizePolicy,
    QGraphicsOpacityEffect, QSpacerItem
)
from PyQt6.QtCore import (
    Qt, pyqtSignal, QTimer, QPropertyAnimation, QEasingCurve,
    QPoint, QSize, pyqtProperty, QParallelAnimationGroup,
    QSequentialAnimationGroup, QRect
)
from PyQt6.QtGui import QFont, QTextCursor, QColor, QPainter, QPainterPath

from oynix.core.ai_manager import ai_manager
from oynix.core.theme_engine import NYX_COLORS
from oynix.UI.icons import svg_icon


# ---------------------------------------------------------------------------
# Typing indicator (three bouncing dots)
# ---------------------------------------------------------------------------

class _BouncingDot(QWidget):
    """A single dot that can animate its vertical offset."""

    def __init__(self, color: str, parent=None):
        super().__init__(parent)
        self._color = QColor(color)
        self._bounce = 0
        self.setFixedSize(8, 20)

    def _get_bounce(self):
        return self._bounce

    def _set_bounce(self, v):
        self._bounce = v
        self.update()

    bounce = pyqtProperty(int, _get_bounce, _set_bounce)

    def paintEvent(self, event):
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        p.setBrush(self._color)
        p.setPen(Qt.PenStyle.NoPen)
        y = 10 - self._bounce
        p.drawEllipse(0, y, 8, 8)
        p.end()


class TypingIndicator(QWidget):
    """Three bouncing dots shown while AI is thinking."""

    def __init__(self, parent=None):
        super().__init__(parent)
        c = NYX_COLORS
        layout = QHBoxLayout(self)
        layout.setContentsMargins(12, 6, 12, 6)
        layout.setSpacing(4)
        layout.setAlignment(Qt.AlignmentFlag.AlignLeft)

        self.dots = []
        for _ in range(3):
            dot = _BouncingDot(c['purple_light'], self)
            layout.addWidget(dot)
            self.dots.append(dot)
        layout.addStretch()

        self._anims = []
        self.setFixedHeight(32)
        self.setStyleSheet("background: transparent;")

    def start(self):
        self._anims = []
        for i, dot in enumerate(self.dots):
            anim = QPropertyAnimation(dot, b"bounce")
            anim.setDuration(600)
            anim.setStartValue(0)
            anim.setKeyValueAt(0.5, 8)
            anim.setEndValue(0)
            anim.setEasingCurve(QEasingCurve.Type.InOutSine)
            anim.setLoopCount(-1)
            # stagger each dot
            QTimer.singleShot(i * 150, anim.start)
            self._anims.append(anim)

    def stop(self):
        for a in self._anims:
            a.stop()
        self._anims = []


# ---------------------------------------------------------------------------
# Chat message bubble
# ---------------------------------------------------------------------------

class ChatBubble(QFrame):
    """A single chat message bubble with avatar and timestamp."""

    def __init__(self, text: str, is_user: bool = True, parent=None):
        super().__init__(parent)
        c = NYX_COLORS
        self.is_user = is_user
        self.setFrameShape(QFrame.Shape.NoFrame)

        # Outer layout controls alignment (left vs right)
        outer = QHBoxLayout(self)
        outer.setContentsMargins(6, 2, 6, 2)
        outer.setSpacing(8)

        if is_user:
            outer.addStretch()

        # Avatar
        avatar = QLabel()
        avatar.setFixedSize(30, 30)
        avatar.setAlignment(Qt.AlignmentFlag.AlignCenter)
        if is_user:
            avatar.setText("U")
            avatar.setStyleSheet(
                f"background: {c['purple_mid']}; color: #fff; "
                f"border-radius: 15px; font-weight: bold; font-size: 13px;"
            )
        else:
            avatar.setPixmap(
                svg_icon('ai_sparkle', c['purple_light'], 22).pixmap(22, 22)
            )
            avatar.setStyleSheet(
                f"background: {c['bg_lighter']}; border-radius: 15px;"
            )

        # Content column (bubble + timestamp)
        content_col = QVBoxLayout()
        content_col.setSpacing(2)
        content_col.setContentsMargins(0, 0, 0, 0)

        # Bubble
        bubble = QLabel(text)
        bubble.setWordWrap(True)
        bubble.setTextInteractionFlags(
            Qt.TextInteractionFlag.TextSelectableByMouse
        )
        bubble.setMinimumWidth(60)
        bubble.setMaximumWidth(320)
        bubble.setSizePolicy(
            QSizePolicy.Policy.Preferred, QSizePolicy.Policy.Minimum
        )

        if is_user:
            bubble.setStyleSheet(
                f"background: qlineargradient("
                f"x1:0, y1:0, x2:1, y2:1, "
                f"stop:0 {c['purple_mid']}, stop:1 {c['purple_dark']});"
                f"color: #ffffff; "
                f"border-radius: 14px; "
                f"border-top-right-radius: 4px; "
                f"padding: 10px 14px; "
                f"font-size: 13px;"
            )
        else:
            bubble.setStyleSheet(
                f"background: rgba(34, 34, 46, 0.85); "
                f"color: {c['text_primary']}; "
                f"border-radius: 14px; "
                f"border-top-left-radius: 4px; "
                f"padding: 10px 14px; "
                f"border: 1px solid {c['border']}; "
                f"font-size: 13px;"
            )

        content_col.addWidget(bubble)

        # Timestamp
        now = datetime.now().strftime("%H:%M")
        ts = QLabel(now)
        ts.setStyleSheet(
            f"font-size: 10px; color: {c['text_muted']}; "
            f"background: transparent; padding: 0 4px;"
        )
        if is_user:
            ts.setAlignment(Qt.AlignmentFlag.AlignRight)
        else:
            ts.setAlignment(Qt.AlignmentFlag.AlignLeft)
        content_col.addWidget(ts)

        # Assemble: avatar on left for AI, on right for user
        if is_user:
            outer.addLayout(content_col)
            outer.addWidget(avatar, alignment=Qt.AlignmentFlag.AlignTop)
        else:
            outer.addWidget(avatar, alignment=Qt.AlignmentFlag.AlignTop)
            outer.addLayout(content_col)
            outer.addStretch()

        self.setStyleSheet("background: transparent;")

        # -- Slide + fade animation --
        self._opacity_effect = QGraphicsOpacityEffect(self)
        self._opacity_effect.setOpacity(0.0)
        self.setGraphicsEffect(self._opacity_effect)

    def animate_in(self):
        """Slide from side + fade in."""
        offset = 40 if self.is_user else -40

        # Fade
        fade = QPropertyAnimation(self._opacity_effect, b"opacity")
        fade.setDuration(300)
        fade.setStartValue(0.0)
        fade.setEndValue(1.0)
        fade.setEasingCurve(QEasingCurve.Type.OutCubic)

        # Slide (using margin trick via pos)
        slide = QPropertyAnimation(self, b"pos")
        slide.setDuration(300)
        start = self.pos()
        slide.setStartValue(QPoint(start.x() + offset, start.y()))
        slide.setEndValue(start)
        slide.setEasingCurve(QEasingCurve.Type.OutCubic)

        group = QParallelAnimationGroup(self)
        group.addAnimation(fade)
        group.addAnimation(slide)
        group.start()
        # prevent gc
        self._anim_group = group


# ---------------------------------------------------------------------------
# Auto-growing text input
# ---------------------------------------------------------------------------

class _AutoGrowTextEdit(QTextEdit):
    """Multi-line input that grows up to 4 lines, then scrolls."""

    submit_pressed = pyqtSignal()

    _MAX_LINES = 4
    _LINE_HEIGHT = 20

    def __init__(self, parent=None):
        super().__init__(parent)
        c = NYX_COLORS
        self.setPlaceholderText("Ask Nyx AI anything...")
        self.setAcceptRichText(False)
        self.setVerticalScrollBarPolicy(
            Qt.ScrollBarPolicy.ScrollBarAsNeeded
        )
        self._base_height = 36
        self.setFixedHeight(self._base_height)
        self.textChanged.connect(self._adjust_height)
        self.setStyleSheet(
            f"QTextEdit {{"
            f"  background: {c['bg_lighter']}; "
            f"  color: {c['text_primary']}; "
            f"  border: 1px solid {c['border']}; "
            f"  border-radius: 12px; "
            f"  padding: 6px 12px; "
            f"  font-size: 13px; "
            f"  selection-background-color: {c['selection']}; "
            f"}}"
        )

    def _adjust_height(self):
        doc_height = self.document().size().height()
        new_h = max(self._base_height,
                    min(int(doc_height) + 14,
                        self._LINE_HEIGHT * self._MAX_LINES + 14))
        if new_h != self.height():
            self.setFixedHeight(new_h)

    def keyPressEvent(self, event):
        if (event.key() in (Qt.Key.Key_Return, Qt.Key.Key_Enter)
                and not event.modifiers() & Qt.KeyboardModifier.ShiftModifier):
            event.accept()
            self.submit_pressed.emit()
            return
        super().keyPressEvent(event)


# ---------------------------------------------------------------------------
# Quick-action pill button
# ---------------------------------------------------------------------------

class _PillButton(QPushButton):

    def __init__(self, text, icon_name=None, parent=None):
        super().__init__(text, parent)
        c = NYX_COLORS
        if icon_name:
            self.setIcon(svg_icon(icon_name, c['purple_light'], 16))
        self.setCursor(Qt.CursorShape.PointingHandCursor)
        self.setFixedHeight(28)
        self.setStyleSheet(
            f"QPushButton {{"
            f"  background: {c['bg_lighter']}; "
            f"  color: {c['text_secondary']}; "
            f"  border: 1px solid {c['border']}; "
            f"  border-radius: 14px; "
            f"  padding: 0 12px; "
            f"  font-size: 11px; "
            f"}}"
            f"QPushButton:hover {{"
            f"  background: {c['purple_dark']}; "
            f"  color: {c['purple_pale']}; "
            f"  border-color: {c['purple_mid']}; "
            f"}}"
        )


# ---------------------------------------------------------------------------
# Main panel
# ---------------------------------------------------------------------------

class AIChatPanel(QWidget):
    """
    Modern side panel for AI chat interaction.
    Docks to the right side of the browser.
    """

    summarize_requested = pyqtSignal()
    close_requested = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumWidth(320)
        self.setMaximumWidth(450)
        self._typing_indicator = None
        self._message_widgets: list[ChatBubble] = []
        self._setup_ui()

        # Connect AI status updates
        ai_manager.status_update.connect(self._on_ai_status)

    # ------------------------------------------------------------------
    # UI construction
    # ------------------------------------------------------------------

    def _setup_ui(self):
        c = NYX_COLORS
        root = QVBoxLayout(self)
        root.setContentsMargins(0, 0, 0, 0)
        root.setSpacing(0)

        # Panel background (frosted glass)
        self.setStyleSheet(
            f"AIChatPanel {{"
            f"  background: rgba(10, 10, 15, 0.92); "
            f"  border-left: 1px solid {c['border']}; "
            f"}}"
        )

        # ── Header ────────────────────────────────────────────────────
        header = QFrame()
        header.setFixedHeight(52)
        header.setStyleSheet(
            f"QFrame {{"
            f"  background: rgba(10, 10, 15, 0.95); "
            f"  border-bottom: 1px solid {c['border']}; "
            f"}}"
        )
        h_lay = QHBoxLayout(header)
        h_lay.setContentsMargins(14, 0, 8, 0)
        h_lay.setSpacing(8)

        # Sparkle icon + title
        sparkle_label = QLabel()
        sparkle_label.setPixmap(
            svg_icon('ai_sparkle', c['purple_light'], 20).pixmap(20, 20)
        )
        sparkle_label.setFixedSize(24, 24)
        sparkle_label.setStyleSheet("background: transparent;")
        h_lay.addWidget(sparkle_label)

        title = QLabel("Nyx AI")
        title.setStyleSheet(
            f"font-size: 15px; font-weight: bold; "
            f"color: {c['purple_light']}; background: transparent;"
        )
        h_lay.addWidget(title)

        # Status dot + text
        self._status_dot = QLabel()
        self._status_dot.setFixedSize(8, 8)
        self._status_dot.setStyleSheet(
            f"background: {c['text_muted']}; border-radius: 4px;"
        )
        h_lay.addWidget(self._status_dot)

        self._status_label = QLabel("Loading model...")
        self._status_label.setStyleSheet(
            f"font-size: 11px; color: {c['text_muted']}; "
            f"background: transparent;"
        )
        h_lay.addWidget(self._status_label)

        h_lay.addStretch()

        close_btn = QPushButton()
        close_btn.setIcon(svg_icon('close', c['text_muted'], 16))
        close_btn.setFixedSize(28, 28)
        close_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        close_btn.setStyleSheet(
            f"QPushButton {{"
            f"  background: transparent; border: none; border-radius: 14px;"
            f"}}"
            f"QPushButton:hover {{"
            f"  background: {c['bg_lighter']};"
            f"}}"
        )
        close_btn.clicked.connect(self.close_requested.emit)
        h_lay.addWidget(close_btn)

        root.addWidget(header)

        # ── Quick Actions Bar ─────────────────────────────────────────
        actions_bar = QFrame()
        actions_bar.setFixedHeight(42)
        actions_bar.setStyleSheet(
            f"QFrame {{"
            f"  background: rgba(14, 14, 20, 0.9); "
            f"  border-bottom: 1px solid {c['border']}; "
            f"}}"
        )
        a_lay = QHBoxLayout(actions_bar)
        a_lay.setContentsMargins(8, 6, 8, 6)
        a_lay.setSpacing(6)

        summarize_pill = _PillButton("Summarize Page", "summarize")
        summarize_pill.clicked.connect(self.summarize_requested.emit)
        a_lay.addWidget(summarize_pill)

        explain_pill = _PillButton("Explain Code", "terminal")
        a_lay.addWidget(explain_pill)

        similar_pill = _PillButton("Find Similar", "globe")
        a_lay.addWidget(similar_pill)

        clear_pill = _PillButton("Clear", "clear")
        clear_pill.clicked.connect(self.clear_chat)
        a_lay.addWidget(clear_pill)

        a_lay.addStretch()
        root.addWidget(actions_bar)

        # ── Chat scroll area ──────────────────────────────────────────
        self._chat_scroll = QScrollArea()
        self._chat_scroll.setWidgetResizable(True)
        self._chat_scroll.setHorizontalScrollBarPolicy(
            Qt.ScrollBarPolicy.ScrollBarAlwaysOff
        )
        self._chat_scroll.setStyleSheet(
            f"QScrollArea {{"
            f"  background: {c['bg_dark']}; border: none;"
            f"}}"
            f"QScrollBar:vertical {{"
            f"  background: {c['bg_dark']}; width: 6px; margin: 0;"
            f"}}"
            f"QScrollBar::handle:vertical {{"
            f"  background: {c['scrollbar']}; border-radius: 3px; "
            f"  min-height: 30px;"
            f"}}"
            f"QScrollBar::handle:vertical:hover {{"
            f"  background: {c['scrollbar_hover']};"
            f"}}"
            f"QScrollBar::add-line:vertical, "
            f"QScrollBar::sub-line:vertical {{"
            f"  height: 0;"
            f"}}"
            f"QScrollBar::add-page:vertical, "
            f"QScrollBar::sub-page:vertical {{"
            f"  background: none;"
            f"}}"
        )

        self._chat_container = QWidget()
        self._chat_container.setStyleSheet(
            f"background: {c['bg_dark']};"
        )
        self._chat_layout = QVBoxLayout(self._chat_container)
        self._chat_layout.setAlignment(Qt.AlignmentFlag.AlignTop)
        self._chat_layout.setSpacing(4)
        self._chat_layout.setContentsMargins(4, 10, 4, 10)

        self._chat_scroll.setWidget(self._chat_container)
        root.addWidget(self._chat_scroll, 1)

        # ── Input area ────────────────────────────────────────────────
        input_frame = QFrame()
        input_frame.setStyleSheet(
            f"QFrame {{"
            f"  background: rgba(10, 10, 15, 0.95); "
            f"  border-top: 1px solid {c['border']}; "
            f"}}"
        )
        i_lay = QHBoxLayout(input_frame)
        i_lay.setContentsMargins(10, 8, 10, 10)
        i_lay.setSpacing(8)

        self._input_field = _AutoGrowTextEdit()
        self._input_field.submit_pressed.connect(self._send_message)
        self._input_field.textChanged.connect(self._on_input_changed)
        i_lay.addWidget(self._input_field)

        self._send_btn = QPushButton()
        self._send_btn.setIcon(svg_icon('send', c['purple_light'], 18))
        self._send_btn.setFixedSize(36, 36)
        self._send_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        self._send_btn.setStyleSheet(
            f"QPushButton {{"
            f"  background: {c['purple_dark']}; "
            f"  border: none; border-radius: 18px;"
            f"}}"
            f"QPushButton:hover {{"
            f"  background: {c['purple_mid']};"
            f"}}"
        )
        self._send_btn.clicked.connect(self._send_message)
        i_lay.addWidget(self._send_btn, alignment=Qt.AlignmentFlag.AlignBottom)

        root.addWidget(input_frame)

        # Pulse animation for send button
        self._pulse_anim = QPropertyAnimation(self._send_btn, b"iconSize")
        self._pulse_anim.setDuration(800)
        self._pulse_anim.setStartValue(QSize(18, 18))
        self._pulse_anim.setKeyValueAt(0.5, QSize(22, 22))
        self._pulse_anim.setEndValue(QSize(18, 18))
        self._pulse_anim.setEasingCurve(QEasingCurve.Type.InOutSine)
        self._pulse_anim.setLoopCount(-1)

        # Welcome message
        self._add_bubble(
            "Hello! I'm Nyx, your local AI assistant.\n\n"
            "I can help you:\n"
            "- Search the web intelligently\n"
            "- Summarize pages you're reading\n"
            "- Answer your questions\n"
            "- Chat about anything\n\n"
            "Type a message below to get started!",
            is_user=False,
        )

    # ------------------------------------------------------------------
    # Public API (used by browser.py)
    # ------------------------------------------------------------------

    def add_message(self, role: str, text: str):
        """Add a message to the chat.

        Parameters
        ----------
        role : str
            ``'user'`` or ``'assistant'``.
        text : str
            The message body.
        """
        self._add_bubble(text, is_user=(role == 'user'))

    def get_input_text(self) -> str:
        """Return the current text in the input box."""
        return self._input_field.toPlainText()

    def clear_chat(self):
        """Remove all messages and reset conversation history."""
        for w in self._message_widgets:
            self._chat_layout.removeWidget(w)
            w.deleteLater()
        self._message_widgets.clear()

        if self._typing_indicator is not None:
            self._hide_typing()

        ai_manager.clear_history()

        self._add_bubble("Chat cleared. How can I help you?", is_user=False)

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _add_bubble(self, text: str, is_user: bool):
        bubble = ChatBubble(text, is_user=is_user)
        self._chat_layout.addWidget(bubble)
        self._message_widgets.append(bubble)
        # Trigger animation after the widget has been laid out
        QTimer.singleShot(30, bubble.animate_in)
        self._scroll_to_bottom()

    def _show_typing(self):
        if self._typing_indicator is not None:
            return
        self._typing_indicator = TypingIndicator()
        self._chat_layout.addWidget(self._typing_indicator)
        self._typing_indicator.start()
        self._scroll_to_bottom()

    def _hide_typing(self):
        if self._typing_indicator is None:
            return
        self._typing_indicator.stop()
        self._chat_layout.removeWidget(self._typing_indicator)
        self._typing_indicator.deleteLater()
        self._typing_indicator = None

    def _scroll_to_bottom(self):
        QTimer.singleShot(60, self._do_scroll)

    def _do_scroll(self):
        vbar = self._chat_scroll.verticalScrollBar()
        vbar.setValue(vbar.maximum())

    def _send_message(self):
        text = self._input_field.toPlainText().strip()
        if not text:
            return
        self._input_field.clear()
        self._add_bubble(text, is_user=True)
        self._show_typing()
        QTimer.singleShot(50, lambda: self._generate_response(text))

    def _generate_response(self, text: str):
        response = ai_manager.chat(text)
        self._hide_typing()
        self._add_bubble(response, is_user=False)

    def summarize_page(self, page_text: str):
        """Summarize page content."""
        self._add_bubble("[Summarize current page]", is_user=True)
        self._show_typing()
        summary = ai_manager.summarize(page_text)
        self._hide_typing()
        self._add_bubble(f"Page Summary:\n\n{summary}", is_user=False)

    # ------------------------------------------------------------------
    # Status & input callbacks
    # ------------------------------------------------------------------

    def _on_ai_status(self, status: str):
        c = NYX_COLORS
        lower = status.lower()
        if "ready" in lower:
            label, color = "Ready", c['success']
        elif "download" in lower:
            label, color = "Downloading...", c['warning']
        elif "loading" in lower:
            label, color = "Loading model...", c['warning']
        else:
            label, color = "Fallback Mode", c['text_muted']

        self._status_label.setText(label)
        self._status_label.setStyleSheet(
            f"font-size: 11px; color: {color}; background: transparent;"
        )
        self._status_dot.setStyleSheet(
            f"background: {color}; border-radius: 4px;"
        )

    def _on_input_changed(self):
        has_text = bool(self._input_field.toPlainText().strip())
        if has_text and self._pulse_anim.state() != QPropertyAnimation.State.Running:
            self._pulse_anim.start()
        elif not has_text and self._pulse_anim.state() == QPropertyAnimation.State.Running:
            self._pulse_anim.stop()
            self._send_btn.setIconSize(QSize(18, 18))

    # ------------------------------------------------------------------
    # Panel slide-in animation (call after adding to parent layout)
    # ------------------------------------------------------------------

    def animate_open(self):
        """Slide the panel in from the right edge."""
        anim = QPropertyAnimation(self, b"pos")
        anim.setDuration(300)
        start = QPoint(self.x() + self.width(), self.y())
        anim.setStartValue(start)
        anim.setEndValue(QPoint(self.x(), self.y()))
        anim.setEasingCurve(QEasingCurve.Type.OutCubic)
        anim.start()
        self._open_anim = anim

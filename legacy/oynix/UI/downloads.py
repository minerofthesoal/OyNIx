"""
OyNIx Browser - Download Manager Panel
Tracks active and completed downloads with progress bars and actions.
"""

import os
from datetime import datetime

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QPushButton,
    QFrame, QScrollArea, QProgressBar, QFileDialog
)
from PyQt6.QtCore import Qt, pyqtSignal, QSize, QTimer, QPropertyAnimation, QEasingCurve
from PyQt6.QtGui import QDesktopServices
from PyQt6.QtWebEngineCore import QWebEngineDownloadRequest

from oynix.UI.icons import svg_icon
from oynix.core.theme_engine import NYX_COLORS

c = NYX_COLORS


class DownloadItem(QFrame):
    """A single download entry with progress bar and controls."""

    cancel_requested = pyqtSignal(object)
    remove_requested = pyqtSignal(object)

    def __init__(self, download: QWebEngineDownloadRequest, parent=None):
        super().__init__(parent)
        self.download = download
        self.setObjectName("downloadItem")
        self._setup_ui()
        self._connect_signals()

        # Animate in
        self.setMaximumHeight(0)
        self._anim = QPropertyAnimation(self, b"maximumHeight")
        self._anim.setDuration(300)
        self._anim.setStartValue(0)
        self._anim.setEndValue(90)
        self._anim.setEasingCurve(QEasingCurve.Type.OutCubic)
        self._anim.start()

    def _setup_ui(self):
        self.setStyleSheet(f"""
            QFrame#downloadItem {{
                background: {c['bg_mid']};
                border: 1px solid {c['border']};
                border-radius: 10px;
                margin: 3px 8px;
            }}
            QFrame#downloadItem:hover {{
                border-color: {c['purple_dark']};
            }}
        """)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(12, 8, 12, 8)
        layout.setSpacing(4)

        # Top row: icon + filename + actions
        top = QHBoxLayout()
        top.setSpacing(8)

        file_icon = QLabel()
        file_icon.setPixmap(svg_icon('downloads', c['purple_light'], 18).pixmap(18, 18))
        file_icon.setStyleSheet("background: transparent;")
        top.addWidget(file_icon)

        filename = os.path.basename(self.download.downloadFileName() or "download")
        self.name_label = QLabel(filename)
        self.name_label.setStyleSheet(f"""
            font-size: 12px; font-weight: 600;
            color: {c['text_primary']}; background: transparent;
        """)
        self.name_label.setToolTip(self.download.downloadFileName())
        top.addWidget(self.name_label, 1)

        self.status_label = QLabel("Downloading...")
        self.status_label.setStyleSheet(f"""
            font-size: 11px; color: {c['text_muted']}; background: transparent;
        """)
        top.addWidget(self.status_label)

        # Open file button
        self.open_btn = QPushButton()
        self.open_btn.setIcon(svg_icon('folder_open', c['text_secondary'], 14))
        self.open_btn.setIconSize(QSize(14, 14))
        self.open_btn.setFixedSize(26, 26)
        self.open_btn.setToolTip("Open File")
        self.open_btn.setStyleSheet(f"""
            QPushButton {{ background: transparent; border: none; border-radius: 6px; }}
            QPushButton:hover {{ background: {c['bg_lighter']}; }}
        """)
        self.open_btn.clicked.connect(self._open_file)
        self.open_btn.hide()
        top.addWidget(self.open_btn)

        # Cancel / Remove button
        self.action_btn = QPushButton()
        self.action_btn.setIcon(svg_icon('close', c['text_secondary'], 14))
        self.action_btn.setIconSize(QSize(14, 14))
        self.action_btn.setFixedSize(26, 26)
        self.action_btn.setToolTip("Cancel")
        self.action_btn.setStyleSheet(f"""
            QPushButton {{ background: transparent; border: none; border-radius: 6px; }}
            QPushButton:hover {{ background: {c['error']}; }}
        """)
        self.action_btn.clicked.connect(self._on_action)
        top.addWidget(self.action_btn)

        layout.addLayout(top)

        # Progress bar
        self.progress = QProgressBar()
        self.progress.setMaximum(100)
        self.progress.setValue(0)
        self.progress.setFixedHeight(4)
        self.progress.setTextVisible(False)
        self.progress.setStyleSheet(f"""
            QProgressBar {{
                background: {c['bg_lighter']};
                border: none;
                border-radius: 2px;
            }}
            QProgressBar::chunk {{
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                    stop:0 {c['purple_mid']}, stop:1 {c['purple_glow']});
                border-radius: 2px;
            }}
        """)
        layout.addWidget(self.progress)

    def _connect_signals(self):
        self.download.receivedBytesChanged.connect(self._update_progress)
        self.download.stateChanged.connect(self._on_state_changed)

    def _update_progress(self):
        received = self.download.receivedBytes()
        total = self.download.totalBytes()
        if total > 0:
            pct = int(received * 100 / total)
            self.progress.setValue(pct)
            size_str = self._format_bytes(received)
            total_str = self._format_bytes(total)
            self.status_label.setText(f"{size_str} / {total_str} ({pct}%)")
        else:
            size_str = self._format_bytes(received)
            self.status_label.setText(f"{size_str} downloaded")

    def _on_state_changed(self, state):
        if state == QWebEngineDownloadRequest.DownloadState.DownloadCompleted:
            self.progress.setValue(100)
            self.progress.setStyleSheet(f"""
                QProgressBar {{
                    background: {c['bg_lighter']}; border: none; border-radius: 2px;
                }}
                QProgressBar::chunk {{
                    background: {c['success']}; border-radius: 2px;
                }}
            """)
            total = self.download.totalBytes()
            self.status_label.setText(f"Complete — {self._format_bytes(total)}")
            self.status_label.setStyleSheet(f"font-size: 11px; color: {c['success']}; background: transparent;")
            self.action_btn.setIcon(svg_icon('close', c['text_muted'], 14))
            self.action_btn.setToolTip("Remove")
            self.open_btn.show()

        elif state == QWebEngineDownloadRequest.DownloadState.DownloadCancelled:
            self.status_label.setText("Cancelled")
            self.status_label.setStyleSheet(f"font-size: 11px; color: {c['error']}; background: transparent;")
            self.progress.hide()
            self.action_btn.setToolTip("Remove")

        elif state == QWebEngineDownloadRequest.DownloadState.DownloadInterrupted:
            self.status_label.setText("Failed")
            self.status_label.setStyleSheet(f"font-size: 11px; color: {c['error']}; background: transparent;")
            self.action_btn.setToolTip("Remove")

    def _on_action(self):
        state = self.download.state()
        if state == QWebEngineDownloadRequest.DownloadState.DownloadInProgress:
            self.download.cancel()
            self.cancel_requested.emit(self)
        else:
            self.remove_requested.emit(self)

    def _open_file(self):
        path = self.download.downloadDirectory() + "/" + self.download.downloadFileName()
        if os.path.exists(path):
            from PyQt6.QtCore import QUrl
            QDesktopServices.openUrl(QUrl.fromLocalFile(path))

    @staticmethod
    def _format_bytes(b):
        for unit in ['B', 'KB', 'MB', 'GB']:
            if b < 1024:
                return f"{b:.1f} {unit}"
            b /= 1024
        return f"{b:.1f} TB"


class DownloadPanel(QWidget):
    """Download manager panel — slides in from bottom or side."""

    close_requested = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("downloadPanel")
        self.downloads = []
        self._setup_ui()

    def _setup_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        # Header
        header = QFrame()
        header.setStyleSheet(f"""
            QFrame {{
                background: {c['bg_mid']};
                border-bottom: 1px solid {c['border']};
            }}
        """)
        header_layout = QHBoxLayout(header)
        header_layout.setContentsMargins(12, 8, 12, 8)

        icon_label = QLabel()
        icon_label.setPixmap(svg_icon('downloads', c['purple_light'], 18).pixmap(18, 18))
        icon_label.setStyleSheet("background: transparent;")
        header_layout.addWidget(icon_label)

        title = QLabel("Downloads")
        title.setStyleSheet(f"""
            font-size: 14px; font-weight: bold;
            color: {c['text_primary']}; background: transparent;
        """)
        header_layout.addWidget(title)

        self.count_label = QLabel("0")
        self.count_label.setStyleSheet(f"""
            background: {c['purple_dark']}; color: {c['purple_pale']};
            border-radius: 8px; padding: 2px 8px;
            font-size: 11px; font-weight: bold;
        """)
        header_layout.addWidget(self.count_label)
        header_layout.addStretch()

        clear_btn = QPushButton("Clear All")
        clear_btn.setStyleSheet(f"""
            QPushButton {{
                background: transparent; color: {c['text_secondary']};
                border: 1px solid {c['border']}; border-radius: 6px;
                padding: 3px 10px; font-size: 11px;
            }}
            QPushButton:hover {{ border-color: {c['purple_mid']}; color: {c['purple_light']}; }}
        """)
        clear_btn.clicked.connect(self._clear_completed)
        header_layout.addWidget(clear_btn)

        close_btn = QPushButton()
        close_btn.setIcon(svg_icon('close', c['text_secondary'], 16))
        close_btn.setIconSize(QSize(16, 16))
        close_btn.setFixedSize(28, 28)
        close_btn.setStyleSheet(f"""
            QPushButton {{ background: transparent; border: none; border-radius: 6px; }}
            QPushButton:hover {{ background: {c['error']}; }}
        """)
        close_btn.clicked.connect(self.close_requested.emit)
        header_layout.addWidget(close_btn)
        layout.addWidget(header)

        # Scroll area for download items
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        scroll.setStyleSheet(f"""
            QScrollArea {{ background: {c['bg_dark']}; border: none; }}
        """)

        self.content = QWidget()
        self.content_layout = QVBoxLayout(self.content)
        self.content_layout.setContentsMargins(0, 4, 0, 4)
        self.content_layout.setSpacing(0)
        self.content_layout.addStretch()

        # Empty state
        self.empty_label = QLabel("No downloads yet")
        self.empty_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.empty_label.setStyleSheet(f"""
            color: {c['text_muted']}; font-size: 13px;
            padding: 40px; background: transparent;
        """)
        self.content_layout.insertWidget(0, self.empty_label)

        scroll.setWidget(self.content)
        layout.addWidget(scroll)

    def add_download(self, download: QWebEngineDownloadRequest):
        """Add a new download to track."""
        self.empty_label.hide()

        item = DownloadItem(download)
        item.remove_requested.connect(self._remove_item)
        self.downloads.append(item)
        # Insert before the stretch
        self.content_layout.insertWidget(
            self.content_layout.count() - 1, item)
        self._update_count()

    def _remove_item(self, item):
        if item in self.downloads:
            self.downloads.remove(item)
            item.setParent(None)
            item.deleteLater()
            self._update_count()
            if not self.downloads:
                self.empty_label.show()

    def _clear_completed(self):
        to_remove = []
        for item in self.downloads:
            state = item.download.state()
            if state != QWebEngineDownloadRequest.DownloadState.DownloadInProgress:
                to_remove.append(item)
        for item in to_remove:
            self._remove_item(item)

    def _update_count(self):
        active = sum(1 for d in self.downloads
                     if d.download.state() == QWebEngineDownloadRequest.DownloadState.DownloadInProgress)
        total = len(self.downloads)
        self.count_label.setText(str(active) if active else str(total))

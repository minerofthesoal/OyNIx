"""
OyNIx Browser - Built-in Audio Player
Plays audio files and URLs, with mini-player in the status bar.
"""
import os
import json
from pathlib import Path

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QPushButton, QLabel,
    QSlider, QListWidget, QListWidgetItem, QFileDialog, QFrame
)
from PyQt6.QtCore import Qt, QUrl, QTimer, pyqtSignal
from PyQt6.QtMultimedia import QMediaPlayer, QAudioOutput

HAS_MULTIMEDIA = True
try:
    from PyQt6.QtMultimedia import QMediaPlayer
except ImportError:
    HAS_MULTIMEDIA = False

PLAYLIST_FILE = os.path.expanduser("~/.config/oynix/audio_playlist.json")

# JavaScript to detect all playing audio/video on a page
DETECT_MEDIA_JS = """
(function() {
    var media = [];
    var els = document.querySelectorAll('audio, video');
    for (var i = 0; i < els.length; i++) {
        var el = els[i];
        if (!el.paused && el.duration > 0) {
            var title = '';
            // Try to get title from nearby elements
            var parent = el.closest('[class*="player"], [id*="player"], [class*="media"]');
            if (parent) {
                var t = parent.querySelector('[class*="title"], [class*="name"], h1, h2, h3');
                if (t) title = t.textContent.trim().substring(0, 100);
            }
            if (!title) title = document.title;
            media.push({
                type: el.tagName.toLowerCase(),
                src: el.currentSrc || el.src || '',
                title: title,
                duration: Math.round(el.duration),
                currentTime: Math.round(el.currentTime),
                paused: el.paused,
                volume: Math.round(el.volume * 100)
            });
        }
    }
    return JSON.stringify(media);
})()
"""


class AudioPlayer(QWidget):
    """Built-in audio player with playlist support and web media detection."""
    status_changed = pyqtSignal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._playlist = []
        self._current_idx = -1
        self._player = None
        self._audio_out = None
        self._setup_player()
        self._setup_ui()
        self._load_playlist()
        self.setMinimumWidth(320)
        self.setMaximumWidth(420)

    def _setup_player(self):
        if not HAS_MULTIMEDIA:
            return
        self._player = QMediaPlayer()
        self._audio_out = QAudioOutput()
        self._player.setAudioOutput(self._audio_out)
        self._audio_out.setVolume(0.7)
        self._player.positionChanged.connect(self._on_position)
        self._player.durationChanged.connect(self._on_duration)
        self._player.mediaStatusChanged.connect(self._on_status)

    def _setup_ui(self):
        from oynix.core.theme_engine import NYX_COLORS
        c = NYX_COLORS
        self.setStyleSheet(f"""
            QWidget {{ background: {c['bg_dark']}; color: {c['text_primary']}; }}
            QPushButton {{ background: {c['bg_lighter']}; border: 1px solid {c['border']};
                border-radius: 8px; padding: 6px 12px; min-width: 32px; font-weight: bold; }}
            QPushButton:hover {{ background: {c['purple_dark']}; border-color: {c['purple_mid']}; }}
            QSlider::groove:horizontal {{ height: 4px; background: {c['bg_lighter']}; border-radius: 2px; }}
            QSlider::handle:horizontal {{ background: {c['purple_mid']}; width: 14px; height: 14px;
                margin: -5px 0; border-radius: 7px; }}
            QSlider::sub-page:horizontal {{ background: {c['purple_mid']}; border-radius: 2px; }}
        """)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(10, 10, 10, 10)
        layout.setSpacing(8)

        # Header
        header = QLabel("Audio Player")
        header.setStyleSheet(f"color: {c['purple_light']}; font-size: 14px; font-weight: bold; background: transparent;")
        layout.addWidget(header)

        # Now playing
        self.now_playing = QLabel("No track loaded")
        self.now_playing.setStyleSheet(f"color: {c['text_secondary']}; font-size: 12px; background: transparent;")
        self.now_playing.setWordWrap(True)
        layout.addWidget(self.now_playing)

        # Progress
        prog_row = QHBoxLayout()
        self.time_label = QLabel("0:00")
        self.time_label.setStyleSheet(f"color: {c['text_muted']}; font-size: 11px; background: transparent;")
        self.time_label.setFixedWidth(40)
        prog_row.addWidget(self.time_label)
        self.progress = QSlider(Qt.Orientation.Horizontal)
        self.progress.setRange(0, 0)
        self.progress.sliderMoved.connect(self._seek)
        prog_row.addWidget(self.progress)
        self.duration_label = QLabel("0:00")
        self.duration_label.setStyleSheet(f"color: {c['text_muted']}; font-size: 11px; background: transparent;")
        self.duration_label.setFixedWidth(40)
        prog_row.addWidget(self.duration_label)
        layout.addLayout(prog_row)

        # Controls
        ctrl = QHBoxLayout()
        ctrl.setSpacing(6)
        self.prev_btn = QPushButton("\u23EE")
        self.prev_btn.setFixedSize(36, 36)
        self.prev_btn.clicked.connect(self.prev_track)
        ctrl.addWidget(self.prev_btn)

        self.play_btn = QPushButton("\u25B6")
        self.play_btn.setFixedSize(44, 44)
        self.play_btn.setStyleSheet(self.play_btn.styleSheet())
        self.play_btn.clicked.connect(self.toggle_play)
        ctrl.addWidget(self.play_btn)

        self.next_btn = QPushButton("\u23ED")
        self.next_btn.setFixedSize(36, 36)
        self.next_btn.clicked.connect(self.next_track)
        ctrl.addWidget(self.next_btn)

        ctrl.addStretch()
        self.vol_slider = QSlider(Qt.Orientation.Horizontal)
        self.vol_slider.setRange(0, 100)
        self.vol_slider.setValue(70)
        self.vol_slider.setFixedWidth(80)
        self.vol_slider.valueChanged.connect(self._set_volume)
        vol_lbl = QLabel("\U0001F50A")
        vol_lbl.setStyleSheet(f"font-size: 14px; background: transparent;")
        ctrl.addWidget(vol_lbl)
        ctrl.addWidget(self.vol_slider)
        layout.addLayout(ctrl)

        # Playlist
        self.playlist_widget = QListWidget()
        self.playlist_widget.setMaximumHeight(180)
        self.playlist_widget.itemDoubleClicked.connect(self._on_item_click)
        layout.addWidget(self.playlist_widget)

        # Add/remove buttons
        btn_row = QHBoxLayout()
        add_btn = QPushButton("+ Add Files")
        add_btn.clicked.connect(self.add_files)
        btn_row.addWidget(add_btn)
        add_url_btn = QPushButton("+ Add URL")
        add_url_btn.clicked.connect(self.add_url)
        btn_row.addWidget(add_url_btn)
        clear_btn = QPushButton("Clear")
        clear_btn.clicked.connect(self.clear_playlist)
        btn_row.addWidget(clear_btn)
        layout.addLayout(btn_row)

        # Web Media Detection Section
        sep = QFrame()
        sep.setFrameShape(QFrame.Shape.HLine)
        sep.setStyleSheet(f"color: {c['border']};")
        layout.addWidget(sep)

        self._web_header = QLabel("Playing in Tabs")
        self._web_header.setStyleSheet(
            f"color: {c['purple_light']}; font-size: 12px; font-weight: bold; background: transparent;")
        layout.addWidget(self._web_header)

        self._web_media_label = QLabel("No web media playing")
        self._web_media_label.setStyleSheet(
            f"color: {c['text_secondary']}; font-size: 11px; background: transparent; padding: 4px;")
        self._web_media_label.setWordWrap(True)
        layout.addWidget(self._web_media_label)

    def add_files(self):
        paths, _ = QFileDialog.getOpenFileNames(
            self, "Add Audio Files", "",
            "Audio (*.mp3 *.wav *.ogg *.flac *.m4a *.aac *.wma);;All (*)")
        for p in paths:
            self._add_track(p, os.path.basename(p))

    def add_url(self):
        from PyQt6.QtWidgets import QInputDialog
        url, ok = QInputDialog.getText(self, "Add Audio URL", "Enter audio URL:")
        if ok and url:
            self._add_track(url, url.split('/')[-1] or url)

    def _add_track(self, path, title):
        self._playlist.append({'path': path, 'title': title})
        item = QListWidgetItem(title)
        self.playlist_widget.addItem(item)
        self._save_playlist()
        if self._current_idx < 0:
            self.play_track(len(self._playlist) - 1)

    def play_track(self, idx):
        if not self._player or idx < 0 or idx >= len(self._playlist):
            return
        self._current_idx = idx
        track = self._playlist[idx]
        path = track['path']
        if path.startswith(('http://', 'https://')):
            self._player.setSource(QUrl(path))
        else:
            self._player.setSource(QUrl.fromLocalFile(path))
        self._player.play()
        self.now_playing.setText(track['title'])
        self.play_btn.setText("\u23F8")
        self.playlist_widget.setCurrentRow(idx)
        self.status_changed.emit(f"Playing: {track['title']}")

    def toggle_play(self):
        if not self._player:
            return
        if self._player.playbackState() == QMediaPlayer.PlaybackState.PlayingState:
            self._player.pause()
            self.play_btn.setText("\u25B6")
        elif self._current_idx >= 0:
            self._player.play()
            self.play_btn.setText("\u23F8")
        elif self._playlist:
            self.play_track(0)

    def next_track(self):
        if self._playlist:
            self.play_track((self._current_idx + 1) % len(self._playlist))

    def prev_track(self):
        if self._playlist:
            self.play_track((self._current_idx - 1) % len(self._playlist))

    def _on_item_click(self, item):
        idx = self.playlist_widget.row(item)
        self.play_track(idx)

    def _on_position(self, pos):
        self.progress.setValue(pos)
        self.time_label.setText(self._fmt(pos))

    def _on_duration(self, dur):
        self.progress.setRange(0, dur)
        self.duration_label.setText(self._fmt(dur))

    def _on_status(self, status):
        if status == QMediaPlayer.MediaStatus.EndOfMedia:
            self.next_track()

    def _seek(self, pos):
        if self._player:
            self._player.setPosition(pos)

    def _set_volume(self, val):
        if self._audio_out:
            self._audio_out.setVolume(val / 100.0)

    def _fmt(self, ms):
        s = ms // 1000
        return f"{s // 60}:{s % 60:02d}"

    def clear_playlist(self):
        if self._player:
            self._player.stop()
        self._playlist.clear()
        self._current_idx = -1
        self.playlist_widget.clear()
        self.now_playing.setText("No track loaded")
        self.play_btn.setText("\u25B6")
        self._save_playlist()

    def play_url(self, url):
        """Play an audio URL (called from browser when audio link clicked)."""
        name = url.split('/')[-1] or url
        self._add_track(url, name)
        self.play_track(len(self._playlist) - 1)

    def _save_playlist(self):
        os.makedirs(os.path.dirname(PLAYLIST_FILE), exist_ok=True)
        with open(PLAYLIST_FILE, 'w') as f:
            json.dump(self._playlist, f, indent=2)

    def _load_playlist(self):
        try:
            with open(PLAYLIST_FILE) as f:
                self._playlist = json.load(f)
            for t in self._playlist:
                self.playlist_widget.addItem(QListWidgetItem(t['title']))
        except (FileNotFoundError, json.JSONDecodeError):
            pass

    # ── Web Media Detection ───────────────────────────────────────────
    def setup_web_media_detection(self, browser_window):
        """Set up periodic ning of open tabs for playing media."""
        self._browser = browser_window
        self._web_media = []  # list of detected media items
        self._media_timer = QTimer(self)
        self._media_timer.timeout.connect(self._scan_tabs_for_media)
        self._media_timer.start(3000)  # scan every 3 seconds

    def _scan_tabs_for_media(self):
        """Scan all open tabs for playing audio/video elements."""
        if not hasattr(self, '_browser'):
            return
        tab_mgr = getattr(self._browser, 'tab_manager', None)
        if not tab_mgr:
            return
        self._pending_scans = 0
        self._new_web_media = []
        views = getattr(tab_mgr, '_views', [])
        for browser in views:
            if browser and hasattr(browser, 'page'):
                self._pending_scans += 1
                page = browser.page()
                tab_url = browser.url().toString()
                tab_title = browser.page().title() or tab_url
                page.runJavaScript(
                    DETECT_MEDIA_JS,
                    lambda result, url=tab_url, title=tab_title:
                        self._on_media_scan(result, url, title))
def _on_media_scan(self, result, tab_url, tab_title):
        """Handle JS callback from a tab's media scan."""
        self._pending_scans -= 1
        if result:
            try:
                items = json.loads(result) if isinstance(result, str) else result
                for m in items:
                    m['tab_url'] = tab_url
                    m['tab_title'] = tab_title
                    self._new_web_media.append(m)
            except (json.JSONDecodeError, TypeError):
                pass
        if self._pending_scans <= 0:
            self._update_web_media_list(self._new_web_media)

    def _update_web_media_list(self, media_items):
        """Update the web media section of the player."""
        if not hasattr(self, '_web_media_label'):
            return
        old_count = len(self._web_media)
        self._web_media = media_items
        if media_items:
            lines = []
            for m in media_items:
                title = m.get('title', m.get('tab_title', 'Unknown'))
                mtype = m.get('type', 'audio')
                dur = m.get('duration', 0)
                cur = m.get('currentTime', 0)
                time_str = f"{cur//60}:{cur%60:02d}/{dur//60}:{dur%60:02d}" if dur else ""
                icon = "\U0001F3B5" if mtype == 'audio' else "\U0001F3AC"
                lines.append(f"{icon} {title[:50]}  {time_str}")
            self._web_media_label.setText('\n'.join(lines))
            self._web_media_label.show()
            self._web_header.show()
        else:
            self._web_media_label.setText("No web media playing")
            if old_count > 0:
                self._web_media_label.show()
                self._web_header.show()

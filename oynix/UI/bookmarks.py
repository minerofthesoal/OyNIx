"""
OyNIx Browser - Bookmark Manager
Folder-based bookmarks with drag-and-drop, import/export.
"""

import json
import os
from datetime import datetime

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QTreeWidget, QTreeWidgetItem,
    QPushButton, QLineEdit, QLabel, QMenu, QInputDialog, QFileDialog,
    QFrame, QHeaderView, QAbstractItemView
)
from PyQt6.QtCore import Qt, pyqtSignal, QPropertyAnimation, QEasingCurve, QSize
from PyQt6.QtGui import QAction

from oynix.UI.icons import svg_icon
from oynix.core.theme_engine import NYX_COLORS

BOOKMARKS_FILE = os.path.expanduser("~/.config/oynix/bookmarks.json")
c = NYX_COLORS


class BookmarkManager:
    """Manages bookmark data (load/save/CRUD)."""

    def __init__(self):
        self.bookmarks = self._load()

    def _load(self):
        if os.path.exists(BOOKMARKS_FILE):
            try:
                with open(BOOKMARKS_FILE, 'r') as f:
                    return json.load(f)
            except (json.JSONDecodeError, IOError):
                pass
        return {
            "folders": [
                {"name": "Quick Access", "bookmarks": []},
                {"name": "Reading List", "bookmarks": []},
            ],
            "unsorted": []
        }

    def save(self):
        os.makedirs(os.path.dirname(BOOKMARKS_FILE), exist_ok=True)
        with open(BOOKMARKS_FILE, 'w') as f:
            json.dump(self.bookmarks, f, indent=2)

    def add_bookmark(self, url, title, folder=None):
        entry = {
            "url": url,
            "title": title,
            "added": datetime.now().isoformat(),
            "visits": 0
        }
        if folder:
            for f in self.bookmarks["folders"]:
                if f["name"] == folder:
                    f["bookmarks"].append(entry)
                    self.save()
                    return True
        self.bookmarks["unsorted"].append(entry)
        self.save()
        return True

    def remove_bookmark(self, url):
        for f in self.bookmarks["folders"]:
            f["bookmarks"] = [b for b in f["bookmarks"] if b["url"] != url]
        self.bookmarks["unsorted"] = [
            b for b in self.bookmarks["unsorted"] if b["url"] != url
        ]
        self.save()

    def is_bookmarked(self, url):
        for f in self.bookmarks["folders"]:
            for b in f["bookmarks"]:
                if b["url"] == url:
                    return True
        return any(b["url"] == url for b in self.bookmarks["unsorted"])

    def get_folder_names(self):
        return [f["name"] for f in self.bookmarks["folders"]]

    def add_folder(self, name):
        self.bookmarks["folders"].append({"name": name, "bookmarks": []})
        self.save()

    def remove_folder(self, name):
        self.bookmarks["folders"] = [
            f for f in self.bookmarks["folders"] if f["name"] != name
        ]
        self.save()

    def move_bookmark(self, url, from_folder, to_folder):
        entry = None
        # Find and remove from source
        if from_folder == "__unsorted__":
            for b in self.bookmarks["unsorted"]:
                if b["url"] == url:
                    entry = b
                    break
            if entry:
                self.bookmarks["unsorted"].remove(entry)
        else:
            for f in self.bookmarks["folders"]:
                if f["name"] == from_folder:
                    for b in f["bookmarks"]:
                        if b["url"] == url:
                            entry = b
                            break
                    if entry:
                        f["bookmarks"].remove(entry)
                    break
        # Add to destination
        if entry:
            if to_folder == "__unsorted__":
                self.bookmarks["unsorted"].append(entry)
            else:
                for f in self.bookmarks["folders"]:
                    if f["name"] == to_folder:
                        f["bookmarks"].append(entry)
                        break
            self.save()

    def export_bookmarks(self, filepath):
        with open(filepath, 'w') as f:
            json.dump(self.bookmarks, f, indent=2)

    def import_bookmarks(self, filepath):
        try:
            with open(filepath, 'r') as f:
                data = json.load(f)
            if "folders" in data:
                for folder in data["folders"]:
                    existing = None
                    for ef in self.bookmarks["folders"]:
                        if ef["name"] == folder["name"]:
                            existing = ef
                            break
                    if existing:
                        urls = {b["url"] for b in existing["bookmarks"]}
                        for b in folder["bookmarks"]:
                            if b["url"] not in urls:
                                existing["bookmarks"].append(b)
                    else:
                        self.bookmarks["folders"].append(folder)
            if "unsorted" in data:
                urls = {b["url"] for b in self.bookmarks["unsorted"]}
                for b in data["unsorted"]:
                    if b["url"] not in urls:
                        self.bookmarks["unsorted"].append(b)
            self.save()
            return True
        except (json.JSONDecodeError, IOError, KeyError):
            return False


# Module-level instance
bookmark_manager = BookmarkManager()


class BookmarkPanel(QWidget):
    """Sidebar panel for browsing and managing bookmarks."""

    bookmark_clicked = pyqtSignal(str)  # URL
    close_requested = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("bookmarkPanel")
        self._setup_ui()
        self._populate()

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
                padding: 8px 12px;
            }}
        """)
        header_layout = QHBoxLayout(header)
        header_layout.setContentsMargins(12, 8, 12, 8)

        icon_label = QLabel()
        icon_label.setPixmap(svg_icon('bookmark', c['purple_light'], 18).pixmap(18, 18))
        header_layout.addWidget(icon_label)

        title = QLabel("Bookmarks")
        title.setStyleSheet(f"""
            font-size: 14px; font-weight: bold;
            color: {c['text_primary']}; background: transparent;
        """)
        header_layout.addWidget(title)
        header_layout.addStretch()

        add_folder_btn = QPushButton()
        add_folder_btn.setIcon(svg_icon('folder', c['text_secondary'], 16))
        add_folder_btn.setIconSize(QSize(16, 16))
        add_folder_btn.setFixedSize(28, 28)
        add_folder_btn.setToolTip("New Folder")
        add_folder_btn.setStyleSheet(f"""
            QPushButton {{
                background: transparent; border: none; border-radius: 6px;
            }}
            QPushButton:hover {{ background: {c['bg_lighter']}; }}
        """)
        add_folder_btn.clicked.connect(self._add_folder)
        header_layout.addWidget(add_folder_btn)

        close_btn = QPushButton()
        close_btn.setIcon(svg_icon('close', c['text_secondary'], 16))
        close_btn.setIconSize(QSize(16, 16))
        close_btn.setFixedSize(28, 28)
        close_btn.setStyleSheet(f"""
            QPushButton {{
                background: transparent; border: none; border-radius: 6px;
            }}
            QPushButton:hover {{ background: {c['error']}; }}
        """)
        close_btn.clicked.connect(self.close_requested.emit)
        header_layout.addWidget(close_btn)
        layout.addWidget(header)

        # Search filter
        self.search_input = QLineEdit()
        self.search_input.setPlaceholderText("Filter bookmarks...")
        self.search_input.setStyleSheet(f"""
            QLineEdit {{
                background: {c['bg_mid']}; color: {c['text_primary']};
                border: 1px solid {c['border']}; border-radius: 8px;
                padding: 6px 12px; margin: 8px 12px; font-size: 12px;
            }}
            QLineEdit:focus {{ border-color: {c['purple_mid']}; }}
        """)
        self.search_input.textChanged.connect(self._filter_bookmarks)
        layout.addWidget(self.search_input)

        # Tree
        self.tree = QTreeWidget()
        self.tree.setHeaderHidden(True)
        self.tree.setIndentation(20)
        self.tree.setAnimated(True)
        self.tree.setDragDropMode(QAbstractItemView.DragDropMode.InternalMove)
        self.tree.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu)
        self.tree.customContextMenuRequested.connect(self._context_menu)
        self.tree.itemDoubleClicked.connect(self._on_item_clicked)
        self.tree.setStyleSheet(f"""
            QTreeWidget {{
                background: {c['bg_dark']}; border: none;
                font-size: 12px; color: {c['text_primary']};
            }}
            QTreeWidget::item {{
                padding: 5px 8px; border-radius: 6px; margin: 1px 4px;
            }}
            QTreeWidget::item:selected {{
                background: {c['purple_dark']}; color: {c['purple_pale']};
            }}
            QTreeWidget::item:hover:!selected {{
                background: {c['bg_lighter']};
            }}
        """)
        layout.addWidget(self.tree)

        # Bottom actions
        bottom = QFrame()
        bottom.setStyleSheet(f"QFrame {{ background: {c['bg_mid']}; border-top: 1px solid {c['border']}; }}")
        bottom_layout = QHBoxLayout(bottom)
        bottom_layout.setContentsMargins(8, 6, 8, 6)

        import_btn = QPushButton("Import")
        import_btn.setIcon(svg_icon('upload', c['text_secondary'], 14))
        import_btn.setStyleSheet(f"""
            QPushButton {{
                background: transparent; color: {c['text_secondary']};
                border: 1px solid {c['border']}; border-radius: 6px;
                padding: 4px 10px; font-size: 11px;
            }}
            QPushButton:hover {{ border-color: {c['purple_mid']}; color: {c['purple_light']}; }}
        """)
        import_btn.clicked.connect(self._import_bookmarks)
        bottom_layout.addWidget(import_btn)

        export_btn = QPushButton("Export")
        export_btn.setIcon(svg_icon('downloads', c['text_secondary'], 14))
        export_btn.setStyleSheet(import_btn.styleSheet())
        export_btn.clicked.connect(self._export_bookmarks)
        bottom_layout.addWidget(export_btn)
        bottom_layout.addStretch()

        layout.addWidget(bottom)

    def _populate(self):
        self.tree.clear()
        data = bookmark_manager.bookmarks

        for folder in data["folders"]:
            folder_item = QTreeWidgetItem(self.tree, [folder["name"]])
            folder_item.setIcon(0, svg_icon('folder', c['purple_light'], 16))
            folder_item.setData(0, Qt.ItemDataRole.UserRole, {"type": "folder", "name": folder["name"]})
            folder_item.setExpanded(True)
            for bm in folder["bookmarks"]:
                child = QTreeWidgetItem(folder_item, [bm["title"] or bm["url"]])
                child.setIcon(0, svg_icon('globe', c['text_secondary'], 14))
                child.setData(0, Qt.ItemDataRole.UserRole, {"type": "bookmark", **bm})
                child.setToolTip(0, bm["url"])

        if data["unsorted"]:
            unsorted = QTreeWidgetItem(self.tree, ["Unsorted"])
            unsorted.setIcon(0, svg_icon('folder_open', c['text_muted'], 16))
            unsorted.setData(0, Qt.ItemDataRole.UserRole, {"type": "folder", "name": "__unsorted__"})
            unsorted.setExpanded(True)
            for bm in data["unsorted"]:
                child = QTreeWidgetItem(unsorted, [bm["title"] or bm["url"]])
                child.setIcon(0, svg_icon('globe', c['text_secondary'], 14))
                child.setData(0, Qt.ItemDataRole.UserRole, {"type": "bookmark", **bm})
                child.setToolTip(0, bm["url"])

    def _on_item_clicked(self, item, column):
        data = item.data(0, Qt.ItemDataRole.UserRole)
        if data and data.get("type") == "bookmark":
            self.bookmark_clicked.emit(data["url"])

    def _add_folder(self):
        name, ok = QInputDialog.getText(self, "New Folder", "Folder name:")
        if ok and name.strip():
            bookmark_manager.add_folder(name.strip())
            self._populate()

    def _filter_bookmarks(self, text):
        text = text.lower()
        for i in range(self.tree.topLevelItemCount()):
            folder = self.tree.topLevelItem(i)
            folder_visible = False
            for j in range(folder.childCount()):
                child = folder.child(j)
                data = child.data(0, Qt.ItemDataRole.UserRole) or {}
                visible = (text in child.text(0).lower() or
                           text in data.get("url", "").lower())
                child.setHidden(not visible)
                if visible:
                    folder_visible = True
            folder.setHidden(not folder_visible and bool(text))

    def _context_menu(self, pos):
        item = self.tree.itemAt(pos)
        if not item:
            return
        data = item.data(0, Qt.ItemDataRole.UserRole) or {}
        menu = QMenu(self)
        menu.setStyleSheet(f"""
            QMenu {{
                background: {c['bg_mid']}; color: {c['text_primary']};
                border: 1px solid {c['border']}; border-radius: 8px; padding: 4px;
            }}
            QMenu::item {{ padding: 6px 20px; border-radius: 4px; }}
            QMenu::item:selected {{ background: {c['purple_dark']}; }}
        """)

        if data.get("type") == "bookmark":
            open_act = menu.addAction("Open")
            open_act.triggered.connect(lambda: self.bookmark_clicked.emit(data["url"]))
            copy_act = menu.addAction("Copy URL")
            copy_act.triggered.connect(lambda: (
                __import__('PyQt6.QtWidgets', fromlist=['QApplication'])
                .QApplication.clipboard().setText(data["url"])
            ))
            menu.addSeparator()
            # Move to folder submenu
            move_menu = menu.addMenu("Move to...")
            for fname in bookmark_manager.get_folder_names():
                act = move_menu.addAction(fname)
                act.triggered.connect(
                    lambda checked, fn=fname: self._move_item(item, fn))
            menu.addSeparator()
            delete_act = menu.addAction("Delete")
            delete_act.triggered.connect(lambda: self._delete_bookmark(data["url"]))

        elif data.get("type") == "folder" and data.get("name") != "__unsorted__":
            rename_act = menu.addAction("Rename")
            rename_act.triggered.connect(lambda: self._rename_folder(item))
            delete_act = menu.addAction("Delete Folder")
            delete_act.triggered.connect(
                lambda: self._delete_folder(data["name"]))

        menu.exec(self.tree.viewport().mapToGlobal(pos))

    def _move_item(self, item, to_folder):
        data = item.data(0, Qt.ItemDataRole.UserRole)
        if not data:
            return
        parent = item.parent()
        from_folder = "__unsorted__"
        if parent:
            pdata = parent.data(0, Qt.ItemDataRole.UserRole) or {}
            from_folder = pdata.get("name", "__unsorted__")
        bookmark_manager.move_bookmark(data["url"], from_folder, to_folder)
        self._populate()

    def _delete_bookmark(self, url):
        bookmark_manager.remove_bookmark(url)
        self._populate()

    def _rename_folder(self, item):
        data = item.data(0, Qt.ItemDataRole.UserRole) or {}
        old_name = data.get("name", "")
        new_name, ok = QInputDialog.getText(self, "Rename Folder", "New name:", text=old_name)
        if ok and new_name.strip():
            for f in bookmark_manager.bookmarks["folders"]:
                if f["name"] == old_name:
                    f["name"] = new_name.strip()
                    break
            bookmark_manager.save()
            self._populate()

    def _delete_folder(self, name):
        bookmark_manager.remove_folder(name)
        self._populate()

    def _import_bookmarks(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "Import Bookmarks", "", "JSON Files (*.json)")
        if path:
            bookmark_manager.import_bookmarks(path)
            self._populate()

    def _export_bookmarks(self):
        path, _ = QFileDialog.getSaveFileName(
            self, "Export Bookmarks", "oynix_bookmarks.json", "JSON Files (*.json)")
        if path:
            bookmark_manager.export_bookmarks(path)

    def refresh(self):
        self._populate()

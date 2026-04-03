"""
OyNIx Browser - Tree Tab Manager v2
Modern tree-style tab sidebar with smooth animations,
drag-drop reordering, context menus, and tab search.
"""
from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QTreeWidget, QTreeWidgetItem,
    QPushButton, QLabel, QLineEdit, QSplitter, QTabWidget, QTabBar,
    QStackedWidget, QMenu, QToolTip
)
from PyQt6.QtCore import (
    Qt, pyqtSignal, QPropertyAnimation, QEasingCurve, QSize, QTimer, QPoint
)
from PyQt6.QtGui import QIcon, QCursor, QPixmap

from oynix.core.theme_engine import NYX_COLORS
try:
    from oynix.UI.icons import svg_icon
except ImportError:
    svg_icon = None


class TreeTabItem(QTreeWidgetItem):
    """A tab node in the tree."""
    def __init__(self, browser_view, parent_item=None):
        super().__init__(parent_item or None)
        self.browser_view = browser_view
        self.tab_url = ""
        self.tab_title = "New Tab"
        self.setText(0, self.tab_title)

    def update_title(self, title):
        self.tab_title = title or "New Tab"
        display = self.tab_title
        if len(display) > 35:
            display = display[:33] + "..."
        self.setText(0, display)
        self.setToolTip(0, self.tab_title)

    def update_url(self, url):
        self.tab_url = url


class TreeTabSidebar(QWidget):
    """Sidebar with tree-style tabs."""
    tab_selected = pyqtSignal(object)
    new_tab_requested = pyqtSignal()
    tab_close_requested = pyqtSignal(object)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumWidth(200)
        self.setMaximumWidth(340)
        self._items = {}  # browser_view -> TreeTabItem
        self._setup_ui()

    def _setup_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(6, 8, 6, 6)
        layout.setSpacing(6)

        # Header
        header = QHBoxLayout()
        lbl = QLabel("TABS")
        lbl.setStyleSheet(f"color: {NYX_COLORS['text_muted']}; font-size: 11px; "
                          f"font-weight: 700; letter-spacing: 2px; background: transparent;")
        header.addWidget(lbl)
        header.addStretch()
        self.count_label = QLabel("0")
        self.count_label.setStyleSheet(f"color: {NYX_COLORS['purple_mid']}; font-size: 11px; "
                                        f"font-weight: bold; background: transparent;")
        header.addWidget(self.count_label)
        new_btn = QPushButton()
        if svg_icon:
            new_btn.setIcon(svg_icon('new_tab', NYX_COLORS['purple_light'], 16))
        else:
            new_btn.setText("+")
        new_btn.setObjectName("navBtn")
        new_btn.setFixedSize(28, 28)
        new_btn.setToolTip("New Tab")
        new_btn.clicked.connect(self.new_tab_requested.emit)
        header.addWidget(new_btn)
        layout.addLayout(header)

        # Search / filter
        self.filter_input = QLineEdit()
        self.filter_input.setPlaceholderText("Filter tabs...")
        self.filter_input.setFixedHeight(30)
        self.filter_input.setStyleSheet(f"""
            QLineEdit {{ border-radius: 15px; padding: 4px 14px; font-size: 12px;
                background: {NYX_COLORS['bg_mid']}; border: 1px solid {NYX_COLORS['border']};
                color: {NYX_COLORS['text_primary']}; }}
            QLineEdit:focus {{ border-color: {NYX_COLORS['purple_mid']}; }}
        """)
        self.filter_input.textChanged.connect(self._filter_tabs)
        layout.addWidget(self.filter_input)

        # Tree
        self.tree = QTreeWidget()
        self.tree.setHeaderHidden(True)
        self.tree.setRootIsDecorated(True)
        self.tree.setAnimated(True)
        self.tree.setIndentation(16)
        self.tree.setDragDropMode(QTreeWidget.DragDropMode.InternalMove)
        self.tree.itemClicked.connect(self._on_item_clicked)
        self.tree.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu)
        self.tree.customContextMenuRequested.connect(self._show_context_menu)
        layout.addWidget(self.tree, 1)

    def add_tab(self, browser_view, title="New Tab", parent_view=None):
        parent_item = self._items.get(parent_view)
        item = TreeTabItem(browser_view, parent_item)
        if parent_item:
            parent_item.addChild(item)
            parent_item.setExpanded(True)
        else:
            self.tree.addTopLevelItem(item)
        item.update_title(title)
        self._items[browser_view] = item
        self.tree.setCurrentItem(item)
        self._update_count()

    def remove_tab(self, browser_view):
        item = self._items.pop(browser_view, None)
        if not item:
            return
        # Re-parent children to grandparent
        parent = item.parent()
        children = [item.child(i) for i in range(item.childCount())]
        for child in children:
            item.removeChild(child)
            if parent:
                parent.addChild(child)
            else:
                self.tree.addTopLevelItem(child)
        # Remove item
        if parent:
            parent.removeChild(item)
        else:
            idx = self.tree.indexOfTopLevelItem(item)
            if idx >= 0:
                self.tree.takeTopLevelItem(idx)
        self._update_count()

    def update_tab_title(self, browser_view, title):
        item = self._items.get(browser_view)
        if item:
            item.update_title(title)

    def update_tab_url(self, browser_view, url):
        item = self._items.get(browser_view)
        if item:
            item.update_url(url)

    def select_tab(self, browser_view):
        item = self._items.get(browser_view)
        if item:
            self.tree.setCurrentItem(item)

    def _on_item_clicked(self, item, col):
        if isinstance(item, TreeTabItem):
            self.tab_selected.emit(item.browser_view)

    def _filter_tabs(self, text):
        text = text.lower()
        for view, item in self._items.items():
            match = text in item.tab_title.lower() or text in item.tab_url.lower()
            item.setHidden(not match if text else False)

    def _show_context_menu(self, pos):
        item = self.tree.itemAt(pos)
        if not isinstance(item, TreeTabItem):
            return
        menu = QMenu(self)
        menu.addAction("Close Tab", lambda: self.tab_close_requested.emit(item.browser_view))
        menu.addAction("Close Other Tabs", lambda: self._close_others(item))
        menu.addSeparator()
        menu.addAction("Duplicate Tab", lambda: self.tab_selected.emit(item.browser_view))
        menu.exec(self.tree.mapToGlobal(pos))

    def _close_others(self, keep_item):
        views_to_close = [v for v, it in self._items.items() if it != keep_item]
        for v in views_to_close:
            self.tab_close_requested.emit(v)

    def _update_count(self):
        self.count_label.setText(str(len(self._items)))

    def get_all_views(self):
        return list(self._items.keys())


class TabManager(QWidget):
    """Manages both tree-style and normal tab modes."""
    current_view_changed = pyqtSignal(object)
    new_tab_requested = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self._views = []
        self._current_view = None
        self._tree_mode = True
        self._setup_ui()

    def _setup_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        self.splitter = QSplitter(Qt.Orientation.Horizontal)
        layout.addWidget(self.splitter)

        # Tree sidebar
        self.tree_sidebar = TreeTabSidebar()
        self.tree_sidebar.tab_selected.connect(self._on_tab_selected)
        self.tree_sidebar.new_tab_requested.connect(self.new_tab_requested.emit)
        self.tree_sidebar.tab_close_requested.connect(self.close_tab)
        self.splitter.addWidget(self.tree_sidebar)

        # Content area
        self.content_stack = QStackedWidget()
        self.splitter.addWidget(self.content_stack)

        # Normal tab bar (hidden in tree mode)
        self.tab_bar = QTabBar()
        self.tab_bar.setTabsClosable(True)
        self.tab_bar.setMovable(True)
        self.tab_bar.setExpanding(False)
        self.tab_bar.tabCloseRequested.connect(self._on_normal_tab_close)
        self.tab_bar.currentChanged.connect(self._on_normal_tab_changed)
        self.tab_bar.hide()

        self.splitter.setSizes([220, 1180])

    def add_tab(self, browser_view, title="New Tab", parent_view=None):
        self._views.append(browser_view)
        self.content_stack.addWidget(browser_view)
        self.content_stack.setCurrentWidget(browser_view)
        self._current_view = browser_view
        self.tree_sidebar.add_tab(browser_view, title, parent_view)
        if not self._tree_mode:
            self.tab_bar.addTab(title[:30])
            self.tab_bar.setCurrentIndex(self.tab_bar.count() - 1)
        self.current_view_changed.emit(browser_view)

    def close_tab(self, browser_view):
        if browser_view not in self._views:
            return
        idx = self._views.index(browser_view)
        self._views.remove(browser_view)
        self.content_stack.removeWidget(browser_view)
        self.tree_sidebar.remove_tab(browser_view)
        if not self._tree_mode and idx < self.tab_bar.count():
            self.tab_bar.removeTab(idx)
        browser_view.deleteLater()
        if self._views:
            new_current = self._views[min(idx, len(self._views) - 1)]
            self.content_stack.setCurrentWidget(new_current)
            self._current_view = new_current
            self.current_view_changed.emit(new_current)

    def update_tab_title(self, browser_view, title):
        self.tree_sidebar.update_tab_title(browser_view, title)
        if not self._tree_mode and browser_view in self._views:
            idx = self._views.index(browser_view)
            if idx < self.tab_bar.count():
                self.tab_bar.setTabText(idx, title[:30] if title else "New Tab")

    def update_tab_url(self, browser_view, url):
        self.tree_sidebar.update_tab_url(browser_view, url)

    def set_tree_mode(self, enabled):
        self._tree_mode = enabled
        if enabled:
            self.tree_sidebar.show()
            self.tab_bar.hide()
        else:
            self.tree_sidebar.hide()
            self.tab_bar.show()
            # Rebuild tab bar
            self.tab_bar.blockSignals(True)
            while self.tab_bar.count():
                self.tab_bar.removeTab(0)
            for v in self._views:
                item = self.tree_sidebar._items.get(v)
                title = item.tab_title if item else "Tab"
                self.tab_bar.addTab(title[:30])
            if self._current_view in self._views:
                self.tab_bar.setCurrentIndex(self._views.index(self._current_view))
            self.tab_bar.blockSignals(False)

    def get_current_view(self):
        return self._current_view

    def _on_tab_selected(self, browser_view):
        if browser_view in self._views:
            self.content_stack.setCurrentWidget(browser_view)
            self._current_view = browser_view
            self.current_view_changed.emit(browser_view)

    def _on_normal_tab_close(self, index):
        if 0 <= index < len(self._views):
            self.close_tab(self._views[index])

    def _on_normal_tab_changed(self, index):
        if 0 <= index < len(self._views):
            view = self._views[index]
            self.content_stack.setCurrentWidget(view)
            self._current_view = view
            self.current_view_changed.emit(view)

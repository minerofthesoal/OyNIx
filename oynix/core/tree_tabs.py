"""
OyNIx Browser - Tree-Style Tab Manager
Provides both tree-graph sidebar tabs and normal horizontal tabs.

Features:
- Tree hierarchy: child tabs grouped under parent
- Drag-and-drop reordering
- Collapsible groups
- Tab previews on hover
- Toggle between tree and normal mode
- Tab search/filter
"""

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QTreeWidget, QTreeWidgetItem,
    QPushButton, QLabel, QLineEdit, QMenu, QSplitter, QTabWidget,
    QStackedWidget, QSizePolicy
)
from PyQt6.QtCore import Qt, pyqtSignal, QSize, QTimer
from PyQt6.QtGui import QIcon, QAction, QFont
from PyQt6.QtWebEngineWidgets import QWebEngineView


class TreeTabItem(QTreeWidgetItem):
    """A single tab item in the tree view."""

    def __init__(self, browser_view, title="New Tab", parent_item=None):
        super().__init__()
        self.browser_view = browser_view
        self.tab_title = title
        self.tab_url = ""
        self.is_loading = False
        self.is_pinned = False

        self.setText(0, title)
        self.setToolTip(0, title)

    def update_title(self, title):
        """Update the displayed title."""
        self.tab_title = title
        display = title if len(title) <= 35 else title[:32] + "..."
        self.setText(0, display)
        self.setToolTip(0, title)

    def update_url(self, url):
        """Update stored URL."""
        self.tab_url = url
        self.setToolTip(0, f"{self.tab_title}\n{url}")


class TreeTabSidebar(QWidget):
    """
    Sidebar widget showing tabs in a tree-graph layout.
    Tabs opened from another tab become children of that tab.
    """

    tab_selected = pyqtSignal(object)        # Emits the QWebEngineView
    tab_close_requested = pyqtSignal(object)  # Emits the QWebEngineView
    new_tab_requested = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.tab_items = {}  # browser_view -> TreeTabItem
        self.setup_ui()

    def setup_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        # Header
        header = QWidget()
        header.setFixedHeight(42)
        header_layout = QHBoxLayout(header)
        header_layout.setContentsMargins(10, 4, 6, 4)

        title_label = QLabel("TABS")
        title_label.setStyleSheet(
            "font-weight: bold; font-size: 11px; letter-spacing: 2px; "
            "color: #706880; background: transparent;"
        )
        header_layout.addWidget(title_label)
        header_layout.addStretch()

        new_btn = QPushButton("+")
        new_btn.setFixedSize(28, 28)
        new_btn.setObjectName("navButton")
        new_btn.setToolTip("New Tab")
        new_btn.clicked.connect(self.new_tab_requested.emit)
        header_layout.addWidget(new_btn)

        layout.addWidget(header)

        # Search / filter bar
        self.filter_input = QLineEdit()
        self.filter_input.setPlaceholderText("Filter tabs...")
        self.filter_input.setFixedHeight(30)
        self.filter_input.setContentsMargins(6, 0, 6, 0)
        self.filter_input.textChanged.connect(self._filter_tabs)
        layout.addWidget(self.filter_input)

        # Tree widget
        self.tree = QTreeWidget()
        self.tree.setHeaderHidden(True)
        self.tree.setRootIsDecorated(True)
        self.tree.setAnimated(True)
        self.tree.setIndentation(16)
        self.tree.setDragDropMode(QTreeWidget.DragDropMode.InternalMove)
        self.tree.setSelectionMode(QTreeWidget.SelectionMode.SingleSelection)
        self.tree.setExpandsOnDoubleClick(False)
        self.tree.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu)
        self.tree.customContextMenuRequested.connect(self._show_context_menu)
        self.tree.currentItemChanged.connect(self._on_item_selected)
        layout.addWidget(self.tree)

        # Bottom bar
        bottom = QWidget()
        bottom.setFixedHeight(36)
        bottom_layout = QHBoxLayout(bottom)
        bottom_layout.setContentsMargins(10, 4, 10, 4)

        self.tab_count_label = QLabel("0 tabs")
        self.tab_count_label.setStyleSheet(
            "font-size: 11px; color: #706880; background: transparent;"
        )
        bottom_layout.addWidget(self.tab_count_label)
        bottom_layout.addStretch()

        layout.addWidget(bottom)

        self.setMinimumWidth(200)
        self.setMaximumWidth(350)

    def add_tab(self, browser_view, title="New Tab", parent_view=None):
        """Add a new tab to the tree."""
        item = TreeTabItem(browser_view, title)
        self.tab_items[browser_view] = item

        if parent_view and parent_view in self.tab_items:
            parent_item = self.tab_items[parent_view]
            parent_item.addChild(item)
            parent_item.setExpanded(True)
        else:
            self.tree.addTopLevelItem(item)

        self.tree.setCurrentItem(item)
        self._update_count()
        return item

    def remove_tab(self, browser_view):
        """Remove a tab from the tree. Re-parents children to grandparent."""
        if browser_view not in self.tab_items:
            return

        item = self.tab_items[browser_view]
        parent = item.parent()

        # Re-parent children
        children = []
        for i in range(item.childCount()):
            children.append(item.child(i))

        for child in children:
            item.removeChild(child)
            if parent:
                parent.addChild(child)
            else:
                self.tree.addTopLevelItem(child)

        # Remove the item
        if parent:
            parent.removeChild(item)
        else:
            idx = self.tree.indexOfTopLevelItem(item)
            if idx >= 0:
                self.tree.takeTopLevelItem(idx)

        del self.tab_items[browser_view]
        self._update_count()

    def update_tab_title(self, browser_view, title):
        """Update a tab's title."""
        if browser_view in self.tab_items:
            self.tab_items[browser_view].update_title(title)

    def update_tab_url(self, browser_view, url):
        """Update a tab's URL."""
        if browser_view in self.tab_items:
            self.tab_items[browser_view].update_url(url)

    def select_tab(self, browser_view):
        """Select a tab in the tree."""
        if browser_view in self.tab_items:
            self.tree.setCurrentItem(self.tab_items[browser_view])

    def _on_item_selected(self, current, previous):
        """Handle tree item selection."""
        if current and isinstance(current, TreeTabItem):
            self.tab_selected.emit(current.browser_view)

    def _filter_tabs(self, text):
        """Filter visible tabs by text."""
        text = text.lower()
        for browser_view, item in self.tab_items.items():
            match = (not text or
                     text in item.tab_title.lower() or
                     text in item.tab_url.lower())
            item.setHidden(not match)

    def _show_context_menu(self, position):
        """Show right-click context menu for tabs."""
        item = self.tree.itemAt(position)
        if not item or not isinstance(item, TreeTabItem):
            return

        menu = QMenu(self)

        close_action = menu.addAction("Close Tab")
        close_action.triggered.connect(
            lambda: self.tab_close_requested.emit(item.browser_view))

        close_others = menu.addAction("Close Other Tabs")
        close_others.triggered.connect(
            lambda: self._close_others(item.browser_view))

        menu.addSeparator()

        close_children = menu.addAction("Close Child Tabs")
        close_children.triggered.connect(
            lambda: self._close_children(item))

        menu.addSeparator()

        reload_action = menu.addAction("Reload")
        reload_action.triggered.connect(
            lambda: item.browser_view.reload())

        duplicate = menu.addAction("Duplicate Tab")
        duplicate.triggered.connect(
            lambda: self._duplicate_tab(item))

        menu.exec(self.tree.viewport().mapToGlobal(position))

    def _close_others(self, keep_view):
        """Close all tabs except the specified one."""
        views_to_close = [v for v in self.tab_items if v is not keep_view]
        for v in views_to_close:
            self.tab_close_requested.emit(v)

    def _close_children(self, item):
        """Close all child tabs of an item."""
        children = []
        for i in range(item.childCount()):
            child = item.child(i)
            if isinstance(child, TreeTabItem):
                children.append(child.browser_view)
        for v in children:
            self.tab_close_requested.emit(v)

    def _duplicate_tab(self, item):
        """Request duplication of a tab."""
        self.new_tab_requested.emit()

    def _update_count(self):
        """Update the tab count label."""
        count = len(self.tab_items)
        self.tab_count_label.setText(f"{count} tab{'s' if count != 1 else ''}")

    def get_all_views(self):
        """Get all browser views in order."""
        views = []
        for i in range(self.tree.topLevelItemCount()):
            self._collect_views(self.tree.topLevelItem(i), views)
        return views

    def _collect_views(self, item, views):
        """Recursively collect views from tree."""
        if isinstance(item, TreeTabItem):
            views.append(item.browser_view)
        for i in range(item.childCount()):
            self._collect_views(item.child(i), views)


class TabManager(QWidget):
    """
    Manages both tree-style and normal tab modes.
    Can toggle between them seamlessly.
    """

    current_view_changed = pyqtSignal(object)  # QWebEngineView
    new_tab_requested = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)

        self.views = []           # All QWebEngineView instances
        self.current_view = None
        self.use_tree_tabs = True  # Default to tree tabs

        self.setup_ui()

    def setup_ui(self):
        self.main_layout = QHBoxLayout(self)
        self.main_layout.setContentsMargins(0, 0, 0, 0)
        self.main_layout.setSpacing(0)

        # Splitter for tree sidebar + content
        self.splitter = QSplitter(Qt.Orientation.Horizontal)

        # Tree tab sidebar
        self.tree_sidebar = TreeTabSidebar()
        self.tree_sidebar.tab_selected.connect(self._on_tab_selected)
        self.tree_sidebar.tab_close_requested.connect(self.close_tab)
        self.tree_sidebar.new_tab_requested.connect(
            self.new_tab_requested.emit)
        self.splitter.addWidget(self.tree_sidebar)

        # Content area (stacked widget to show one tab at a time)
        self.content_stack = QStackedWidget()
        self.splitter.addWidget(self.content_stack)

        # Set splitter proportions
        self.splitter.setSizes([240, 1160])
        self.splitter.setCollapsible(0, False)
        self.splitter.setCollapsible(1, False)

        # Normal tab bar (hidden by default)
        self.normal_tabs = QTabWidget()
        self.normal_tabs.setTabsClosable(True)
        self.normal_tabs.tabCloseRequested.connect(self._on_normal_tab_close)
        self.normal_tabs.currentChanged.connect(self._on_normal_tab_changed)
        self.normal_tabs.hide()

        self.main_layout.addWidget(self.splitter)
        self.main_layout.addWidget(self.normal_tabs)

    def add_tab(self, browser_view, title="New Tab", parent_view=None):
        """Add a new tab."""
        self.views.append(browser_view)

        if self.use_tree_tabs:
            self.content_stack.addWidget(browser_view)
            self.tree_sidebar.add_tab(browser_view, title, parent_view)
            self.content_stack.setCurrentWidget(browser_view)
        else:
            idx = self.normal_tabs.addTab(browser_view, title)
            self.normal_tabs.setCurrentIndex(idx)

        self.current_view = browser_view
        self.current_view_changed.emit(browser_view)

    def close_tab(self, browser_view):
        """Close a tab."""
        if browser_view not in self.views:
            return
        if len(self.views) <= 1:
            return  # Don't close last tab

        self.views.remove(browser_view)

        if self.use_tree_tabs:
            self.tree_sidebar.remove_tab(browser_view)
            self.content_stack.removeWidget(browser_view)
        else:
            idx = self.normal_tabs.indexOf(browser_view)
            if idx >= 0:
                self.normal_tabs.removeTab(idx)

        browser_view.deleteLater()

        # Select another tab
        if self.views:
            self._on_tab_selected(self.views[-1])

    def update_tab_title(self, browser_view, title):
        """Update a tab's displayed title."""
        if self.use_tree_tabs:
            self.tree_sidebar.update_tab_title(browser_view, title)
        else:
            idx = self.normal_tabs.indexOf(browser_view)
            if idx >= 0:
                display = title if len(title) <= 25 else title[:22] + "..."
                self.normal_tabs.setTabText(idx, display)

    def update_tab_url(self, browser_view, url):
        """Update a tab's URL in the tree."""
        if self.use_tree_tabs:
            self.tree_sidebar.update_tab_url(browser_view, url)

    def set_tree_mode(self, enabled):
        """Toggle between tree and normal tab modes."""
        if enabled == self.use_tree_tabs:
            return

        self.use_tree_tabs = enabled

        if enabled:
            # Switch to tree tabs
            self.normal_tabs.hide()
            self.splitter.show()

            # Move all views from normal tabs to tree
            for view in self.views:
                idx = self.normal_tabs.indexOf(view)
                if idx >= 0:
                    self.normal_tabs.removeTab(idx)
                self.content_stack.addWidget(view)
                title = view.page().title() or "New Tab"
                self.tree_sidebar.add_tab(view, title)

            if self.current_view:
                self.content_stack.setCurrentWidget(self.current_view)
                self.tree_sidebar.select_tab(self.current_view)
        else:
            # Switch to normal tabs
            self.splitter.hide()
            self.normal_tabs.show()

            # Move all views from tree to normal tabs
            for view in self.views:
                self.tree_sidebar.remove_tab(view)
                self.content_stack.removeWidget(view)
                title = view.page().title() or "New Tab"
                self.normal_tabs.addTab(view, title)

            if self.current_view:
                idx = self.normal_tabs.indexOf(self.current_view)
                if idx >= 0:
                    self.normal_tabs.setCurrentIndex(idx)

    def get_current_view(self):
        """Get the currently active browser view."""
        return self.current_view

    def toggle_mode(self):
        """Toggle between tree tabs and normal tabs."""
        self.set_tree_tabs_enabled(not self.use_tree_tabs)

    def tab_count(self):
        """Return the total number of open tabs."""
        return len(self.views)

    def _on_tab_selected(self, browser_view):
        """Handle tab selection from tree sidebar."""
        if browser_view and browser_view in self.views:
            self.current_view = browser_view
            self.content_stack.setCurrentWidget(browser_view)
            self.current_view_changed.emit(browser_view)

    def _on_normal_tab_close(self, index):
        """Handle close from normal tab bar."""
        view = self.normal_tabs.widget(index)
        if view:
            self.close_tab(view)

    def _on_normal_tab_changed(self, index):
        """Handle tab change in normal mode."""
        view = self.normal_tabs.widget(index)
        if view and view in self.views:
            self.current_view = view
            self.current_view_changed.emit(view)

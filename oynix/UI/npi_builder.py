"""
OyNIx Browser - NPI Extension Builder
Visual node-based editor and text-based editor for creating OyNIx .npi extensions.
Includes a compiler that packages the extension into a .npi zip file.
"""
import os
import json

from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QTabWidget, QWidget,
    QLabel, QPushButton, QLineEdit, QTextEdit, QComboBox,
    QListWidget, QListWidgetItem, QFileDialog, QMessageBox,
    QGroupBox, QFormLayout, QCheckBox, QSplitter, QScrollArea,
    QFrame, QGraphicsView, QGraphicsScene, QGraphicsRectItem,
    QGraphicsTextItem, QGraphicsLineItem, QGraphicsEllipseItem,
    QMenu
)
from PyQt6.QtCore import Qt, QRectF, QPointF, pyqtSignal
from PyQt6.QtGui import (
    QColor, QPen, QBrush, QFont, QPainter, QAction
)

from oynix.core.extensions import compile_npi


# ── Node Graph Types ──────────────────────────────────────────────────

class NodePort(QGraphicsEllipseItem):
    """A connection port on a node."""
    def __init__(self, parent_node, port_type, label, is_output=False):
        size = 10
        super().__init__(-size/2, -size/2, size, size, parent_node)
        self.port_type = port_type
        self.label = label
        self.is_output = is_output
        self.parent_node = parent_node
        self.connections = []
        self.setBrush(QBrush(QColor("#7B4FBF")))
        self.setPen(QPen(QColor("#9B6FDF"), 1))
        self.setZValue(2)

    def center_scene_pos(self):
        return self.scenePos() + QPointF(0, 0)


class NodeItem(QGraphicsRectItem):
    """A draggable node in the visual editor."""

    def __init__(self, node_type, title, x=0, y=0):
        super().__init__(0, 0, 180, 80)
        self.node_type = node_type
        self.title = title
        self.setPos(x, y)
        self.setFlag(QGraphicsRectItem.GraphicsItemFlag.ItemIsMovable, True)
        self.setFlag(QGraphicsRectItem.GraphicsItemFlag.ItemIsSelectable, True)
        self.setBrush(QBrush(QColor("#1e1e2a")))
        self.setPen(QPen(QColor("#7B4FBF"), 2))
        self.setZValue(1)

        # Title bar
        self._title_bg = QGraphicsRectItem(0, 0, 180, 24, self)
        self._title_bg.setBrush(QBrush(QColor("#4a2d7a")))
        self._title_bg.setPen(QPen(Qt.PenStyle.NoPen))

        self._title_text = QGraphicsTextItem(title, self)
        self._title_text.setDefaultTextColor(QColor("#E8E0F0"))
        font = QFont("Segoe UI", 9, QFont.Weight.Bold)
        self._title_text.setFont(font)
        self._title_text.setPos(6, 2)

        # Type label
        self._type_text = QGraphicsTextItem(node_type, self)
        self._type_text.setDefaultTextColor(QColor("#A8A0B8"))
        self._type_text.setFont(QFont("Segoe UI", 8))
        self._type_text.setPos(6, 30)

        # Ports
        self.input_ports = []
        self.output_ports = []
        self.data = {}  # user-configurable data for this node

    def add_input(self, label, port_type="any"):
        port = NodePort(self, port_type, label, is_output=False)
        idx = len(self.input_ports)
        port.setPos(-5, 40 + idx * 18)
        self.input_ports.append(port)
        # Resize node if needed
        needed_h = 50 + max(len(self.input_ports), len(self.output_ports)) * 18
        self.setRect(0, 0, 180, max(80, needed_h))
        self._title_bg.setRect(0, 0, 180, 24)
        return port

    def add_output(self, label, port_type="any"):
        port = NodePort(self, port_type, label, is_output=True)
        idx = len(self.output_ports)
        port.setPos(175, 40 + idx * 18)
        self.output_ports.append(port)
        needed_h = 50 + max(len(self.input_ports), len(self.output_ports)) * 18
        self.setRect(0, 0, 180, max(80, needed_h))
        self._title_bg.setRect(0, 0, 180, 24)
        return port


class ConnectionLine(QGraphicsLineItem):
    """A line connecting two node ports."""
    def __init__(self, start_port, end_port):
        super().__init__()
        self.start_port = start_port
        self.end_port = end_port
        self.setPen(QPen(QColor("#9B6FDF"), 2))
        self.setZValue(0)
        self.update_position()

    def update_position(self):
        p1 = self.start_port.center_scene_pos()
        p2 = self.end_port.center_scene_pos()
        self.setLine(p1.x(), p1.y(), p2.x(), p2.y())


# ── Node Templates ────────────────────────────────────────────────────

NODE_TEMPLATES = {
    'Manifest': {
        'outputs': [('extension', 'manifest')],
        'inputs': [],
        'desc': 'Extension metadata (name, version, permissions)',
    },
    'Content Script': {
        'outputs': [('script', 'js')],
        'inputs': [('manifest', 'manifest')],
        'desc': 'JavaScript injected into matching pages',
    },
    'Content CSS': {
        'outputs': [('style', 'css')],
        'inputs': [('manifest', 'manifest')],
        'desc': 'CSS injected into matching pages',
    },
    'Popup HTML': {
        'outputs': [('popup', 'html')],
        'inputs': [('manifest', 'manifest')],
        'desc': 'Toolbar popup window HTML',
    },
    'Background Script': {
        'outputs': [('bg', 'js')],
        'inputs': [('manifest', 'manifest')],
        'desc': 'Background script that runs on load',
    },
    'URL Match Pattern': {
        'outputs': [('pattern', 'match')],
        'inputs': [],
        'desc': 'URL patterns for content scripts',
    },
    'Data File': {
        'outputs': [('data', 'file')],
        'inputs': [('manifest', 'manifest')],
        'desc': 'JSON/text data file bundled with extension',
    },
    'Icon': {
        'outputs': [('icon', 'image')],
        'inputs': [('manifest', 'manifest')],
        'desc': 'Extension icon (PNG)',
    },
}


# ── Visual Node Editor ────────────────────────────────────────────────

class NodeEditorView(QGraphicsView):
    """The visual node editor canvas."""

    def __init__(self):
        super().__init__()
        self.scene = QGraphicsScene()
        self.scene.setSceneRect(-2000, -2000, 4000, 4000)
        self.setScene(self.scene)
        self.setRenderHint(QPainter.RenderHint.Antialiasing)
        self.setDragMode(QGraphicsView.DragMode.RubberBandDrag)
        self.setBackgroundBrush(QBrush(QColor("#0e0e16")))

        self.nodes = []
        self.connections = []
        self._connecting_from = None

    def contextMenuEvent(self, event):
        menu = QMenu()
        menu.setStyleSheet(
            "QMenu { background: #1e1e2a; color: #E8E0F0; border: 1px solid #3a3a4a; }"
            "QMenu::item:selected { background: #4a2d7a; }")
        for name in NODE_TEMPLATES:
            action = menu.addAction(f"Add {name}")
            action.setData(name)
        action = menu.exec(event.globalPos())
        if action and action.data():
            scene_pos = self.mapToScene(event.pos())
            self.add_node(action.data(), scene_pos.x(), scene_pos.y())

    def add_node(self, node_type, x=0, y=0):
        template = NODE_TEMPLATES.get(node_type, {})
        node = NodeItem(node_type, node_type, x, y)
        for label, ptype in template.get('inputs', []):
            node.add_input(label, ptype)
        for label, ptype in template.get('outputs', []):
            node.add_output(label, ptype)
        self.scene.addItem(node)
        self.nodes.append(node)
        return node

    def get_nodes_data(self):
        """Export all nodes and their data for compilation."""
        result = []
        for node in self.nodes:
            result.append({
                'type': node.node_type,
                'title': node.title,
                'data': node.data,
                'x': node.pos().x(),
                'y': node.pos().y(),
            })
        return result


# ── Text-Based NPI Editor ─────────────────────────────────────────────

class TextNPIEditor(QWidget):
    """Text-based NPI extension editor with manifest form + code editors."""

    def __init__(self):
        super().__init__()
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)

        splitter = QSplitter(Qt.Orientation.Horizontal)
        layout.addWidget(splitter)

        # Left: manifest form
        form_widget = QWidget()
        form_layout = QVBoxLayout(form_widget)
        form_label = QLabel("Extension Manifest")
        form_label.setStyleSheet(
            "font-size: 14px; font-weight: bold; color: #C9A8F0; background: transparent;")
        form_layout.addWidget(form_label)

        form = QFormLayout()
        self.name_edit = QLineEdit()
        self.name_edit.setPlaceholderText("My Extension")
        form.addRow("Name:", self.name_edit)

        self.version_edit = QLineEdit("1.0.0")
        form.addRow("Version:", self.version_edit)

        self.desc_edit = QLineEdit()
        self.desc_edit.setPlaceholderText("A brief description")
        form.addRow("Description:", self.desc_edit)

        self.permissions_edit = QLineEdit()
        self.permissions_edit.setPlaceholderText("tabs, storage, <all_urls>")
        form.addRow("Permissions:", self.permissions_edit)

        self.match_edit = QLineEdit("<all_urls>")
        form.addRow("URL Match:", self.match_edit)

        self.run_at_combo = QComboBox()
        self.run_at_combo.addItems(['document_idle', 'document_start', 'document_end'])
        form.addRow("Run at:", self.run_at_combo)

        self.has_popup = QCheckBox("Include popup")
        form.addRow("", self.has_popup)

        self.has_background = QCheckBox("Include background script")
        form.addRow("", self.has_background)

        form_layout.addLayout(form)
        form_layout.addStretch()
        splitter.addWidget(form_widget)

        # Right: code editors
        code_widget = QWidget()
        code_layout = QVBoxLayout(code_widget)

        tabs = QTabWidget()
        self.content_js_edit = QTextEdit()
        self.content_js_edit.setPlaceholderText("// Content script JS\nconsole.log('Hello from extension!');")
        self.content_js_edit.setStyleSheet(
            "font-family: 'JetBrains Mono', 'Fira Code', monospace; font-size: 13px;")
        tabs.addTab(self.content_js_edit, "content.js")

        self.content_css_edit = QTextEdit()
        self.content_css_edit.setPlaceholderText("/* Content CSS */")
        self.content_css_edit.setStyleSheet(
            "font-family: 'JetBrains Mono', 'Fira Code', monospace; font-size: 13px;")
        tabs.addTab(self.content_css_edit, "content.css")

        self.popup_html_edit = QTextEdit()
        self.popup_html_edit.setPlaceholderText(
            "<html>\n<body>\n  <h1>My Extension</h1>\n</body>\n</html>")
        self.popup_html_edit.setStyleSheet(
            "font-family: 'JetBrains Mono', 'Fira Code', monospace; font-size: 13px;")
        tabs.addTab(self.popup_html_edit, "popup.html")

        self.bg_js_edit = QTextEdit()
        self.bg_js_edit.setPlaceholderText("// Background script")
        self.bg_js_edit.setStyleSheet(
            "font-family: 'JetBrains Mono', 'Fira Code', monospace; font-size: 13px;")
        tabs.addTab(self.bg_js_edit, "background.js")

        code_layout.addWidget(tabs)
        splitter.addWidget(code_widget)
        splitter.setSizes([300, 500])

    def get_manifest(self):
        """Build manifest dict from form fields."""
        perms = [p.strip() for p in self.permissions_edit.text().split(',') if p.strip()]
        manifest = {
            'manifest_version': 2,
            'name': self.name_edit.text() or 'Untitled Extension',
            'version': self.version_edit.text() or '1.0.0',
            'description': self.desc_edit.text(),
            'npi_version': 1,
            'permissions': perms,
            'oynix_features': ['content_injection'],
            'content_scripts': [{
                'matches': [m.strip() for m in self.match_edit.text().split(',') if m.strip()],
                'js': ['content_scripts/content.js'],
                'css': ['content_scripts/content.css'],
                'run_at': self.run_at_combo.currentText(),
            }],
        }
        if self.has_popup.isChecked():
            manifest['action'] = {'default_popup': 'popup/popup.html'}
            manifest['oynix_features'].append('popup')
        if self.has_background.isChecked():
            manifest['background'] = {'scripts': ['background/background.js']}
            manifest['oynix_features'].append('background')
        return manifest

    def get_files(self):
        """Build files dict from code editors."""
        files = {}
        js = self.content_js_edit.toPlainText()
        if js.strip():
            files['content_scripts/content.js'] = js
        css = self.content_css_edit.toPlainText()
        if css.strip():
            files['content_scripts/content.css'] = css
        if self.has_popup.isChecked():
            html = self.popup_html_edit.toPlainText()
            files['popup/popup.html'] = html or '<html><body><h1>Popup</h1></body></html>'
        if self.has_background.isChecked():
            bg = self.bg_js_edit.toPlainText()
            files['background/background.js'] = bg or '// Background script'
        return files


# ── Main NPI Builder Dialog ───────────────────────────────────────────

class NPIBuilderDialog(QDialog):
    """NPI Extension Builder with visual node editor and text editor tabs."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("OyNIx NPI Extension Builder")
        self.setMinimumSize(900, 600)
        self.resize(1000, 700)

        layout = QVBoxLayout(self)

        # Header
        header = QLabel("NPI Extension Builder")
        header.setStyleSheet(
            "font-size: 18px; font-weight: bold; color: #C9A8F0;"
            " background: transparent; padding: 8px;")
        layout.addWidget(header)

        # Tabs: Visual | Text
        self.tabs = QTabWidget()
        layout.addWidget(self.tabs)

        # Visual node editor tab
        visual_widget = QWidget()
        vis_layout = QVBoxLayout(visual_widget)
        vis_toolbar = QHBoxLayout()
        for node_name in ['Manifest', 'Content Script', 'Content CSS',
                          'Popup HTML', 'Background Script', 'URL Match Pattern']:
            btn = QPushButton(f"+ {node_name}")
            btn.setStyleSheet("font-size: 11px; padding: 4px 8px;")
            btn.clicked.connect(lambda checked, n=node_name: self._add_node(n))
            vis_toolbar.addWidget(btn)
        vis_toolbar.addStretch()
        vis_layout.addLayout(vis_toolbar)

        self.node_editor = NodeEditorView()
        vis_layout.addWidget(self.node_editor)
        # Add default manifest node
        self.node_editor.add_node('Manifest', 50, 50)
        self.tabs.addTab(visual_widget, "Visual Builder")

        # Text editor tab
        self.text_editor = TextNPIEditor()
        self.tabs.addTab(self.text_editor, "Text Builder")

        # Bottom: compile button
        btn_row = QHBoxLayout()
        compile_btn = QPushButton("Compile NPI")
        compile_btn.setObjectName("accentBtn")
        compile_btn.setStyleSheet(
            "QPushButton { background: #7B4FBF; color: #E8E0F0; font-size: 14px;"
            " padding: 10px 24px; border-radius: 8px; font-weight: bold; }"
            "QPushButton:hover { background: #9B6FDF; }")
        compile_btn.clicked.connect(self._compile)
        btn_row.addStretch()
        btn_row.addWidget(compile_btn)
        btn_row.addStretch()
        layout.addLayout(btn_row)

    def _add_node(self, node_type):
        self.node_editor.add_node(node_type, 200, 100)

    def _compile(self):
        """Compile the extension from whichever tab is active."""
        if self.tabs.currentIndex() == 0:
            self._compile_from_nodes()
        else:
            self._compile_from_text()

    def _compile_from_text(self):
        manifest = self.text_editor.get_manifest()
        files = self.text_editor.get_files()
        self._save_npi(manifest, files)

    def _compile_from_nodes(self):
        """Build manifest and files from the visual node graph."""
        nodes = self.node_editor.get_nodes_data()
        manifest_nodes = [n for n in nodes if n['type'] == 'Manifest']
        if not manifest_nodes:
            QMessageBox.warning(self, "Missing Manifest",
                "Add a Manifest node to the graph first.")
            return

        manifest = {
            'manifest_version': 2,
            'name': manifest_nodes[0].get('data', {}).get('name', 'Node Extension'),
            'version': '1.0.0',
            'npi_version': 1,
            'permissions': [],
            'oynix_features': [],
        }

        files = {}
        cs_js = []
        cs_css = []
        matches = ['<all_urls>']

        for node in nodes:
            ntype = node['type']
            data = node.get('data', {})
            if ntype == 'Content Script':
                code = data.get('code', '// content script\nconsole.log("OyNIx extension loaded");')
                fname = f"content_scripts/content_{len(cs_js)}.js"
                files[fname] = code
                cs_js.append(fname)
            elif ntype == 'Content CSS':
                code = data.get('code', '/* content css */')
                fname = f"content_scripts/style_{len(cs_css)}.css"
                files[fname] = code
                cs_css.append(fname)
            elif ntype == 'Popup HTML':
                html = data.get('code', '<html><body><h1>Popup</h1></body></html>')
                files['popup/popup.html'] = html
                manifest['action'] = {'default_popup': 'popup/popup.html'}
                manifest['oynix_features'].append('popup')
            elif ntype == 'Background Script':
                code = data.get('code', '// background script')
                files['background/background.js'] = code
                manifest['background'] = {'scripts': ['background/background.js']}
                manifest['oynix_features'].append('background')
            elif ntype == 'URL Match Pattern':
                pat = data.get('pattern', '<all_urls>')
                matches = [p.strip() for p in pat.split(',') if p.strip()]

        if cs_js or cs_css:
            manifest['content_scripts'] = [{
                'matches': matches,
                'js': cs_js,
                'css': cs_css,
                'run_at': 'document_idle',
            }]
            manifest['oynix_features'].append('content_injection')

        self._save_npi(manifest, files)

    def _save_npi(self, manifest, files):
        name = manifest.get('name', 'extension').replace(' ', '_')
        path, _ = QFileDialog.getSaveFileName(
            self, "Save NPI Extension",
            os.path.expanduser(f"~/{name}.npi"),
            "OyNIx Extensions (*.npi)")
        if not path:
            return
        ok, err = compile_npi(manifest, files, path)
        if ok:
            QMessageBox.information(self, "NPI Compiled",
                f"Extension saved: {path}\n\n"
                f"Name: {manifest.get('name')}\n"
                f"Files: {len(files)}\n"
                f"Features: {', '.join(manifest.get('oynix_features', []))}")
        else:
            QMessageBox.warning(self, "Compile Error", err)

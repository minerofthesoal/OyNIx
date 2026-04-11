"""
OyNIx Browser - Custom Search Engine Builder
Visual, settings-based, and code-based search engine creation.
Users can create their own search engines with URL templates, parse rules, etc.
"""
import os
import json

from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QLabel, QLineEdit,
    QPushButton, QListWidget, QListWidgetItem, QTabWidget,
    QWidget, QTextEdit, QComboBox, QCheckBox, QGroupBox,
    QFormLayout, QMessageBox, QSpinBox
)
from PyQt6.QtCore import Qt

ENGINES_FILE = os.path.expanduser("~/.config/oynix/custom_engines.json")


def load_custom_engines():
    """Load user-created search engines."""
    try:
        with open(ENGINES_FILE) as f:
            return json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        return []


def save_custom_engines(engines):
    os.makedirs(os.path.dirname(ENGINES_FILE), exist_ok=True)
    with open(ENGINES_FILE, 'w') as f:
        json.dump(engines, f, indent=2)


def get_custom_search_url(engine_name, query):
    """Get the search URL for a custom engine."""
    engines = load_custom_engines()
    for eng in engines:
        if eng['name'].lower() == engine_name.lower():
            return eng['url_template'].replace('{query}', query).replace('%s', query)
    return None


class SearchEngineBuilder(QDialog):
    """Dialog for building custom search engines in three modes."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Custom Search Engine Builder")
        self.setMinimumSize(650, 550)
        self._engines = load_custom_engines()

        layout = QVBoxLayout(self)
        header = QLabel("Build Your Own Search Engines")
        header.setObjectName("sectionHeader")
        layout.addWidget(header)

        # Tabs for three modes
        tabs = QTabWidget()

        # ── Tab 1: Visual Builder ────────────────────────────────
        visual_tab = QWidget()
        vl = QVBoxLayout(visual_tab)

        form = QFormLayout()
        self.v_name = QLineEdit()
        self.v_name.setPlaceholderText("My Search Engine")
        form.addRow("Name:", self.v_name)

        self.v_url = QLineEdit()
        self.v_url.setPlaceholderText("https://example.com/search?q={query}")
        form.addRow("Search URL:", self.v_url)

        self.v_icon = QLineEdit()
        self.v_icon.setPlaceholderText("https://example.com/favicon.ico (optional)")
        form.addRow("Icon URL:", self.v_icon)

        self.v_suggest = QLineEdit()
        self.v_suggest.setPlaceholderText("https://example.com/suggest?q={query} (optional)")
        form.addRow("Suggestions:", self.v_suggest)

        self.v_method = QComboBox()
        self.v_method.addItems(["GET", "POST"])
        form.addRow("Method:", self.v_method)

        self.v_default = QCheckBox("Set as default search engine")
        form.addRow("", self.v_default)

        vl.addLayout(form)

        help_label = QLabel(
            "Use {query} or %s as placeholder for the search terms.\n"
            "Example: https://duckduckgo.com/?q={query}")
        help_label.setStyleSheet("color: #605878; font-size: 12px; background: transparent;")
        help_label.setWordWrap(True)
        vl.addWidget(help_label)

        test_row = QHBoxLayout()
        self.v_test_input = QLineEdit()
        self.v_test_input.setPlaceholderText("Test query...")
        test_row.addWidget(self.v_test_input)
        test_btn = QPushButton("Test")
        test_btn.clicked.connect(self._test_visual)
        test_row.addWidget(test_btn)
        vl.addLayout(test_row)

        self.v_test_result = QLabel("")
        self.v_test_result.setStyleSheet("color: #6FCF97; font-size: 12px; background: transparent;")
        self.v_test_result.setWordWrap(True)
        vl.addWidget(self.v_test_result)

        save_visual = QPushButton("Save Engine")
        save_visual.setObjectName("accentBtn")
        save_visual.clicked.connect(self._save_visual)
        vl.addWidget(save_visual)
        vl.addStretch()

        tabs.addTab(visual_tab, "Visual Builder")

        # ── Tab 2: Settings-Based ────────────────────────────────
        settings_tab = QWidget()
        sl = QVBoxLayout(settings_tab)

        sl.addWidget(QLabel("Import from OpenSearch XML or browser settings"))

        self.s_import_text = QTextEdit()
        self.s_import_text.setPlaceholderText(
            "Paste OpenSearch XML here, or a JSON config:\n\n"
            '{\n  "name": "MyEngine",\n  "url_template": "https://search.example.com/?q={query}",\n'
            '  "suggest_url": "https://search.example.com/suggest?q={query}",\n'
            '  "icon": "https://search.example.com/favicon.ico"\n}')
        self.s_import_text.setMaximumHeight(200)
        sl.addWidget(self.s_import_text)

        import_btn = QPushButton("Import & Save")
        import_btn.setObjectName("accentBtn")
        import_btn.clicked.connect(self._import_settings)
        sl.addWidget(import_btn)
        sl.addStretch()

        tabs.addTab(settings_tab, "Settings Import")

        # ── Tab 3: Code-Based ────────────────────────────────────
        code_tab = QWidget()
        cl = QVBoxLayout(code_tab)

        cl.addWidget(QLabel("Write a custom search engine with JavaScript"))

        self.c_name = QLineEdit()
        self.c_name.setPlaceholderText("Engine name")
        cl.addWidget(self.c_name)

        self.c_code = QTextEdit()
        self.c_code.setPlaceholderText(
            "// JavaScript search function\n"
            "// 'query' variable contains the search text\n"
            "// Return an array of {url, title, description} objects\n\n"
            "async function search(query) {\n"
            "  const resp = await fetch(\n"
            "    'https://api.example.com/search?q=' + encodeURIComponent(query)\n"
            "  );\n"
            "  const data = await resp.json();\n"
            "  return data.results.map(r => ({\n"
            "    url: r.link,\n"
            "    title: r.title,\n"
            "    description: r.snippet\n"
            "  }));\n"
            "}")
        cl.addWidget(self.c_code)

        save_code = QPushButton("Save Code Engine")
        save_code.setObjectName("accentBtn")
        save_code.clicked.connect(self._save_code)
        cl.addWidget(save_code)

        tabs.addTab(code_tab, "Code Builder")

        layout.addWidget(tabs)

        # ── Existing engines list ────────────────────────────────
        existing_group = QGroupBox("Your Custom Engines")
        eg_layout = QVBoxLayout(existing_group)
        self.engines_list = QListWidget()
        self.engines_list.setMaximumHeight(140)
        self._refresh_list()
        eg_layout.addWidget(self.engines_list)

        btn_row = QHBoxLayout()
        del_btn = QPushButton("Delete Selected")
        del_btn.clicked.connect(self._delete_engine)
        btn_row.addWidget(del_btn)
        btn_row.addStretch()
        close_btn = QPushButton("Close")
        close_btn.clicked.connect(self.accept)
        btn_row.addWidget(close_btn)
        eg_layout.addLayout(btn_row)
        layout.addWidget(existing_group)

    def _refresh_list(self):
        self.engines_list.clear()
        for eng in self._engines:
            mode = eng.get('mode', 'visual')
            default = " [DEFAULT]" if eng.get('is_default') else ""
            item = QListWidgetItem(f"{eng['name']} ({mode}){default}")
            item.setData(Qt.ItemDataRole.UserRole, eng['name'])
            self.engines_list.addItem(item)

    def _save_visual(self):
        name = self.v_name.text().strip()
        url_tpl = self.v_url.text().strip()
        if not name or not url_tpl:
            QMessageBox.warning(self, "Error", "Name and URL template are required")
            return
        if '{query}' not in url_tpl and '%s' not in url_tpl:
            QMessageBox.warning(self, "Error", "URL must contain {query} or %s placeholder")
            return

        engine = {
            'name': name,
            'url_template': url_tpl,
            'icon': self.v_icon.text().strip(),
            'suggest_url': self.v_suggest.text().strip(),
            'method': self.v_method.currentText(),
            'is_default': self.v_default.isChecked(),
            'mode': 'visual'
        }
        # Remove any existing engine with same name
        self._engines = [e for e in self._engines if e['name'] != name]
        if engine['is_default']:
            for e in self._engines:
                e['is_default'] = False
        self._engines.append(engine)
        save_custom_engines(self._engines)
        self._refresh_list()
        QMessageBox.information(self, "Saved", f"Engine '{name}' saved")

    def _test_visual(self):
        query = self.v_test_input.text().strip() or "test"
        url_tpl = self.v_url.text().strip()
        if not url_tpl:
            return
        result = url_tpl.replace('{query}', query).replace('%s', query)
        self.v_test_result.setText(f"Result URL: {result}")

    def _import_settings(self):
        text = self.s_import_text.toPlainText().strip()
        if not text:
            return

        # Try JSON first
        try:
            data = json.loads(text)
            if isinstance(data, dict):
                name = data.get('name', data.get('short_name', 'Imported'))
                url_tpl = data.get('url_template', data.get('url', data.get('search_url', '')))
                if url_tpl:
                    engine = {
                        'name': name, 'url_template': url_tpl,
                        'icon': data.get('icon', data.get('favicon_url', '')),
                        'suggest_url': data.get('suggest_url', data.get('suggestions_url', '')),
                        'method': data.get('method', 'GET'),
                        'mode': 'settings'
                    }
                    self._engines = [e for e in self._engines if e['name'] != name]
                    self._engines.append(engine)
                    save_custom_engines(self._engines)
                    self._refresh_list()
                    QMessageBox.information(self, "Imported", f"Engine '{name}' imported")
                    return
        except json.JSONDecodeError:
            pass

        # Try OpenSearch XML
        try:
            from xml.etree import ElementTree
            ns = {'os': 'http://a9.com/-/spec/opensearch/1.1/'}
            root = ElementTree.fromstring(text)
            name = ''
            url_tpl = ''
            for child in root:
                tag = child.tag.split('}')[-1] if '}' in child.tag else child.tag
                if tag == 'ShortName':
                    name = child.text or ''
                elif tag == 'Url' and child.get('type', '').startswith('text/html'):
                    url_tpl = child.get('template', '')
            if name and url_tpl:
                url_tpl = url_tpl.replace('{searchTerms}', '{query}')
                engine = {'name': name, 'url_template': url_tpl, 'mode': 'settings',
                          'icon': '', 'suggest_url': '', 'method': 'GET'}
                self._engines = [e for e in self._engines if e['name'] != name]
                self._engines.append(engine)
                save_custom_engines(self._engines)
                self._refresh_list()
                QMessageBox.information(self, "Imported", f"OpenSearch engine '{name}' imported")
                return
        except Exception:
            pass

        QMessageBox.warning(self, "Error", "Could not parse the input as JSON or OpenSearch XML")

    def _save_code(self):
        name = self.c_name.text().strip()
        code = self.c_code.toPlainText().strip()
        if not name or not code:
            QMessageBox.warning(self, "Error", "Name and code are required")
            return
        engine = {'name': name, 'mode': 'code', 'code': code,
                  'url_template': '', 'icon': '', 'suggest_url': '', 'method': 'GET'}
        self._engines = [e for e in self._engines if e['name'] != name]
        self._engines.append(engine)
        save_custom_engines(self._engines)
        self._refresh_list()
        QMessageBox.information(self, "Saved", f"Code engine '{name}' saved")

    def _delete_engine(self):
        current = self.engines_list.currentItem()
        if not current:
            return
        name = current.data(Qt.ItemDataRole.UserRole)
        self._engines = [e for e in self._engines if e['name'] != name]
        save_custom_engines(self._engines)
        self._refresh_list()

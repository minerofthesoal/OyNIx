"""
OyNIx Browser - Nyx Theme Engine
Medium purple and black themed UI system with multiple theme variants.

The signature look: deep blacks with medium purple accents,
glowing highlights, and smooth gradients.
"""


# ── Nyx Purple/Black Palette ──────────────────────────────────────────
NYX_COLORS = {
    # Core blacks
    'bg_darkest':       '#0a0a0f',
    'bg_dark':          '#111118',
    'bg_mid':           '#1a1a24',
    'bg_light':         '#22222e',
    'bg_lighter':       '#2a2a38',
    'bg_surface':       '#32323f',

    # Purple spectrum (medium purple focus)
    'purple_dark':      '#4a2d7a',
    'purple_mid':       '#7B4FBF',     # Main accent - medium purple
    'purple_light':     '#9B6FDF',
    'purple_glow':      '#B088F0',
    'purple_soft':      '#C9A8F0',
    'purple_pale':      '#E0D0F8',

    # Text colors
    'text_primary':     '#E8E0F0',
    'text_secondary':   '#A8A0B8',
    'text_muted':       '#706880',
    'text_accent':      '#C9A8F0',

    # Status colors
    'success':          '#6FCF97',
    'warning':          '#F2C94C',
    'error':            '#EB5757',
    'info':             '#7B4FBF',

    # Special
    'glow':             'rgba(123, 79, 191, 0.4)',
    'glow_strong':      'rgba(123, 79, 191, 0.7)',
    'border':           '#3a3a4a',
    'border_active':    '#7B4FBF',
    'selection':        'rgba(123, 79, 191, 0.3)',
    'scrollbar':        '#4a2d7a',
    'scrollbar_hover':  '#7B4FBF',
}

# ── Alternate Themes ──────────────────────────────────────────────────
THEMES = {
    'nyx': NYX_COLORS,

    'nyx_midnight': {
        **NYX_COLORS,
        'bg_darkest':   '#050510',
        'bg_dark':      '#0a0a18',
        'bg_mid':       '#101020',
        'purple_mid':   '#6040A0',
        'purple_glow':  '#9070C0',
    },

    'nyx_violet': {
        **NYX_COLORS,
        'purple_mid':   '#8855CC',
        'purple_light': '#AA77EE',
        'purple_glow':  '#CC99FF',
        'purple_soft':  '#DDBBFF',
    },

    'nyx_amethyst': {
        **NYX_COLORS,
        'bg_darkest':   '#0d0810',
        'bg_dark':      '#150e1a',
        'purple_mid':   '#9966CC',
        'purple_light': '#BB88DD',
    },
}


def get_theme(name='nyx'):
    """Get a theme color palette by name."""
    return THEMES.get(name, THEMES['nyx'])


def get_qt_stylesheet(colors=None):
    """Generate the full Qt stylesheet for the OyNIx browser."""
    c = colors or NYX_COLORS
    return f"""
    /* ── Global ───────────────────────────────── */
    QMainWindow {{
        background-color: {c['bg_darkest']};
        color: {c['text_primary']};
    }}

    QWidget {{
        background-color: {c['bg_dark']};
        color: {c['text_primary']};
        font-family: 'Segoe UI', 'Ubuntu', 'Cantarell', sans-serif;
        font-size: 13px;
    }}

    /* ── Menu Bar ─────────────────────────────── */
    QMenuBar {{
        background-color: {c['bg_darkest']};
        color: {c['text_primary']};
        border-bottom: 1px solid {c['border']};
        padding: 2px;
    }}

    QMenuBar::item {{
        padding: 6px 12px;
        border-radius: 4px;
    }}

    QMenuBar::item:selected {{
        background-color: {c['purple_dark']};
        color: {c['purple_pale']};
    }}

    QMenu {{
        background-color: {c['bg_mid']};
        color: {c['text_primary']};
        border: 1px solid {c['border']};
        border-radius: 8px;
        padding: 4px;
    }}

    QMenu::item {{
        padding: 8px 30px 8px 20px;
        border-radius: 4px;
    }}

    QMenu::item:selected {{
        background-color: {c['purple_dark']};
        color: {c['purple_pale']};
    }}

    QMenu::separator {{
        height: 1px;
        background: {c['border']};
        margin: 4px 10px;
    }}

    /* ── Toolbar ──────────────────────────────── */
    QToolBar {{
        background-color: {c['bg_darkest']};
        border-bottom: 1px solid {c['border']};
        padding: 4px 8px;
        spacing: 4px;
    }}

    /* ── URL Bar ──────────────────────────────── */
    QLineEdit {{
        background-color: {c['bg_mid']};
        color: {c['text_primary']};
        border: 2px solid {c['border']};
        border-radius: 20px;
        padding: 8px 16px;
        font-size: 14px;
        selection-background-color: {c['selection']};
    }}

    QLineEdit:focus {{
        border-color: {c['purple_mid']};
        background-color: {c['bg_light']};
    }}

    /* ── Buttons ──────────────────────────────── */
    QPushButton {{
        background-color: {c['bg_lighter']};
        color: {c['text_primary']};
        border: 1px solid {c['border']};
        border-radius: 8px;
        padding: 8px 14px;
        font-weight: 600;
        min-width: 32px;
    }}

    QPushButton:hover {{
        background-color: {c['purple_dark']};
        border-color: {c['purple_mid']};
        color: {c['purple_pale']};
    }}

    QPushButton:pressed {{
        background-color: {c['purple_mid']};
        color: {c['bg_darkest']};
    }}

    QPushButton#navButton {{
        background-color: transparent;
        border: none;
        border-radius: 6px;
        padding: 6px 10px;
        font-size: 16px;
        min-width: 36px;
    }}

    QPushButton#navButton:hover {{
        background-color: {c['bg_lighter']};
    }}

    QPushButton#accentButton {{
        background-color: {c['purple_mid']};
        color: {c['bg_darkest']};
        border: none;
        font-weight: bold;
    }}

    QPushButton#accentButton:hover {{
        background-color: {c['purple_light']};
    }}

    /* ── Tab Widget (Normal Tabs) ─────────────── */
    QTabWidget::pane {{
        border: none;
        background-color: {c['bg_darkest']};
    }}

    QTabBar {{
        background-color: {c['bg_darkest']};
    }}

    QTabBar::tab {{
        background-color: {c['bg_mid']};
        color: {c['text_secondary']};
        padding: 8px 20px;
        margin-right: 1px;
        border-top-left-radius: 10px;
        border-top-right-radius: 10px;
        border: 1px solid {c['border']};
        border-bottom: none;
        min-width: 120px;
        max-width: 250px;
    }}

    QTabBar::tab:selected {{
        background-color: {c['bg_light']};
        color: {c['purple_light']};
        border-color: {c['purple_mid']};
        border-bottom: 2px solid {c['purple_mid']};
    }}

    QTabBar::tab:hover:!selected {{
        background-color: {c['bg_lighter']};
        color: {c['text_primary']};
    }}

    QTabBar::close-button {{
        image: none;
        subcontrol-position: right;
        border-radius: 4px;
        padding: 2px;
    }}

    QTabBar::close-button:hover {{
        background-color: {c['error']};
    }}

    /* ── Tree Widget (Tree Tabs) ──────────────── */
    QTreeWidget {{
        background-color: {c['bg_dark']};
        color: {c['text_primary']};
        border: none;
        outline: none;
        font-size: 12px;
    }}

    QTreeWidget::item {{
        padding: 6px 8px;
        border-radius: 4px;
        margin: 1px 4px;
    }}

    QTreeWidget::item:selected {{
        background-color: {c['purple_dark']};
        color: {c['purple_pale']};
    }}

    QTreeWidget::item:hover:!selected {{
        background-color: {c['bg_lighter']};
    }}

    QTreeWidget::branch {{
        background: transparent;
    }}

    QTreeWidget::branch:has-siblings:!adjoins-item {{
        border-image: none;
        image: none;
    }}

    QTreeWidget::branch:has-siblings:adjoins-item {{
        border-image: none;
        image: none;
    }}

    QTreeWidget::branch:!has-children:!has-siblings:adjoins-item {{
        border-image: none;
        image: none;
    }}

    QTreeWidget::branch:has-children:!has-siblings:closed,
    QTreeWidget::branch:closed:has-children:has-siblings {{
        image: none;
        border-image: none;
    }}

    QTreeWidget::branch:open:has-children:!has-siblings,
    QTreeWidget::branch:open:has-children:has-siblings {{
        image: none;
        border-image: none;
    }}

    /* ── Scrollbars ───────────────────────────── */
    QScrollBar:vertical {{
        background: {c['bg_dark']};
        width: 10px;
        margin: 0;
        border-radius: 5px;
    }}

    QScrollBar::handle:vertical {{
        background: {c['scrollbar']};
        border-radius: 5px;
        min-height: 30px;
    }}

    QScrollBar::handle:vertical:hover {{
        background: {c['scrollbar_hover']};
    }}

    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {{
        height: 0;
    }}

    QScrollBar:horizontal {{
        background: {c['bg_dark']};
        height: 10px;
        border-radius: 5px;
    }}

    QScrollBar::handle:horizontal {{
        background: {c['scrollbar']};
        border-radius: 5px;
        min-width: 30px;
    }}

    QScrollBar::handle:horizontal:hover {{
        background: {c['scrollbar_hover']};
    }}

    QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {{
        width: 0;
    }}

    /* ── Status Bar ───────────────────────────── */
    QStatusBar {{
        background-color: {c['bg_darkest']};
        color: {c['text_muted']};
        border-top: 1px solid {c['border']};
        font-size: 12px;
    }}

    QStatusBar::item {{
        border: none;
    }}

    /* ── Dialogs ──────────────────────────────── */
    QDialog {{
        background-color: {c['bg_dark']};
        color: {c['text_primary']};
        border-radius: 12px;
    }}

    QGroupBox {{
        font-weight: bold;
        border: 1px solid {c['border']};
        border-radius: 8px;
        margin-top: 12px;
        padding-top: 16px;
        color: {c['purple_light']};
    }}

    QGroupBox::title {{
        subcontrol-origin: margin;
        subcontrol-position: top left;
        padding: 4px 12px;
        background-color: {c['bg_dark']};
        border-radius: 4px;
    }}

    QCheckBox {{
        spacing: 8px;
        color: {c['text_primary']};
    }}

    QCheckBox::indicator {{
        width: 18px;
        height: 18px;
        border-radius: 4px;
        border: 2px solid {c['border']};
        background-color: {c['bg_mid']};
    }}

    QCheckBox::indicator:checked {{
        background-color: {c['purple_mid']};
        border-color: {c['purple_mid']};
    }}

    QComboBox {{
        background-color: {c['bg_mid']};
        color: {c['text_primary']};
        border: 1px solid {c['border']};
        border-radius: 6px;
        padding: 6px 12px;
    }}

    QComboBox:hover {{
        border-color: {c['purple_mid']};
    }}

    QComboBox QAbstractItemView {{
        background-color: {c['bg_mid']};
        color: {c['text_primary']};
        border: 1px solid {c['border']};
        selection-background-color: {c['purple_dark']};
    }}

    QSpinBox {{
        background-color: {c['bg_mid']};
        color: {c['text_primary']};
        border: 1px solid {c['border']};
        border-radius: 6px;
        padding: 4px 8px;
    }}

    QSlider::groove:horizontal {{
        height: 6px;
        background: {c['bg_lighter']};
        border-radius: 3px;
    }}

    QSlider::handle:horizontal {{
        background: {c['purple_mid']};
        width: 16px;
        height: 16px;
        margin: -5px 0;
        border-radius: 8px;
    }}

    QSlider::handle:horizontal:hover {{
        background: {c['purple_light']};
    }}

    /* ── Splitter ─────────────────────────────── */
    QSplitter::handle {{
        background: {c['border']};
    }}

    QSplitter::handle:hover {{
        background: {c['purple_mid']};
    }}

    /* ── Text Edit (AI Chat) ──────────────────── */
    QTextEdit, QPlainTextEdit {{
        background-color: {c['bg_mid']};
        color: {c['text_primary']};
        border: 1px solid {c['border']};
        border-radius: 8px;
        padding: 8px;
        selection-background-color: {c['selection']};
    }}

    QTextEdit:focus, QPlainTextEdit:focus {{
        border-color: {c['purple_mid']};
    }}

    /* ── Labels ───────────────────────────────── */
    QLabel {{
        color: {c['text_primary']};
        background: transparent;
    }}

    QLabel#sectionHeader {{
        color: {c['purple_light']};
        font-size: 15px;
        font-weight: bold;
    }}

    /* ── Message Box ──────────────────────────── */
    QMessageBox {{
        background-color: {c['bg_dark']};
    }}

    /* ── ToolTip ──────────────────────────────── */
    QToolTip {{
        background-color: {c['bg_mid']};
        color: {c['text_primary']};
        border: 1px solid {c['purple_mid']};
        border-radius: 6px;
        padding: 6px;
    }}
    """


def get_homepage_html(colors=None):
    """Generate the Nyx-themed homepage HTML."""
    c = colors or NYX_COLORS
    return f'''<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>OyNIx - Home</title>
<style>
* {{ margin:0; padding:0; box-sizing:border-box; }}

body {{
    background: {c['bg_darkest']};
    color: {c['text_primary']};
    font-family: 'Segoe UI', 'Ubuntu', 'Cantarell', sans-serif;
    min-height: 100vh;
    overflow-x: hidden;
}}

/* Animated background */
body::before {{
    content: '';
    position: fixed;
    top: 0; left: 0; right: 0; bottom: 0;
    background:
        radial-gradient(ellipse at 20% 50%, rgba(123,79,191,0.08) 0%, transparent 50%),
        radial-gradient(ellipse at 80% 20%, rgba(155,111,223,0.06) 0%, transparent 50%),
        radial-gradient(ellipse at 50% 80%, rgba(74,45,122,0.1) 0%, transparent 50%);
    pointer-events: none;
    z-index: 0;
}}

.container {{
    position: relative;
    z-index: 1;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    min-height: 100vh;
    padding: 40px 20px;
}}

/* Logo */
.logo {{
    font-size: 5.5em;
    font-weight: 900;
    letter-spacing: -3px;
    background: linear-gradient(135deg, {c['purple_mid']}, {c['purple_glow']}, {c['purple_soft']});
    -webkit-background-clip: text;
    -webkit-text-fill-color: transparent;
    text-shadow: none;
    margin-bottom: 8px;
    animation: logoGlow 3s ease-in-out infinite alternate;
}}

@keyframes logoGlow {{
    from {{ filter: drop-shadow(0 0 20px rgba(123,79,191,0.3)); }}
    to {{ filter: drop-shadow(0 0 40px rgba(123,79,191,0.6)); }}
}}

.tagline {{
    color: {c['text_muted']};
    font-size: 1.1em;
    margin-bottom: 50px;
    letter-spacing: 4px;
    text-transform: uppercase;
}}

/* Search */
.search-wrap {{
    position: relative;
    width: 650px;
    max-width: 90vw;
    margin-bottom: 60px;
}}

.search-box {{
    width: 100%;
    padding: 18px 60px 18px 24px;
    font-size: 1.15em;
    background: {c['bg_mid']};
    border: 2px solid {c['border']};
    border-radius: 50px;
    color: {c['text_primary']};
    outline: none;
    transition: all 0.3s ease;
}}

.search-box:focus {{
    border-color: {c['purple_mid']};
    box-shadow: 0 0 30px rgba(123,79,191,0.3), inset 0 0 10px rgba(123,79,191,0.05);
    background: {c['bg_light']};
}}

.search-box::placeholder {{
    color: {c['text_muted']};
}}

.search-icon {{
    position: absolute;
    right: 20px;
    top: 50%;
    transform: translateY(-50%);
    font-size: 1.3em;
    opacity: 0.5;
}}

/* Engine switcher */
.engine-row {{
    display: flex;
    gap: 12px;
    justify-content: center;
    margin-top: 16px;
}}

.engine-btn {{
    padding: 6px 18px;
    border-radius: 20px;
    border: 1px solid {c['border']};
    background: {c['bg_mid']};
    color: {c['text_secondary']};
    cursor: pointer;
    font-size: 0.9em;
    transition: all 0.2s;
}}

.engine-btn:hover, .engine-btn.active {{
    border-color: {c['purple_mid']};
    color: {c['purple_light']};
    background: {c['bg_lighter']};
}}

.engine-btn.active {{
    background: {c['purple_dark']};
    color: {c['purple_pale']};
}}

/* Quick links */
.quick-links {{
    display: grid;
    grid-template-columns: repeat(4, 1fr);
    gap: 16px;
    max-width: 800px;
    width: 100%;
}}

.link-card {{
    background: linear-gradient(145deg, {c['bg_mid']}, {c['bg_dark']});
    border: 1px solid {c['border']};
    border-radius: 16px;
    padding: 28px 16px;
    text-align: center;
    cursor: pointer;
    transition: all 0.3s ease;
    text-decoration: none;
}}

.link-card:hover {{
    transform: translateY(-6px);
    border-color: {c['purple_mid']};
    box-shadow: 0 12px 30px rgba(123,79,191,0.2);
}}

.link-icon {{
    font-size: 2.4em;
    margin-bottom: 10px;
    display: block;
}}

.link-title {{
    color: {c['text_primary']};
    font-weight: 600;
    font-size: 0.95em;
}}

.link-sub {{
    color: {c['text_muted']};
    font-size: 0.8em;
    margin-top: 4px;
}}

/* Stats footer */
.stats {{
    position: fixed;
    bottom: 0;
    left: 0;
    right: 0;
    display: flex;
    justify-content: center;
    gap: 50px;
    padding: 16px;
    background: linear-gradient(transparent, {c['bg_darkest']});
}}

.stat {{ text-align: center; }}

.stat-num {{
    font-size: 1.8em;
    font-weight: 800;
    color: {c['purple_mid']};
}}

.stat-label {{
    color: {c['text_muted']};
    font-size: 0.85em;
}}
</style>
</head>
<body>
<div class="container">
    <div class="logo">OyNIx</div>
    <div class="tagline">Nyx-Powered Local AI Browser</div>

    <div class="search-wrap">
        <input type="text" class="search-box"
               placeholder="Search the web or ask Nyx AI..."
               id="searchInput" autofocus
               onkeypress="if(event.key==='Enter') doSearch(this.value)">
        <span class="search-icon">&#128270;</span>
        <div class="engine-row">
            <button class="engine-btn active" onclick="setEngine('nyx',this)">Nyx</button>
            <button class="engine-btn" onclick="setEngine('duckduckgo',this)">DuckDuckGo</button>
            <button class="engine-btn" onclick="setEngine('google',this)">Google</button>
            <button class="engine-btn" onclick="setEngine('brave',this)">Brave</button>
        </div>
    </div>

    <div class="quick-links">
        <a class="link-card" href="oyn://ai-chat">
            <span class="link-icon">&#129302;</span>
            <div class="link-title">AI Chat</div>
            <div class="link-sub">Local LLM</div>
        </a>
        <a class="link-card" href="oyn://nyx-search">
            <span class="link-icon">&#128270;</span>
            <div class="link-title">Nyx Search</div>
            <div class="link-sub">Auto-Indexed</div>
        </a>
        <a class="link-card" href="oyn://settings">
            <span class="link-icon">&#9881;</span>
            <div class="link-title">Settings</div>
            <div class="link-sub">Configure</div>
        </a>
        <a class="link-card" href="oyn://database">
            <span class="link-icon">&#128218;</span>
            <div class="link-title">Site Database</div>
            <div class="link-sub">1400+ Sites</div>
        </a>
    </div>
</div>

<div class="stats">
    <div class="stat">
        <div class="stat-num">1400+</div>
        <div class="stat-label">Indexed Sites</div>
    </div>
    <div class="stat">
        <div class="stat-num">Local</div>
        <div class="stat-label">AI Model</div>
    </div>
    <div class="stat">
        <div class="stat-num">Nyx</div>
        <div class="stat-label">Search Engine</div>
    </div>
    <div class="stat">
        <div class="stat-num">&#9889;</div>
        <div class="stat-label">Tree Tabs</div>
    </div>
</div>

<script>
var currentEngine = 'nyx';

function setEngine(engine, btn) {{
    currentEngine = engine;
    document.querySelectorAll('.engine-btn').forEach(b => b.classList.remove('active'));
    btn.classList.add('active');
    var ph = {{
        'nyx': 'Search with Nyx engine...',
        'duckduckgo': 'Search with DuckDuckGo...',
        'google': 'Search with Google...',
        'brave': 'Search with Brave...'
    }};
    document.getElementById('searchInput').placeholder = ph[engine] || 'Search...';
}}

function doSearch(q) {{
    if (!q.trim()) return;
    window.location = 'oyn://search?engine=' + currentEngine + '&q=' + encodeURIComponent(q);
}}
</script>
</body>
</html>'''


def get_search_results_html(query, local_results, web_results, colors=None):
    """Generate Nyx-themed search results page."""
    c = colors or NYX_COLORS

    local_html = ""
    for i, r in enumerate(local_results):
        local_html += f'''
        <div class="result" style="animation-delay:{i*0.06}s">
            <div class="r-top">
                <a class="r-title" href="{r['url']}">{r['title']}</a>
                <span class="badge local">Local</span>
                <span class="badge rating">{r.get('rating','')}</span>
            </div>
            <div class="r-url">{r['url']}</div>
            <div class="r-desc">{r.get('description','')}</div>
        </div>'''

    web_html = ""
    for i, r in enumerate(web_results):
        web_html += f'''
        <div class="result" style="animation-delay:{(len(local_results)+i)*0.06}s">
            <div class="r-top">
                <a class="r-title" href="{r['url']}">{r['title']}</a>
                <span class="badge web">Web</span>
            </div>
            <div class="r-url">{r['url']}</div>
            <div class="r-desc">{r.get('description','')}</div>
        </div>'''

    total = len(local_results) + len(web_results)

    return f'''<!DOCTYPE html>
<html><head><meta charset="utf-8">
<title>Nyx Search: {query}</title>
<style>
* {{ margin:0;padding:0;box-sizing:border-box; }}
body {{
    background:{c['bg_darkest']};
    color:{c['text_primary']};
    font-family:'Segoe UI','Ubuntu',sans-serif;
    padding:30px;
}}
.header {{
    text-align:center;
    margin-bottom:40px;
    padding:24px;
    background:linear-gradient(135deg,{c['bg_mid']},{c['bg_dark']});
    border-radius:16px;
    border:1px solid {c['border']};
}}
.header h1 {{
    font-size:2em;
    background:linear-gradient(135deg,{c['purple_mid']},{c['purple_glow']});
    -webkit-background-clip:text;
    -webkit-text-fill-color:transparent;
}}
.header .stats {{
    color:{c['text_muted']};
    margin-top:8px;
}}
.section {{
    max-width:850px;
    margin:0 auto 30px;
}}
.section-title {{
    font-size:1.2em;
    color:{c['purple_light']};
    margin-bottom:16px;
    padding-bottom:8px;
    border-bottom:2px solid {c['border']};
}}
.result {{
    background:{c['bg_mid']};
    border:1px solid {c['border']};
    border-radius:12px;
    padding:16px 20px;
    margin-bottom:12px;
    transition:all 0.3s;
    animation:fadeUp 0.4s ease-out both;
}}
.result:hover {{
    border-color:{c['purple_mid']};
    transform:translateY(-3px);
    box-shadow:0 8px 24px rgba(123,79,191,0.15);
}}
@keyframes fadeUp {{
    from {{ opacity:0;transform:translateY(12px); }}
    to {{ opacity:1;transform:translateY(0); }}
}}
.r-top {{ display:flex; align-items:center; gap:10px; margin-bottom:6px; }}
.r-title {{
    color:{c['purple_light']};
    text-decoration:none;
    font-size:1.15em;
    font-weight:600;
}}
.r-title:hover {{ color:{c['purple_glow']}; }}
.badge {{
    padding:3px 10px;
    border-radius:12px;
    font-size:0.75em;
    font-weight:bold;
}}
.badge.local {{ background:{c['purple_dark']}; color:{c['purple_pale']}; }}
.badge.web {{ background:{c['bg_lighter']}; color:{c['text_secondary']}; }}
.badge.rating {{ background:rgba(242,201,76,0.15); color:#F2C94C; }}
.r-url {{ color:{c['text_muted']}; font-size:0.85em; margin-bottom:6px; word-break:break-all; }}
.r-desc {{ color:{c['text_secondary']}; line-height:1.5; }}
.no-results {{
    text-align:center;
    padding:50px;
    background:{c['bg_mid']};
    border-radius:16px;
    color:{c['text_muted']};
}}
</style></head>
<body>
<div class="header">
    <h1>Nyx Search Results</h1>
    <div class="stats">"{query}" &mdash; {total} results found</div>
</div>
<div class="section">
    {"<div class='section-title'>Local Database</div>" + local_html if local_results else ""}
    {"<div class='section-title'>Web Results</div>" + web_html if web_results else ""}
    {"<div class='no-results'>No results found. Try different keywords.</div>" if total == 0 else ""}
</div>
</body></html>'''

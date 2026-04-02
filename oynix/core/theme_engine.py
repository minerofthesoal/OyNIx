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
    """Generate the Nyx-themed homepage with animated particle canvas."""
    c = colors or NYX_COLORS
    return f'''<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>OyNIx</title>
<style>
*{{margin:0;padding:0;box-sizing:border-box}}
body{{background:{c['bg_darkest']};color:{c['text_primary']};font-family:'Segoe UI','Ubuntu',sans-serif;min-height:100vh;overflow:hidden}}
canvas#bg{{position:fixed;top:0;left:0;width:100%;height:100%;z-index:0}}
.wrap{{position:relative;z-index:1;display:flex;flex-direction:column;align-items:center;justify-content:center;min-height:100vh;padding:30px 20px}}
.logo{{font-size:5.5em;font-weight:900;letter-spacing:-3px;background:linear-gradient(135deg,{c['purple_mid']},{c['purple_glow']},{c['purple_soft']});-webkit-background-clip:text;-webkit-text-fill-color:transparent;margin-bottom:6px;animation:pulse 3s ease-in-out infinite alternate;opacity:0;animation:fadeIn .8s ease forwards,pulse 3s 1s ease-in-out infinite alternate}}
.tagline{{color:{c['text_muted']};font-size:1em;letter-spacing:5px;text-transform:uppercase;margin-bottom:40px;opacity:0;animation:fadeIn .8s .3s ease forwards}}
@keyframes pulse{{from{{filter:drop-shadow(0 0 15px rgba(123,79,191,.3))}}to{{filter:drop-shadow(0 0 35px rgba(123,79,191,.6))}}}}
@keyframes fadeIn{{to{{opacity:1}}}}
@keyframes slideUp{{from{{opacity:0;transform:translateY(20px)}}to{{opacity:1;transform:translateY(0)}}}}
@keyframes scaleIn{{from{{opacity:0;transform:scale(.9)}}to{{opacity:1;transform:scale(1)}}}}
.search-wrap{{position:relative;width:620px;max-width:90vw;margin-bottom:50px;opacity:0;animation:slideUp .6s .5s ease forwards}}
.search-box{{width:100%;padding:16px 55px 16px 22px;font-size:1.1em;background:{c['bg_mid']};border:2px solid {c['border']};border-radius:50px;color:{c['text_primary']};outline:none;transition:all .3s}}
.search-box:focus{{border-color:{c['purple_mid']};box-shadow:0 0 25px rgba(123,79,191,.3);background:{c['bg_light']}}}
.search-box::placeholder{{color:{c['text_muted']}}}
.si{{position:absolute;right:18px;top:50%;transform:translateY(-50%);font-size:1.2em;opacity:.4}}
.er{{display:flex;gap:10px;justify-content:center;margin-top:14px}}
.eb{{padding:5px 16px;border-radius:18px;border:1px solid {c['border']};background:{c['bg_mid']};color:{c['text_secondary']};cursor:pointer;font-size:.85em;transition:all .25s}}
.eb:hover,.eb.a{{border-color:{c['purple_mid']};color:{c['purple_light']};background:{c['bg_lighter']}}}
.eb.a{{background:{c['purple_dark']};color:{c['purple_pale']}}}
.cards{{display:grid;grid-template-columns:repeat(4,1fr);gap:14px;max-width:820px;width:100%}}
.card{{background:linear-gradient(145deg,{c['bg_mid']},{c['bg_dark']});border:1px solid {c['border']};border-radius:14px;padding:22px 14px;text-align:center;cursor:pointer;transition:all .35s cubic-bezier(.4,0,.2,1);text-decoration:none;opacity:0;animation:scaleIn .5s ease forwards}}
.card:nth-child(1){{animation-delay:.6s}}.card:nth-child(2){{animation-delay:.7s}}.card:nth-child(3){{animation-delay:.8s}}.card:nth-child(4){{animation-delay:.9s}}.card:nth-child(5){{animation-delay:1s}}.card:nth-child(6){{animation-delay:1.1s}}.card:nth-child(7){{animation-delay:1.15s}}.card:nth-child(8){{animation-delay:1.2s}}
.card:hover{{transform:translateY(-7px) scale(1.02);border-color:{c['purple_mid']};box-shadow:0 14px 35px rgba(123,79,191,.2)}}
.ci{{font-size:2em;margin-bottom:8px;display:block}}.ct{{color:{c['text_primary']};font-weight:600;font-size:.9em}}.cs{{color:{c['text_muted']};font-size:.75em;margin-top:3px}}
.ft{{position:fixed;bottom:0;left:0;right:0;display:flex;justify-content:center;gap:40px;padding:14px;background:linear-gradient(transparent,{c['bg_darkest']})}}
.st{{text-align:center}}.sn{{font-size:1.5em;font-weight:800;color:{c['purple_mid']}}}.sl{{color:{c['text_muted']};font-size:.8em}}
.credit{{position:fixed;bottom:6px;right:14px;color:{c['text_muted']};font-size:.7em;opacity:.5;z-index:2}}
.credit a{{color:{c['purple_light']};text-decoration:none}}
</style></head><body>
<canvas id="bg"></canvas>
<div class="wrap">
<div class="logo">OyNIx</div>
<div class="tagline">Nyx-Powered Local AI Browser</div>
<div class="search-wrap">
<input class="search-box" id="si" placeholder="Search the web or ask Nyx AI..." autofocus onkeypress="if(event.key==='Enter')go(this.value)">
<span class="si">&#128270;</span>
<div class="er">
<button class="eb a" onclick="se('nyx',this)">Nyx</button>
<button class="eb" onclick="se('duckduckgo',this)">DuckDuckGo</button>
<button class="eb" onclick="se('google',this)">Google</button>
<button class="eb" onclick="se('brave',this)">Brave</button>
</div></div>
<div class="cards">
<a class="card" href="oyn://ai-chat"><span class="ci">&#129302;</span><div class="ct">AI Chat</div><div class="cs">Local LLM</div></a>
<a class="card" href="oyn://nyx-search"><span class="ci">&#128270;</span><div class="ct">Nyx Search</div><div class="cs">Auto-Indexed</div></a>
<a class="card" href="oyn://settings"><span class="ci">&#9881;</span><div class="ct">Settings</div><div class="cs">Configure</div></a>
<a class="card" href="oyn://database"><span class="ci">&#128218;</span><div class="ct">Site Database</div><div class="cs">Auto-Expands</div></a>
<a class="card" href="oyn://categories"><span class="ci">&#128193;</span><div class="ct">Categories</div><div class="cs">Browse All</div></a>
<a class="card" href="oyn://history"><span class="ci">&#128336;</span><div class="ct">History</div><div class="cs">Recent Pages</div></a>
<a class="card" href="https://duckduckgo.com"><span class="ci">&#129413;</span><div class="ct">DuckDuckGo</div><div class="cs">Private Search</div></a>
<a class="card" href="https://github.com"><span class="ci">&#128025;</span><div class="ct">GitHub</div><div class="cs">Code</div></a>
</div></div>
<div class="ft">
<div class="st"><div class="sn">1400+</div><div class="sl">Curated Sites</div></div>
<div class="st"><div class="sn">Auto</div><div class="sl">Expanding DB</div></div>
<div class="st"><div class="sn">Local</div><div class="sl">AI Model</div></div>
<div class="st"><div class="sn">Nyx</div><div class="sl">Search Engine</div></div>
</div>
<div class="credit">Coded by Claude (Anthropic) &middot; Native Desktop App &middot; <a href="oyn://about">About</a></div>
<script>
var E='nyx';
function se(e,b){{E=e;document.querySelectorAll('.eb').forEach(x=>x.classList.remove('a'));b.classList.add('a');var p={{'nyx':'Search with Nyx...','duckduckgo':'DuckDuckGo...','google':'Google...','brave':'Brave...'}};document.getElementById('si').placeholder=p[e]||'Search...'}}
function go(q){{if(!q.trim())return;window.location='oyn://search?engine='+E+'&q='+encodeURIComponent(q)}}
/* Particle canvas animation */
var c=document.getElementById('bg'),x=c.getContext('2d'),W,H,pts=[];
function resize(){{W=c.width=window.innerWidth;H=c.height=window.innerHeight}}
window.addEventListener('resize',resize);resize();
for(var i=0;i<80;i++)pts.push({{x:Math.random()*W,y:Math.random()*H,r:Math.random()*2+.5,dx:(Math.random()-.5)*.4,dy:(Math.random()-.5)*.4,o:Math.random()*.4+.1}});
function draw(){{
x.clearRect(0,0,W,H);
for(var i=0;i<pts.length;i++){{
var p=pts[i];
p.x+=p.dx;p.y+=p.dy;
if(p.x<0)p.x=W;if(p.x>W)p.x=0;if(p.y<0)p.y=H;if(p.y>H)p.y=0;
x.beginPath();x.arc(p.x,p.y,p.r,0,Math.PI*2);
x.fillStyle='rgba(123,79,191,'+p.o+')';x.fill();
for(var j=i+1;j<pts.length;j++){{
var q=pts[j],d=Math.hypot(p.x-q.x,p.y-q.y);
if(d<130){{x.beginPath();x.moveTo(p.x,p.y);x.lineTo(q.x,q.y);
x.strokeStyle='rgba(123,79,191,'+((.15)*(1-d/130))+')';x.lineWidth=.6;x.stroke()}}
}}}}
requestAnimationFrame(draw)}}
draw();
</script></body></html>'''


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


def get_external_search_theme_css():
    """CSS to inject into external search engines (DDG, Google, Brave) for Nyx theming."""
    c = NYX_COLORS
    return f"""
    body, html {{
        background-color: {c['bg_darkest']} !important;
        color: {c['text_primary']} !important;
    }}
    a {{ color: {c['purple_light']} !important; }}
    a:visited {{ color: {c['purple_soft']} !important; }}
    a:hover {{ color: {c['purple_glow']} !important; }}
    input, textarea, select {{
        background-color: {c['bg_mid']} !important;
        color: {c['text_primary']} !important;
        border: 1px solid {c['border']} !important;
        border-radius: 8px !important;
    }}
    input:focus, textarea:focus {{
        border-color: {c['purple_mid']} !important;
        box-shadow: 0 0 12px rgba(123,79,191,0.3) !important;
    }}
    button, [type="submit"] {{
        background-color: {c['purple_dark']} !important;
        color: {c['purple_pale']} !important;
        border: 1px solid {c['purple_mid']} !important;
        border-radius: 6px !important;
    }}
    /* DuckDuckGo specific */
    .result, .nrn-react-div, .results--main {{
        background-color: {c['bg_dark']} !important;
    }}
    .result__body, .result__snippet {{
        color: {c['text_secondary']} !important;
    }}
    .result__a {{
        color: {c['purple_light']} !important;
    }}
    .result__url {{
        color: {c['text_muted']} !important;
    }}
    .header, .header--aside {{
        background-color: {c['bg_darkest']} !important;
    }}
    /* Google specific */
    #search, #main, .g, .hlcw0c {{
        background-color: {c['bg_dark']} !important;
    }}
    .LC20lb {{ color: {c['purple_light']} !important; }}
    .VwiC3b {{ color: {c['text_secondary']} !important; }}
    .yuRUbf a {{ color: {c['purple_light']} !important; }}
    #searchform, .sfbg {{
        background-color: {c['bg_darkest']} !important;
    }}
    /* Brave specific */
    .snippet, .card {{
        background-color: {c['bg_mid']} !important;
        border-color: {c['border']} !important;
    }}
    .snippet-title {{ color: {c['purple_light']} !important; }}
    .snippet-description {{ color: {c['text_secondary']} !important; }}
    /* General overrides */
    div, section, article, aside, nav, header, footer, main {{
        background-color: inherit !important;
        color: inherit !important;
    }}
    img {{ opacity: 0.9; }}
    * {{
        scrollbar-color: {c['scrollbar']} {c['bg_dark']} !important;
    }}
    ::-webkit-scrollbar {{ width: 8px; background: {c['bg_dark']}; }}
    ::-webkit-scrollbar-thumb {{ background: {c['scrollbar']}; border-radius: 4px; }}
    ::-webkit-scrollbar-thumb:hover {{ background: {c['scrollbar_hover']}; }}
    """


def get_categories_html(categories_data, colors=None):
    """Generate browse-by-category page."""
    c = colors or NYX_COLORS
    cards = ""
    for i, (cat, sites) in enumerate(sorted(categories_data.items())):
        count = len(sites)
        cards += f'''
        <div class="cat-card" style="animation-delay:{i*0.05}s" onclick="toggle(this)">
            <div class="cat-header">
                <span class="cat-name">{cat.replace('_',' ').title()}</span>
                <span class="cat-count">{count}</span>
            </div>
            <div class="cat-sites" style="display:none">
                {''.join(f'<a class="site" href="{s[1]}">{s[0]}</a>' for s in sites[:20])}
                {f'<div class="more">+{count-20} more</div>' if count > 20 else ''}
            </div>
        </div>'''

    return f'''<!DOCTYPE html><html><head><meta charset="utf-8"><title>Browse Categories</title>
<style>
*{{margin:0;padding:0;box-sizing:border-box}}
body{{background:{c['bg_darkest']};color:{c['text_primary']};font-family:'Segoe UI',sans-serif;padding:30px}}
h1{{text-align:center;font-size:2em;margin-bottom:30px;background:linear-gradient(135deg,{c['purple_mid']},{c['purple_glow']});-webkit-background-clip:text;-webkit-text-fill-color:transparent}}
.grid{{display:grid;grid-template-columns:repeat(auto-fill,minmax(280px,1fr));gap:12px;max-width:1200px;margin:0 auto}}
.cat-card{{background:{c['bg_mid']};border:1px solid {c['border']};border-radius:12px;padding:16px;cursor:pointer;transition:all .3s;opacity:0;animation:fadeUp .4s ease forwards}}
.cat-card:hover{{border-color:{c['purple_mid']};transform:translateY(-2px)}}
@keyframes fadeUp{{from{{opacity:0;transform:translateY(10px)}}to{{opacity:1;transform:translateY(0)}}}}
.cat-header{{display:flex;justify-content:space-between;align-items:center}}
.cat-name{{font-weight:600;color:{c['purple_light']};font-size:1.05em}}
.cat-count{{background:{c['purple_dark']};color:{c['purple_pale']};border-radius:10px;padding:2px 10px;font-size:.8em}}
.cat-sites{{margin-top:12px;display:flex;flex-direction:column;gap:4px}}
.site{{color:{c['text_secondary']};text-decoration:none;padding:4px 8px;border-radius:6px;font-size:.85em;transition:all .2s}}
.site:hover{{background:{c['bg_lighter']};color:{c['purple_light']}}}
.more{{color:{c['text_muted']};font-size:.8em;text-align:center;padding:4px}}
</style></head><body>
<h1>Browse Categories</h1>
<div class="grid">{cards}</div>
<script>function toggle(el){{var s=el.querySelector('.cat-sites');s.style.display=s.style.display==='none'?'flex':'none'}}</script>
</body></html>'''


def get_history_html(history_entries, colors=None):
    """Generate history page grouped by date."""
    c = colors or NYX_COLORS
    from datetime import datetime

    # Group by date
    grouped = {}
    for entry in history_entries:
        ts = entry.get('timestamp', 0)
        try:
            date_key = datetime.fromtimestamp(ts).strftime('%Y-%m-%d')
            date_label = datetime.fromtimestamp(ts).strftime('%B %d, %Y')
        except (ValueError, OSError):
            date_key = 'Unknown'
            date_label = 'Unknown Date'
        grouped.setdefault((date_key, date_label), []).append(entry)

    sections = ""
    for i, ((date_key, date_label), entries) in enumerate(
            sorted(grouped.items(), key=lambda x: x[0][0], reverse=True)):
        items = ""
        for e in entries:
            title = e.get('title', e.get('url', ''))
            url = e.get('url', '')
            visits = e.get('visit_count', 1)
            items += f'''<a class="h-item" href="{url}">
                <span class="h-title">{title}</span>
                <span class="h-url">{url}</span>
                <span class="h-visits">{visits}x</span>
            </a>'''
        sections += f'''
        <div class="h-section" style="animation-delay:{i*0.1}s">
            <div class="h-date" onclick="this.parentElement.classList.toggle('collapsed')">{date_label}</div>
            <div class="h-entries">{items}</div>
        </div>'''

    return f'''<!DOCTYPE html><html><head><meta charset="utf-8"><title>History</title>
<style>
*{{margin:0;padding:0;box-sizing:border-box}}
body{{background:{c['bg_darkest']};color:{c['text_primary']};font-family:'Segoe UI',sans-serif;padding:30px}}
h1{{text-align:center;font-size:2em;margin-bottom:30px;background:linear-gradient(135deg,{c['purple_mid']},{c['purple_glow']});-webkit-background-clip:text;-webkit-text-fill-color:transparent}}
.container{{max-width:900px;margin:0 auto}}
.h-section{{margin-bottom:16px;opacity:0;animation:fadeUp .4s ease forwards}}
.h-section.collapsed .h-entries{{display:none}}
@keyframes fadeUp{{from{{opacity:0;transform:translateY(10px)}}to{{opacity:1;transform:translateY(0)}}}}
.h-date{{font-size:1.1em;font-weight:600;color:{c['purple_light']};padding:10px 16px;background:{c['bg_mid']};border-radius:10px;cursor:pointer;margin-bottom:6px;transition:all .2s}}
.h-date:hover{{background:{c['bg_light']}}}
.h-entries{{display:flex;flex-direction:column;gap:2px;padding-left:12px}}
.h-item{{display:flex;align-items:center;gap:12px;padding:8px 14px;border-radius:8px;text-decoration:none;transition:all .2s}}
.h-item:hover{{background:{c['bg_lighter']}}}
.h-title{{color:{c['text_primary']};font-size:.9em;flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}}
.h-url{{color:{c['text_muted']};font-size:.75em;max-width:250px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}}
.h-visits{{color:{c['purple_soft']};font-size:.75em;background:{c['purple_dark']};padding:2px 8px;border-radius:8px}}
</style></head><body>
<h1>Browsing History</h1>
<div class="container">{sections}</div>
</body></html>'''

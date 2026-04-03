"""
OyNIx Browser - Nyx Theme Engine v2.1.2
Modern glassmorphism UI with dynamic mouse-tracking refractions,
GPU-accelerated glass effects, enhanced animations, custom SVG icons,
and multiple theme variants. Deep blacks + medium purple accents.
"""


# ── Nyx Purple/Black Palette ──────────────────────────────────────────
NYX_COLORS = {
    'bg_darkest':       '#08080d',
    'bg_dark':          '#0e0e16',
    'bg_mid':           '#16161f',
    'bg_light':         '#1e1e2a',
    'bg_lighter':       '#282836',
    'bg_surface':       '#303042',
    'bg_glass':         'rgba(18, 18, 28, 0.85)',
    'bg_glass_light':   'rgba(30, 30, 46, 0.7)',
    'purple_dark':      '#4a2d7a',
    'purple_mid':       '#7B4FBF',
    'purple_light':     '#9B6FDF',
    'purple_glow':      '#B088F0',
    'purple_soft':      '#C9A8F0',
    'purple_pale':      '#E0D0F8',
    'text_primary':     '#E8E0F0',
    'text_secondary':   '#A8A0B8',
    'text_muted':       '#605878',
    'text_accent':      '#C9A8F0',
    'success':          '#6FCF97',
    'warning':          '#F2C94C',
    'error':            '#EB5757',
    'info':             '#7B4FBF',
    'glow':             'rgba(123, 79, 191, 0.35)',
    'glow_strong':      'rgba(123, 79, 191, 0.6)',
    'border':           'rgba(58, 58, 74, 0.6)',
    'border_active':    '#7B4FBF',
    'selection':        'rgba(123, 79, 191, 0.25)',
    'scrollbar':        '#3a2560',
    'scrollbar_hover':  '#7B4FBF',
}

THEMES = {
    'nyx': NYX_COLORS,
    'nyx_midnight': {**NYX_COLORS,
        'bg_darkest': '#030308', 'bg_dark': '#080810', 'bg_mid': '#0c0c18',
        'purple_mid': '#5a3a9a', 'purple_glow': '#8060c0'},
    'nyx_violet': {**NYX_COLORS,
        'purple_mid': '#8855CC', 'purple_light': '#AA77EE',
        'purple_glow': '#CC99FF', 'purple_soft': '#DDBBFF'},
    'nyx_amethyst': {**NYX_COLORS,
        'bg_darkest': '#0d0810', 'bg_dark': '#150e1a',
        'purple_mid': '#9966CC', 'purple_light': '#BB88DD'},
    'nyx_ember': {**NYX_COLORS,
        'purple_mid': '#BF4F4F', 'purple_light': '#DF6F6F',
        'purple_glow': '#F08888', 'purple_soft': '#F0A8A8',
        'purple_dark': '#7a2d2d', 'border_active': '#BF4F4F'},
}


def get_theme(name='nyx'):
    return THEMES.get(name, THEMES['nyx'])


def get_qt_stylesheet(colors=None):
    """Generate the full Qt stylesheet for OyNIx browser."""
    c = colors or NYX_COLORS
    return f"""
    QMainWindow {{ background-color: {c['bg_darkest']}; color: {c['text_primary']}; }}
    QWidget {{ background-color: {c['bg_dark']}; color: {c['text_primary']};
        font-family: 'Segoe UI', 'Ubuntu', 'Cantarell', sans-serif; font-size: 13px; }}

    /* Menu Bar */
    QMenuBar {{ background: {c['bg_darkest']}; color: {c['text_primary']};
        border-bottom: 1px solid {c['border']}; padding: 2px; }}
    QMenuBar::item {{ padding: 6px 14px; border-radius: 6px; }}
    QMenuBar::item:selected {{ background: {c['purple_dark']}; color: {c['purple_pale']}; }}
    QMenu {{ background: {c['bg_mid']}; color: {c['text_primary']};
        border: 1px solid {c['border']}; border-radius: 10px; padding: 6px; }}
    QMenu::item {{ padding: 8px 30px 8px 20px; border-radius: 6px; margin: 1px 2px; }}
    QMenu::item:selected {{ background: {c['purple_dark']}; color: {c['purple_pale']}; }}
    QMenu::separator {{ height: 1px; background: {c['border']}; margin: 4px 12px; }}

    /* Toolbar */
    QToolBar {{ background: {c['bg_darkest']}; border: none; padding: 4px 8px; spacing: 2px; }}
    QToolBar::separator {{ width: 1px; background: {c['border']}; margin: 4px 6px; }}

    /* URL Bar */
    QLineEdit {{ background: {c['bg_mid']}; color: {c['text_primary']};
        border: 2px solid {c['border']}; border-radius: 22px; padding: 9px 18px; font-size: 14px;
        selection-background-color: {c['selection']}; }}
    QLineEdit:focus {{ border-color: {c['purple_mid']}; background: {c['bg_light']}; }}

    /* Buttons */
    QPushButton {{ background: {c['bg_lighter']}; color: {c['text_primary']};
        border: 1px solid {c['border']}; border-radius: 10px; padding: 8px 14px;
        font-weight: 600; min-width: 28px; }}
    QPushButton:hover {{ background: {c['purple_dark']}; border-color: {c['purple_mid']};
        color: {c['purple_pale']}; }}
    QPushButton:pressed {{ background: {c['purple_mid']}; color: {c['bg_darkest']}; }}
    QPushButton#navBtn {{ background: transparent; border: none; border-radius: 8px;
        padding: 6px; min-width: 34px; min-height: 34px; }}
    QPushButton#navBtn:hover {{ background: {c['bg_lighter']}; }}
    QPushButton#navBtn:pressed {{ background: {c['purple_dark']}; }}
    QPushButton#accentBtn {{ background: {c['purple_mid']}; color: {c['bg_darkest']};
        border: none; font-weight: bold; }}
    QPushButton#accentBtn:hover {{ background: {c['purple_light']}; }}
    QPushButton#pillBtn {{ background: {c['bg_mid']}; border: 1px solid {c['border']};
        border-radius: 14px; padding: 5px 14px; font-size: 12px; }}
    QPushButton#pillBtn:hover {{ border-color: {c['purple_mid']}; color: {c['purple_light']}; }}

    /* Tabs */
    QTabWidget::pane {{ border: none; background: {c['bg_darkest']}; }}
    QTabBar {{ background: {c['bg_darkest']}; }}
    QTabBar::tab {{ background: {c['bg_mid']}; color: {c['text_secondary']};
        padding: 8px 20px; margin-right: 1px; border-top-left-radius: 10px;
        border-top-right-radius: 10px; border: 1px solid {c['border']}; border-bottom: none;
        min-width: 100px; max-width: 220px; }}
    QTabBar::tab:selected {{ background: {c['bg_light']}; color: {c['purple_light']};
        border-color: {c['purple_mid']}; }}
    QTabBar::tab:hover:!selected {{ background: {c['bg_lighter']}; color: {c['text_primary']}; }}
    QTabBar::close-button {{ subcontrol-position: right; border-radius: 4px; padding: 2px; }}
    QTabBar::close-button:hover {{ background: {c['error']}; }}

    /* Tree Tabs */
    QTreeWidget {{ background: {c['bg_dark']}; color: {c['text_primary']};
        border: none; outline: none; font-size: 12px; }}
    QTreeWidget::item {{ padding: 7px 8px; border-radius: 6px; margin: 1px 4px; }}
    QTreeWidget::item:selected {{ background: {c['purple_dark']}; color: {c['purple_pale']}; }}
    QTreeWidget::item:hover:!selected {{ background: {c['bg_lighter']}; }}
    QTreeWidget::branch {{ background: transparent; }}
    QTreeWidget::branch:has-siblings:!adjoins-item {{ border-image: none; }}
    QTreeWidget::branch:has-children:closed,
    QTreeWidget::branch:has-children:open {{ image: none; }}

    /* Scrollbars */
    QScrollBar:vertical {{ background: transparent; width: 8px; }}
    QScrollBar::handle:vertical {{ background: {c['scrollbar']}; border-radius: 4px; min-height: 30px; }}
    QScrollBar::handle:vertical:hover {{ background: {c['scrollbar_hover']}; }}
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {{ height: 0; }}
    QScrollBar:horizontal {{ background: transparent; height: 8px; }}
    QScrollBar::handle:horizontal {{ background: {c['scrollbar']}; border-radius: 4px; min-width: 30px; }}
    QScrollBar::handle:horizontal:hover {{ background: {c['scrollbar_hover']}; }}
    QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {{ width: 0; }}

    /* Status Bar */
    QStatusBar {{ background: {c['bg_darkest']}; color: {c['text_muted']};
        border-top: 1px solid {c['border']}; font-size: 11px; padding: 2px 8px; }}
    QStatusBar::item {{ border: none; }}

    /* Dialogs & Groups */
    QDialog {{ background: {c['bg_dark']}; color: {c['text_primary']}; border-radius: 14px; }}
    QGroupBox {{ font-weight: bold; border: 1px solid {c['border']}; border-radius: 10px;
        margin-top: 14px; padding-top: 18px; color: {c['purple_light']}; }}
    QGroupBox::title {{ subcontrol-origin: margin; subcontrol-position: top left;
        padding: 4px 12px; background: {c['bg_dark']}; border-radius: 6px; }}

    /* Inputs */
    QCheckBox {{ spacing: 8px; }}
    QCheckBox::indicator {{ width: 20px; height: 20px; border-radius: 6px;
        border: 2px solid {c['border']}; background: {c['bg_mid']}; }}
    QCheckBox::indicator:checked {{ background: {c['purple_mid']}; border-color: {c['purple_mid']}; }}
    QComboBox {{ background: {c['bg_mid']}; color: {c['text_primary']};
        border: 1px solid {c['border']}; border-radius: 8px; padding: 7px 14px; }}
    QComboBox:hover {{ border-color: {c['purple_mid']}; }}
    QComboBox QAbstractItemView {{ background: {c['bg_mid']}; color: {c['text_primary']};
        border: 1px solid {c['border']}; selection-background-color: {c['purple_dark']}; }}
    QSpinBox, QDoubleSpinBox {{ background: {c['bg_mid']}; color: {c['text_primary']};
        border: 1px solid {c['border']}; border-radius: 8px; padding: 5px 10px; }}
    QSlider::groove:horizontal {{ height: 6px; background: {c['bg_lighter']}; border-radius: 3px; }}
    QSlider::handle:horizontal {{ background: {c['purple_mid']}; width: 18px; height: 18px;
        margin: -6px 0; border-radius: 9px; }}
    QSlider::handle:horizontal:hover {{ background: {c['purple_light']}; }}

    /* Splitter / TextEdit / Labels / Tooltips */
    QSplitter::handle {{ background: {c['border']}; width: 2px; }}
    QSplitter::handle:hover {{ background: {c['purple_mid']}; }}
    QTextEdit, QPlainTextEdit {{ background: {c['bg_mid']}; color: {c['text_primary']};
        border: 1px solid {c['border']}; border-radius: 10px; padding: 8px;
        selection-background-color: {c['selection']}; }}
    QTextEdit:focus, QPlainTextEdit:focus {{ border-color: {c['purple_mid']}; }}
    QLabel {{ color: {c['text_primary']}; background: transparent; }}
    QLabel#sectionHeader {{ color: {c['purple_light']}; font-size: 15px; font-weight: bold; }}
    QToolTip {{ background: {c['bg_mid']}; color: {c['text_primary']};
        border: 1px solid {c['purple_mid']}; border-radius: 8px; padding: 6px 10px; }}
    QMessageBox {{ background: {c['bg_dark']}; }}

    /* List Widget (settings sidebar) */
    QListWidget {{ background: {c['bg_dark']}; border: none; outline: none; }}
    QListWidget::item {{ padding: 10px 16px; border-radius: 8px; margin: 2px 4px;
        color: {c['text_secondary']}; }}
    QListWidget::item:selected {{ background: {c['purple_dark']}; color: {c['purple_pale']}; }}
    QListWidget::item:hover:!selected {{ background: {c['bg_lighter']}; }}

    /* Progress Bar */
    QProgressBar {{ background: {c['bg_lighter']}; border: none; border-radius: 4px;
        height: 6px; text-align: center; }}
    QProgressBar::chunk {{ background: {c['purple_mid']}; border-radius: 4px; }}
    """


def _refraction_css(c):
    """Shared CSS for dynamic mouse-tracking refractions across all pages."""
    return f'''
/* ── Dynamic Mouse Refractions (GPU-accelerated) ── */
.refract-target {{
  position: relative;
  overflow: hidden;
  transform: translateZ(0);
  will-change: transform;
}}
.refract-target::before {{
  content: '';
  position: absolute;
  inset: -1px;
  border-radius: inherit;
  background: radial-gradient(
    600px circle at var(--mx, 50%) var(--my, 50%),
    rgba(123, 79, 191, 0.18),
    rgba(155, 111, 223, 0.08) 25%,
    transparent 55%
  );
  opacity: 0;
  transition: opacity 0.35s ease;
  pointer-events: none;
  z-index: 1;
  mix-blend-mode: screen;
}}
.refract-target:hover::before {{
  opacity: 1;
}}
/* Secondary shimmer layer */
.refract-target::after {{
  content: '';
  position: absolute;
  inset: -1px;
  border-radius: inherit;
  background: radial-gradient(
    350px circle at var(--mx, 50%) var(--my, 50%),
    rgba(201, 168, 240, 0.12),
    transparent 40%
  );
  opacity: 0;
  transition: opacity 0.3s ease;
  pointer-events: none;
  z-index: 2;
}}
.refract-target:hover::after {{
  opacity: 1;
}}
/* Border glow on hover */
.refract-glow {{
  transition: border-color 0.4s ease, box-shadow 0.4s ease, transform 0.4s cubic-bezier(.4,0,.2,1);
}}
.refract-glow:hover {{
  border-color: {c['purple_mid']} !important;
  box-shadow: 0 0 20px rgba(123, 79, 191, 0.15),
              0 8px 32px rgba(123, 79, 191, 0.1),
              inset 0 0 20px rgba(123, 79, 191, 0.03);
}}

/* ── Page-level ambient refraction (follows mouse globally) ── */
#page-refraction {{
  position: fixed;
  width: 500px;
  height: 500px;
  border-radius: 50%;
  background: radial-gradient(circle, rgba(123,79,191,0.07), transparent 70%);
  pointer-events: none;
  z-index: 0;
  transform: translate(-50%, -50%) translateZ(0);
  will-change: left, top;
  filter: blur(40px);
  transition: left 0.15s ease-out, top 0.15s ease-out;
}}

/* ── Glass orb enhancements ── */
.glass-orb {{
  position: fixed;
  border-radius: 50%;
  pointer-events: none;
  z-index: 0;
  filter: blur(80px);
  opacity: 0.09;
  transform: translateZ(0);
  will-change: transform;
  animation: orbFloat 18s ease-in-out infinite alternate;
}}
.glass-orb:nth-child(2) {{ animation-duration: 22s; animation-delay: -6s; }}
.glass-orb:nth-child(3) {{ animation-duration: 26s; animation-delay: -12s; }}
@keyframes orbFloat {{
  0%   {{ transform: translate(0, 0) scale(1) translateZ(0); }}
  33%  {{ transform: translate(30px, -40px) scale(1.05) translateZ(0); }}
  66%  {{ transform: translate(-20px, 20px) scale(0.95) translateZ(0); }}
  100% {{ transform: translate(15px, -15px) scale(1.02) translateZ(0); }}
}}

/* ── Enhanced entrance animations ── */
@keyframes fadeIn {{
  from {{ opacity: 0; }}
  to {{ opacity: 1; }}
}}
@keyframes slideUp {{
  from {{ opacity: 0; transform: translateY(20px) translateZ(0); }}
  to {{ opacity: 1; transform: translateY(0) translateZ(0); }}
}}
@keyframes scaleIn {{
  from {{ opacity: 0; transform: scale(0.88) translateZ(0); }}
  to {{ opacity: 1; transform: scale(1) translateZ(0); }}
}}
@keyframes fadeUp {{
  from {{ opacity: 0; transform: translateY(10px) translateZ(0); }}
  to {{ opacity: 1; transform: translateY(0) translateZ(0); }}
}}
@keyframes shimmerIn {{
  0% {{ opacity: 0; transform: translateY(16px) scale(0.97) translateZ(0); filter: blur(4px); }}
  100% {{ opacity: 1; transform: translateY(0) scale(1) translateZ(0); filter: blur(0); }}
}}
@keyframes glowPulse {{
  from {{ filter: drop-shadow(0 0 12px rgba(123,79,191,.15)); }}
  to {{ filter: drop-shadow(0 0 30px rgba(123,79,191,.4)); }}
}}
@keyframes borderShine {{
  0% {{ border-color: rgba(58,58,74,.3); }}
  50% {{ border-color: rgba(123,79,191,.3); }}
  100% {{ border-color: rgba(58,58,74,.3); }}
}}
'''


def _refraction_js():
    """Shared JS for dynamic mouse-tracking refractions across all pages."""
    return '''
/* ── Dynamic Mouse Refraction Engine ── */
(function() {
  // Page-level ambient light
  var ambient = document.getElementById('page-refraction');
  if (!ambient) {
    ambient = document.createElement('div');
    ambient.id = 'page-refraction';
    document.body.appendChild(ambient);
  }

  // Throttled mouse tracking for performance
  var lastMove = 0;
  document.addEventListener('mousemove', function(e) {
    var now = Date.now();
    if (now - lastMove < 16) return; // ~60fps cap
    lastMove = now;

    // Move ambient light
    ambient.style.left = e.clientX + 'px';
    ambient.style.top = e.clientY + 'px';
  }, { passive: true });

  // Per-element refraction tracking
  function setupRefractions() {
    document.querySelectorAll('.refract-target').forEach(function(el) {
      if (el._refractBound) return;
      el._refractBound = true;
      el.addEventListener('mousemove', function(ev) {
        var b = el.getBoundingClientRect();
        var x = ((ev.clientX - b.left) / b.width * 100);
        var y = ((ev.clientY - b.top) / b.height * 100);
        el.style.setProperty('--mx', x + '%');
        el.style.setProperty('--my', y + '%');
      }, { passive: true });
    });
  }

  setupRefractions();

  // Re-run on DOM changes (infinite scroll, dynamic content)
  var observer = new MutationObserver(function() {
    requestAnimationFrame(setupRefractions);
  });
  observer.observe(document.body, { childList: true, subtree: true });
})();
'''


# ── SVG icons for HTML pages ────────────────────────────────────────
_SVG = {
    'ai': '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"><path d="M12 2L14 8 20 8 15 12 17 18 12 14 7 18 9 12 4 8 10 8Z"/></svg>',
    'search': '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"><circle cx="11" cy="11" r="8"/><line x1="21" y1="21" x2="16.65" y2="16.65"/></svg>',
    'settings': '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"><circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 1 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 1 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 1 1-2.83-2.83l.06-.06A1.65 1.65 0 0 0 4.68 15a1.65 1.65 0 0 0-1.51-1H3a2 2 0 1 1 0-4h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 1 1 2.83-2.83l.06.06A1.65 1.65 0 0 0 9 4.68a1.65 1.65 0 0 0 1-1.51V3a2 2 0 1 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 1 1 2.83 2.83l-.06.06A1.65 1.65 0 0 0 19.4 9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 1 1 0 4h-.09a1.65 1.65 0 0 0-1.51 1z"/></svg>',
    'database': '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"><ellipse cx="12" cy="5" rx="9" ry="3"/><path d="M21 12c0 1.66-4 3-9 3s-9-1.34-9-3"/><path d="M3 5v14c0 1.66 4 3 9 3s9-1.34 9-3V5"/></svg>',
    'folder': '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"><path d="M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z"/></svg>',
    'history': '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"><circle cx="12" cy="12" r="10"/><polyline points="12 6 12 12 16 14"/></svg>',
    'bookmark': '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"><path d="M19 21l-7-5-7 5V5a2 2 0 0 1 2-2h10a2 2 0 0 1 2 2z"/></svg>',
    'download': '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="7 10 12 15 17 10"/><line x1="12" y1="15" x2="12" y2="3"/></svg>',
    'globe': '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"><circle cx="12" cy="12" r="10"/><line x1="2" y1="12" x2="22" y2="12"/><path d="M12 2a15.3 15.3 0 0 1 4 10 15.3 15.3 0 0 1-4 10 15.3 15.3 0 0 1-4-10 15.3 15.3 0 0 1 4-10z"/></svg>',
    'github': '<svg viewBox="0 0 24 24" fill="currentColor"><path d="M12 0C5.37 0 0 5.37 0 12c0 5.3 3.44 9.8 8.2 11.39.6.11.82-.26.82-.58v-2.03c-3.34.73-4.04-1.61-4.04-1.61-.55-1.39-1.34-1.76-1.34-1.76-1.09-.75.08-.73.08-.73 1.21.08 1.85 1.24 1.85 1.24 1.07 1.84 2.81 1.31 3.5 1 .11-.78.42-1.31.76-1.61-2.67-.3-5.47-1.33-5.47-5.93 0-1.31.47-2.38 1.24-3.22-.12-.3-.54-1.52.12-3.18 0 0 1.01-.32 3.3 1.23a11.5 11.5 0 0 1 6.02 0c2.28-1.55 3.29-1.23 3.29-1.23.66 1.66.24 2.88.12 3.18.77.84 1.24 1.91 1.24 3.22 0 4.61-2.81 5.63-5.48 5.92.43.37.81 1.1.81 2.22v3.29c0 .32.21.7.82.58A12.01 12.01 0 0 0 24 12c0-6.63-5.37-12-12-12z"/></svg>',
}


def get_homepage_html(colors=None):
    """Generate the Nyx-themed homepage with dynamic refractions and particle canvas."""
    c = colors or NYX_COLORS
    rcss = _refraction_css(c)
    rjs = _refraction_js()
    return f'''<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>OyNIx</title>
<style>
*{{margin:0;padding:0;box-sizing:border-box}}
body{{background:{c['bg_darkest']};color:{c['text_primary']};font-family:'Segoe UI','Ubuntu',sans-serif;
  min-height:100vh;overflow-x:hidden}}
canvas#bg{{position:fixed;top:0;left:0;width:100%;height:100%;z-index:0}}
.wrap{{position:relative;z-index:1;display:flex;flex-direction:column;align-items:center;
  justify-content:center;min-height:100vh;padding:30px 20px}}

{rcss}

/* Logo */
.logo{{font-size:5em;font-weight:900;letter-spacing:-2px;
  background:linear-gradient(135deg,{c['purple_mid']},{c['purple_glow']},{c['purple_soft']});
  -webkit-background-clip:text;-webkit-text-fill-color:transparent;
  opacity:0;animation:fadeIn .7s ease forwards, glowPulse 4s 1s ease-in-out infinite alternate;
  transform:translateZ(0)}}
.tagline{{color:{c['text_muted']};font-size:.9em;letter-spacing:6px;text-transform:uppercase;
  margin-bottom:36px;opacity:0;animation:shimmerIn .8s .2s ease forwards}}

/* Search */
.search-wrap{{position:relative;width:580px;max-width:88vw;margin-bottom:44px;
  opacity:0;animation:slideUp .6s .4s ease forwards}}
.search-box{{width:100%;padding:15px 52px 15px 20px;font-size:1.05em;
  background:{c['bg_mid']};border:2px solid rgba(58,58,74,0.4);border-radius:50px;
  color:{c['text_primary']};outline:none;
  transition:all .4s cubic-bezier(.4,0,.2,1);
  backdrop-filter:blur(12px)}}
.search-box:focus{{border-color:{c['purple_mid']};
  box-shadow:0 0 40px rgba(123,79,191,.3),0 0 80px rgba(123,79,191,.08),inset 0 0 20px rgba(123,79,191,.05);
  background:{c['bg_light']}}}
.search-box::placeholder{{color:{c['text_muted']}}}
.search-icon{{position:absolute;right:16px;top:50%;transform:translateY(-50%);
  width:22px;height:22px;color:{c['text_muted']};opacity:.6;transition:all .3s}}
.search-box:focus ~ .search-icon{{color:{c['purple_light']};opacity:1}}
.engines{{display:flex;gap:8px;justify-content:center;margin-top:12px}}
.eng{{padding:5px 16px;border-radius:20px;border:1px solid rgba(58,58,74,0.4);
  background:transparent;color:{c['text_secondary']};cursor:pointer;font-size:.82em;
  transition:all .3s cubic-bezier(.4,0,.2,1);backdrop-filter:blur(5px)}}
.eng:hover,.eng.a{{border-color:{c['purple_mid']};color:{c['purple_light']};
  background:rgba(123,79,191,.1);transform:translateY(-1px)}}
.eng.a{{background:rgba(123,79,191,.15);color:{c['purple_pale']};font-weight:600;
  box-shadow:0 2px 12px rgba(123,79,191,.15)}}

/* Cards — use refract-target for mouse tracking */
.cards{{display:grid;grid-template-columns:repeat(4,1fr);gap:12px;max-width:780px;width:100%}}
.card{{background:rgba(22,22,31,.6);border:1px solid rgba(58,58,74,0.3);border-radius:16px;
  padding:24px 12px 18px;text-align:center;cursor:pointer;
  transition:all .45s cubic-bezier(.4,0,.2,1);
  text-decoration:none;opacity:0;animation:shimmerIn .5s ease forwards;
  backdrop-filter:blur(10px);transform:translateZ(0);will-change:transform}}
.card:nth-child(1){{animation-delay:.5s}}.card:nth-child(2){{animation-delay:.57s}}
.card:nth-child(3){{animation-delay:.64s}}.card:nth-child(4){{animation-delay:.71s}}
.card:nth-child(5){{animation-delay:.78s}}.card:nth-child(6){{animation-delay:.85s}}
.card:nth-child(7){{animation-delay:.92s}}.card:nth-child(8){{animation-delay:.99s}}
.card:hover{{transform:translateY(-8px) scale(1.04) translateZ(0);border-color:{c['purple_mid']};
  box-shadow:0 20px 50px rgba(123,79,191,.22),0 0 30px rgba(123,79,191,.08)}}
.card-icon{{width:32px;height:32px;margin:0 auto 10px;color:{c['purple_light']};
  opacity:.85;transition:all .4s cubic-bezier(.4,0,.2,1)}}
.card:hover .card-icon{{color:{c['purple_glow']};opacity:1;transform:scale(1.15) translateZ(0);
  filter:drop-shadow(0 0 8px rgba(123,79,191,.4))}}
.card-title{{color:{c['text_primary']};font-weight:600;font-size:.88em;
  transition:color .3s}}
.card:hover .card-title{{color:{c['purple_pale']}}}
.card-sub{{color:{c['text_muted']};font-size:.72em;margin-top:3px}}

/* Footer stats */
.footer{{position:fixed;bottom:0;left:0;right:0;display:flex;justify-content:center;gap:48px;
  padding:16px;background:linear-gradient(transparent,{c['bg_darkest']} 60%)}}
.stat{{text-align:center}}.stat-num{{font-size:1.4em;font-weight:800;
  color:{c['purple_mid']}}}.stat-label{{color:{c['text_muted']};font-size:.75em}}
.credit{{position:fixed;bottom:6px;right:14px;color:{c['text_muted']};font-size:.65em;opacity:.4}}
.credit a{{color:{c['purple_light']};text-decoration:none}}
</style></head><body>
<canvas id="bg"></canvas>
<div class="glass-orb" style="width:450px;height:450px;background:{c['purple_mid']};top:8%;left:-5%"></div>
<div class="glass-orb" style="width:350px;height:350px;background:{c['purple_glow']};bottom:3%;right:-4%"></div>
<div class="glass-orb" style="width:280px;height:280px;background:{c['purple_dark']};top:50%;left:60%"></div>
<div class="wrap">
<div class="logo">OyNIx</div>
<div class="tagline">Nyx-Powered Local AI Browser</div>
<div class="search-wrap refract-target refract-glow" style="border-radius:52px">
<input class="search-box" id="si" placeholder="Search the web or ask Nyx AI..." autofocus
  onkeypress="if(event.key==='Enter')go(this.value)">
<div class="search-icon">{_SVG['search']}</div>
<div class="engines">
<button class="eng a" onclick="se('nyx',this)">Nyx</button>
<button class="eng" onclick="se('duckduckgo',this)">DuckDuckGo</button>
<button class="eng" onclick="se('google',this)">Google</button>
<button class="eng" onclick="se('brave',this)">Brave</button>
</div></div>
<div class="cards">
<a class="card refract-target refract-glow" href="oyn://ai-chat"><div class="card-icon">{_SVG['ai']}</div><div class="card-title">AI Chat</div><div class="card-sub">Local LLM</div></a>
<a class="card refract-target refract-glow" href="oyn://nyx-search"><div class="card-icon">{_SVG['search']}</div><div class="card-title">Nyx Search</div><div class="card-sub">Auto-Indexed</div></a>
<a class="card refract-target refract-glow" href="oyn://bookmarks"><div class="card-icon">{_SVG['bookmark']}</div><div class="card-title">Bookmarks</div><div class="card-sub">Your Saves</div></a>
<a class="card refract-target refract-glow" href="oyn://downloads"><div class="card-icon">{_SVG['download']}</div><div class="card-title">Downloads</div><div class="card-sub">Manage Files</div></a>
<a class="card refract-target refract-glow" href="oyn://database"><div class="card-icon">{_SVG['database']}</div><div class="card-title">Site Database</div><div class="card-sub">1400+ Sites</div></a>
<a class="card refract-target refract-glow" href="oyn://history"><div class="card-icon">{_SVG['history']}</div><div class="card-title">History</div><div class="card-sub">Recent Pages</div></a>
<a class="card refract-target refract-glow" href="oyn://settings"><div class="card-icon">{_SVG['settings']}</div><div class="card-title">Settings</div><div class="card-sub">Configure</div></a>
<a class="card refract-target refract-glow" href="https://github.com"><div class="card-icon">{_SVG['github']}</div><div class="card-title">GitHub</div><div class="card-sub">Code</div></a>
</div></div>
<div class="footer">
<div class="stat"><div class="stat-num">1400+</div><div class="stat-label">Curated Sites</div></div>
<div class="stat"><div class="stat-num">Auto</div><div class="stat-label">Expanding DB</div></div>
<div class="stat"><div class="stat-num">Local</div><div class="stat-label">AI Model</div></div>
<div class="stat"><div class="stat-num">Nyx</div><div class="stat-label">Search Engine</div></div>
</div>
<div class="credit">Coded by Claude &middot; <a href="oyn://about">About</a></div>
<script>
var E='nyx';
function se(e,b){{E=e;document.querySelectorAll('.eng').forEach(x=>x.classList.remove('a'));
  b.classList.add('a');
  var p={{'nyx':'Search with Nyx...','duckduckgo':'DuckDuckGo...','google':'Google...','brave':'Brave...'}};
  document.getElementById('si').placeholder=p[e]||'Search...'}}
function go(q){{if(!q.trim())return;window.location='oyn://search?engine='+E+'&q='+encodeURIComponent(q)}}
/* Particle canvas */
var c=document.getElementById('bg'),x=c.getContext('2d'),W,H,pts=[];
function resize(){{W=c.width=window.innerWidth;H=c.height=window.innerHeight}}
window.addEventListener('resize',resize);resize();
for(var i=0;i<90;i++)pts.push({{x:Math.random()*W,y:Math.random()*H,
  r:Math.random()*1.8+.4,dx:(Math.random()-.5)*.35,dy:(Math.random()-.5)*.35,
  o:Math.random()*.35+.08}});
function draw(){{
x.clearRect(0,0,W,H);
for(var i=0;i<pts.length;i++){{var p=pts[i];p.x+=p.dx;p.y+=p.dy;
if(p.x<0)p.x=W;if(p.x>W)p.x=0;if(p.y<0)p.y=H;if(p.y>H)p.y=0;
x.beginPath();x.arc(p.x,p.y,p.r,0,Math.PI*2);
x.fillStyle='rgba(123,79,191,'+p.o+')';x.fill();
for(var j=i+1;j<pts.length;j++){{var q=pts[j],d=Math.hypot(p.x-q.x,p.y-q.y);
if(d<120){{x.beginPath();x.moveTo(p.x,p.y);x.lineTo(q.x,q.y);
x.strokeStyle='rgba(123,79,191,'+((.12)*(1-d/120))+')';x.lineWidth=.5;x.stroke()}}
}}}}requestAnimationFrame(draw)}}draw();
{rjs}
</script></body></html>'''


def get_search_results_html(query, local_results, web_results, colors=None):
    """Generate Nyx-themed search results page with refractions and infinite scroll."""
    c = colors or NYX_COLORS
    rcss = _refraction_css(c)
    rjs = _refraction_js()
    import json as _json

    all_local = _json.dumps([{
        'url': r.get('url', ''), 'title': r.get('title', ''),
        'description': r.get('description', ''), 'category': r.get('category', ''),
        'score': r.get('score', 0), 'type': 'nyx'
    } for r in local_results])
    all_web = _json.dumps([{
        'url': r.get('url', ''), 'title': r.get('title', ''),
        'description': r.get('description', ''), 'type': 'web'
    } for r in web_results])

    return f'''<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>Nyx Search: {query}</title>
<style>
*{{margin:0;padding:0;box-sizing:border-box}}
body{{background:{c['bg_darkest']};color:{c['text_primary']};
  font-family:'Segoe UI','Ubuntu',sans-serif;padding:24px 30px;overflow-x:hidden}}

{rcss}

.header{{text-align:center;margin-bottom:32px;padding:22px;
  background:rgba(22,22,31,.6);border-radius:18px;border:1px solid rgba(58,58,74,.3);
  backdrop-filter:blur(12px);animation:shimmerIn .5s ease forwards}}
.header h1{{font-size:1.8em;background:linear-gradient(135deg,{c['purple_mid']},{c['purple_glow']});
  -webkit-background-clip:text;-webkit-text-fill-color:transparent;position:relative}}
.header .stats{{color:{c['text_muted']};margin-top:6px;font-size:.9em;position:relative}}
.section{{max-width:820px;margin:0 auto 24px}}
.section-title{{font-size:1.1em;color:{c['purple_light']};margin-bottom:14px;padding-bottom:6px;
  border-bottom:2px solid rgba(58,58,74,.3);display:flex;align-items:center;gap:8px;
  animation:slideUp .4s ease forwards}}
.result{{background:rgba(22,22,31,.5);border:1px solid rgba(58,58,74,.3);border-radius:14px;
  padding:16px 20px;margin-bottom:10px;
  opacity:0;transform:translateY(14px) translateZ(0);
  backdrop-filter:blur(8px);
  transition:all .4s cubic-bezier(.4,0,.2,1);
  will-change:transform}}
.result.visible{{opacity:1;transform:translateY(0) translateZ(0)}}
.result:hover{{border-color:{c['purple_mid']};
  transform:translateX(6px) translateZ(0);
  box-shadow:0 8px 28px rgba(123,79,191,.15),0 0 20px rgba(123,79,191,.06)}}
.r-top{{display:flex;align-items:center;gap:8px;margin-bottom:5px;flex-wrap:wrap}}
.r-title{{color:{c['purple_light']};text-decoration:none;font-size:1.1em;font-weight:600;
  transition:color .3s,text-shadow .3s}}
.r-title:hover{{color:{c['purple_glow']};text-shadow:0 0 12px rgba(123,79,191,.3)}}
.badge{{padding:2px 10px;border-radius:10px;font-size:.7em;font-weight:bold;
  transition:all .3s}}
.badge.nyx{{background:rgba(123,79,191,.2);color:{c['purple_pale']}}}
.badge.web{{background:rgba(40,40,54,.8);color:{c['text_secondary']}}}
.badge.score{{background:rgba(111,207,151,.15);color:#6FCF97}}
.r-cat{{color:{c['text_muted']};font-size:.75em;font-style:italic}}
.r-url{{color:{c['text_muted']};font-size:.8em;margin-bottom:5px;word-break:break-all}}
.r-desc{{color:{c['text_secondary']};line-height:1.5;font-size:.92em}}
.no-results{{text-align:center;padding:50px;background:rgba(22,22,31,.5);
  border-radius:18px;color:{c['text_muted']};animation:shimmerIn .5s ease forwards}}
#loader{{text-align:center;padding:20px;display:none}}
#loader .spinner{{display:inline-block;width:28px;height:28px;border:3px solid {c['bg_lighter']};
  border-top-color:{c['purple_mid']};border-radius:50%;animation:spin .7s linear infinite}}
@keyframes spin{{to{{transform:rotate(360deg)}}}}
#end-msg{{text-align:center;padding:16px;color:{c['text_muted']};font-size:.85em;display:none}}
</style></head><body>
<div class="glass-orb" style="width:350px;height:350px;background:{c['purple_mid']};top:5%;right:-3%"></div>
<div class="glass-orb" style="width:250px;height:250px;background:{c['purple_dark']};bottom:10%;left:-2%"></div>
<div class="header refract-target refract-glow"><h1>Nyx Search Results</h1>
<div class="stats">"{query}" &mdash; <span id="count">0</span> results shown</div></div>
<div class="section" id="results-container">
<div class="section-title" id="local-header" style="display:none">From Your Database</div>
<div id="local-results"></div>
<div class="section-title" id="web-header" style="display:none">Supplemental Web Results</div>
<div id="web-results"></div>
<div id="no-results" class="no-results" style="display:none">No results found. Try different keywords.</div>
</div>
<div id="loader"><div class="spinner"></div></div>
<div id="end-msg">All results loaded</div>
<script>
var localData={all_local};
var webData={all_web};
var PAGE=10,localIdx=0,webIdx=0,loading=false;
function makeCard(r){{
  var d=document.createElement('div');
  d.className='result refract-target refract-glow';
  var scoreBadge=r.score?'<span class="badge score">'+Math.round(r.score)+'%</span>':'';
  var catSpan=r.category?'<span class="r-cat">'+r.category+'</span>':'';
  var typeBadge=r.type==='nyx'?'<span class="badge nyx">Nyx</span>':'<span class="badge web">Web</span>';
  d.innerHTML='<div class="r-top"><a class="r-title" href="'+r.url+'">'+r.title+'</a>'+typeBadge+scoreBadge+catSpan+'</div>'
    +'<div class="r-url">'+r.url+'</div><div class="r-desc">'+(r.description||'')+'</div>';
  return d;
}}
function loadMore(){{
  if(loading)return;loading=true;
  document.getElementById('loader').style.display='block';
  setTimeout(function(){{
    var added=0;
    while(localIdx<localData.length&&added<PAGE){{
      document.getElementById('local-results').appendChild(makeCard(localData[localIdx]));
      localIdx++;added++;
    }}
    while(webIdx<webData.length&&added<PAGE){{
      document.getElementById('web-results').appendChild(makeCard(webData[webIdx]));
      webIdx++;added++;
    }}
    if(localIdx>0)document.getElementById('local-header').style.display='';
    if(webIdx>0)document.getElementById('web-header').style.display='';
    document.getElementById('count').textContent=localIdx+webIdx;
    document.getElementById('loader').style.display='none';
    loading=false;
    if(localIdx>=localData.length&&webIdx>=webData.length){{
      document.getElementById('end-msg').style.display='block';
      if(localIdx+webIdx===0)document.getElementById('no-results').style.display='block';
    }}
    document.querySelectorAll('.result:not(.visible)').forEach(function(el){{
      var observer=new IntersectionObserver(function(entries){{
        entries.forEach(function(e){{if(e.isIntersecting){{e.target.classList.add('visible');observer.unobserve(e.target);}}}});
      }},{{threshold:0.1}});observer.observe(el);
    }});
  }},80);
}}
window.addEventListener('scroll',function(){{
  if(window.innerHeight+window.scrollY>=document.body.offsetHeight-300)loadMore();
}});
loadMore();
{rjs}
</script></body></html>'''


def get_external_search_theme_css(colors=None):
    """CSS injected into external pages to apply Nyx theme with dynamic refractions."""
    c = colors or NYX_COLORS
    return f'''
    body {{ background-color: {c['bg_darkest']} !important; color: {c['text_primary']} !important; }}
    * {{ border-color: rgba(58,58,74,0.4) !important; }}
    a {{ color: {c['purple_light']} !important; transition: color .3s, text-shadow .3s !important; }}
    a:visited {{ color: {c['purple_soft']} !important; }}
    a:hover {{ text-shadow: 0 0 10px rgba(123,79,191,.25) !important; }}
    input, textarea, select {{ background-color: {c['bg_mid']} !important;
        color: {c['text_primary']} !important; border: 1px solid {c['border']} !important;
        border-radius: 8px !important; transition: all .3s ease !important; }}
    input:focus, textarea:focus {{ border-color: {c['purple_mid']} !important;
        box-shadow: 0 0 20px rgba(123,79,191,.25), inset 0 0 8px rgba(123,79,191,.05) !important; }}
    img {{ opacity: 0.92; }}
    header, nav, footer, [role="banner"], [role="navigation"] {{
        background-color: {c['bg_dark']} !important;
        transition: background-color .3s !important; }}
    button, [role="button"] {{ background-color: {c['bg_lighter']} !important;
        color: {c['text_primary']} !important; border-radius: 8px !important;
        transition: all .3s cubic-bezier(.4,0,.2,1) !important; }}
    button:hover, [role="button"]:hover {{ background-color: {c['purple_dark']} !important;
        box-shadow: 0 4px 16px rgba(123,79,191,.15) !important;
        transform: translateY(-1px) !important; }}
    ::selection {{ background: rgba(123,79,191,0.3) !important; }}
    ::-webkit-scrollbar {{ width: 8px; }}
    ::-webkit-scrollbar-thumb {{ background: {c['scrollbar']}; border-radius: 4px;
        transition: background .2s !important; }}
    ::-webkit-scrollbar-thumb:hover {{ background: {c['scrollbar_hover']}; }}
    ::-webkit-scrollbar-track {{ background: transparent; }}

    /* Dynamic refraction overlay for external pages */
    body::after {{
        content: '';
        position: fixed;
        width: 500px;
        height: 500px;
        border-radius: 50%;
        background: radial-gradient(circle, rgba(123,79,191,0.06), transparent 65%);
        pointer-events: none;
        z-index: 99999;
        transform: translate(-50%, -50%);
        filter: blur(40px);
        top: var(--nyx-my, -500px);
        left: var(--nyx-mx, -500px);
        transition: top .12s ease-out, left .12s ease-out;
    }}
    '''


def get_external_refraction_js():
    """JS injected into external pages for mouse-tracking ambient refraction."""
    return '''
    (function() {
        var last = 0;
        document.addEventListener('mousemove', function(e) {
            var now = Date.now();
            if (now - last < 16) return;
            last = now;
            document.body.style.setProperty('--nyx-my', e.clientY + 'px');
            document.body.style.setProperty('--nyx-mx', e.clientX + 'px');
        }, { passive: true });
    })();
    '''


def get_history_html(history_entries, colors=None):
    """Generate history page HTML with dynamic refractions."""
    c = colors or NYX_COLORS
    rcss = _refraction_css(c)
    rjs = _refraction_js()
    rows = ""
    for i, h in enumerate(history_entries):
        rows += f'''<div class="entry refract-target refract-glow" style="animation-delay:{min(i*0.03, 1.5)}s">
            <div class="e-time">{h.get('time','')}</div>
            <a class="e-title" href="{h['url']}">{h.get('title', h['url'])}</a>
            <div class="e-url">{h['url']}</div>
        </div>'''
    return f'''<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>History</title>
<style>
*{{margin:0;padding:0;box-sizing:border-box}}
body{{background:{c['bg_darkest']};color:{c['text_primary']};
  font-family:'Segoe UI','Ubuntu',sans-serif;padding:30px;overflow-x:hidden}}
{rcss}
h1{{font-size:1.6em;color:{c['purple_light']};margin-bottom:24px;
  animation:shimmerIn .5s ease forwards}}
.entry{{background:rgba(22,22,31,.5);border:1px solid rgba(58,58,74,.3);border-radius:12px;
  padding:12px 16px;margin-bottom:8px;animation:shimmerIn .4s ease-out both;
  transition:all .4s cubic-bezier(.4,0,.2,1);will-change:transform}}
.entry:hover{{border-color:{c['purple_mid']};transform:translateX(6px) translateZ(0);
  box-shadow:0 6px 24px rgba(123,79,191,.12)}}
.e-time{{color:{c['text_muted']};font-size:.75em;float:right}}
.e-title{{color:{c['purple_light']};text-decoration:none;font-weight:600;
  transition:color .3s,text-shadow .3s}}
.e-title:hover{{color:{c['purple_glow']};text-shadow:0 0 10px rgba(123,79,191,.3)}}
.e-url{{color:{c['text_muted']};font-size:.8em;margin-top:2px}}
.empty{{text-align:center;padding:40px;color:{c['text_muted']};animation:shimmerIn .5s ease forwards}}
</style></head><body>
<div class="glass-orb" style="width:300px;height:300px;background:{c['purple_mid']};top:5%;right:-3%"></div>
<h1>Browsing History</h1>
{rows if rows else '<div class="empty">No history yet. Start browsing!</div>'}
<script>{rjs}</script>
</body></html>'''


def get_bookmarks_html(bookmarks, colors=None):
    """Generate bookmarks page HTML with dynamic refractions."""
    c = colors or NYX_COLORS
    rcss = _refraction_css(c)
    rjs = _refraction_js()
    items = ""
    for i, b in enumerate(bookmarks):
        items += f'''<div class="bm refract-target refract-glow" style="animation-delay:{min(i*0.04, 1.5)}s">
            <a class="bm-title" href="{b['url']}">{b.get('title', b['url'])}</a>
            <span class="bm-folder">{b.get('folder', '')}</span>
            <div class="bm-url">{b['url']}</div>
        </div>'''
    return f'''<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>Bookmarks</title>
<style>
*{{margin:0;padding:0;box-sizing:border-box}}
body{{background:{c['bg_darkest']};color:{c['text_primary']};
  font-family:'Segoe UI','Ubuntu',sans-serif;padding:30px;overflow-x:hidden}}
{rcss}
h1{{font-size:1.6em;color:{c['purple_light']};margin-bottom:24px;
  animation:shimmerIn .5s ease forwards}}
.bm{{background:rgba(22,22,31,.5);border:1px solid rgba(58,58,74,.3);border-radius:12px;
  padding:14px 18px;margin-bottom:8px;animation:shimmerIn .4s ease-out both;
  transition:all .4s cubic-bezier(.4,0,.2,1);will-change:transform}}
.bm:hover{{border-color:{c['purple_mid']};transform:translateX(6px) translateZ(0);
  box-shadow:0 8px 28px rgba(123,79,191,.14)}}
.bm-title{{color:{c['purple_light']};text-decoration:none;font-weight:600;font-size:1.05em;
  transition:color .3s,text-shadow .3s}}
.bm-title:hover{{color:{c['purple_glow']};text-shadow:0 0 10px rgba(123,79,191,.3)}}
.bm-folder{{background:rgba(123,79,191,.15);color:{c['purple_pale']};padding:2px 10px;
  border-radius:8px;font-size:.7em;margin-left:8px;transition:all .3s}}
.bm:hover .bm-folder{{background:rgba(123,79,191,.25)}}
.bm-url{{color:{c['text_muted']};font-size:.8em;margin-top:3px}}
.empty{{text-align:center;padding:40px;color:{c['text_muted']};animation:shimmerIn .5s ease forwards}}
</style></head><body>
<div class="glass-orb" style="width:300px;height:300px;background:{c['purple_mid']};top:5%;right:-3%"></div>
<h1>Bookmarks</h1>
{items if items else '<div class="empty">No bookmarks yet. Press Ctrl+D to bookmark a page.</div>'}
<script>{rjs}</script>
</body></html>'''


def get_downloads_html(downloads, colors=None):
    """Generate downloads page HTML with dynamic refractions."""
    c = colors or NYX_COLORS
    rcss = _refraction_css(c)
    rjs = _refraction_js()
    items = ""
    for i, d in enumerate(downloads):
        pct = d.get('progress', 100)
        status = d.get('status', 'complete')
        bar = f'<div class="prog"><div class="prog-fill" style="width:{pct}%"></div></div>' if status == 'downloading' else ''
        items += f'''<div class="dl refract-target refract-glow" style="animation-delay:{min(i*0.04, 1.5)}s">
            <div class="dl-name">{d.get('filename','Unknown')}</div>
            <div class="dl-info">{d.get('size','')} &middot; {status}</div>
            {bar}
        </div>'''
    return f'''<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>Downloads</title>
<style>
*{{margin:0;padding:0;box-sizing:border-box}}
body{{background:{c['bg_darkest']};color:{c['text_primary']};
  font-family:'Segoe UI','Ubuntu',sans-serif;padding:30px;overflow-x:hidden}}
{rcss}
h1{{font-size:1.6em;color:{c['purple_light']};margin-bottom:24px;
  animation:shimmerIn .5s ease forwards}}
.dl{{background:rgba(22,22,31,.5);border:1px solid rgba(58,58,74,.3);border-radius:12px;
  padding:14px 18px;margin-bottom:8px;animation:shimmerIn .4s ease-out both;
  transition:all .4s cubic-bezier(.4,0,.2,1);will-change:transform}}
.dl:hover{{border-color:{c['purple_mid']};transform:translateX(6px) translateZ(0);
  box-shadow:0 6px 24px rgba(123,79,191,.12)}}
.dl-name{{color:{c['text_primary']};font-weight:600}}
.dl-info{{color:{c['text_muted']};font-size:.8em;margin-top:3px}}
.prog{{height:5px;background:{c['bg_lighter']};border-radius:3px;margin-top:8px;overflow:hidden}}
.prog-fill{{height:100%;background:linear-gradient(90deg,{c['purple_mid']},{c['purple_glow']},{c['purple_light']});
  border-radius:3px;transition:width .5s cubic-bezier(.4,0,.2,1);
  box-shadow:0 0 8px rgba(123,79,191,.3)}}
.empty{{text-align:center;padding:40px;color:{c['text_muted']};animation:shimmerIn .5s ease forwards}}
</style></head><body>
<div class="glass-orb" style="width:300px;height:300px;background:{c['purple_mid']};top:5%;right:-3%"></div>
<h1>Downloads</h1>
{items if items else '<div class="empty">No downloads yet.</div>'}
<script>{rjs}</script>
</body></html>'''

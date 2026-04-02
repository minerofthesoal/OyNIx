"""
OyNIx Browser - Custom SVG Icon System
All icons are hand-crafted SVG paths — no emoji, no external files.
Each icon returns a QIcon via svg_icon() or raw SVG via svg_str().
"""

from PyQt6.QtGui import QIcon, QPixmap, QPainter, QColor
from PyQt6.QtCore import QByteArray, Qt, QSize
from PyQt6.QtSvg import QSvgRenderer


def _recolor(svg: str, color: str) -> str:
    """Replace fill/stroke placeholder with actual color."""
    return svg.replace('{{COLOR}}', color)


def svg_icon(name: str, color: str = "#c4a0f5", size: int = 20) -> QIcon:
    """Get a QIcon from an SVG icon name."""
    raw = _recolor(ICONS.get(name, ICONS['fallback']), color)
    renderer = QSvgRenderer(QByteArray(raw.encode()))
    pm = QPixmap(QSize(size, size))
    pm.fill(Qt.GlobalColor.transparent)
    p = QPainter(pm)
    renderer.render(p)
    p.end()
    icon = QIcon(pm)
    # Also add a lighter version for hover/active
    hover_raw = _recolor(ICONS.get(name, ICONS['fallback']),
                         _lighten(color, 40))
    renderer2 = QSvgRenderer(QByteArray(hover_raw.encode()))
    pm2 = QPixmap(QSize(size, size))
    pm2.fill(Qt.GlobalColor.transparent)
    p2 = QPainter(pm2)
    renderer2.render(p2)
    p2.end()
    icon.addPixmap(pm2, QIcon.Mode.Active)
    icon.addPixmap(pm2, QIcon.Mode.Selected)
    return icon


def _lighten(hex_color: str, amount: int) -> str:
    """Lighten a hex color."""
    c = QColor(hex_color)
    h, s, l, a = c.getHsl()
    c.setHsl(h, s, min(255, l + amount), a)
    return c.name()


# ─── SVG Icon Definitions ──────────────────────────────────────────
# All icons use a 24x24 viewBox. {{COLOR}} is replaced at render time.

_V = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="{{COLOR}}" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">'
_E = '</svg>'

ICONS = {
    # Navigation
    'back': f'{_V}<polyline points="15 18 9 12 15 6"/>{_E}',
    'forward': f'{_V}<polyline points="9 18 15 12 9 6"/>{_E}',
    'reload': f'{_V}<polyline points="23 4 23 10 17 10"/><path d="M1 20a10 10 0 0 1 17.3-8.2L23 10"/>{_E}',
    'home': f'{_V}<path d="M3 12l9-9 9 9"/><path d="M5 10v10h5v-6h4v6h5V10"/>{_E}',
    'search': f'{_V}<circle cx="11" cy="11" r="8"/><line x1="21" y1="21" x2="16.65" y2="16.65"/>{_E}',

    # Tabs
    'new_tab': f'{_V}<line x1="12" y1="5" x2="12" y2="19"/><line x1="5" y1="12" x2="19" y2="12"/>{_E}',
    'close_tab': f'{_V}<line x1="18" y1="6" x2="6" y2="18"/><line x1="6" y1="6" x2="18" y2="18"/>{_E}',
    'tree_tabs': f'{_V}<line x1="4" y1="6" x2="4" y2="18"/><line x1="4" y1="6" x2="10" y2="6"/><line x1="4" y1="12" x2="10" y2="12"/><line x1="8" y1="18" x2="14" y2="18"/><rect x="11" y="3" width="9" height="5" rx="1"/><rect x="11" y="9" width="9" height="5" rx="1"/><rect x="15" y="15" width="6" height="5" rx="1"/>{_E}',
    'normal_tabs': f'{_V}<rect x="2" y="4" width="8" height="5" rx="1"/><rect x="12" y="4" width="8" height="5" rx="1"/><rect x="2" y="11" width="18" height="9" rx="1"/>{_E}',

    # Toolbar actions
    'bookmark': f'{_V}<path d="M19 21l-7-5-7 5V5a2 2 0 0 1 2-2h10a2 2 0 0 1 2 2z"/>{_E}',
    'bookmark_filled': '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="{{COLOR}}" stroke="{{COLOR}}" stroke-width="2"><path d="M19 21l-7-5-7 5V5a2 2 0 0 1 2-2h10a2 2 0 0 1 2 2z"/></svg>',
    'downloads': f'{_V}<path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="7 10 12 15 17 10"/><line x1="12" y1="15" x2="12" y2="3"/>{_E}',
    'settings': f'{_V}<circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 1 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 1 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 1 1-2.83-2.83l.06-.06A1.65 1.65 0 0 0 4.68 15a1.65 1.65 0 0 0-1.51-1H3a2 2 0 1 1 0-4h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 1 1 2.83-2.83l.06.06A1.65 1.65 0 0 0 9 4.68a1.65 1.65 0 0 0 1-1.51V3a2 2 0 1 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 1 1 2.83 2.83l-.06.06A1.65 1.65 0 0 0 19.4 9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 1 1 0 4h-.09a1.65 1.65 0 0 0-1.51 1z"/>{_E}',
    'menu': f'{_V}<line x1="3" y1="12" x2="21" y2="12"/><line x1="3" y1="6" x2="21" y2="6"/><line x1="3" y1="18" x2="21" y2="18"/>{_E}',

    # AI / Features
    'ai': f'{_V}<path d="M12 2a4 4 0 0 1 4 4v2a4 4 0 0 1-8 0V6a4 4 0 0 1 4-4z"/><path d="M8 14s-4 2-4 6h16c0-4-4-6-4-6"/><circle cx="9" cy="6" r="0.5" fill="{{COLOR}}"/><circle cx="15" cy="6" r="0.5" fill="{{COLOR}}"/>{_E}',
    'ai_sparkle': f'{_V}<path d="M12 2L14 8 20 8 15 12 17 18 12 14 7 18 9 12 4 8 10 8Z" fill="{{COLOR}}" opacity="0.3"/><path d="M12 2L14 8 20 8 15 12 17 18 12 14 7 18 9 12 4 8 10 8Z"/>{_E}',
    'send': f'{_V}<line x1="22" y1="2" x2="11" y2="13"/><polygon points="22 2 15 22 11 13 2 9 22 2"/>{_E}',
    'clear': f'{_V}<polyline points="3 6 5 6 21 6"/><path d="M19 6l-1 14a2 2 0 0 1-2 2H8a2 2 0 0 1-2-2L5 6"/><path d="M10 11v6"/><path d="M14 11v6"/><path d="M9 6V3h6v3"/>{_E}',
    'summarize': f'{_V}<rect x="3" y="3" width="18" height="18" rx="2"/><line x1="7" y1="8" x2="17" y2="8"/><line x1="7" y1="12" x2="14" y2="12"/><line x1="7" y1="16" x2="11" y2="16"/>{_E}',

    # Security
    'shield': f'{_V}<path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"/>{_E}',
    'shield_check': f'{_V}<path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"/><polyline points="9 12 11 14 15 10"/>{_E}',
    'lock': f'{_V}<rect x="3" y="11" width="18" height="11" rx="2"/><path d="M7 11V7a5 5 0 0 1 10 0v4"/>{_E}',

    # Database / Sync
    'database': f'{_V}<ellipse cx="12" cy="5" rx="9" ry="3"/><path d="M21 12c0 1.66-4 3-9 3s-9-1.34-9-3"/><path d="M3 5v14c0 1.66 4 3 9 3s9-1.34 9-3V5"/>{_E}',
    'sync': f'{_V}<polyline points="23 4 23 10 17 10"/><polyline points="1 20 1 14 7 14"/><path d="M3.51 9a9 9 0 0 1 14.85-3.36L23 10"/><path d="M20.49 15a9 9 0 0 1-14.85 3.36L1 14"/>{_E}',
    'upload': f'{_V}<path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="17 8 12 3 7 8"/><line x1="12" y1="3" x2="12" y2="15"/>{_E}',
    'globe': f'{_V}<circle cx="12" cy="12" r="10"/><line x1="2" y1="12" x2="22" y2="12"/><path d="M12 2a15.3 15.3 0 0 1 4 10 15.3 15.3 0 0 1-4 10 15.3 15.3 0 0 1-4-10 15.3 15.3 0 0 1 4-10z"/>{_E}',

    # Bookmarks
    'folder': f'{_V}<path d="M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z"/>{_E}',
    'folder_open': f'{_V}<path d="M5 19a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h8a2 2 0 0 1 2 2v1"/><path d="M22 10l-3 9H5l-3-9h20z"/>{_E}',
    'star': f'{_V}<polygon points="12 2 15.09 8.26 22 9.27 17 14.14 18.18 21.02 12 17.77 5.82 21.02 7 14.14 2 9.27 8.91 8.26 12 2"/>{_E}',
    'star_filled': '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="{{COLOR}}" stroke="{{COLOR}}" stroke-width="1"><polygon points="12 2 15.09 8.26 22 9.27 17 14.14 18.18 21.02 12 17.77 5.82 21.02 7 14.14 2 9.27 8.91 8.26 12 2"/></svg>',

    # Window controls
    'minimize': f'{_V}<line x1="5" y1="12" x2="19" y2="12"/>{_E}',
    'maximize': f'{_V}<rect x="4" y="4" width="16" height="16" rx="1"/>{_E}',
    'close': f'{_V}<line x1="18" y1="6" x2="6" y2="18"/><line x1="6" y1="6" x2="18" y2="18"/>{_E}',

    # View / Layout
    'sidebar': f'{_V}<rect x="3" y="3" width="18" height="18" rx="2"/><line x1="9" y1="3" x2="9" y2="21"/>{_E}',
    'panel_right': f'{_V}<rect x="3" y="3" width="18" height="18" rx="2"/><line x1="15" y1="3" x2="15" y2="21"/>{_E}',
    'fullscreen': f'{_V}<polyline points="15 3 21 3 21 9"/><polyline points="9 21 3 21 3 15"/><line x1="21" y1="3" x2="14" y2="10"/><line x1="3" y1="21" x2="10" y2="14"/>{_E}',

    # Search related
    'filter': f'{_V}<polygon points="22 3 2 3 10 12.46 10 19 14 21 14 12.46 22 3"/>{_E}',
    'sort': f'{_V}<line x1="12" y1="5" x2="12" y2="19"/><polyline points="5 12 12 19 19 12"/>{_E}',

    # Command palette
    'command': f'{_V}<path d="M18 3a3 3 0 0 0-3 3v12a3 3 0 0 0 3 3 3 3 0 0 0 3-3 3 3 0 0 0-3-3H6a3 3 0 0 0-3 3 3 3 0 0 0 3 3 3 3 0 0 0 3-3V6a3 3 0 0 0-3-3 3 3 0 0 0-3 3 3 3 0 0 0 3 3h12a3 3 0 0 0 3-3 3 3 0 0 0-3-3z"/>{_E}',
    'terminal': f'{_V}<polyline points="4 17 10 11 4 5"/><line x1="12" y1="19" x2="20" y2="19"/>{_E}',

    # Status indicators
    'check': f'{_V}<polyline points="20 6 9 17 4 12"/>{_E}',
    'warning': f'{_V}<path d="M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z"/><line x1="12" y1="9" x2="12" y2="13"/><line x1="12" y1="17" x2="12.01" y2="17"/>{_E}',
    'info': f'{_V}<circle cx="12" cy="12" r="10"/><line x1="12" y1="16" x2="12" y2="12"/><line x1="12" y1="8" x2="12.01" y2="8"/>{_E}',
    'error': f'{_V}<circle cx="12" cy="12" r="10"/><line x1="15" y1="9" x2="9" y2="15"/><line x1="9" y1="9" x2="15" y2="15"/>{_E}',

    # Misc
    'copy': f'{_V}<rect x="9" y="9" width="13" height="13" rx="2"/><path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1"/>{_E}',
    'link': f'{_V}<path d="M10 13a5 5 0 0 0 7.54.54l3-3a5 5 0 0 0-7.07-7.07l-1.72 1.71"/><path d="M14 11a5 5 0 0 0-7.54-.54l-3 3a5 5 0 0 0 7.07 7.07l1.71-1.71"/>{_E}',
    'eye': f'{_V}<path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/><circle cx="12" cy="12" r="3"/>{_E}',
    'history': f'{_V}<circle cx="12" cy="12" r="10"/><polyline points="12 6 12 12 16 14"/>{_E}',
    'zoom_in': f'{_V}<circle cx="11" cy="11" r="8"/><line x1="21" y1="21" x2="16.65" y2="16.65"/><line x1="11" y1="8" x2="11" y2="14"/><line x1="8" y1="11" x2="14" y2="11"/>{_E}',
    'zoom_out': f'{_V}<circle cx="11" cy="11" r="8"/><line x1="21" y1="21" x2="16.65" y2="16.65"/><line x1="8" y1="11" x2="14" y2="11"/>{_E}',
    'print': f'{_V}<polyline points="6 9 6 2 18 2 18 9"/><path d="M6 18H4a2 2 0 0 1-2-2v-5a2 2 0 0 1 2-2h16a2 2 0 0 1 2 2v5a2 2 0 0 1-2 2h-2"/><rect x="6" y="14" width="12" height="8"/>{_E}',
    'nyx_logo': '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"><defs><linearGradient id="ng" x1="0%" y1="0%" x2="100%" y2="100%"><stop offset="0%" style="stop-color:#9b59b6"/><stop offset="100%" style="stop-color:#c4a0f5"/></linearGradient></defs><circle cx="12" cy="12" r="11" fill="url(#ng)" opacity="0.2" stroke="{{COLOR}}" stroke-width="1.5"/><path d="M8 7l4 10 4-10" stroke="{{COLOR}}" stroke-width="2.5" fill="none" stroke-linecap="round" stroke-linejoin="round"/><circle cx="12" cy="4" r="1.5" fill="{{COLOR}}"/></svg>',

    'fallback': f'{_V}<circle cx="12" cy="12" r="10"/><line x1="12" y1="8" x2="12" y2="12"/><line x1="12" y1="16" x2="12.01" y2="16"/>{_E}',
}

# OyNIx Browser

A custom Firefox-inspired desktop browser with local AI, auto-indexing search, and a Nyx purple/black theme.

**Coded by Claude (Anthropic)** for the OyNIx project.

## Features

- **Local LLM Assistant** - TinyLlama 1.1B runs on CPU, auto-downloads on first launch (~700MB). No GPU or internet needed after download.
- **Nyx Search Engine** - Auto-indexes every page you visit. Combines local index + 1400+ curated sites + DuckDuckGo web results.
- **Auto-Expanding Database** - Visited sites and DuckDuckGo results are automatically added to the local database with smart categorization.
- **Community Upload** - Optionally auto-upload discovered sites to a shared GitHub repo so future users benefit.
- **Tree-Style Tabs** - Sidebar tree-graph tab hierarchy with drag-drop, or toggle to normal tabs.
- **Nyx Theme** - Medium purple and black UI with 4 theme variants (Nyx, Midnight, Violet, Amethyst).
- **Animated Homepage** - Canvas particle background with purple connecting particles.
- **GitHub Database Sync** - Export/import your site database to/from GitHub.
- **Security Prompts** - Detects login pages (Microsoft, Google, GitHub, etc.) and warns before submitting credentials.
- **DuckDuckGo Integration** - Privacy-focused web search as fallback/complement.
- **Native Desktop App** - Built with PyQt6 + Chromium WebEngine. Not a web app.

## Installation

### Quick Install (Linux)

```bash
chmod +x install.sh
./install.sh
```

### Manual Install

```bash
pip install -r requirements.txt
python -m oynix.oynix
```

### Requirements

- Python 3.10+
- PyQt6, PyQt6-WebEngine
- llama-cpp-python (for local AI)
- whoosh (for full-text search, optional - falls back to SQLite FTS5)
- requests, beautifulsoup4 (for DuckDuckGo search)

## Usage

```bash
# Launch the browser
oynix

# Or run directly
python -m oynix.oynix
```

### Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+T | New Tab |
| Ctrl+W | Close Tab |
| Ctrl+L | Focus URL Bar |
| Ctrl+F | Find on Page |
| Ctrl+Shift+I | Developer Tools |
| F5 | Refresh |
| Alt+Left | Back |
| Alt+Right | Forward |

### Search

Type in the URL bar:
- **URLs** load directly
- **Keywords** trigger Nyx search (local index + curated DB + DuckDuckGo)
- Use `!ddg`, `!g`, `!brave` bangs for specific engines

### Settings

Access via the menu or Ctrl+, to configure:
- Tree tabs vs normal tabs
- Default search engine
- Auto-indexing and auto-expand database
- Community upload to GitHub
- AI model parameters
- Theme variant
- Privacy settings (DNT, cookie blocking)

## Architecture

```
oynix/
├── oynix.py              # Entry point
├── core/
│   ├── browser.py        # Main browser window
│   ├── theme_engine.py   # Nyx theme + homepage + search results HTML
│   ├── tree_tabs.py      # Tree-style tab sidebar
│   ├── nyx_search.py     # Nyx search engine (Whoosh/SQLite FTS)
│   ├── database.py       # Auto-expanding site database
│   ├── ai_manager.py     # Local LLM (TinyLlama via llama-cpp-python)
│   ├── github_sync.py    # GitHub database sync + community upload
│   └── security.py       # Login page detection + security prompts
├── UI/
│   ├── ai_chat.py        # AI chat sidebar panel
│   ├── menus.py          # Menu bar
│   └── settings.py       # Settings dialog (6 tabs)
└── utils/
    ├── helpers.py         # URL utilities
    └── logger.py          # Logging
```

## Building Packages

GitHub Actions automatically builds packages on every release tag:

- **Debian (.deb)** - For Ubuntu, Debian, Mint, etc.
- **Windows (.exe)** - Standalone installer via PyInstaller
- **Arch Linux (pacman)** - PKGBUILD-based package

Create a release:
```bash
git tag v1.1.0
git push origin v1.1.0
```

## License

See [LICENSE](LICENSE) for details.

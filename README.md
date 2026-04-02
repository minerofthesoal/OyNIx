# OyNIx Browser

A custom Firefox-inspired **native desktop browser** with local AI, auto-indexing search, and a Nyx purple/black theme.

**Coded by Claude (Anthropic)** for the OyNIx project.

## Features

- **Local LLM Assistant** — TinyLlama 1.1B runs on CPU, auto-downloads on first launch (~700MB). No GPU needed.
- **Nyx Search Engine** — Auto-indexes every page you visit. Local index + 1400+ curated sites + DuckDuckGo.
- **Auto-Expanding Database** — Visited sites and DDG results auto-added with smart categorization.
- **Community Upload** — Optionally upload discovered sites to shared GitHub repo for future users.
- **Tree-Style Tabs** — Sidebar tree-graph hierarchy with drag-drop, or toggle to normal tabs.
- **Nyx Theme** — Medium purple and black UI with 4 variants (Nyx, Midnight, Violet, Amethyst).
- **Animated Homepage** — Canvas particle background with connecting purple particles.
- **Native C++ Indexer** — Fast text tokenization and BM25 scoring via C++ shared library (Python fallback).
- **Native C Launcher** — Compiled binary that finds Python, installs deps, and launches the browser.
- **GitHub Database Sync** — Export/import your site database to/from GitHub.
- **Security Prompts** — Detects login pages and warns before credential submission.
- **DuckDuckGo Integration** — Privacy-focused web search.
- **Not a web app** — Native desktop app built with PyQt6 + Chromium WebEngine.

## Languages

| Language | Purpose |
|----------|---------|
| **Python** | Browser core, UI, AI, search, database (PyQt6) |
| **C** | Native launcher binary (`src/launcher.c`) |
| **C++** | Fast text indexer & BM25 scoring (`src/native/fast_index.cpp`) |
| **Shell** | Installer, build scripts (`install.sh`, `scripts/`) |

## Installation

### Quick Install (Linux/macOS)

```bash
git clone https://github.com/minerofthesoal/OyNIx.git
cd OyNIx
chmod +x install.sh
./install.sh
./oynix-browser
```

The installer will:
1. Find Python 3.10+
2. Create a venv and install Python dependencies
3. Build native C/C++ components (if gcc/g++ available, optional)
4. Create a launcher script

### Manual Install

```bash
# Python deps (pick one)
python3 -m venv .venv && source .venv/bin/activate && pip install -r requirements.txt
# OR
pip install --break-system-packages -r requirements.txt

# Optional: build native components for faster search indexing
make

# Run
python3 -m oynix
```

### Build from Source (Native Components)

```bash
# Build everything (C launcher + C++ library)
make

# Or individually
make launcher    # C launcher binary
make lib         # C++ shared library (libnyx_index.so)
make cli         # C++ CLI indexer tool

# Install system-wide
sudo make install
```

### Requirements

- **Python 3.10+** (required)
- **gcc** (optional — for native C launcher)
- **g++** with C++17 (optional — for native C++ indexer, ~10x faster search)
- **PyQt6 + PyQt6-WebEngine** (pip-installed)

## Usage

```bash
# Via installer launcher
./oynix-browser

# Direct Python
python3 -m oynix

# If make install'd
oynix

# Native CLI indexer (if built)
./build/nyx-index tokenize "hello world test"
./build/nyx-index score "python browser" "a python-based web browser"
./build/nyx-index normalize "https://www.Example.com/path/"
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

## Architecture

```
OyNIx/
├── src/
│   ├── launcher.c              # C native launcher (finds Python, launches browser)
│   └── native/
│       └── fast_index.cpp      # C++ fast tokenizer + BM25 scorer (ctypes lib)
├── oynix/
│   ├── __init__.py             # Package init
│   ├── __main__.py             # python -m oynix entry point
│   ├── oynix.py                # Main launcher (Python)
│   ├── core/
│   │   ├── browser.py          # Main browser window
│   │   ├── native_bridge.py    # Python ↔ C++ bridge (ctypes, with fallback)
│   │   ├── theme_engine.py     # Nyx theme + animated homepage
│   │   ├── tree_tabs.py        # Tree-style tab sidebar
│   │   ├── nyx_search.py       # Nyx search engine (Whoosh/SQLite FTS)
│   │   ├── database.py         # Auto-expanding SQLite site database
│   │   ├── ai_manager.py       # Local LLM (TinyLlama via llama-cpp-python)
│   │   ├── github_sync.py      # GitHub sync + community upload
│   │   └── security.py         # Login page detection
│   ├── UI/
│   │   ├── ai_chat.py          # AI chat sidebar panel
│   │   ├── menus.py            # Menu bar
│   │   └── settings.py         # Settings dialog (6 tabs)
│   └── utils/
│       ├── helpers.py           # URL utilities
│       └── logger.py            # Logging
├── scripts/
│   ├── build-deb.sh            # Shell: build Debian package
│   └── build-pacman.sh         # Shell: build Arch Linux package
├── Makefile                     # Build C/C++ components
├── pyproject.toml               # Python packaging (pip install -e .)
├── requirements.txt             # Python dependencies
└── install.sh                   # One-click installer
```

## Building Packages

### Automated (GitHub Actions)

Push a version tag to trigger builds for all platforms:

```bash
git tag v1.1.0
git push origin v1.1.0
```

Or trigger manually from the Actions tab (workflow_dispatch).

Builds:
- **Debian (.deb)** — Ships Python source + native C launcher + C++ lib + postinst pip install
- **Windows (.exe)** — PyInstaller standalone binary
- **Arch Linux (pacman)** — PKGBUILD with optional native components

### Manual

```bash
# Debian
./scripts/build-deb.sh v1.1.0

# Arch Linux
./scripts/build-pacman.sh v1.1.0
```

## License

See [LICENSE](LICENSE) for details.

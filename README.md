# OyNIx Browser

A custom Chromium-based **native desktop browser** built with C++17, Qt6 WebEngine, and .NET 8 NativeAOT. Features a built-in AI assistant, local search engine, web crawler, and an obsidian purple/black theme.

**Coded by Claude (Anthropic)** for the OyNIx project.

## Features

- **Nyx Search Engine** — Local FTS5 full-text search index with BM25 ranking. Hybrid mode combines local results with web search.
- **Source Labels** — OYN, Nyx, OYN+, Nyx+ badges distinguish result origins (curated, crawled, enhanced).
- **Native Web Crawler** — BFS crawler with configurable depth, concurrency, language filter, robots.txt support, and politeness delay.
- **AI Assistant** — Sidebar chat panel with Ollama (local) or OpenAI-compatible backend support.
- **Tree-Style Tabs** — Domain-grouped sidebar with drag-drop hierarchy, or toggle to classic tabs.
- **Command Palette** — VS Code-style Ctrl+K launcher for quick actions.
- **Community Database** — JSONL-based community site database synced from GitHub on startup. Export your crawled data to contribute.
- **Web Cache** — SHA-256 hashed offline cache in `~/.cache/oynix/webcache/` with automatic pruning.
- **Multiple Themes** — Nyx Dark, Midnight, Violet, Amethyst, Ember, Aether Light, and System.
- **Chrome Import** — Import bookmarks, history, and credentials from Chrome/Chromium.
- **GitHub Sync** — Sync bookmarks and settings via GitHub personal access token.
- **Session Manager** — Save and restore browser sessions.
- **Security Manager** — Detects login pages and warns before credential submission.
- **NPI Extensions** — Custom extension format with a built-in compiler.
- **Audio Player** — Web media detection and playback (when Qt6 Multimedia is available).
- **Download Manager** — Built-in download manager with progress tracking.
- **.NET 8 Core** — NativeAOT compiled C# library for advanced interop tasks.

## Languages

| Language | Purpose |
|----------|---------|
| **C++17** | Browser core, UI, search engine, crawler, all subsystems (Qt6) |
| **C#** | NativeAOT interop core library (`core/OyNIx.Core/`) |
| **CMake** | Build system |
| **Shell** | Build scripts, packaging, installer (`scripts/`, `debian/`) |

## Installation

### Pre-built Packages

Download from [GitHub Releases](https://github.com/minerofthesoal/OyNIx/releases):

| Platform | Package | Install |
|----------|---------|---------|
| **Ubuntu/Mint** | `.deb` | `sudo apt install ./oynix_*.deb` |
| **Arch Linux** | `.pkg.tar.zst` | `sudo pacman -U oynix-*.pkg.tar.zst` |
| **Windows** | `.zip` / `.exe` | Extract and run `OyNIx.exe` |

### Build from Source (Ubuntu/Mint)

```bash
# Install dependencies and build
./scripts/build-ubuntu.sh --deps-only   # install build dependencies
./scripts/build-ubuntu.sh                # build with CMake + Ninja
./scripts/build-ubuntu.sh --install      # install to ~/.local
```

### Build from Source (Manual)

```bash
# Prerequisites: Qt6 (WebEngine, WebChannel, Network, Sql, Concurrent), .NET 8 SDK,
#                CMake 3.16+, Ninja, g++ with C++17, SQLite3

cmake -S . -B build -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/qt6
cmake --build build -j$(nproc)

# Run
./build/oynix
```

### Build .deb Package

```bash
chmod +x scripts/build-deb.sh debian/rules debian/bundle-qt.sh
./scripts/build-deb.sh v3.1.0
sudo apt install ./oynix_3.1.0_amd64.deb
```

### Build Dependencies

- **Qt6 6.7+** — Core, Widgets, WebEngineWidgets, WebEngineCore, Network, Sql, Concurrent, WebChannel
- **CMake 3.16+** and **Ninja**
- **g++ 10+** with C++17 support
- **.NET 8 SDK** — for NativeAOT C# core library
- **SQLite3** (optional — Qt6::Sql bundles a driver)
- **Qt6 Multimedia** (optional — enables audio player)

## Usage

### Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+T | New Tab |
| Ctrl+W | Close Tab |
| Ctrl+L | Focus URL Bar |
| Ctrl+F | Find on Page |
| Ctrl+K | Command Palette |
| Ctrl+D | Bookmark Page |
| Ctrl+Shift+W | Web Crawler Panel |
| F5 | Refresh |
| F11 | Toggle Fullscreen |
| Ctrl+= / Ctrl+- | Zoom In / Out |
| Ctrl+0 | Reset Zoom |
| Alt+Left | Back |
| Alt+Right | Forward |

### Search

Type in the URL bar:
- **URLs** load directly
- **Keywords** trigger Nyx search (local FTS5 index + community DB + web results)
- Hybrid mode fetches results from DuckDuckGo, Google, Bing, Brave, or Startpage

### Web Crawler

Open the crawler panel (Ctrl+Shift+W or toolbar button):
1. **List crawl** — Enter URLs (one per line) and crawl those sites
2. **Broad crawl** — BFS crawl starting from a seed URL
- Configurable max depth, max pages, concurrency, and language filter
- Respects robots.txt and politeness delays
- Results are indexed into the local Nyx search database

## Architecture

```
OyNIx/
├── src/
│   ├── main.cpp                    # Application entry point
│   ├── core/
│   │   ├── BrowserWindow.cpp/h     # Main browser window
│   │   ├── TabWidget.cpp/h         # Tab management
│   │   ├── WebView.cpp/h           # WebEngine view wrapper
│   │   ├── WebPage.cpp/h           # WebEngine page wrapper
│   │   ├── UrlBar.cpp/h            # URL bar with search integration
│   │   └── FindBar.cpp/h           # In-page find bar
│   ├── ai/
│   │   ├── AiManager.cpp/h         # AI backend (Ollama/OpenAI)
│   │   └── AiChatPanel.cpp/h       # Sidebar AI chat panel
│   ├── search/
│   │   ├── NyxSearch.cpp/h         # FTS5 search engine
│   │   ├── WebCrawler.cpp/h        # BFS web crawler
│   │   ├── WebResultsFetcher.cpp/h # Web search integration
│   │   └── SearchEngineBuilder.cpp/h
│   ├── data/
│   │   ├── Database.cpp/h          # SQLite database
│   │   ├── CommunityDbSync.cpp/h   # GitHub community DB sync
│   │   ├── WebCache.cpp/h          # Offline page cache
│   │   ├── BookmarkManager.cpp/h   # Bookmark storage
│   │   ├── HistoryManager.cpp/h    # Browsing history
│   │   ├── ProfileManager.cpp/h    # User profiles
│   │   ├── CredentialManager.cpp/h # Credential storage
│   │   ├── SessionManager.cpp/h    # Session save/restore
│   │   ├── ChromeImport.cpp/h      # Chrome data import
│   │   └── NydtaFormat.cpp/h       # Custom data format
│   ├── ui/
│   │   ├── SettingsDialog.cpp/h    # Settings (8 tabs)
│   │   ├── CommandPalette.cpp/h    # Ctrl+K command palette
│   │   ├── DownloadManager.cpp/h   # Download management
│   │   ├── TreeTabSidebar.cpp/h    # Tree-style tab sidebar
│   │   ├── BookmarkPanel.cpp/h     # Bookmark sidebar
│   │   ├── ExtensionPanel.cpp/h    # Extension manager UI
│   │   └── CrawlerPanel.cpp/h      # Web crawler UI
│   ├── security/
│   │   └── SecurityManager.cpp/h   # Login detection & warnings
│   ├── extensions/
│   │   ├── ExtensionManager.cpp/h  # Extension lifecycle
│   │   ├── ExtensionBridge.cpp/h   # JS ↔ C++ bridge
│   │   └── NpiCompiler.cpp/h       # NPI extension compiler
│   ├── theme/
│   │   └── ThemeEngine.cpp/h       # Theme management (7 themes)
│   ├── media/
│   │   └── AudioPlayer.cpp/h       # Audio playback
│   ├── sync/
│   │   └── GitHubSync.cpp/h        # GitHub settings sync
│   ├── interop/
│   │   └── CoreBridge.cpp/h        # C# NativeAOT bridge
│   └── pages/
│       └── InternalPages.cpp/h     # oyn:// internal pages
├── core/
│   └── OyNIx.Core/                 # C# NativeAOT library (.NET 8)
├── data/
│   └── community.jsonl             # Seed community database
├── debian/                         # Debian packaging (dpkg-buildpackage)
├── scripts/
│   ├── build-deb.sh                # Build .deb package
│   ├── build-pacman.sh             # Build Arch pacman package
│   └── build-ubuntu.sh             # Build from source on Ubuntu
├── themes/                         # Theme files
├── resources/                      # Qt resources (icons, QRC)
├── assets/                         # Application icons
└── CMakeLists.txt                  # Build system
```

## Building Packages

### Automated (GitHub Actions)

Push a version tag to trigger release builds for all platforms:

```bash
git tag v3.1.0
git push origin v3.1.0
```

Or trigger manually from the Actions tab (workflow_dispatch). Builds on `main` and `claude/*` branches also create pre-releases.

Platforms built:
- **Ubuntu/Mint (.deb)** — dpkg-buildpackage with bundled Qt6
- **Windows (.exe/.zip)** — MSVC build with windeployqt
- **Arch Linux (.pkg.tar.zst)** — makepkg with PKGBUILD

## License

See [LICENSE](LICENSE) for details.

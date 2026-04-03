#!/usr/bin/env python3
"""
OyNIx Browser v1.1.0 - The Nyx-Powered Local AI Browser
Main Launcher and Entry Point

Features:
- Firefox-inspired browser with local LLM
- Tree-style tabs + normal tabs
- Medium purple + black Nyx theme
- Nyx search engine with auto-indexing
- DuckDuckGo / Google / Brave search
- GitHub database sync
- 1400+ curated site database
- Security prompts for login pages

Architecture:
- oynix.py (this file)       - Main launcher
- core/browser.py            - Browser core engine
- core/tree_tabs.py          - Tree-style tab manager
- core/theme_engine.py       - Nyx purple/black theme
- core/nyx_search.py         - Nyx search engine + auto-indexing
- core/ai_manager.py         - Local LLM (auto-download + inference)
- core/database.py           - 1400+ curated site database
- core/security.py           - Security prompts
- core/github_sync.py        - GitHub database sync
- UI/menus.py                - Menu system
- UI/settings.py             - Settings dialog
- UI/ai_chat.py              - AI chat panel
- utils/helpers.py           - Utility functions
- utils/logger.py            - Logging

Author: OyNIx Team
License: MIT
Version: 1.1.0
"""

import sys
import os

# Ensure the package root is on the path
OYNIX_DIR = os.path.dirname(os.path.abspath(__file__))
PACKAGE_ROOT = os.path.dirname(OYNIX_DIR)
if PACKAGE_ROOT not in sys.path:
    sys.path.insert(0, PACKAGE_ROOT)

# Fix LD_LIBRARY_PATH for pip-installed Qt6 libraries
# pip's PyQt6-WebEngine-Qt6 bundles .so files that the system linker
# can't find without this (e.g. libQt6WebChannel.so.6)
try:
    import PyQt6
    _qt6_lib = os.path.join(os.path.dirname(PyQt6.__file__), 'Qt6', 'lib')
    if os.path.isdir(_qt6_lib):
        _ld_path = os.environ.get('LD_LIBRARY_PATH', '')
        if _qt6_lib not in _ld_path:
            os.environ['LD_LIBRARY_PATH'] = _qt6_lib + (':' + _ld_path if _ld_path else '')
            # Re-exec to pick up new LD_LIBRARY_PATH (must happen before .so loading)
            if not os.environ.get('_OYNIX_REEXEC'):
                os.environ['_OYNIX_REEXEC'] = '1'
                os.execv(sys.executable, [sys.executable] + sys.argv)
except ImportError:
    pass  # PyQt6 not installed, will be caught later

PURPLE = "\033[95m"
BOLD = "\033[1m"
RESET = "\033[0m"
DIM = "\033[2m"

print()
print(f"{PURPLE}{BOLD}  ╔═══════════════════════════════════════════════╗{RESET}")
print(f"{PURPLE}{BOLD}  ║         OyNIx Browser v1.1.0                 ║{RESET}")
print(f"{PURPLE}{BOLD}  ║    The Nyx-Powered Local AI Browser          ║{RESET}")
print(f"{PURPLE}{BOLD}  ╚═══════════════════════════════════════════════╝{RESET}")
print()
print(f"{DIM}  Loading modules...{RESET}")

# Check Python version
if sys.version_info < (3, 8):
    print("ERROR: Python 3.8+ required")
    sys.exit(1)

# Import PyQt6
try:
    from PyQt6.QtWidgets import QApplication
    from PyQt6.QtCore import Qt
    print(f"  {PURPLE}+{RESET} PyQt6 loaded")
except ImportError as e:
    err = str(e)
    if 'libEGL' in err or 'libGL' in err or '.so' in err:
        print(f"ERROR: Missing system library: {err}")
        print("Fix:  sudo apt install libegl1 libgl1 libxkbcommon0")
        print("      (or equivalent for your distro)")
    else:
        print("ERROR: PyQt6 not installed")
        print("Run: pip install --break-system-packages PyQt6 PyQt6-WebEngine")
    sys.exit(1)

# CRITICAL: QtWebEngineWidgets must be imported BEFORE QApplication is created.
# Qt6 WebEngine requires AA_ShareOpenGLContexts set before the app starts.
try:
    Qt.ApplicationAttribute.AA_ShareOpenGLContexts
    QApplication.setAttribute(Qt.ApplicationAttribute.AA_ShareOpenGLContexts)
except AttributeError:
    pass  # Older PyQt6 versions may not have this

try:
    import PyQt6.QtWebEngineWidgets  # noqa: F401 — must be imported before QApplication
    print(f"  {PURPLE}+{RESET} QtWebEngine loaded")
except ImportError as e:
    print(f"  {BOLD}!{RESET} QtWebEngine not available: {e}")
    print("    Fix: pip install --break-system-packages PyQt6-WebEngine PyQt6-WebEngine-Qt6")

def main():
    """Main entry point."""
    # Diagnostic mode
    if '--diagnose' in sys.argv:
        print(f"\n{PURPLE}{BOLD}  OyNIx Diagnostics{RESET}\n")
        print(f"  Python: {sys.version}")
        print(f"  Executable: {sys.executable}")
        print(f"  sys.path: {sys.path[:5]}")
        print(f"  LD_LIBRARY_PATH: {os.environ.get('LD_LIBRARY_PATH', '(not set)')}")
        for mod in ['PyQt6', 'PyQt6.QtWidgets', 'PyQt6.QtWebEngineWidgets',
                     'PyQt6.QtWebEngineCore', 'llama_cpp', 'whoosh', 'bs4', 'PIL']:
            try:
                __import__(mod)
                loc = getattr(sys.modules.get(mod), '__file__', '?')
                print(f"  {PURPLE}+{RESET} {mod}: OK ({loc})")
            except ImportError as e:
                print(f"  {BOLD}x{RESET} {mod}: MISSING ({e})")
        try:
            import PyQt6
            qt6_lib = os.path.join(os.path.dirname(PyQt6.__file__), 'Qt6', 'lib')
            if os.path.isdir(qt6_lib):
                import glob
                sos = glob.glob(os.path.join(qt6_lib, 'libQt6*.so*'))
                print(f"\n  Qt6 lib dir: {qt6_lib}")
                print(f"  Qt6 .so files: {len(sos)}")
                for s in sorted(sos)[:10]:
                    print(f"    {os.path.basename(s)}")
            else:
                print(f"\n  Qt6 lib dir NOT found at: {qt6_lib}")
        except ImportError:
            pass
        print()
        return

    print()
    print(f"{DIM}  Initializing...{RESET}")

    # Create QApplication FIRST - required before any QObject/QWidget
    app = QApplication(sys.argv)
    app.setApplicationName("OyNIx Browser")
    app.setApplicationVersion("2.1.2")
    app.setOrganizationName("OyNIx")

    # Create data directories
    config_dir = os.path.expanduser("~/.config/oynix")
    for subdir in ['models', 'search_index', 'database', 'cache', 'sync']:
        os.makedirs(os.path.join(config_dir, subdir), exist_ok=True)

    # Import browser core AFTER QApplication exists
    # (modules contain QObject singletons that need it)
    try:
        from oynix.core.browser import OynixBrowser
        print(f"  {PURPLE}+{RESET} Browser core loaded (Chromium WebEngine)")
    except ImportError as e:
        err = str(e)
        print(f"ERROR: Failed to load browser core: {err}")
        if 'WebChannel' in err or 'WebEngine' in err or '.so' in err:
            print()
            print("  Missing Qt6 WebEngine system libraries.")
            print("  Fix with:")
            print("    sudo apt install libqt6webchannel6 libqt6webenginecore6 libqt6webenginewidgets6")
            print("  Or reinstall PyQt6-WebEngine:")
            print("    pip install --break-system-packages --force-reinstall PyQt6-WebEngine PyQt6-WebEngine-Qt6")
        else:
            import traceback
            traceback.print_exc()
        sys.exit(1)

    # Create and show browser
    print(f"  {PURPLE}+{RESET} Creating browser window...")
    try:
        browser = OynixBrowser()
        browser.show()
    except Exception as e:
        print(f"ERROR: Failed to create browser window: {e}")
        import traceback
        traceback.print_exc()
        print()
        print("  Try running: python3 -m oynix --diagnose")
        sys.exit(1)

    print()
    print(f"{PURPLE}{BOLD}  OyNIx Browser is ready!{RESET}")
    print(f"{DIM}  Features: Tree Tabs | Nyx Search | Local AI | 1400+ Sites{RESET}")
    print()

    sys.exit(app.exec())


if __name__ == "__main__":
    main()

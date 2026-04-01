#!/usr/bin/env python3
"""
OyNIx Browser v1.0.0 - The Nyx-Powered Local AI Browser
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
Version: 1.0.0
"""

import sys
import os

# Ensure the package root is on the path
OYNIX_DIR = os.path.dirname(os.path.abspath(__file__))
PACKAGE_ROOT = os.path.dirname(OYNIX_DIR)
if PACKAGE_ROOT not in sys.path:
    sys.path.insert(0, PACKAGE_ROOT)

PURPLE = "\033[95m"
BOLD = "\033[1m"
RESET = "\033[0m"
DIM = "\033[2m"

print()
print(f"{PURPLE}{BOLD}  ╔═══════════════════════════════════════════════╗{RESET}")
print(f"{PURPLE}{BOLD}  ║         OyNIx Browser v1.0.0                 ║{RESET}")
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
except ImportError:
    print("ERROR: PyQt6 not installed")
    print("Run: pip install PyQt6 PyQt6-WebEngine")
    sys.exit(1)

# Import browser core
try:
    from oynix.core.browser import OynixBrowser
    print(f"  {PURPLE}+{RESET} Browser core loaded")
except ImportError as e:
    print(f"ERROR: Failed to load browser core: {e}")
    import traceback
    traceback.print_exc()
    sys.exit(1)


def main():
    """Main entry point."""
    print()
    print(f"{DIM}  Initializing...{RESET}")

    # Create application
    app = QApplication(sys.argv)
    app.setApplicationName("OyNIx Browser")
    app.setApplicationVersion("1.0.0")
    app.setOrganizationName("OyNIx")

    # Create data directories
    config_dir = os.path.expanduser("~/.config/oynix")
    for subdir in ['models', 'search_index', 'database', 'cache', 'sync']:
        os.makedirs(os.path.join(config_dir, subdir), exist_ok=True)

    # Create and show browser
    print(f"  {PURPLE}+{RESET} Creating browser window...")
    browser = OynixBrowser()
    browser.show()

    print()
    print(f"{PURPLE}{BOLD}  OyNIx Browser is ready!{RESET}")
    print(f"{DIM}  Features: Tree Tabs | Nyx Search | Local AI | 1400+ Sites{RESET}")
    print()

    sys.exit(app.exec())


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""
Oynix Browser v0.6.11.1 ULTIMATE - Multi-File Edition
Main Launcher and Entry Point

ARCHITECTURE:
- oynix.py (this file) - Main launcher
- core/browser.py - Browser core engine
- core/search_engine_v2.py - Advanced search engine
- core/database.py - Massive 1400+ site database
- core/ai_manager.py - Nano-mini-6.99-v1 integration
- core/security.py - Security prompts and protection
- ui/menus.py - Completely remade menus
- ui/settings.py - Revamped settings system
- ui/themes.py - Theme management
- utils/helpers.py - Utility functions

Author: Oynix Team
License: MIT
Version: 0.6.11.1
"""

import sys
import os

# Add core modules to path
OYNIX_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(OYNIX_DIR, 'core'))
sys.path.insert(0, os.path.join(OYNIX_DIR, 'ui'))
sys.path.insert(0, os.path.join(OYNIX_DIR, 'utils'))

print("=" * 80)
print("OYNIX BROWSER v0.6.11.1 ULTIMATE - Multi-File Edition")
print("=" * 80)
print("Loading modules...")

# Check Python version
if sys.version_info < (3, 8):
    print("ERROR: Python 3.8+ required")
    sys.exit(1)

# Import core modules
try:
    from PyQt6.QtWidgets import QApplication
    from PyQt6.QtCore import Qt
    print("✓ PyQt6 loaded")
except ImportError:
    print("ERROR: PyQt6 not installed")
    print("Run: pip install PyQt6 PyQt6-WebEngine")
    sys.exit(1)

try:
    from core.browser import OynixBrowser
    print("✓ Browser core loaded")
except ImportError as e:
    print(f"ERROR: Failed to load browser core: {e}")
    sys.exit(1)

def main():
    """Main entry point for Oynix Browser."""
    print("\nInitializing Oynix Browser...")
    
    # Create application
    app = QApplication(sys.argv)
    app.setApplicationName("Oynix Browser")
    app.setApplicationVersion("0.6.11.1")
    app.setOrganizationName("Oynix")
    
    # Set application attributes
    app.setAttribute(Qt.ApplicationAttribute.AA_EnableHighDpiScaling, True)
    app.setAttribute(Qt.ApplicationAttribute.AA_UseHighDpiPixmaps, True)
    
    # Create browser window
    print("Creating browser window...")
    browser = OynixBrowser()
    browser.show()
    
    print("\n" + "=" * 80)
    print("Oynix Browser v0.6.11.1 is ready!")
    print("Features: 1400+ sites | Multi-file | V2 Search | Nano-mini AI")
    print("=" * 80 + "\n")
    
    # Run application
    sys.exit(app.exec())

if __name__ == "__main__":
    main()

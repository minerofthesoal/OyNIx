#!/usr/bin/env python3
"""
OyNIx Browser v2.2 - The Nyx-Powered Local AI Browser
Main Launcher and Entry Point

Author: OyNIx Team
License: MIT
Version: 2.2
"""

import sys
import os
import logging

# Ensure the package root is on the path
OYNIX_DIR = os.path.dirname(os.path.abspath(__file__))
PACKAGE_ROOT = os.path.dirname(OYNIX_DIR)
if PACKAGE_ROOT not in sys.path:
    sys.path.insert(0, PACKAGE_ROOT)

# Also check /usr/lib/oynix (for .deb installs)
_deb_path = '/usr/lib/oynix'
if os.path.isdir(os.path.join(_deb_path, 'oynix')) and _deb_path not in sys.path:
    sys.path.insert(0, _deb_path)

# Set up file logging so errors are captured even without a terminal
_log_dir = os.path.expanduser("~/.config/oynix")
os.makedirs(_log_dir, exist_ok=True)
logging.basicConfig(
    filename=os.path.join(_log_dir, "oynix.log"),
    level=logging.INFO,
    format="%(asctime)s %(levelname)s %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
)
_logger = logging.getLogger("oynix")


def _safe_print(*args, **kwargs):
    """Print that silently fails when there is no terminal (e.g. desktop launch)."""
    try:
        print(*args, **kwargs)
    except (OSError, ValueError):
        pass


def _fix_ld_library_path():
    """Set LD_LIBRARY_PATH for pip-installed Qt6 libraries and re-exec if needed."""
    try:
        import PyQt6
        _qt6_lib = os.path.join(os.path.dirname(PyQt6.__file__), 'Qt6', 'lib')
        if os.path.isdir(_qt6_lib):
            _ld_path = os.environ.get('LD_LIBRARY_PATH', '')
            if _qt6_lib not in _ld_path:
                os.environ['LD_LIBRARY_PATH'] = _qt6_lib + (':' + _ld_path if _ld_path else '')
                if not os.environ.get('_OYNIX_REEXEC'):
                    os.environ['_OYNIX_REEXEC'] = '1'
                    os.execv(sys.executable, [sys.executable] + sys.argv)
    except ImportError:
        pass


def _diagnose():
    """Run diagnostics and exit."""
    P = "\033[95m"
    B = "\033[1m"
    R = "\033[0m"
    _safe_print(f"\n{P}{B}  OyNIx Diagnostics{R}\n")
    _safe_print(f"  Python: {sys.version}")
    _safe_print(f"  Executable: {sys.executable}")
    _safe_print(f"  sys.path: {sys.path[:5]}")
    _safe_print(f"  LD_LIBRARY_PATH: {os.environ.get('LD_LIBRARY_PATH', '(not set)')}")
    for mod in ['PyQt6', 'PyQt6.QtWidgets', 'PyQt6.QtWebEngineWidgets',
                'PyQt6.QtWebEngineCore', 'llama_cpp', 'whoosh', 'bs4', 'PIL']:
        try:
            __import__(mod)
            loc = getattr(sys.modules.get(mod), '__file__', '?')
            _safe_print(f"  {P}+{R} {mod}: OK ({loc})")
        except ImportError as e:
            _safe_print(f"  {B}x{R} {mod}: MISSING ({e})")
    try:
        import PyQt6
        qt6_lib = os.path.join(os.path.dirname(PyQt6.__file__), 'Qt6', 'lib')
        if os.path.isdir(qt6_lib):
            import glob
            sos = glob.glob(os.path.join(qt6_lib, 'libQt6*.so*'))
            _safe_print(f"\n  Qt6 lib dir: {qt6_lib}")
            _safe_print(f"  Qt6 .so files: {len(sos)}")
            for s in sorted(sos)[:10]:
                _safe_print(f"    {os.path.basename(s)}")
        else:
            _safe_print(f"\n  Qt6 lib dir NOT found at: {qt6_lib}")
    except ImportError:
        pass
    _safe_print()


def main():
    """Main entry point — safe to call from desktop launchers (no terminal required)."""
    _logger.info("OyNIx starting (Python %s, pid %d)", sys.version.split()[0], os.getpid())
    _logger.info("sys.path: %s", sys.path[:5])
    _logger.info("LD_LIBRARY_PATH: %s", os.environ.get('LD_LIBRARY_PATH', '(not set)'))

    # Fix LD_LIBRARY_PATH before anything loads Qt .so files
    _fix_ld_library_path()

    P = "\033[95m"
    B = "\033[1m"
    R = "\033[0m"
    D = "\033[2m"

    _safe_print()
    _safe_print(f"{P}{B}  ╔═══════════════════════════════════════════════╗{R}")
    _safe_print(f"{P}{B}  ║         OyNIx Browser v2.2                 ║{R}")
    _safe_print(f"{P}{B}  ║    The Nyx-Powered Local AI Browser          ║{R}")
    _safe_print(f"{P}{B}  ╚═══════════════════════════════════════════════╝{R}")
    _safe_print()
    _safe_print(f"{D}  Loading modules...{R}")

    # Check Python version
    if sys.version_info < (3, 8):
        _safe_print("ERROR: Python 3.8+ required")
        sys.exit(1)

    # Diagnostic mode
    if '--diagnose' in sys.argv:
        _diagnose()
        return

    # Import PyQt6
    try:
        from PyQt6.QtWidgets import QApplication
        from PyQt6.QtCore import Qt
        _safe_print(f"  {P}+{R} PyQt6 loaded")
    except ImportError as e:
        err = str(e)
        _logger.error("PyQt6 import failed: %s", err)
        _safe_print(f"ERROR: {err}")
        _safe_print("Run: pip install --break-system-packages PyQt6 PyQt6-WebEngine")
        try:
            import tkinter as tk
            from tkinter import messagebox
            root = tk.Tk()
            root.withdraw()
            messagebox.showerror("OyNIx Browser", f"Cannot start: {err}\n\nInstall PyQt6:\npip install PyQt6 PyQt6-WebEngine")
            root.destroy()
        except Exception:
            # Last resort: desktop notification
            try:
                import subprocess
                subprocess.run(['notify-send', 'OyNIx Browser',
                                f'Failed to start: {err}', '--icon=dialog-error'],
                               timeout=5)
            except Exception:
                pass
        sys.exit(1)

    # Set OpenGL sharing before QApplication
    try:
        QApplication.setAttribute(Qt.ApplicationAttribute.AA_ShareOpenGLContexts)
    except AttributeError:
        pass

    try:
        import PyQt6.QtWebEngineWidgets  # noqa: F401 — must be imported before QApplication
        _safe_print(f"  {P}+{R} QtWebEngine loaded")
    except ImportError as e:
        _safe_print(f"  {B}!{R} QtWebEngine not available: {e}")
        _safe_print("    Fix: pip install --break-system-packages PyQt6-WebEngine PyQt6-WebEngine-Qt6")

    _safe_print()
    _safe_print(f"{D}  Initializing...{R}")

    # Create QApplication FIRST - required before any QObject/QWidget
    app = QApplication(sys.argv)
    app.setApplicationName("OyNIx Browser")
    app.setApplicationVersion("2.2")
    app.setOrganizationName("OyNIx")
    app.setDesktopFileName("oynix")

    # Create data directories
    config_dir = os.path.expanduser("~/.config/oynix")
    for subdir in ['models', 'search_index', 'database', 'cache', 'sync']:
        os.makedirs(os.path.join(config_dir, subdir), exist_ok=True)

    # Import browser core AFTER QApplication exists
    try:
        from oynix.core.browser import OynixBrowser
        _safe_print(f"  {P}+{R} Browser core loaded (Chromium WebEngine)")
    except Exception as e:
        err = str(e)
        _logger.error("Browser core import failed: %s", err, exc_info=True)
        _safe_print(f"ERROR: Failed to load browser core: {err}")
        try:
            from PyQt6.QtWidgets import QMessageBox
            QMessageBox.critical(None, "OyNIx Browser",
                                 f"Failed to load browser:\n{err}\n\nRun: python3 -m oynix --diagnose")
        except Exception:
            pass
        sys.exit(1)

    # Create and show browser
    _safe_print(f"  {P}+{R} Creating browser window...")
    try:
        browser = OynixBrowser()
        browser.show()
    except Exception as e:
        _logger.error("Browser window creation failed: %s", e, exc_info=True)
        _safe_print(f"ERROR: Failed to create browser window: {e}")
        try:
            from PyQt6.QtWidgets import QMessageBox
            QMessageBox.critical(None, "OyNIx Browser",
                                 f"Failed to create browser window:\n{e}")
        except Exception:
            pass
        sys.exit(1)

    _safe_print()
    _safe_print(f"{P}{B}  OyNIx Browser is ready!{R}")
    _safe_print(f"{D}  Features: Tree Tabs | Nyx Search | Local AI | 1400+ Sites{R}")
    _safe_print()

    sys.exit(app.exec())


if __name__ == "__main__":
    main()

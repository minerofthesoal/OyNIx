#!/bin/bash
# ============================================================
#  OyNIx Browser - Quick Launcher
#  Run the browser directly from the source tree without installing.
#  Usage: ./run.sh [--diagnose] [--install-desktop]
# ============================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Find Python 3.10+
PYTHON=""
for cmd in python3.12 python3.11 python3.10 python3 python; do
    if command -v "$cmd" &>/dev/null; then
        if $cmd -c "import sys; exit(0 if sys.version_info >= (3,10) else 1)" 2>/dev/null; then
            PYTHON="$cmd"
            break
        fi
    fi
done

if [ -z "$PYTHON" ]; then
    echo "ERROR: Python 3.10+ required"
    echo "Install: sudo apt install python3 python3-pip"
    exit 1
fi

# Activate venv if present
if [ -f "$SCRIPT_DIR/.venv/bin/activate" ]; then
    source "$SCRIPT_DIR/.venv/bin/activate"
fi

# Set LD_LIBRARY_PATH for pip-installed Qt6 libraries
QT6_LIB=$($PYTHON -c "import PyQt6,os; print(os.path.join(os.path.dirname(PyQt6.__file__),'Qt6','lib'))" 2>/dev/null)
if [ -n "$QT6_LIB" ] && [ -d "$QT6_LIB" ]; then
    export LD_LIBRARY_PATH="${QT6_LIB}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

# Auto-install desktop entry on first run (so app appears in menu)
install_desktop() {
    APPS_DIR="$HOME/.local/share/applications"
    mkdir -p "$APPS_DIR"

    # Find best icon
    ICON="oynix"
    for size in 256 128 64 48; do
        if [ -f "$SCRIPT_DIR/assets/icon-${size}.png" ]; then
            ICON_DIR="$HOME/.local/share/icons/hicolor/${size}x${size}/apps"
            mkdir -p "$ICON_DIR"
            cp "$SCRIPT_DIR/assets/icon-${size}.png" "$ICON_DIR/oynix.png" 2>/dev/null
        fi
    done

    cat > "$APPS_DIR/oynix.desktop" <<DESKTOP
[Desktop Entry]
Name=OyNIx Browser
Comment=Nyx-Powered Local AI Browser
Exec=$SCRIPT_DIR/run.sh %u
Icon=oynix
Terminal=false
Type=Application
Categories=Network;WebBrowser;
MimeType=text/html;text/xml;application/xhtml+xml;x-scheme-handler/http;x-scheme-handler/https;
Keywords=browser;web;internet;ai;nyx;
StartupNotify=true
StartupWMClass=OyNIx Browser
DESKTOP
    chmod +x "$APPS_DIR/oynix.desktop"

    # Update desktop database if available
    if command -v update-desktop-database &>/dev/null; then
        update-desktop-database "$APPS_DIR" 2>/dev/null || true
    fi
    if command -v gtk-update-icon-cache &>/dev/null; then
        gtk-update-icon-cache "$HOME/.local/share/icons/hicolor" 2>/dev/null || true
    fi
    echo "Desktop entry installed — OyNIx will appear in your app menu"
}

# Install desktop entry if requested or if it doesn't exist yet
if [ "$1" = "--install-desktop" ]; then
    install_desktop
    exit 0
fi

if [ ! -f "$HOME/.local/share/applications/oynix.desktop" ]; then
    install_desktop 2>/dev/null
fi

cd "$SCRIPT_DIR"
exec $PYTHON -m oynix "$@"

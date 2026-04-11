#!/bin/bash
# ============================================================
#  OyNIx Browser - Quick Launcher
#  Run the browser directly from the source tree without installing.
#  Works from terminal AND desktop launchers (no terminal required).
#  Usage: ./run.sh [--diagnose] [--install-desktop]
# ============================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Redirect output to log file when no terminal (desktop launch)
if ! [ -t 1 ]; then
    LOG_DIR="$HOME/.config/oynix"
    mkdir -p "$LOG_DIR"
    exec >>"$LOG_DIR/launch.log" 2>&1
    echo "--- Launch at $(date) ---"
fi

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
    # Try to show a GUI error if no terminal
    if ! [ -t 1 ] && command -v notify-send &>/dev/null; then
        notify-send "OyNIx Browser" "Python 3.10+ is required but not found." --icon=dialog-error
    fi
    echo "ERROR: Python 3.10+ required"
    exit 1
fi

# Activate venv if present
if [ -f "$SCRIPT_DIR/.venv/bin/activate" ]; then
    source "$SCRIPT_DIR/.venv/bin/activate"
fi

# Set PYTHONPATH so subshell can find PyQt6 in the venv or system
export PYTHONPATH="${SCRIPT_DIR}${PYTHONPATH:+:$PYTHONPATH}"

# Also check common system install paths
for extra in "/usr/lib/oynix" "/app/lib/oynix"; do
    [ -d "$extra/oynix" ] && export PYTHONPATH="$extra:$PYTHONPATH"
done

# Set LD_LIBRARY_PATH for pip-installed Qt6 libraries
# Try multiple known locations before falling back to import probe
for qt6_candidate in \
    "$($PYTHON -c "import PyQt6,os; print(os.path.join(os.path.dirname(PyQt6.__file__),'Qt6','lib'))" 2>/dev/null)" \
    "$VIRTUAL_ENV/lib/python3.*/site-packages/PyQt6/Qt6/lib" \
    "/usr/lib/python3/dist-packages/PyQt6/Qt6/lib" \
    "/usr/lib64/python3.*/site-packages/PyQt6/Qt6/lib"; do
    # Expand globs
    for qt6_dir in $qt6_candidate; do
        if [ -d "$qt6_dir" ]; then
            export LD_LIBRARY_PATH="${qt6_dir}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
            break 2
        fi
    done
done

# Auto-install desktop entry on first run (so app appears in menu)
install_desktop() {
    APPS_DIR="$HOME/.local/share/applications"
    mkdir -p "$APPS_DIR"

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

    if command -v update-desktop-database &>/dev/null; then
        update-desktop-database "$APPS_DIR" 2>/dev/null || true
    fi
    if command -v gtk-update-icon-cache &>/dev/null; then
        gtk-update-icon-cache "$HOME/.local/share/icons/hicolor" 2>/dev/null || true
    fi
    echo "Desktop entry installed"
}

if [ "$1" = "--install-desktop" ]; then
    install_desktop
    exit 0
fi

if [ ! -f "$HOME/.local/share/applications/oynix.desktop" ]; then
    install_desktop 2>/dev/null
fi

cd "$SCRIPT_DIR"
exec $PYTHON -m oynix "$@"

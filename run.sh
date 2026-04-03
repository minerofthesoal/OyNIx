#!/bin/bash
# ============================================================
#  OyNIx Browser - Quick Launcher
#  Run the browser directly from the source tree without installing.
#  Usage: ./run.sh [--diagnose]
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

cd "$SCRIPT_DIR"
exec $PYTHON -m oynix "$@"

#!/bin/bash
# ============================================================
#  OyNIx Browser - One-Click Installer
#  Installs Python deps, builds native components, sets up launcher
#  Coded by Claude (Anthropic)
# ============================================================

set -e

PURPLE='\033[0;35m'
BOLD='\033[1m'
DIM='\033[2m'
NC='\033[0m'

echo -e "${PURPLE}${BOLD}"
echo "  ╔═══════════════════════════════════════════╗"
echo "  ║          OyNIx Browser Installer          ║"
echo "  ║       The Nyx-Powered Local AI Browser    ║"
echo "  ╚═══════════════════════════════════════════╝"
echo -e "${NC}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# ── Step 1: Python ──────────────────────────────────────────────
echo -e "${PURPLE}[1/5]${NC} Checking Python..."
PYTHON_CMD=""
for cmd in python3.12 python3.11 python3.10 python3 python; do
    if command -v "$cmd" &>/dev/null; then
        if $cmd -c "import sys; exit(0 if sys.version_info >= (3,10) else 1)" 2>/dev/null; then
            PYTHON_CMD="$cmd"
            break
        fi
    fi
done

if [ -z "$PYTHON_CMD" ]; then
    echo "ERROR: Python 3.10+ is required but not found."
    echo "Install Python from https://python.org"
    exit 1
fi

PYVER=$($PYTHON_CMD -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")')
echo -e "  Found ${BOLD}Python ${PYVER}${NC} ($PYTHON_CMD)"

# ── Step 2: Python dependencies ─────────────────────────────────
echo -e "${PURPLE}[2/5]${NC} Installing Python dependencies..."

if [ -d ".venv" ] && [ -f ".venv/bin/activate" ]; then
    echo -e "  ${DIM}Using existing venv${NC}"
    source .venv/bin/activate
elif command -v pipx &>/dev/null || $PYTHON_CMD -c "import venv" 2>/dev/null; then
    echo -e "  ${DIM}Creating virtual environment...${NC}"
    $PYTHON_CMD -m venv .venv 2>/dev/null && source .venv/bin/activate 2>/dev/null
fi

# Install with best available method
if [ -n "$VIRTUAL_ENV" ]; then
    pip install --upgrade pip
    pip install -r requirements.txt
else
    $PYTHON_CMD -m pip install --break-system-packages --upgrade pip 2>/dev/null || \
        $PYTHON_CMD -m pip install --upgrade pip
    $PYTHON_CMD -m pip install --break-system-packages -r requirements.txt 2>/dev/null || \
        $PYTHON_CMD -m pip install -r requirements.txt
fi

# ── Step 3: Build native components ─────────────────────────────
echo -e "${PURPLE}[3/5]${NC} Building native components..."

if command -v make &>/dev/null && command -v gcc &>/dev/null; then
    make all 2>/dev/null && echo "  Built C launcher + C++ indexer" || \
    echo -e "  ${DIM}Native build optional, skipping${NC}"
elif command -v gcc &>/dev/null; then
    mkdir -p build
    gcc -O2 -o build/oynix src/launcher.c 2>/dev/null && \
        echo "  Built C launcher" || true
    if command -v g++ &>/dev/null; then
        g++ -O2 -shared -fPIC -std=c++17 \
            -o build/libnyx_index.so src/native/fast_index.cpp 2>/dev/null && \
            echo "  Built C++ indexer" || true
    fi
else
    echo -e "  ${DIM}No C compiler found, using Python-only mode${NC}"
fi

# ── Step 4: Data directories ────────────────────────────────────
echo -e "${PURPLE}[4/5]${NC} Setting up data directories..."
mkdir -p ~/.config/oynix/{models,search_index,database,cache,sync}

# ── Step 5: Create launcher ────────────────────────────────────
echo -e "${PURPLE}[5/5]${NC} Creating launcher..."

# Use native launcher if built, otherwise shell script
if [ -f "build/oynix" ]; then
    cp build/oynix "$SCRIPT_DIR/oynix-browser"
    chmod +x "$SCRIPT_DIR/oynix-browser"
    echo "  Using native C launcher"
else
    ACTIVATE=""
    if [ -f "$SCRIPT_DIR/.venv/bin/activate" ]; then
        ACTIVATE="source \"$SCRIPT_DIR/.venv/bin/activate\""
    fi
    cat > "$SCRIPT_DIR/oynix-browser" <<LAUNCHER
#!/bin/bash
cd "$SCRIPT_DIR"
${ACTIVATE}
exec $PYTHON_CMD -m oynix "\$@"
LAUNCHER
    chmod +x "$SCRIPT_DIR/oynix-browser"
    echo "  Using shell launcher"
fi

echo ""
echo -e "${PURPLE}${BOLD}  ✓ Installation complete!${NC}"
echo ""
echo "  To run OyNIx Browser:"
echo "    ./oynix-browser"
echo ""
echo "  Or directly:"
echo "    $PYTHON_CMD -m oynix"
echo ""
echo -e "${DIM}  The LLM model will auto-download on first launch (~700MB).${NC}"
echo -e "${DIM}  Coded by Claude (Anthropic)${NC}"
echo ""

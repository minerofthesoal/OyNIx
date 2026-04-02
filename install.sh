#!/bin/bash
# ============================================================
#  OyNIx Browser - One-Click Installer
#  Installs Python deps, builds native components, sets up launcher
#  Coded by Claude (Anthropic)
# ============================================================

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
    echo "Install: sudo apt install python3 python3-pip python3-venv"
    exit 1
fi

PYVER=$($PYTHON_CMD -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")')
echo -e "  Found ${BOLD}Python ${PYVER}${NC} ($PYTHON_CMD)"

# ── Step 1.5: System libraries ──────────────────────────────────
echo -e "${PURPLE}[1.5/5]${NC} Checking system libraries..."
if ! $PYTHON_CMD -c "from ctypes import cdll; cdll.LoadLibrary('libEGL.so.1')" 2>/dev/null; then
    echo -e "  ${DIM}Installing missing system libraries...${NC}"
    if command -v apt-get &>/dev/null; then
        sudo apt-get install -y libegl1 libgl1 libxkbcommon0 libnss3 libxcomposite1 2>/dev/null || \
            echo -e "  ${DIM}Could not auto-install. Run: sudo apt install libegl1 libgl1 libxkbcommon0${NC}"
    elif command -v pacman &>/dev/null; then
        sudo pacman -S --noconfirm --needed libglvnd nss libxcomposite 2>/dev/null || true
    elif command -v dnf &>/dev/null; then
        sudo dnf install -y mesa-libEGL mesa-libGL libxkbcommon nss 2>/dev/null || true
    fi
else
    echo -e "  ${DIM}System libraries OK${NC}"
fi

# ── Step 2: Python dependencies ─────────────────────────────────
echo -e "${PURPLE}[2/5]${NC} Installing Python dependencies..."

INSTALLED=0

# Method 1: Try existing venv
if [ -d ".venv" ] && [ -f ".venv/bin/activate" ]; then
    echo -e "  ${DIM}Using existing venv${NC}"
    source .venv/bin/activate
    pip install --upgrade pip 2>/dev/null
    pip install -r requirements.txt && INSTALLED=1
fi

# Method 2: Try creating a new venv
if [ "$INSTALLED" = "0" ]; then
    if $PYTHON_CMD -m venv .venv 2>/dev/null; then
        echo -e "  ${DIM}Created virtual environment${NC}"
        source .venv/bin/activate
        pip install --upgrade pip 2>/dev/null
        pip install -r requirements.txt && INSTALLED=1
    fi
fi

# Method 3: pip with --break-system-packages
if [ "$INSTALLED" = "0" ]; then
    echo -e "  ${DIM}Trying pip with --break-system-packages...${NC}"
    if $PYTHON_CMD -m pip install --break-system-packages -r requirements.txt 2>/dev/null; then
        INSTALLED=1
    fi
fi

# Method 4: plain pip (older systems)
if [ "$INSTALLED" = "0" ]; then
    echo -e "  ${DIM}Trying plain pip...${NC}"
    if $PYTHON_CMD -m pip install -r requirements.txt 2>/dev/null; then
        INSTALLED=1
    fi
fi

if [ "$INSTALLED" = "0" ]; then
    echo ""
    echo -e "  ${BOLD}Could not install Python dependencies automatically.${NC}"
    echo ""
    echo "  Try one of these manually:"
    echo "    sudo apt install python3-venv && $PYTHON_CMD -m venv .venv && source .venv/bin/activate && pip install -r requirements.txt"
    echo "    $PYTHON_CMD -m pip install --break-system-packages -r requirements.txt"
    echo ""
    echo "  Then re-run: ./install.sh"
    exit 1
fi

echo -e "  ${BOLD}Dependencies installed${NC}"

# ── Step 3: Build native components ─────────────────────────────
echo -e "${PURPLE}[3/5]${NC} Building native components..."

if command -v make &>/dev/null && command -v gcc &>/dev/null; then
    if make all 2>/dev/null; then
        echo "  Built C launcher + C++ indexer"
    else
        echo -e "  ${DIM}Native build skipped (optional)${NC}"
    fi
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
mkdir -p ~/.config/oynix/models
mkdir -p ~/.config/oynix/search_index
mkdir -p ~/.config/oynix/database
mkdir -p ~/.config/oynix/cache
mkdir -p ~/.config/oynix/sync

# ── Step 5: Create launcher ────────────────────────────────────
echo -e "${PURPLE}[5/5]${NC} Creating launcher..."

# Use native launcher if built, otherwise shell script
if [ -f "build/oynix" ]; then
    cp build/oynix "$SCRIPT_DIR/oynix-browser"
    chmod +x "$SCRIPT_DIR/oynix-browser"
    echo "  Using native C launcher"
else
    # Build shell launcher with venv activation if needed
    if [ -f "$SCRIPT_DIR/.venv/bin/activate" ]; then
        cat > "$SCRIPT_DIR/oynix-browser" <<LAUNCHER
#!/bin/bash
cd "$SCRIPT_DIR"
source "$SCRIPT_DIR/.venv/bin/activate"
exec $PYTHON_CMD -m oynix "\$@"
LAUNCHER
    else
        cat > "$SCRIPT_DIR/oynix-browser" <<LAUNCHER
#!/bin/bash
cd "$SCRIPT_DIR"
exec $PYTHON_CMD -m oynix "\$@"
LAUNCHER
    fi
    chmod +x "$SCRIPT_DIR/oynix-browser"
    echo "  Using shell launcher"
fi

echo ""
echo -e "${PURPLE}${BOLD}  Installation complete!${NC}"
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

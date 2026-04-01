#!/bin/bash
# ============================================================
#  OyNIx Browser - One-Click Installer
#  Installs all dependencies and sets up the browser
# ============================================================

set -e

PURPLE='\033[0;35m'
BLACK='\033[0;30m'
WHITE='\033[1;37m'
BOLD='\033[1m'
NC='\033[0m'

echo -e "${PURPLE}${BOLD}"
echo "  ╔═══════════════════════════════════════════╗"
echo "  ║          OyNIx Browser Installer          ║"
echo "  ║       The Nyx-Powered Local AI Browser    ║"
echo "  ╚═══════════════════════════════════════════╝"
echo -e "${NC}"

# Check Python version
echo -e "${PURPLE}[1/4]${NC} Checking Python..."
PYTHON_CMD=""
if command -v python3 &> /dev/null; then
    PYTHON_CMD="python3"
elif command -v python &> /dev/null; then
    PYTHON_CMD="python"
else
    echo "ERROR: Python 3.8+ is required but not found."
    echo "Install Python from https://python.org"
    exit 1
fi

PYTHON_VERSION=$($PYTHON_CMD -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")')
echo "  Found Python $PYTHON_VERSION"

# Install pip dependencies
echo -e "${PURPLE}[2/4]${NC} Installing dependencies..."

# Prefer pipx if available, otherwise pip with --break-system-packages
if command -v pipx &> /dev/null; then
    echo "  Using pipx..."
    # pipx is for apps; for libraries we still need pip but in a venv
    $PYTHON_CMD -m venv .venv 2>/dev/null && source .venv/bin/activate 2>/dev/null
    pip install --upgrade pip
    pip install -r requirements.txt
elif $PYTHON_CMD -m pip install --help 2>&1 | grep -q "break-system-packages"; then
    echo "  Using pip with --break-system-packages..."
    $PYTHON_CMD -m pip install --break-system-packages --upgrade pip
    $PYTHON_CMD -m pip install --break-system-packages -r requirements.txt
else
    $PYTHON_CMD -m pip install --upgrade pip
    $PYTHON_CMD -m pip install -r requirements.txt
fi

# Create data directories
echo -e "${PURPLE}[3/4]${NC} Setting up data directories..."
mkdir -p ~/.config/oynix/models
mkdir -p ~/.config/oynix/search_index
mkdir -p ~/.config/oynix/database
mkdir -p ~/.config/oynix/cache

# Create launcher script
echo -e "${PURPLE}[4/4]${NC} Creating launcher..."
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

cat > "$SCRIPT_DIR/oynix-browser" << LAUNCHER
#!/bin/bash
cd "$SCRIPT_DIR"
$PYTHON_CMD -m oynix "\$@"
LAUNCHER
chmod +x "$SCRIPT_DIR/oynix-browser"

echo ""
echo -e "${PURPLE}${BOLD}  Installation complete!${NC}"
echo ""
echo "  To run OyNIx Browser:"
echo "    ./oynix-browser"
echo ""
echo "  Or directly:"
echo "    $PYTHON_CMD -m oynix.oynix"
echo ""
echo -e "${PURPLE}  The LLM model will auto-download on first launch.${NC}"
echo ""

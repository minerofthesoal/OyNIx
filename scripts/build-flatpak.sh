#!/usr/bin/env bash
# build-flatpak.sh — Build OyNIx as a Flatpak (non-essential)
# Requires: flatpak, flatpak-builder
# Usage: ./scripts/build-flatpak.sh [version]

set -euo pipefail

VERSION="${1:-2.0.0}"
VERSION="${VERSION#v}"
APP_ID="io.github.oynix.browser"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$ROOT/flatpak-build"

echo "=== OyNIx Flatpak Builder v${VERSION} ==="
echo

# Check deps
for cmd in flatpak flatpak-builder; do
    if ! command -v "$cmd" &>/dev/null; then
        echo "ERROR: $cmd not found."
        echo "Install: sudo apt install flatpak flatpak-builder  (or equivalent)"
        exit 1
    fi
done

# Ensure Freedesktop runtime
echo "[1/4] Ensuring Flatpak runtime..."
flatpak install --user -y flathub org.freedesktop.Platform//23.08 org.freedesktop.Sdk//23.08 2>/dev/null || true

# Create manifest
echo "[2/4] Generating manifest..."
mkdir -p "$BUILD_DIR"

cat > "$BUILD_DIR/$APP_ID.yml" << 'MANIFEST'
app-id: io.github.oynix.browser
runtime: org.freedesktop.Platform
runtime-version: '23.08'
sdk: org.freedesktop.Sdk
command: oynix-launch
finish-args:
  - --share=ipc
  - --share=network
  - --socket=x11
  - --socket=wayland
  - --socket=pulseaudio
  - --device=dri
  - --filesystem=home
  - --filesystem=xdg-download
  - --env=QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox

modules:
  - name: python3-deps
    buildsystem: simple
    build-commands:
      - pip3 install --prefix=/app --no-build-isolation
          PyQt6>=6.5.0
          PyQt6-WebEngine>=6.5.0
          PyQt6-WebEngine-Qt6
          PyQt6-Qt6
          PyQt6-sip
          whoosh>=2.7.4
          beautifulsoup4>=4.12.0
          requests>=2.31.0
          Pillow>=10.0.0
          huggingface-hub>=0.20.0

  - name: oynix
    buildsystem: simple
    build-commands:
      - mkdir -p /app/lib/oynix
      - cp -r oynix /app/lib/oynix/
      - cp pyproject.toml /app/lib/oynix/

      # Create launcher script
      - |
        cat > /app/bin/oynix-launch << 'EOF'
        #!/bin/bash
        export PYTHONPATH="/app/lib/oynix:$PYTHONPATH"
        exec python3 -m oynix "$@"
        EOF
      - chmod +x /app/bin/oynix-launch

      # Desktop file
      - install -Dm644 oynix.desktop /app/share/applications/io.github.oynix.browser.desktop
      - sed -i 's/Exec=oynix/Exec=oynix-launch/' /app/share/applications/io.github.oynix.browser.desktop
      - sed -i 's/Icon=oynix/Icon=io.github.oynix.browser/' /app/share/applications/io.github.oynix.browser.desktop

    sources:
      - type: dir
        path: ..
MANIFEST

# Build
echo "[3/4] Building Flatpak (this may take a while)..."
cd "$BUILD_DIR"
flatpak-builder --user --install-deps-from=flathub --force-clean \
    build "$APP_ID.yml"

# Bundle
echo "[4/4] Creating .flatpak bundle..."
flatpak build-export repo build
flatpak build-bundle repo "OyNIx-${VERSION}.flatpak" "$APP_ID"

echo
echo "=== Flatpak built successfully ==="
echo "  Install: flatpak install --user OyNIx-${VERSION}.flatpak"
echo "  Run:     flatpak run $APP_ID"
echo

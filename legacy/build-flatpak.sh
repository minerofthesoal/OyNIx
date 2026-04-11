#!/usr/bin/env bash
# ============================================================
#  OyNIx Browser - Build Flatpak Package
#  Usage: ./scripts/build-flatpak.sh [version]
#
#  Requires: flatpak, flatpak-builder
# ============================================================
set -euo pipefail

VERSION="${1:-2.1.0}"
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

# Ensure KDE runtime (has Qt6 built in — no pip install needed)
echo "[1/5] Ensuring Flatpak runtime..."
flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo 2>/dev/null || true
flatpak install --user -y --noninteractive flathub \
    org.kde.Platform//6.6 \
    org.kde.Sdk//6.6 2>/dev/null || true

# Generate manifest
echo "[2/5] Generating manifest..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

cat > "$BUILD_DIR/$APP_ID.yml" <<MANIFEST
app-id: ${APP_ID}
runtime: org.kde.Platform
runtime-version: '6.6'
sdk: org.kde.Sdk
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
  # Python dependencies (ones not in KDE runtime)
  - name: python3-deps
    buildsystem: simple
    build-commands:
      - pip3 install --no-build-isolation --prefix=/app --no-deps .
    sources:
      - type: archive
        url: https://files.pythonhosted.org/packages/source/W/Whoosh/Whoosh-2.7.4.tar.gz
        sha256: 7ca5633dbfa9e0e0fa400d3151c8a0e3b0e71b33e7571209e5c4b84a69287506

  - name: python3-soupsieve
    buildsystem: simple
    build-commands:
      - pip3 install --no-build-isolation --prefix=/app --no-deps .
    sources:
      - type: archive
        url: https://files.pythonhosted.org/packages/d7/ce/fbaeed4f9fb8b2daa961f90591662df6a86c1abf25c548571a946571571e/soupsieve-2.6.tar.gz
        sha256: e2e68417777af359ec65dacd0ab06dc12ad03c401fc75c18c753bbd36e43fc8d

  - name: python3-beautifulsoup4
    buildsystem: simple
    build-commands:
      - pip3 install --no-build-isolation --prefix=/app --no-deps .
    sources:
      - type: archive
        url: https://files.pythonhosted.org/packages/b3/ca/824b1195773571e64f86ac4dd0d12e4861e50f1dc204a3860401be8be3cc/beautifulsoup4-4.13.3.tar.gz
        sha256: 1bd32405dacc920b42b7b1f076b31ea1b6af55f6c5c842feae5943e7626b1d1e

  - name: python3-requests
    buildsystem: simple
    build-commands:
      - pip3 install --no-build-isolation --prefix=/app --no-deps .
    sources:
      - type: archive
        url: https://files.pythonhosted.org/packages/63/70/2bf7780ad2d390a8d301ad0b550f1581eadbd9a20f896afe06353c2a2913/requests-2.32.3.tar.gz
        sha256: 55365417734eb18255590a9ff9eb97e9e1da868d4ccd6402399eaf68af20a760

  - name: python3-pillow
    buildsystem: simple
    build-commands:
      - pip3 install --no-build-isolation --prefix=/app --no-deps .
    sources:
      - type: archive
        url: https://files.pythonhosted.org/packages/f3/af/c097e544e7bd278333db77933e535098e7e8cfa8d7d1d2a96028c6a3cd3e/pillow-11.1.0.tar.gz
        sha256: 368da70808b36d73b4b390a8ffac11069f8a5c85f29eff1f1b01bcf3ef375a72

  - name: python3-huggingface-hub
    buildsystem: simple
    build-commands:
      - pip3 install --no-build-isolation --prefix=/app --no-deps .
    sources:
      - type: archive
        url: https://files.pythonhosted.org/packages/df/22/8eb91736b1dcb83d879bd49050a09df29a57cc5cd9f38e48a4b1c45ee890/huggingface_hub-0.27.1.tar.gz
        sha256: 67a9caba79b71235be3752852ca27da781c2a3fabe6ef10b4258667a6e0e5424

  - name: python3-llama-cpp
    buildsystem: simple
    build-commands:
      - pip3 install --prefix=/app --no-deps .
    sources:
      - type: archive
        url: https://files.pythonhosted.org/packages/48/e5/de80cf3e2d8b8a33e3a1a3b0ea5fecf7b28ec36b5e6d3c1f0a8be4bfe7c8/llama_cpp_python-0.3.4.tar.gz
        sha256: 4682992cc0c0b0419f66d24c8d684c5a6a0dc5b8e8e7f95ebdb1e9b55c3e740e
    build-options:
      env:
        CMAKE_ARGS: "-DGGML_BLAS=OFF"

  # OyNIx browser itself
  - name: oynix
    buildsystem: simple
    build-commands:
      - mkdir -p /app/lib/oynix
      - cp -r oynix /app/lib/oynix/
      - cp pyproject.toml /app/lib/oynix/
      - cp requirements.txt /app/lib/oynix/

      # Launcher script
      - install -Dm755 /dev/stdin /app/bin/oynix-launch <<LAUNCH
#!/bin/bash
export PYTHONPATH="/app/lib/oynix:\${PYTHONPATH:+:\$PYTHONPATH}"
export QTWEBENGINE_CHROMIUM_FLAGS="\${QTWEBENGINE_CHROMIUM_FLAGS} --no-sandbox"
exec python3 -m oynix "\\\$@"
LAUNCH

      # Desktop file + icon
      - install -Dm644 oynix.desktop /app/share/applications/${APP_ID}.desktop
      - desktop-file-edit --set-key=Exec --set-value=oynix-launch /app/share/applications/${APP_ID}.desktop
      - desktop-file-edit --set-key=Icon --set-value=${APP_ID} /app/share/applications/${APP_ID}.desktop

      # Install icons if present
      - |
        if [ -f assets/icon-256.png ]; then
          install -Dm644 assets/icon-256.png /app/share/icons/hicolor/256x256/apps/${APP_ID}.png
        fi
        if [ -f assets/icon-128.png ]; then
          install -Dm644 assets/icon-128.png /app/share/icons/hicolor/128x128/apps/${APP_ID}.png
        fi
        if [ -f assets/icon-64.png ]; then
          install -Dm644 assets/icon-64.png /app/share/icons/hicolor/64x64/apps/${APP_ID}.png
        fi
        if [ -f assets/icon-48.png ]; then
          install -Dm644 assets/icon-48.png /app/share/icons/hicolor/48x48/apps/${APP_ID}.png
        fi

      # AppStream metadata
      - install -Dm644 /dev/stdin /app/share/metainfo/${APP_ID}.metainfo.xml <<APPDATA
<?xml version="1.0" encoding="UTF-8"?>
<component type="desktop-application">
  <id>${APP_ID}</id>
  <name>OyNIx Browser</name>
  <summary>Nyx-powered local AI browser with tree tabs</summary>
  <metadata_license>MIT</metadata_license>
  <project_license>MIT</project_license>
  <description>
    <p>Custom Chromium-based browser with local LLM assistant, Nyx search engine,
    tree-style tabs, and purple/black Nyx theme.</p>
  </description>
  <url type="homepage">https://github.com/minerofthesoal/OyNIx</url>
  <launchable type="desktop-id">${APP_ID}.desktop</launchable>
</component>
APPDATA

    sources:
      - type: dir
        path: ..
MANIFEST

# Build
echo "[3/5] Building Flatpak (this may take a while)..."
cd "$BUILD_DIR"
flatpak-builder --user --install-deps-from=flathub --force-clean \
    --disable-cache \
    app "$APP_ID.yml"

# Export to repo
echo "[4/5] Exporting to repo..."
flatpak build-export repo app
flatpak build-bundle repo "OyNIx-${VERSION}.flatpak" "$APP_ID"

# Move to project root
mv "OyNIx-${VERSION}.flatpak" "$ROOT/"

echo
echo "[5/5] Done!"
echo "=== Flatpak built successfully ==="
echo "  File:    OyNIx-${VERSION}.flatpak"
echo "  Install: flatpak install --user OyNIx-${VERSION}.flatpak"
echo "  Run:     flatpak run $APP_ID"
echo

# OyNIx Browser v2.3 - Windows Build Script
# Builds a self-contained Windows distribution using PyInstaller or plain zip
# Run from the repository root: powershell -File scripts/build-windows.ps1

$ErrorActionPreference = "Stop"
$VERSION = "2.3"
$ROOT = Split-Path -Parent $PSScriptRoot
$BUILD_DIR = "$ROOT\build\windows"
$DIST_DIR = "$ROOT\dist"

Write-Host ""
Write-Host "  OyNIx Browser v$VERSION - Windows Build" -ForegroundColor Magenta
Write-Host "  ======================================" -ForegroundColor DarkGray
Write-Host ""

# Clean
if (Test-Path $BUILD_DIR) { Remove-Item -Recurse -Force $BUILD_DIR }
New-Item -ItemType Directory -Path $BUILD_DIR -Force | Out-Null
New-Item -ItemType Directory -Path $DIST_DIR -Force | Out-Null

# Check Python
$python = Get-Command python -ErrorAction SilentlyContinue
if (-not $python) {
    Write-Host "ERROR: Python not found. Install from https://python.org" -ForegroundColor Red
    exit 1
}
Write-Host "  Python: $(python --version)" -ForegroundColor Green

# Install dependencies
Write-Host "  Installing dependencies..." -ForegroundColor Yellow
python -m pip install --quiet PyQt6 PyQt6-WebEngine PyQt6-WebEngine-Qt6 2>$null

# Method 1: Try PyInstaller
$hasInstaller = python -c "import PyInstaller" 2>$null
if ($LASTEXITCODE -eq 0) {
    Write-Host "  Building with PyInstaller..." -ForegroundColor Yellow
    python -m PyInstaller `
        --name "OyNIx" `
        --windowed `
        --noconfirm `
        --clean `
        --add-data "oynix;oynix" `
        --hidden-import "PyQt6.QtWebEngineWidgets" `
        --hidden-import "PyQt6.QtWebEngineCore" `
        --hidden-import "PyQt6.sip" `
        --exclude-module "llama_cpp" `
        --exclude-module "transformers" `
        --exclude-module "torch" `
        --exclude-module "huggingface_hub" `
        --distpath $DIST_DIR `
        --workpath "$BUILD_DIR\work" `
        --specpath "$BUILD_DIR" `
        oynix/oynix.py

    if (Test-Path "$DIST_DIR\OyNIx\OyNIx.exe") {
        Write-Host "  PyInstaller build successful!" -ForegroundColor Green
        # Copy launchers
        Copy-Item "$ROOT\oynix-launch.bat" "$DIST_DIR\OyNIx\"
        Copy-Item "$ROOT\oynix-launch.vbs" "$DIST_DIR\OyNIx\"
    }
} else {
    # Method 2: Plain zip distribution
    Write-Host "  PyInstaller not found, creating zip distribution..." -ForegroundColor Yellow

    $zipDir = "$BUILD_DIR\OyNIx-$VERSION"
    New-Item -ItemType Directory -Path $zipDir -Force | Out-Null

    # Copy source
    Copy-Item -Recurse "$ROOT\oynix" "$zipDir\oynix"
    Copy-Item "$ROOT\oynix-launch.bat" $zipDir
    Copy-Item "$ROOT\oynix-launch.vbs" $zipDir
    Copy-Item "$ROOT\pyproject.toml" $zipDir

    # Create icons dir
    if (Test-Path "$ROOT\icons") {
        Copy-Item -Recurse "$ROOT\icons" "$zipDir\icons"
    }

    # Zip it
    $zipPath = "$DIST_DIR\OyNIx-$VERSION-windows.zip"
    Compress-Archive -Path $zipDir -DestinationPath $zipPath -Force
    Write-Host "  Created: $zipPath" -ForegroundColor Green
}

# Try NSIS if available
$nsis = Get-Command makensis -ErrorAction SilentlyContinue
if ($nsis) {
    Write-Host "  Building NSIS installer..." -ForegroundColor Yellow
    makensis "$ROOT\installer\windows\oynix-installer.nsi"
    Write-Host "  NSIS installer created!" -ForegroundColor Green
} else {
    Write-Host "  NSIS not found - skipping installer (install from https://nsis.sourceforge.io)" -ForegroundColor DarkGray
}

Write-Host ""
Write-Host "  Build complete! Check $DIST_DIR" -ForegroundColor Magenta
Write-Host ""

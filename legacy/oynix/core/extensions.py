"""
OyNIx Browser - Extension System
Supports both .xpi (Firefox) and .npi (OyNIx native) extensions.

NPI format (.npi):
  A zip file containing:
    manifest.json       — Extension metadata (name, version, permissions, etc.)
    content_scripts/    — JS/CSS injected into matching pages
    background/         — Background scripts (run once)
    popup/              — Popup HTML/JS/CSS (toolbar popup)
    icons/              — Extension icons
    styles/             — Additional CSS files
    data/               — Extension data files (JSON, etc.)

  NPI manifest.json extensions over XPI:
    "npi_version": 1,
    "oynix_features": ["theme_override", "sidebar", "database_access"],
    "data_files": ["data/wordlist.json"],
    "styles": ["styles/dark.css"],

XPI format (.xpi):
  Standard Firefox WebExtension zip with manifest.json.
  OyNIx extracts and loads content_scripts and CSS.
"""
import os
import json
import zipfile
import shutil
import re
from pathlib import Path

EXT_DIR = os.path.expanduser("~/.config/oynix/extensions")
REGISTRY_FILE = os.path.join(EXT_DIR, "_registry.json")


def _ensure_dirs():
    os.makedirs(EXT_DIR, exist_ok=True)


def load_registry():
    """Load the extension registry."""
    _ensure_dirs()
    try:
        with open(REGISTRY_FILE) as f:
            return json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        return []


def save_registry(registry):
    """Save the extension registry."""
    _ensure_dirs()
    with open(REGISTRY_FILE, 'w') as f:
        json.dump(registry, f, indent=2)


def install_extension(filepath):
    """
    Install an extension from .npi or .xpi file.
    Returns (success: bool, info: dict, error: str).
    """
    _ensure_dirs()
    ext = os.path.splitext(filepath)[1].lower()
    if ext not in ('.npi', '.xpi'):
        return False, {}, f"Unsupported format: {ext}. Use .npi or .xpi"

    try:
        with zipfile.ZipFile(filepath, 'r') as zf:
            # Read manifest
            if 'manifest.json' not in zf.namelist():
                return False, {}, "No manifest.json found in extension"

            manifest = json.loads(zf.read('manifest.json'))
            name = manifest.get('name', os.path.basename(filepath))
            version = manifest.get('version', '1.0')
            safe_name = re.sub(r'[^a-zA-Z0-9_-]', '_', name)
            dest = os.path.join(EXT_DIR, safe_name)

            # Remove old version if exists
            if os.path.isdir(dest):
                shutil.rmtree(dest)

            os.makedirs(dest, exist_ok=True)
            zf.extractall(dest)

        # Build extension info
        is_npi = ext == '.npi'
        info = {
            'name': name,
            'version': version,
            'path': dest,
            'format': 'npi' if is_npi else 'xpi',
            'enabled': True,
            'manifest': manifest,
            'content_scripts': _parse_content_scripts(manifest, dest),
            'background_scripts': _parse_background(manifest, dest),
            'popup': _parse_popup(manifest, dest),
            'icons': manifest.get('icons', {}),
            'permissions': manifest.get('permissions', []),
            'description': manifest.get('description', ''),
        }

        # NPI-specific features
        if is_npi:
            info['npi_version'] = manifest.get('npi_version', 1)
            info['oynix_features'] = manifest.get('oynix_features', [])
            info['data_files'] = manifest.get('data_files', [])
            info['extra_styles'] = manifest.get('styles', [])

        # Update registry
        registry = load_registry()
        registry = [e for e in registry if e.get('name') != name]
        registry.append(info)
        save_registry(registry)

        return True, info, ""

    except zipfile.BadZipFile:
        return False, {}, "File is not a valid zip archive"
    except json.JSONDecodeError as e:
        return False, {}, f"Invalid manifest.json: {e}"
    except Exception as e:
        return False, {}, str(e)


def uninstall_extension(name):
    """Uninstall an extension by name."""
    registry = load_registry()
    ext = next((e for e in registry if e['name'] == name), None)
    if not ext:
        return False, "Extension not found"
    path = ext.get('path', '')
    if path and os.path.isdir(path):
        shutil.rmtree(path)
    registry = [e for e in registry if e['name'] != name]
    save_registry(registry)
    return True, f"Uninstalled {name}"


def toggle_extension(name):
    """Toggle an extension enabled/disabled."""
    registry = load_registry()
    for ext in registry:
        if ext['name'] == name:
            ext['enabled'] = not ext.get('enabled', True)
            save_registry(registry)
            return ext['enabled']
    return None


def get_content_scripts_for_url(url):
    """Get all content scripts that should be injected for a given URL."""
    registry = load_registry()
    scripts = []
    for ext in registry:
        if not ext.get('enabled', True):
            continue
        for cs in ext.get('content_scripts', []):
            if _url_matches(url, cs.get('matches', [])):
                scripts.append({
                    'ext_name': ext['name'],
                    'js': cs.get('js_content', []),
                    'css': cs.get('css_content', []),
                    'run_at': cs.get('run_at', 'document_idle'),
                })
    return scripts


def get_extra_styles_for_url(url):
    """Get NPI extra styles for a URL."""
    registry = load_registry()
    styles = []
    for ext in registry:
        if not ext.get('enabled', True) or ext.get('format') != 'npi':
            continue
        for style_path in ext.get('extra_styles', []):
            full_path = os.path.join(ext['path'], style_path)
            if os.path.isfile(full_path):
                with open(full_path) as f:
                    styles.append(f.read())
    return styles


def _parse_content_scripts(manifest, ext_dir):
    """Parse content_scripts from manifest, load file contents."""
    scripts = []
    for cs in manifest.get('content_scripts', []):
        entry = {
            'matches': cs.get('matches', ['<all_urls>']),
            'run_at': cs.get('run_at', 'document_idle'),
            'js_content': [],
            'css_content': [],
        }
        for js_file in cs.get('js', []):
            path = os.path.join(ext_dir, js_file)
            if os.path.isfile(path):
                with open(path) as f:
                    entry['js_content'].append(f.read())
        for css_file in cs.get('css', []):
            path = os.path.join(ext_dir, css_file)
            if os.path.isfile(path):
                with open(path) as f:
                    entry['css_content'].append(f.read())
        scripts.append(entry)
    return scripts


def _parse_background(manifest, ext_dir):
    """Parse background scripts."""
    bg = manifest.get('background', {})
    scripts = []
    for js_file in bg.get('scripts', []):
        path = os.path.join(ext_dir, js_file)
        if os.path.isfile(path):
            with open(path) as f:
                scripts.append(f.read())
    # Service worker
    sw = bg.get('service_worker', '')
    if sw:
        path = os.path.join(ext_dir, sw)
        if os.path.isfile(path):
            with open(path) as f:
                scripts.append(f.read())
    return scripts


def _parse_popup(manifest, ext_dir):
    """Parse popup HTML."""
    action = manifest.get('action', manifest.get('browser_action', {}))
    popup = action.get('default_popup', '')
    if popup:
        path = os.path.join(ext_dir, popup)
        if os.path.isfile(path):
            with open(path) as f:
                return {'html': f.read(), 'file': popup}
    return None


def convert_xpi_to_npi(xpi_path, output_path=None):
    """
    Convert a Firefox .xpi extension to an OyNIx .npi extension.

    Reads the XPI manifest, translates Firefox-specific keys into NPI format,
    and repacks as a .npi zip file.

    Returns (success, output_path, error_string).
    """
    if not os.path.isfile(xpi_path):
        return False, '', 'XPI file not found'

    if output_path is None:
        base = os.path.splitext(xpi_path)[0]
        output_path = base + '.npi'

    try:
        with zipfile.ZipFile(xpi_path, 'r') as zin:
            if 'manifest.json' not in zin.namelist():
                return False, '', 'No manifest.json in XPI'
            manifest = json.loads(zin.read('manifest.json'))

            # Build NPI manifest from Firefox WebExtension manifest
            npi_manifest = {
                'manifest_version': manifest.get('manifest_version', 2),
                'name': manifest.get('name', 'Converted Extension'),
                'version': manifest.get('version', '1.0'),
                'description': manifest.get('description', ''),
                'npi_version': 1,
                'permissions': manifest.get('permissions', []),
                'oynix_features': [],
                'icons': manifest.get('icons', {}),
            }

            # Map browser_action / action -> action
            action = manifest.get('action', manifest.get('browser_action', {}))
            if action:
                npi_manifest['action'] = action

            # Map content_scripts
            if 'content_scripts' in manifest:
                npi_manifest['content_scripts'] = manifest['content_scripts']

            # Map background
            bg = manifest.get('background', {})
            if bg:
                npi_manifest['background'] = bg

            # Detect OyNIx-compatible features
            if 'content_scripts' in manifest:
                npi_manifest['oynix_features'].append('content_injection')
            if action and action.get('default_popup'):
                npi_manifest['oynix_features'].append('popup')
            if bg:
                npi_manifest['oynix_features'].append('background')

            # Copy everything into NPI zip, replacing manifest
            with zipfile.ZipFile(output_path, 'w', zipfile.ZIP_DEFLATED) as zout:
                for item in zin.namelist():
                    if item == 'manifest.json':
                        continue
                    # Skip Firefox-only metadata
                    if item.startswith('META-INF/'):
                        continue
                    zout.writestr(item, zin.read(item))
                # Write the NPI manifest
                zout.writestr('manifest.json',
                              json.dumps(npi_manifest, indent=2))

        return True, output_path, ''

    except zipfile.BadZipFile:
        return False, '', 'File is not a valid zip/xpi archive'
    except Exception as e:
        return False, '', str(e)


def compile_npi(manifest_dict, files_dict, output_path):
    """
    Compile an NPI extension from a manifest dict and files dict.

    Args:
        manifest_dict: dict — the NPI manifest.json content
        files_dict: dict mapping relative paths to file content (str or bytes)
        output_path: path for the .npi output

    Returns (success, error_string).
    """
    try:
        os.makedirs(os.path.dirname(output_path) or '.', exist_ok=True)
        with zipfile.ZipFile(output_path, 'w', zipfile.ZIP_DEFLATED) as zf:
            zf.writestr('manifest.json', json.dumps(manifest_dict, indent=2))
            for rel_path, content in files_dict.items():
                if isinstance(content, str):
                    zf.writestr(rel_path, content)
                else:
                    zf.writestr(rel_path, content)
        return True, ''
    except Exception as e:
        return False, str(e)


def _url_matches(url, patterns):
    """Check if URL matches any of the given match patterns."""
    if not patterns:
        return False
    for pattern in patterns:
        if pattern == '<all_urls>':
            return True
        if pattern == '*://*/*':
            return url.startswith('http://') or url.startswith('https://')
        # Simple glob matching
        regex = pattern.replace('.', r'\.').replace('*', '.*')
        try:
            if re.match(regex, url):
                return True
        except re.error:
            if pattern in url:
                return True
    return False

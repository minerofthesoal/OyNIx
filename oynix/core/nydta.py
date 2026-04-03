"""
OyNIx Browser - .nydta Profile Export/Import
.nydta is a zip-based archive containing the user's full browser profile:
  - history.jsonl      — browsing history (one JSON object per line)
  - database.jsonl     — site database entries (one JSON object per line)
  - settings.md        — human-readable settings overview
  - settings.json      — machine-readable settings
  - profile.png        — profile picture (optional, PNG or JPEG XL)
"""
import os
import io
import json
import time
import zipfile
import tempfile
from pathlib import Path


NYDTA_VERSION = "1.0"
PROFILE_PIC_PATH = os.path.expanduser("~/.config/oynix/profile.png")


def export_nydta(filepath, history, config, database_export_fn, profile_pic_path=None):
    """
    Export a .nydta archive.

    Args:
        filepath: Output .nydta file path
        history: List of history dicts [{url, title, time, ...}]
        config: Browser config dict
        database_export_fn: Callable that returns list of site dicts
        profile_pic_path: Optional path to profile picture
    Returns:
        (ok: bool, message: str)
    """
    try:
        with zipfile.ZipFile(filepath, 'w', zipfile.ZIP_DEFLATED) as zf:
            # ── Manifest ──
            manifest = {
                'nydta_version': NYDTA_VERSION,
                'exported_at': time.strftime('%Y-%m-%d %H:%M:%S'),
                'browser_version': '2.1.2',
                'contains': ['history.jsonl', 'database.jsonl',
                              'settings.md', 'settings.json'],
            }

            # ── History as JSONL ──
            history_buf = io.StringIO()
            for entry in history:
                history_buf.write(json.dumps(entry, ensure_ascii=False) + '\n')
            zf.writestr('history.jsonl', history_buf.getvalue())

            # ── Database as JSONL ──
            db_entries = database_export_fn()
            db_buf = io.StringIO()
            if isinstance(db_entries, list):
                for entry in db_entries:
                    db_buf.write(json.dumps(entry, ensure_ascii=False) + '\n')
            elif isinstance(db_entries, dict):
                # Export dict format: flatten all categories
                for section_name, section in db_entries.items():
                    if isinstance(section, dict):
                        for cat, items in section.items():
                            if isinstance(items, list):
                                for item in items:
                                    if isinstance(item, dict):
                                        item['_category'] = cat
                                        item['_section'] = section_name
                                        db_buf.write(json.dumps(item, ensure_ascii=False) + '\n')
                    elif isinstance(section, list):
                        for item in section:
                            if isinstance(item, dict):
                                db_buf.write(json.dumps(item, ensure_ascii=False) + '\n')
            zf.writestr('database.jsonl', db_buf.getvalue())

            # ── Settings as Markdown ──
            md = _config_to_markdown(config)
            zf.writestr('settings.md', md)

            # ── Settings as JSON ──
            zf.writestr('settings.json', json.dumps(config, indent=2, ensure_ascii=False))

            # ── Profile picture ──
            pic_path = profile_pic_path or PROFILE_PIC_PATH
            if pic_path and os.path.isfile(pic_path):
                ext = os.path.splitext(pic_path)[1].lower()
                if ext in ('.png', '.jpg', '.jpeg', '.jxl', '.jpgxl', '.webp'):
                    arcname = 'profile' + ext
                else:
                    arcname = 'profile.png'
                zf.write(pic_path, arcname)
                manifest['contains'].append(arcname)

            zf.writestr('manifest.json', json.dumps(manifest, indent=2))

        count_h = len(history)
        count_d = db_buf.getvalue().count('\n')
        return True, f"Exported {count_h} history + {count_d} DB entries to {os.path.basename(filepath)}"

    except Exception as e:
        return False, f"Export failed: {e}"


def import_nydta(filepath):
    """
    Import a .nydta archive.

    Returns:
        (ok, data_dict, message)
        data_dict has keys: history, database, settings, profile_pic_path
    """
    if not os.path.isfile(filepath):
        return False, {}, "File not found"

    try:
        with zipfile.ZipFile(filepath, 'r') as zf:
            names = zf.namelist()
            result = {
                'history': [],
                'database': [],
                'settings': {},
                'profile_pic_path': None,
                'manifest': {},
            }

            # ── Manifest ──
            if 'manifest.json' in names:
                result['manifest'] = json.loads(zf.read('manifest.json'))

            # ── History (JSONL) ──
            if 'history.jsonl' in names:
                text = zf.read('history.jsonl').decode('utf-8')
                for line in text.strip().split('\n'):
                    line = line.strip()
                    if line:
                        try:
                            result['history'].append(json.loads(line))
                        except json.JSONDecodeError:
                            pass

            # ── Database (JSONL) ──
            if 'database.jsonl' in names:
                text = zf.read('database.jsonl').decode('utf-8')
                for line in text.strip().split('\n'):
                    line = line.strip()
                    if line:
                        try:
                            result['database'].append(json.loads(line))
                        except json.JSONDecodeError:
                            pass

            # ── Settings ──
            if 'settings.json' in names:
                result['settings'] = json.loads(zf.read('settings.json'))

            # ── Profile picture ──
            pic_names = [n for n in names if n.startswith('profile.')]
            if pic_names:
                pic_name = pic_names[0]
                tmp_dir = os.path.expanduser("~/.config/oynix")
                os.makedirs(tmp_dir, exist_ok=True)
                ext = os.path.splitext(pic_name)[1]
                dest = os.path.join(tmp_dir, f"profile{ext}")
                with open(dest, 'wb') as f:
                    f.write(zf.read(pic_name))
                result['profile_pic_path'] = dest

            h = len(result['history'])
            d = len(result['database'])
            msg = f"Found {h} history entries, {d} database entries"
            if result['profile_pic_path']:
                msg += ", profile picture"
            return True, result, msg

    except zipfile.BadZipFile:
        return False, {}, "Not a valid .nydta file (bad zip)"
    except Exception as e:
        return False, {}, f"Import failed: {e}"


def _config_to_markdown(config):
    """Convert config dict to human-readable Markdown."""
    lines = [
        "# OyNIx Browser Settings",
        "",
        f"*Exported: {time.strftime('%Y-%m-%d %H:%M:%S')}*",
        "",
        "## General",
        "",
    ]

    sections = {
        'General': ['theme', 'use_tree_tabs', 'default_search_engine',
                     'homepage_url', 'startup_behavior', 'font_size', 'compact_mode'],
        'Search': ['nyx_search_enabled', 'search_result_count', 'safe_search',
                    'auto_index_pages', 'auto_expand_database'],
        'Privacy': ['do_not_track', 'cookie_setting', 'show_security_prompts',
                     'force_nyx_theme_external'],
        'AI': ['ai_enabled', 'ai_model_selection', 'ai_auto_download', 'ai_max_length'],
        'Database & Sync': ['auto_compare_sites', 'auto_update_repo_db',
                             'community_upload', 'github_repo_url', 'github_sync_frequency'],
        'Downloads': ['download_dir', 'ask_download_location', 'auto_open_downloads_panel'],
    }

    for section_name, keys in sections.items():
        lines.append(f"## {section_name}")
        lines.append("")
        for key in keys:
            val = config.get(key, '*not set*')
            label = key.replace('_', ' ').title()
            if isinstance(val, bool):
                val = 'Yes' if val else 'No'
            lines.append(f"- **{label}**: {val}")
        lines.append("")

    # Custom shortcuts
    custom = config.get('custom_shortcuts', {})
    if custom:
        lines.append("## Custom Keyboard Shortcuts")
        lines.append("")
        for action, key in custom.items():
            lines.append(f"- **{action}**: `{key}`")
        lines.append("")

    return '\n'.join(lines)

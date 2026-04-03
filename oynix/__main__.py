"""Allow running as: python -m oynix"""
import sys
import os

# Ensure parent directory is on path so 'oynix' package is importable
pkg_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if pkg_dir not in sys.path:
    sys.path.insert(0, pkg_dir)

# Also check /usr/lib/oynix (for .deb installs)
_deb_path = '/usr/lib/oynix'
if os.path.isdir(os.path.join(_deb_path, 'oynix')) and _deb_path not in sys.path:
    sys.path.insert(0, _deb_path)

from oynix.oynix import main  # noqa: E402

main()

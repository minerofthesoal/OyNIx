"""Allow running as: python -m oynix"""
import sys
import os

# Ensure parent directory is on path so 'oynix' package is importable
pkg_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if pkg_dir not in sys.path:
    sys.path.insert(0, pkg_dir)

# Fix LD_LIBRARY_PATH for pip-installed Qt6 libraries
try:
    import PyQt6
    _qt6_lib = os.path.join(os.path.dirname(PyQt6.__file__), 'Qt6', 'lib')
    if os.path.isdir(_qt6_lib):
        _ld_path = os.environ.get('LD_LIBRARY_PATH', '')
        if _qt6_lib not in _ld_path:
            os.environ['LD_LIBRARY_PATH'] = _qt6_lib + (':' + _ld_path if _ld_path else '')
            if not os.environ.get('_OYNIX_REEXEC'):
                os.environ['_OYNIX_REEXEC'] = '1'
                os.execv(sys.executable, [sys.executable, '-m', 'oynix'] + sys.argv[1:])
except ImportError:
    pass

from oynix.oynix import main  # noqa: E402

main()

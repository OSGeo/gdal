# __init__ for osgeo package.
import sys
import os

# On UNIX-based platforms,
# The default mode for dynamic loading keeps all the symbol private and not
# visible to other libraries that may be loaded. Setting the mode to
# RTLD_GLOBAL to make the symbols visible.

_use_rtld_global = (hasattr(sys, 'getdlopenflags')
                    and hasattr(sys, 'setdlopenflags'))
if _use_rtld_global:
    _default_dlopen_flags = sys.getdlopenflags()

def set_dlopen_flags():
    if _use_rtld_global:
        if version_info >= (3, 3, 0):
            RTLD_GLOBAL = os.RTLD_GLOBAL
        else:
            import ctypes
            RTLD_GLOBAL = ctypes.RTLD_GLOBAL
        sys.setdlopenflags(_default_dlopen_flags | RTLD_GLOBAL)

def reset_dlopen_flags():
    if _use_rtld_global:
        sys.setdlopenflags(_default_dlopen_flags)

# making the osgeo package version the same as the gdal version:
from sys import version_info
if version_info >= (2, 6, 0):
    def swig_import_helper():
        from os.path import dirname
        import imp
        fp = None
        try:
            fp, pathname, description = imp.find_module('_gdal', [dirname(__file__)])
        except ImportError:
            import _gdal
            return _gdal
        if fp is not None:
            try:
                _mod = imp.load_module('_gdal', fp, pathname, description)
            finally:
                fp.close()
            return _mod
    set_dlopen_flags()
    _gdal = swig_import_helper()
    reset_dlopen_flags()
    del swig_import_helper
else:
    import _gdal

__version__ = _gdal.__version__ = _gdal.VersionInfo("RELEASE_NAME")

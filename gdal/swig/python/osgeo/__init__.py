# __init__ for osgeo package.

# making the osgeo package version the same as the gdal version:
from sys import platform, version_info
if version_info >= (3, 8, 0) and platform == 'win32':
    import os
    if 'USE_PATH_FOR_GDAL_PYTHON' in os.environ and 'PATH' in os.environ:
        for p in os.environ['PATH'].split(';'):
            if p:
                os.add_dll_directory(p)

if version_info >= (2, 7, 0):
    def swig_import_helper():
        import importlib
        from os.path import dirname, basename
        mname = basename(dirname(__file__)) + '._gdal'
        try:
            return importlib.import_module(mname)
        except ImportError as e:
            if version_info >= (3, 8, 0) and platform == 'win32':
                import os
                if not 'USE_PATH_FOR_GDAL_PYTHON' in os.environ:
                    msg = 'On Windows, with Python >= 3.8, DLLs are no longer imported from the PATH.\n'
                    msg += 'If gdalXXX.dll is in the PATH, then set the USE_PATH_FOR_GDAL_PYTHON=YES environment variable\n'
                    msg += 'to feed the PATH into os.add_dll_directory().'

                    import sys
                    import traceback
                    traceback_string = ''.join(traceback.format_exception(*sys.exc_info()))
                    raise ImportError(traceback_string + '\n' + msg)
            return importlib.import_module('_gdal')
    _gdal = swig_import_helper()
    del swig_import_helper
elif version_info >= (2, 6, 0):
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
            except ImportError as e:
                if version_info >= (3, 8, 0) and platform == 'win32':
                    import os
                    if not 'USE_PATH_FOR_GDAL_PYTHON' in os.environ:
                        msg = 'On Windows, with Python >= 3.8, DLLs are no longer imported from the PATH.\n'
                        msg += 'If gdalXXX.dll is in the PATH, then set the USE_PATH_FOR_GDAL_PYTHON=YES environment variable\n'
                        msg += 'to feed the PATH into os.add_dll_directory().'

                        import sys
                        import traceback
                        traceback_string = ''.join(traceback.format_exception(*sys.exc_info()))
                        raise ImportError(traceback_string + '\n' + msg)
                raise
            finally:
                fp.close()
            return _mod
    _gdal = swig_import_helper()
    del swig_import_helper
else:
    import _gdal

__version__ = _gdal.__version__ = _gdal.VersionInfo("RELEASE_NAME")

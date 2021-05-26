# __init__ for osgeo package.

# making the osgeo package version the same as the gdal version:
from sys import platform, version_info
if version_info >= (3, 8, 0) and platform == 'win32':
    import os
    if 'USE_PATH_FOR_GDAL_PYTHON' in os.environ and 'PATH' in os.environ:
        for p in os.environ['PATH'].split(';'):
            if p:
                os.add_dll_directory(p)


def swig_import_helper():
    import importlib
    from os.path import dirname, basename
    mname = basename(dirname(__file__)) + '._gdal'
    try:
        return importlib.import_module(mname)
    except ImportError:
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

__version__ = _gdal.__version__ = _gdal.VersionInfo("RELEASE_NAME")

gdal_version = tuple(int(s) for s in str(__version__).split('.') if s.isdigit())[:3]
python_version = tuple(version_info)[:3]

# Setting this flag to True will cause importing osgeo to fail on an unsupported Python version.
# Otherwise a deprecation warning will be issued instead.
# Importing osgeo fom an unsupported Python version might still partially work
# because the core of GDAL Python bindings might still support an older Python version.
# Hence the default option to just issue a warning.
# To get complete functionality upgrading to the minimum supported version is needed.
fail_on_unsupported_version = False

# The following is a Sequence of tuples in the form of (gdal_version, python_version).
# Each line represents the minimum supported Python version of a given GDAL version.
# Introducing a new line for the next GDAL version will trigger a deprecation warning
# when importing osgeo from a Python version which will not be
# supported in the next version of GDAL.
gdal_version_and_min_supported_python_version = (
    ((0, 0), (0, 0)),
    ((1, 0), (2, 0)),
    ((2, 0), (2, 7)),
    ((3, 3), (3, 6)),
    # ((3, 4), (3, 7)),
    # ((3, 5), (3, 8)),
)


def ver_str(ver):
    return '.'.join(str(v) for v in ver) if ver is not None else None


minimum_supported_python_version_for_this_gdal_version = None
this_python_version_will_be_deprecated_in_gdal_version = None
last_gdal_version_to_supported_your_python_version = None
next_version_of_gdal_will_use_python_version = None
for gdal_ver, py_ver in gdal_version_and_min_supported_python_version:
    if gdal_version >= gdal_ver:
        minimum_supported_python_version_for_this_gdal_version = py_ver
    if python_version >= py_ver:
        last_gdal_version_to_supported_your_python_version = gdal_ver
    if not this_python_version_will_be_deprecated_in_gdal_version:
        if python_version < py_ver:
            this_python_version_will_be_deprecated_in_gdal_version = gdal_ver
            next_version_of_gdal_will_use_python_version = py_ver


if python_version < minimum_supported_python_version_for_this_gdal_version:
    msg = 'Your Python version is {}, which is no longer supported by GDAL {}. ' \
          'Please upgrade your Python version to Python >= {}, ' \
          'or use GDAL <= {}, which supports your Python version.'.\
        format(ver_str(python_version), ver_str(gdal_version),
               ver_str(minimum_supported_python_version_for_this_gdal_version),
               ver_str(last_gdal_version_to_supported_your_python_version))

    if fail_on_unsupported_version:
        raise Exception(msg)
    else:
        from warnings import warn, simplefilter
        simplefilter('always', DeprecationWarning)
        warn(msg, DeprecationWarning)
elif this_python_version_will_be_deprecated_in_gdal_version:
    msg = 'You are using Python {} with GDAL {}. ' \
          'This Python version will be deprecated in GDAL {}. ' \
          'Please consider upgrading your Python version to Python >= {}, ' \
          'Which will be the minimum supported Python version of GDAL {}.'.\
        format(ver_str(python_version), ver_str(gdal_version),
               ver_str(this_python_version_will_be_deprecated_in_gdal_version),
               ver_str(next_version_of_gdal_will_use_python_version),
               ver_str(this_python_version_will_be_deprecated_in_gdal_version))

    from warnings import warn, simplefilter
    simplefilter('always', DeprecationWarning)
    warn(msg, DeprecationWarning)

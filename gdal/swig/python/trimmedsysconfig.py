# Trimmed version of /usr/lib/python3.8/distutils/sysconfig.py

"""Provide access to Python's configuration information.  The specific
configuration variables available depend heavily on the platform and
configuration.  The values may be retrieved using
get_config_var(name), and the list of variables is available via
get_config_vars().keys().  Additional convenience functions are also
available.

Written by:   Fred L. Drake, Jr.
Email:        <fdrake@acm.org>
"""

import os
import sys
from sysconfig import get_python_version

# These are needed in a couple of spots, so just compute them once.
PREFIX = os.path.normpath(sys.prefix)
EXEC_PREFIX = os.path.normpath(sys.exec_prefix)
BASE_PREFIX = os.path.normpath(sys.base_prefix)
BASE_EXEC_PREFIX = os.path.normpath(sys.base_exec_prefix)

# Added by GDAL
def _is_debian():
    for p in sys.path:
        if 'dist-packages' in p:
            return True
    return False


def get_python_lib(plat_specific=0, standard_lib=0, prefix=None):
    """Return the directory containing the Python library (standard or
    site additions).

    If 'plat_specific' is true, return the directory containing
    platform-specific modules, i.e. any module from a non-pure-Python
    module distribution; otherwise, return the platform-shared library
    directory.  If 'standard_lib' is true, return the directory
    containing standard Python library modules; otherwise, return the
    directory for site-specific modules.

    If 'prefix' is supplied, use it instead of sys.base_prefix or
    sys.base_exec_prefix -- i.e., ignore 'plat_specific'.
    """
    is_default_prefix = not prefix or os.path.normpath(prefix) in ('/usr', '/usr/local')
    if prefix is None:
        if standard_lib:
            prefix = plat_specific and BASE_EXEC_PREFIX or BASE_PREFIX
        else:
            prefix = plat_specific and EXEC_PREFIX or PREFIX

    if os.name == "posix":
        libpython = os.path.join(prefix,
                                 "lib", "python" + get_python_version())
        if standard_lib:
            return libpython
        # This check is Debian specific
        elif (_is_debian() and is_default_prefix and
              'PYTHONUSERBASE' not in os.environ and
              'VIRTUAL_ENV' not in os.environ and
              'real_prefix' not in sys.__dict__ and
              sys.prefix == sys.base_prefix):
            return os.path.join(prefix, "lib", "python3", "dist-packages")
        else:
            return os.path.join(libpython, "site-packages")
    elif os.name == "nt":
        if standard_lib:
            return os.path.join(prefix, "Lib")
        else:
            return os.path.join(prefix, "Lib", "site-packages")
    else:
        raise Exception(
            "I don't know where Python installs its library "
            "on platform '%s'" % os.name)


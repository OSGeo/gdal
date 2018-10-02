# coding: utf-8
from __future__ import absolute_import, division, print_function, unicode_literals

import os
import sys

import pytest

# Put the pymod dir on the path, so modules can `import gdaltest`
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "pymod"))

# These files may be non-importable, and don't contain tests anyway.
# So we skip searching them during test collection.
collect_ignore = ["gdrivers/netcdf_cfchecks.py"]


@pytest.fixture(scope="module", autouse=True)
def chdir_to_test_file(request):
    """
    Changes to the same directory as the test file.
    Also puts that directory at the start of sys.path,
    so that imports of other files in the same directory are easy.

    Tests have grown to expect this.

    NOTE: This happens when the test is *run*, not during collection.
    So test modules must not rely on it at module level.
    """
    old = os.getcwd()

    os.chdir(os.path.dirname(request.module.__file__))
    sys.path.insert(0, ".")
    yield
    if sys.path and sys.path[0] == ".":
        sys.path.pop(0)
    os.chdir(old)

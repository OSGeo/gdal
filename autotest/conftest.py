# coding: utf-8
from __future__ import absolute_import, division, print_function, unicode_literals

import os
import sys

import pytest

sys.path.insert(0, "pymod")
sys.path.insert(0, ".")


@pytest.fixture(scope="module", autouse=True)
def chdir_to_test_file(request):
    """
    Changes to the same directory as the test file.
    Tests have grown to expect this.
    """
    old = os.getcwd()

    os.chdir(os.path.dirname(request.module.__file__))
    yield
    os.chdir(old)

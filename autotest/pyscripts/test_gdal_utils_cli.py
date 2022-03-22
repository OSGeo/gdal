#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  osgeo_utils (gdal-utils) testing
# Author:   Matt Wilkie <maphew@gmail.ca>
#
###############################################################################
# Copyright (c) 2022, Matt Wilkie <maphew@gmail.ca>
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
###############################################################################
'''
*** STUB *** Test that command line gdal-utils are in PATH and run. We only
test script starts and returns expected defaults. We do not test if script
functions properly.

Tested:
    gdalcompare <no args>
Returns:
    Usage: gdalcompare.py [-sds] <golden_file> <new_file>

Not tested:
    gdalcompare file1.tif file2.tif
Returns:
    <valid results>

Resources used:
 - https://github.com/rhcarvalho/pytest-subprocess-example
 - https://stackoverflow.com/questions/30528099/unittesting-python-code-which-uses-subprocess-popen
 - https://www.digitalocean.com/community/tutorials/how-to-use-subprocess-to-run-external-programs-in-python-3
 - https://stackoverflow.com/questions/13493288/python-cli-program-unit-testing
'''
import sys
import subprocess
import pytest

# Skip if gdal-utils is not known to pip (and therefore not registered in
# python 'site-packages' and 'Scripts')
installed = subprocess.run([sys.executable, '-m', 'pip', 'show', 'gdal-utils'])
if not installed.returncode == 0:
    pytest.skip("The 'gdal-utils' package is not installed.", allow_module_level=True)


@pytest.mark.parametrize("input,want", [
    pytest.param("gdalcompare", {
        "returncode": 1,
        "stdout": "Usage: gdalcompare.py [-sds] <golden_file> <new_file>\n",
        "stderr": "",
    })
])
def test_program(input, want):
    completed_process = run_program(input)
    got = {
        "returncode": completed_process.returncode,
        "stdout": completed_process.stdout,
        "stderr": completed_process.stderr,
    }
    assert got == want


def run_program(program, args=None):
    return subprocess.run(
        [program],
        input=args,
        capture_output=True,
        shell=True,
        text=True,
    )

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
"""
Verify that gdal-utils scripts all exit with the same return code when
called without arguments.

Spawned from https://github.com/OSGeo/gdal/issues/5561

Programs: python files installed into PYTHONHOME/Scripts and can be executed
as just names e.g. 'gdal_edit somefile.tif ...'

Scripts: python files that need to called from python in order to be run,
e.g. 'python path/to/samples/pct2rgb.py somefile.tif ...'
"""
import os
import glob
import sys
import subprocess
import pytest
from pathlib import Path

# pytest.skip("THIS TEST IN DRAFT MODE, SKIPPING", allow_module_level=True)

# here = r"D:\code\public\gdal\autotest\pyscripts"
# script_path = r"D:\code\public\gdal\swig\python\gdal-utils\osgeo_utils"
here = Path(__file__).parent.absolute()
script_path = Path(here / "../../swig/python/gdal-utils/osgeo_utils/").resolve()

excludes = [
    "setup.py",
    "__init__.py",
    "gdal_auth.py",  # gdal_auth doesn't take arguments
]


def get_scripts(script_path, excludes):
    # Using glob instead of Path.glob because removing excludes from a list of
    # strings is easier than with objects

    # scripts = list(script_path.glob("**/.py" ))
    os.chdir(script_path)
    s1 = glob.glob("*.py")
    s2 = glob.glob("samples/*.py")
    os.chdir(here)
    scripts = s1 + s2
    del s1, s2


    for e in excludes:
        for s in scripts:
            if e in s:
                scripts.remove(s)

    # add full path back in
    scripts = [Path.joinpath(script_path, x).resolve() for x in scripts]
    return scripts


if not Path(script_path).exists():
    pytest.skip("Can't find gdal-utils dir, skipping.", allow_module_level=True)
    print("Can't find gdal-utils, skipping.")
else:
    scripts = get_scripts(script_path, excludes)


# Programs - standard gdal-utils we expect to be installed in PYTHONHOME\Scripts
# Skip if gdal-utils is not known to pip (and therefore not registered in
# python 'site-packages' and 'Scripts')
installed = subprocess.run([sys.executable, "-m", "pip", "show", "gdal-utils"])
if installed.returncode != 0:
    pytest.skip("The 'gdal-utils' package is not installed.", allow_module_level=True)
utils = [
    "gdal2tiles",
    "gdal2xyz",
    "gdal_calc",
    "gdal_edit",
    "gdal_fillnodata",
    "gdal_merge",
    "gdal_pansharpen",
    "gdal_polygonize",
    "gdal_proximity",
    "gdal_retile",
    "gdal_sieve",
    "gdalattachpct",
    "gdalcompare",
    "gdalmove",
    "ogrmerge",
    "pct2rgb",
    "rgb2pct",
]

# Correct for-loop with pytest courtesy of @niccodemus
# https://github.com/pytest-dev/pytest/discussions/9822#discussioncomment-2446025
params = [pytest.param(util) for util in utils]
sparams = [pytest.param(script) for script in scripts]


@pytest.mark.parametrize("input", params)
def test_program(input):
    completed_process = run_program(input)
    assert (
        "usage:" in completed_process.stderr.lower()
        or "usage:" in completed_process.stdout.lower()
        and completed_process.returncode == 2
    )


@pytest.mark.parametrize("input", sparams)
def test_script(input):
    completed_process = run_script(input)
    assert (
        "usage:" in completed_process.stderr.lower()
        or "usage:" in completed_process.stdout.lower()
        and completed_process.returncode == 2
    )


def run_program(program, args=None):
    return subprocess.run(
        [program],
        input=args,
        capture_output=True,
        shell=True,
        text=True,
    )


def run_script(program, args=None):
    return subprocess.run(
        [sys.executable, program],  # ["path/to/this/env's/python", 'path/to/script.py']
        input=args,
        capture_output=True,
        shell=True,
        text=True,
    )

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
"""
import sys
import subprocess
import pytest

# pytest.skip("THIS TEST IN DRAFT MODE, SKIPPING", allow_module_level=True)

from pathlib import Path

# here = r"D:\code\public\gdal\autotest\pyscripts"
# script_path = r"D:\code\public\gdal\swig\python\gdal-utils\osgeo_utils"
here = Path(__file__).parent.absolute()
script_path = Path(here / '../../swig/python/gdal-utils/osgeo_utils/').resolve()
excludes = ['setup.py', '__init__.py']
exclude_dirs = ['auxiliary'] # aux files are for importing, not running as scripts

if not Path(script_path).exists():
    pytest.skip("Can't find gdal-utils dir, skipping.", allow_module_level=True)
    print("Can't find gdal-utils, skipping.")
else:
    scripts = list(script_path.glob("*.py" ))
    #todo: add samples dir, remove excludes

##
## Standard gdal-utils we expect to be installed in PYTHONHOME\Scripts
##
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

@pytest.mark.parametrize("input", params)
def test_program(input):
    completed_process = run_program(input)
    assert (
        "usage:" in completed_process.stderr.lower()
        or "usage:" in completed_process.stdout.lower()
        and completed_process.returncode == 2
    )

sparams = [pytest.param(script) for script in scripts]
    # broken: runs `script.py` instead of `python script.py`
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
        ['python'],
        input=args,
        capture_output=True,
        shell=True,
        text=True,
    )

##
## Scripts
##   FIXME: this isn't a pytest. It only works when run as
##   `python test_gdal_utils_retcodes.py`

i = '.' # progress meter step
results = {}
for s in scripts:
    file = Path(s)
    print(i, end='\r')
    if 'gdal_auth.py' not in file.name:
        # skip gdal_auth because it doesn't take inputs

        r = subprocess.run([sys.executable,
            file],
            shell=True,
            capture_output=True,
            text=True,
            )

        # (absolute / relative_path) join courtesy of Thierry at https://stackoverflow.com/a/52879083/14420
        results[Path(here / file).resolve()] = r.returncode
    i = i+'.'

# sort by return code value and display results
results = sorted(results.items(), key=lambda x:x[1])
print('\n')
[print(x[1], str(x[0])) for x in results]

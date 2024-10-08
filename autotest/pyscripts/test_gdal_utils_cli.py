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
# SPDX-License-Identifier: MIT
###############################################################################
"""
Test that command line gdal-utils are in PATH and run. We only test script
returns 'Usage:' in it's output messages. We do not test for proper script
functioning.

Tested:
    gdalcompare <no args>
Returns:
    Usage: ...

Not tested:
    gdalcompare file1.tif file2.tif
Returns:
    <valid results>

Resources used:
 - https://github.com/rhcarvalho/pytest-subprocess-example
 - https://stackoverflow.com/questions/30528099/unittesting-python-code-which-uses-subprocess-popen
 - https://www.digitalocean.com/community/tutorials/how-to-use-subprocess-to-run-external-programs-in-python-3
 - https://stackoverflow.com/questions/13493288/python-cli-program-unit-testing
"""

import subprocess
import sys

import pytest

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
    )


def run_program(program, args=None):
    return subprocess.run(
        [program],
        input=args,
        capture_output=True,
        shell=True,
        text=True,
    )

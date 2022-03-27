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
Test that command line gdal-utils are in PATH and run. We only test script
starts and returns expected defaults. We do not test if script functions
properly.

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
from pathlib import Path
import sys
import subprocess
import pytest

# Skip if gdal-utils is not known to pip (and therefore not registered in
# python 'site-packages' and 'Scripts')
installed = subprocess.run([sys.executable, '-m', 'pip', 'show', 'gdal-utils'])
if not installed.returncode == 0:
    pytest.skip("The 'gdal-utils' package is not installed.", allow_module_level=True)

utils = ['gdal2tiles', 'gdal2xyz', 'gdal_calc', 'gdal_edit', 'gdal_fillnodata',
         'gdal_merge', 'gdal_pansharpen', 'gdal_polygonize', 'gdal_proximity',
         'gdal_retile', 'gdal_sieve', 'gdalattachpct', 'gdalcompare', 'gdalmove',
         'ogrmerge', 'pct2rgb', 'rgb2pct']

here = Path(__file__).parent.absolute()
outputs_dir = Path.joinpath(here, "cli_outs")


def get_utils_responses():
    '''Return dict of "utility_name: [stdout_msg, stderr_msg]"

    pct2rgb:
        ''
        'usage: pct2rgb [-h] [-of gdal_format] [-rgba] [-b band] ...\n'

   The messages are what we expect the program name to report to console
   when called with no parameters.

    The expected results must be in pre-generated in files named:

        cli_outs/scriptname.stdout  # normal
        cli_outs/scriptname.stderr  # errors

    These output files can be generated with:

        gdalcompare 1>gdalcompare.stdout 2>gdalcompare.stderr
    '''
    responses = {}
    for prog in utils:
        data_out = Path.joinpath(outputs_dir, f"{prog}.stdout")  # .../cli_outs/pctrgb.stdout
        data_err = Path.joinpath(outputs_dir, f"{prog}.stderr")  # .../cli_outs/pctrgb.stderr
        with open(data_out) as f:
            responses[prog] = [f.read()]
        with open(data_err) as f:
            responses[prog] = [responses[prog][0], f.read()]
    return responses


responses = get_utils_responses()

# 'returncode' not used because the utils are not consistent in what they
# return for "no parameters supplied"
# Correct for-loop with pytest courtesy of @niccodemus
# https://github.com/pytest-dev/pytest/discussions/9822#discussioncomment-2446025
params = [
    pytest.param(util, {
        # "returncode": 1,
        "stdout": responses[util][0],
        "stderr": responses[util][1],
    })
    for util in utils
]


@pytest.mark.parametrize("input,want", params)
def test_program(input, want):
    completed_process = run_program(input)
    got = {
        # "returncode": completed_process.returncode,
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

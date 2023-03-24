#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic read support for all datatypes from a HFA file.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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


import gdaltest
import pytest

from osgeo import gdal

init_list = [
    ("byte.img", 4672),
    ("int16.img", 4672),
    ("uint16.img", 4672),
    ("int32.img", 4672),
    ("uint32.img", 4672),
    ("float32.img", 4672),
    ("float64.img", 4672),
    ("utmsmall.img", 50054),
    ("2bit_compressed.img", 11918),
]


@pytest.mark.parametrize(
    "filename,checksum",
    init_list,
    ids=[tup[0].split(".")[0] for tup in init_list],
)
@pytest.mark.require_driver("HFA")
def test_hfa_open(filename, checksum):
    ut = gdaltest.GDALTest("HFA", filename, 1, checksum)
    ut.testOpen()


###############################################################################
# Test bugfix for https://oss-fuzz.com/v2/testcase-detail/6053338875428864


def test_hfa_read_completedefn_recursion():

    with pytest.raises(Exception):
        gdal.Open("data/hfa_completedefn_recursion.img")

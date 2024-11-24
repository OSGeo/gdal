#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic read support for all datatypes from a HFA file.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
#
# SPDX-License-Identifier: MIT
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

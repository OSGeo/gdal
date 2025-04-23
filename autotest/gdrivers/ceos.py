#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test conventional CEOS driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import gdaltest

###############################################################################
# First 75K of an IRS (Indian Remote Sensing Satellite) LGSOWG scene.  Only
# contains 3 complete scanlines.  Bizarre little endian CEOS variant. (#1862)


def test_ceos_1():

    tst = gdaltest.GDALTest(
        "CEOS", "ceos/IMAGERY-75K.L-3", 4, 9956, xoff=0, yoff=0, xsize=5932, ysize=3
    )
    return tst.testOpen()

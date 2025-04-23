#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for NGSGEOID driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import gdaltest

###############################################################################
# Test opening a little endian file


def test_ngsgeoid_1():

    tst = gdaltest.GDALTest("NGSGEOID", "ngsgeoid/g2009u01_le_truncated.bin", 1, 65534)
    tst.testOpen(
        check_gt=(
            229.99166666666667,
            0.016666666666670001,
            0.0,
            40.00833333333334,
            0.0,
            -0.016666666666670001,
        ),
        check_prj="WGS84",
    )


###############################################################################
# Test opening a big endian file


def test_ngsgeoid_2():

    tst = gdaltest.GDALTest("NGSGEOID", "ngsgeoid/g2009u01_be_truncated.bin", 1, 65534)
    tst.testOpen(
        check_gt=(
            229.99166666666667,
            0.016666666666670001,
            0.0,
            40.00833333333334,
            0.0,
            -0.016666666666670001,
        ),
        check_prj="WGS84",
    )

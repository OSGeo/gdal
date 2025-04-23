#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test SAR_CEOS driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2009, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest

###############################################################################


@gdaltest.disable_exceptions()
def test_sar_ceos_app_1():
    tst = gdaltest.GDALTest(
        "SAR_CEOS", "data/sar_ceos/ottawa_patch.img", 1, -1, filename_absolute=1
    )
    tst.testOpen()


@gdaltest.disable_exceptions()
def test_sar_ceos_asf_2():
    tst = gdaltest.GDALTest(
        "SAR_CEOS", "data/sar_ceos/R1_26161_FN1_F164.D", 1, -1, filename_absolute=1
    )
    tst.testOpen()

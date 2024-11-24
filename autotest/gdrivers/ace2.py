#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test ACE2 driver
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("ACE2")

###############################################################################
# Test a fake ACE2 dataset


def test_ace2_1():

    f = gdal.VSIFOpenL("/vsimem/45N015E_5M.ACE2", "wb")
    gdal.VSIFSeekL(f, 180 * 180 * 4 - 1, 0)
    gdal.VSIFWriteL("\0", 1, 1, f)
    gdal.VSIFCloseL(f)

    tst = gdaltest.GDALTest(
        "ACE2", "/vsimem/45N015E_5M.ACE2", 1, 0, filename_absolute=1
    )
    expected_gt = [15.0, 0.08333333333333333, 0.0, 60.0, 0.0, -0.08333333333333333]
    expected_srs = """GEOGCS["WGS 84",
    DATUM["WGS_1984",
        SPHEROID["WGS 84",6378137,298.257223563,
            AUTHORITY["EPSG","7030"]],
        TOWGS84[0,0,0,0,0,0,0],
        AUTHORITY["EPSG","6326"]],
    PRIMEM["Greenwich",0,
        AUTHORITY["EPSG","8901"]],
    UNIT["degree",0.0174532925199433,
        AUTHORITY["EPSG","9108"]],
    AUTHORITY["EPSG","4326"]]"""
    tst.testOpen(check_gt=expected_gt, check_prj=expected_srs)

    gdal.Unlink("/vsimem/45N015E_5M.ACE2")

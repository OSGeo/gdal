#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test BLX support.
# Author:   Even Rouault < even dot rouault @ spatialys.com >
#
###############################################################################
# Copyright (c) 2008, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("BLX")

###############################################################################
# Test reading a little-endian BLX


def test_blx_1():

    prj = "WGS84"
    gt = [20.0004166, 0.0008333, 0.0, 50.0004166, 0.0, -0.0008333]
    tst = gdaltest.GDALTest("BLX", "blx/s4103.blx", 1, 47024)
    tst.testOpen(check_prj=prj, check_gt=gt)


###############################################################################
# Test reading a big-endian BLX


def test_blx_2():

    prj = "WGS84"
    gt = [20.0004166, 0.0008333, 0.0, 50.0004166, 0.0, -0.0008333]
    tst = gdaltest.GDALTest("BLX", "blx/s4103.xlb", 1, 47024)
    tst.testOpen(check_prj=prj, check_gt=gt)


###############################################################################
# Test writing a little-endian BLX


def test_blx_3():

    tst = gdaltest.GDALTest("BLX", "blx/s4103.xlb", 1, 47024)
    tst.testCreateCopy(check_gt=1, check_srs=1)


###############################################################################
# Test writing a big-endian BLX


def test_blx_4():

    tst = gdaltest.GDALTest("BLX", "blx/s4103.blx", 1, 47024, options=["BIGENDIAN=YES"])
    tst.testCreateCopy(check_gt=1, check_srs=1)


###############################################################################
# Test overviews


def test_blx_5():

    ds = gdal.Open("data/blx/s4103.blx")

    band = ds.GetRasterBand(1)
    assert band.GetOverviewCount() == 4, "did not get expected overview count"

    cs = band.GetOverview(0).Checksum()
    assert cs == 42981, "wrong overview checksum (%d)" % cs

    cs = band.GetOverview(1).Checksum()
    assert cs == 61363, "wrong overview checksum (%d)" % cs

    cs = band.GetOverview(2).Checksum()
    assert cs == 48060, "wrong overview checksum (%d)" % cs

    cs = band.GetOverview(3).Checksum()
    assert cs == 12058, "wrong overview checksum (%d)" % cs

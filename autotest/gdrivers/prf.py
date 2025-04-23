#!/usr/bin/env pytest
###############################################################################
# Purpose:  Tests for Racurs PHOTOMOD tiled format reader (http://www.racurs.ru)
# Author:   Andrew Sudorgin (drons [a] list dot ru)
###############################################################################
# Copyright (c) 2016, Andrew Sudorgin
#
# SPDX-License-Identifier: MIT
###############################################################################


import gdaltest
import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("PRF")

###############################################################################


def test_prf_1():

    tst = gdaltest.GDALTest("prf", "./PRF/ph.prf", 1, 43190)
    tst.testOpen(check_gt=(1, 2, 3, -7, 5, 6))


def test_prf_2():

    ds = gdal.Open("./data/PRF/dem.x-dem")

    assert ds.RasterXSize == 4330, "Invalid dataset width"

    assert ds.RasterYSize == 4663, "Invalid dataset height"

    unittype = ds.GetRasterBand(1).GetUnitType()
    assert unittype == "m", "Failed to read elevation units from x-dem"

    datatype = ds.GetRasterBand(1).DataType
    assert datatype == gdal.GDT_Float32, "Failed to read datatype"

    expectedOvCount = 1
    if ds.GetRasterBand(1).GetOverviewCount() != expectedOvCount:
        print("Overview count must be %d" % expectedOvCount)
        print(
            "But GetOverviewCount returned %d" % ds.GetRasterBand(1).GetOverviewCount()
        )
        pytest.fail("did not get expected number of overviews")

    overview = ds.GetRasterBand(1).GetOverview(0)
    assert overview.XSize == 1082, "Invalid dataset width %d" % overview.XSize
    assert overview.YSize == 1165, "Invalid dataset height %d" % overview.YSize

    ds = None


def test_prf_3():

    ds = gdal.Open("./data/PRF/ph.prf")

    expectedOvCount = 0
    if ds.GetRasterBand(1).GetOverviewCount() != expectedOvCount:
        print("Overview count must be %d" % expectedOvCount)
        print(
            "But GetOverviewCount returned %d" % ds.GetRasterBand(1).GetOverviewCount()
        )
        pytest.fail("did not get expected number of overviews")

    ds = None


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_prf_4():

    tst = gdaltest.GDALTest("prf", "./PRF/dem.x-dem", 1, 0)
    tst.testOpen(check_gt=(1.5, 1.0, 0.0, 9329.0, 0.0, -2.0))


def test_prf_5():

    ds = gdal.Open("./data/PRF/ph.prf")

    assert (
        ds.GetSpatialRef().GetAuthorityCode("PROJCS") == "32601"
    ), "Invalid spatial reference"

    ds = None


###############################################################################

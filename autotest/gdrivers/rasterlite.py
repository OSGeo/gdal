#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for Rasterlite driver.
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
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

from osgeo import gdal, ogr

pytestmark = pytest.mark.require_driver("RASTERLITE")


@pytest.fixture(scope="module", autouse=True)
def setup():

    # This is to speed-up the runtime of tests on EXT4 filesystems
    # Do not use this for production environment if you care about data safety
    # w.r.t system/OS crashes, unless you know what you are doing.
    with gdal.config_option("OGR_SQLITE_SYNCHRONOUS", "OFF"):
        yield


def has_spatialite():
    drv = ogr.GetDriverByName("SQLite")
    return drv is not None and "SPATIALITE" in drv.GetMetadataItem(
        "DMD_CREATIONOPTIONLIST"
    )


def sqlite_supports_rtree(tmp_path):
    # Test if SQLite3 supports rtrees
    ds2 = ogr.GetDriverByName("SQLite").CreateDataSource(
        str(tmp_path / "testrtree.sqlite")
    )
    gdal.ErrorReset()
    ds2.ExecuteSQL("CREATE VIRTUAL TABLE testrtree USING rtree(id,minX,maxX,minY,maxY)")

    return "rtree" not in gdal.GetLastErrorMsg()


def sqlite_supports_rasterlite():

    gdal.ErrorReset()
    gdal.Open("data/rasterlite/rasterlite.sqlite")

    return "unsupported file format" not in gdal.GetLastErrorMsg()


###############################################################################
# Test opening a rasterlite DB without overviews


def test_rasterlite_2(tmp_path):

    if not sqlite_supports_rtree(tmp_path):
        pytest.skip(
            "Please upgrade your sqlite3 library to be able to read Rasterlite DBs (needs rtree support)!"
        )

    if not sqlite_supports_rasterlite():
        pytest.skip(
            "Please upgrade your sqlite3 library to be able to read Rasterlite DBs!"
        )

    ds = gdal.Open("data/rasterlite/rasterlite.sqlite")

    if ds is None:
        pytest.fail()

    assert ds.RasterCount == 3, "expected 3 bands"

    assert ds.GetRasterBand(1).GetOverviewCount() == 0, "did not expect overview"

    cs = ds.GetRasterBand(1).Checksum()
    expected_cs = 11746
    assert (
        cs == expected_cs or cs == 11751
    ), "for band 1, cs = %d, different from expected_cs = %d" % (cs, expected_cs)

    cs = ds.GetRasterBand(2).Checksum()
    expected_cs = 19843
    assert (
        cs == expected_cs or cs == 20088 or cs == 20083
    ), "for band 2, cs = %d, different from expected_cs = %d" % (cs, expected_cs)

    cs = ds.GetRasterBand(3).Checksum()
    expected_cs = 48911
    assert (
        cs == expected_cs or cs == 47978
    ), "for band 3, cs = %d, different from expected_cs = %d" % (cs, expected_cs)

    assert ds.GetProjectionRef().find("WGS_1984") != -1, (
        "projection_ref = %s" % ds.GetProjectionRef()
    )

    gt = ds.GetGeoTransform()
    expected_gt = (
        -180.0,
        360.0 / ds.RasterXSize,
        0.0,
        90.0,
        0.0,
        -180.0 / ds.RasterYSize,
    )

    gdaltest.check_geotransform(gt, expected_gt, 1e-15)

    ds = None


###############################################################################
# Test opening a rasterlite DB with overviews


def test_rasterlite_3():

    ds = gdal.Open("RASTERLITE:data/rasterlite/rasterlite_pyramids.sqlite,table=test")

    assert ds.RasterCount == 3, "expected 3 bands"

    assert ds.GetRasterBand(1).GetOverviewCount() == 1, "expected 1 overview"

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    expected_cs = 59551
    assert (
        cs == expected_cs or cs == 59833
    ), "for overview of band 1, cs = %d, different from expected_cs = %d" % (
        cs,
        expected_cs,
    )

    cs = ds.GetRasterBand(2).GetOverview(0).Checksum()
    expected_cs = 59603
    assert (
        cs == expected_cs or cs == 59588
    ), "for overview of band 2, cs = %d, different from expected_cs = %d" % (
        cs,
        expected_cs,
    )

    cs = ds.GetRasterBand(3).GetOverview(0).Checksum()
    expected_cs = 42173
    assert (
        cs == expected_cs or cs == 42361
    ), "for overview of band 3, cs = %d, different from expected_cs = %d" % (
        cs,
        expected_cs,
    )

    ds = None


###############################################################################
# Test opening a rasterlite DB with color table and user-defined spatial extent


def test_rasterlite_4():

    ds = gdal.Open(
        "RASTERLITE:data/rasterlite/rasterlite_pct.sqlite,minx=0,miny=0,maxx=180,maxy=90"
    )

    assert ds.RasterCount == 1, "expected 1 band"

    assert ds.RasterXSize == 169 and ds.RasterYSize == 85

    ct = ds.GetRasterBand(1).GetRasterColorTable()
    assert ct is not None, "did not get color table"

    cs = ds.GetRasterBand(1).Checksum()
    expected_cs = 36473
    assert cs == expected_cs, "for band 1, cs = %d, different from expected_cs = %d" % (
        cs,
        expected_cs,
    )

    ds = None


###############################################################################
# Test opening a rasterlite DB with color table and do color table expansion


def test_rasterlite_5():

    ds = gdal.Open("RASTERLITE:data/rasterlite/rasterlite_pct.sqlite,bands=3")

    assert ds.RasterCount == 3, "expected 3 bands"

    ct = ds.GetRasterBand(1).GetRasterColorTable()
    assert ct is None, "did not expect color table"

    cs = ds.GetRasterBand(1).Checksum()
    expected_cs = 506
    assert cs == expected_cs, "for band 1, cs = %d, different from expected_cs = %d" % (
        cs,
        expected_cs,
    )

    cs = ds.GetRasterBand(2).Checksum()
    expected_cs = 3842
    assert cs == expected_cs, "for band 2, cs = %d, different from expected_cs = %d" % (
        cs,
        expected_cs,
    )

    cs = ds.GetRasterBand(3).Checksum()
    expected_cs = 59282
    assert cs == expected_cs, "for band 3, cs = %d, different from expected_cs = %d" % (
        cs,
        expected_cs,
    )

    ds = None


###############################################################################
# Test CreateCopy()


@pytest.fixture()
def byte_sqlite(tmp_path):
    byte_sqlite_path = str(tmp_path / "byte.sqlite")
    byte_sqlite_dsn = f"RASTERLITE:{byte_sqlite_path},table=byte"

    with gdal.Open("data/byte.tif") as src_ds:
        ds = gdal.GetDriverByName("RASTERLITE").CreateCopy(byte_sqlite_dsn, src_ds)
        assert ds is not None
        del ds

    return byte_sqlite_dsn


@pytest.mark.skipif(not has_spatialite(), reason="spatialite not available")
def test_rasterlite_6(byte_sqlite):

    # Test result of CreateCopy()
    ds = gdal.Open(byte_sqlite)
    assert ds is not None

    with gdal.Open("data/byte.tif") as src_ds:
        assert (
            ds.GetRasterBand(1).Checksum() == src_ds.GetRasterBand(1).Checksum()
        ), "Wrong checksum"

        expected_gt = src_ds.GetGeoTransform()
    gt = ds.GetGeoTransform()

    gdaltest.check_geotransform(gt, expected_gt, 1e-5)

    assert "NAD27 / UTM zone 11N" in ds.GetProjectionRef(), "Wrong SRS"

    ds = None


###############################################################################
# Test BuildOverviews()


@pytest.mark.skipif(not has_spatialite(), reason="spatialite not available")
def test_rasterlite_7(byte_sqlite):

    ds = gdal.Open(byte_sqlite, gdal.GA_Update)

    # Resampling method is not taken into account
    ds.BuildOverviews("NEAREST", overviewlist=[2, 4])

    assert (
        ds.GetRasterBand(1).GetOverview(0).Checksum() == 1192
    ), "Wrong checksum for overview 0"

    assert (
        ds.GetRasterBand(1).GetOverview(1).Checksum() == 233
    ), "Wrong checksum for overview 1"

    # Reopen and test
    ds = None
    ds = gdal.Open(byte_sqlite)

    assert (
        ds.GetRasterBand(1).GetOverview(0).Checksum() == 1192
    ), "Wrong checksum for overview 0"

    assert (
        ds.GetRasterBand(1).GetOverview(1).Checksum() == 233
    ), "Wrong checksum for overview 1"

    ###############################################################################
    # Test CleanOverviews()

    ds = gdal.Open(byte_sqlite, gdal.GA_Update)

    ds.BuildOverviews(overviewlist=[])

    assert ds.GetRasterBand(1).GetOverviewCount() == 0


###############################################################################
# Test BuildOverviews() with AVERAGE resampling


@pytest.mark.skipif(not has_spatialite(), reason="spatialite not available")
def test_rasterlite_11(byte_sqlite):

    ds = gdal.Open(byte_sqlite, gdal.GA_Update)

    # Resampling method is not taken into account
    ds.BuildOverviews("AVERAGE", overviewlist=[2, 4])

    # Reopen and test
    ds = None
    ds = gdal.Open(byte_sqlite)

    assert (
        ds.GetRasterBand(1).GetOverview(0).Checksum() == 1152
    ), "Wrong checksum for overview 0"

    assert (
        ds.GetRasterBand(1).GetOverview(1).Checksum() == 215
    ), "Wrong checksum for overview 1"


###############################################################################
# Test opening a .rasterlite file


@pytest.mark.skipif(not has_spatialite(), reason="spatialite not available")
def test_rasterlite_12():

    ds = gdal.Open("data/rasterlite/byte.rasterlite")
    assert ds.GetRasterBand(1).Checksum() == 4672, "validation failed"


###############################################################################
# Test opening a .rasterlite.sql file


@pytest.mark.skipif(not has_spatialite(), reason="spatialite not available")
def test_rasterlite_13():

    if gdaltest.rasterlite_drv.GetMetadataItem("ENABLE_SQL_SQLITE_FORMAT") != "YES":
        pytest.skip()

    ds = gdal.Open("data/rasterlite/byte.rasterlite.sql")
    assert ds.GetRasterBand(1).Checksum() == 4672, "validation failed"

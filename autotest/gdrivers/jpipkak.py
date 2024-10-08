#!/usr/bin/env pytest
###############################################################################
# $Id: jpipkak.py 18659 2010-01-25 03:39:15Z warmerdam $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test reading with JPIPKAK driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2010, Frank Warmerdam <warmerdam@pobox.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal

###############################################################################
# Read test of simple byte reference data.


def test_jpipkak_1():

    pytest.skip()

    # pylint: disable=unreachable
    gdaltest.jpipkak_drv = gdal.GetDriverByName("JPIPKAK")
    if gdaltest.jpipkak_drv is None:
        pytest.skip()

    ds = gdal.Open("jpip://216.150.195.220/JP2Server/qb_boulder_msi_uint")
    assert ds is not None, "failed to open jpip stream."

    target = ds.GetRasterBand(3).GetOverview(3)

    stats = target.GetStatistics(0, 1)

    assert stats[2] == pytest.approx(6791.121, abs=1.0) and stats[3] == pytest.approx(
        3046.536, abs=1.0
    ), "did not get expected mean/stddev"


###############################################################################
#


def test_jpipkak_2():

    pytest.skip()

    # pylint: disable=unreachable
    if gdaltest.jpipkak_drv is None:
        pytest.skip()

    ds = gdal.Open("jpip://216.150.195.220/JP2Server/qb_boulder_pan_byte")
    assert ds is not None, "failed to open jpip stream."

    wkt = ds.GetProjectionRef()
    exp_wkt = 'PROJCS["WGS 84 / UTM zone 13N",GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433],AUTHORITY["EPSG","4326"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-105],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AUTHORITY["EPSG","32613"]]'

    assert gdaltest.equal_srs_from_wkt(exp_wkt, wkt)

    target = ds.GetRasterBand(1).GetOverview(3)

    stats = target.GetStatistics(0, 1)

    assert stats[2] == pytest.approx(43.429, abs=1.0) and stats[3] == pytest.approx(
        18.526, abs=1.0
    ), "did not get expected mean/stddev"


###############################################################################
# Test an 11bit image.


def test_jpipkak_3():

    pytest.skip()

    # pylint: disable=unreachable
    if gdaltest.jpipkak_drv is None:
        pytest.skip()

    ds = gdal.Open("jpip://216.150.195.220/JP2Server/qb_boulder_pan_11bit")
    assert ds is not None, "failed to open jpip stream."

    target = ds.GetRasterBand(1)

    stats = target.GetStatistics(0, 1)

    assert stats[2] == pytest.approx(483.501, abs=1.0) and stats[3] == pytest.approx(
        117.972, abs=1.0
    ), "did not get expected mean/stddev"


###############################################################################
# Test a 20bit image, reduced to 16bit during processing.


def test_jpipkak_4():

    pytest.skip()

    # pylint: disable=unreachable
    if gdaltest.jpipkak_drv is None:
        pytest.skip()

    ds = gdal.Open("jpip://216.150.195.220/JP2Server/qb_boulder_pan_20bit")
    assert ds is not None, "failed to open jpip stream."

    target = ds.GetRasterBand(1)

    stats = target.GetStatistics(0, 1)

    assert stats[2] == pytest.approx(5333.148, abs=1.0) and stats[3] == pytest.approx(
        2522.023, abs=1.0
    ), "did not get expected mean/stddev"


###############################################################################
# Test an overview level that will result in multiple fetches with subwindows.


def test_jpipkak_5():

    pytest.skip()

    # pylint: disable=unreachable
    if gdaltest.jpipkak_drv is None:
        pytest.skip()

    ds = gdal.Open("jpip://216.150.195.220/JP2Server/qb_boulder_pan_byte")
    assert ds is not None, "failed to open jpip stream."

    target = ds.GetRasterBand(1).GetOverview(1)

    stats = target.GetStatistics(0, 1)

    assert stats[2] == pytest.approx(42.462, abs=1.0) and stats[3] == pytest.approx(
        20.611, abs=1.0
    ), "did not get expected mean/stddev"

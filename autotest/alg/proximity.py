#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test ComputeProximity() algorithm.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal


@pytest.fixture(scope="module", autouse=True)
def set_cpl_tmpdir(tmp_path_factory):
    with gdaltest.set_cpl_tmpdir(tmp_path_factory, "proximity"):
        yield


###############################################################################
# Test a fairly default case.


def test_proximity_1(tmp_path):

    drv = gdal.GetDriverByName("GTiff")
    src_ds = gdal.Open("data/pat.tif")
    src_band = src_ds.GetRasterBand(1)

    dst_ds = drv.Create(tmp_path / "proximity_1.tif", 25, 25, 1, gdal.GDT_UInt8)
    dst_band = dst_ds.GetRasterBand(1)

    gdal.ComputeProximity(src_band, dst_band)

    cs_expected = 1941
    cs = dst_band.Checksum()

    dst_band = None
    dst_ds = None

    assert cs == cs_expected


###############################################################################
# Try several options


def test_proximity_2(tmp_path):

    drv = gdal.GetDriverByName("GTiff")
    src_ds = gdal.Open("data/pat.tif")
    src_band = src_ds.GetRasterBand(1)

    dst_ds = drv.Create(tmp_path / "proximity_2.tif", 25, 25, 1, gdal.GDT_Float32)
    dst_band = dst_ds.GetRasterBand(1)

    gdal.ComputeProximity(
        src_band,
        dst_band,
        options=["VALUES=65,64", "MAXDIST=12", "NODATA=-1", "FIXED_BUF_VAL=255"],
    )

    cs_expected = 3256
    cs = dst_band.Checksum()

    dst_band = None
    dst_ds = None

    assert cs == cs_expected


###############################################################################
# Try input nodata option


def test_proximity_3(tmp_path):

    drv = gdal.GetDriverByName("GTiff")
    src_ds = gdal.Open("data/pat.tif")
    src_band = src_ds.GetRasterBand(1)

    dst_ds = drv.Create(tmp_path / "proximity_3.tif", 25, 25, 1, gdal.GDT_UInt8)
    dst_band = dst_ds.GetRasterBand(1)

    gdal.ComputeProximity(
        src_band,
        dst_band,
        options=["VALUES=65,64", "MAXDIST=12", "USE_INPUT_NODATA=YES", "NODATA=0"],
    )

    cs_expected = 1465
    cs = dst_band.Checksum()

    dst_band = None
    dst_ds = None

    assert cs == cs_expected

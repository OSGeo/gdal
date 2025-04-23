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


import pytest

from osgeo import gdal

###############################################################################
# Test a fairly default case.


def test_proximity_1():

    drv = gdal.GetDriverByName("GTiff")
    src_ds = gdal.Open("data/pat.tif")
    src_band = src_ds.GetRasterBand(1)

    dst_ds = drv.Create("tmp/proximity_1.tif", 25, 25, 1, gdal.GDT_Byte)
    dst_band = dst_ds.GetRasterBand(1)

    gdal.ComputeProximity(src_band, dst_band)

    cs_expected = 1941
    cs = dst_band.Checksum()

    dst_band = None
    dst_ds = None

    if cs == cs_expected or gdal.GetConfigOption("CPL_DEBUG", "OFF") != "ON":
        drv.Delete("tmp/proximity_1.tif")

    if cs != cs_expected:
        print("Got: ", cs)
        pytest.fail("got wrong checksum")


###############################################################################
# Try several options


def test_proximity_2():

    drv = gdal.GetDriverByName("GTiff")
    src_ds = gdal.Open("data/pat.tif")
    src_band = src_ds.GetRasterBand(1)

    dst_ds = drv.Create("tmp/proximity_2.tif", 25, 25, 1, gdal.GDT_Float32)
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

    if cs == cs_expected or gdal.GetConfigOption("CPL_DEBUG", "OFF") != "ON":
        drv.Delete("tmp/proximity_2.tif")

    if cs != cs_expected:
        print("Got: ", cs)
        pytest.fail("got wrong checksum")


###############################################################################
# Try input nodata option


def test_proximity_3():

    drv = gdal.GetDriverByName("GTiff")
    src_ds = gdal.Open("data/pat.tif")
    src_band = src_ds.GetRasterBand(1)

    dst_ds = drv.Create("tmp/proximity_3.tif", 25, 25, 1, gdal.GDT_Byte)
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

    if cs == cs_expected or gdal.GetConfigOption("CPL_DEBUG", "OFF") != "ON":
        drv.Delete("tmp/proximity_3.tif")

    if cs != cs_expected:
        print("Got: ", cs)
        pytest.fail("got wrong checksum")

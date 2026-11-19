#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Golden Software ASCII and binary grid format.
# Author:   Andrey Kiselev <dron@ak4719.spb.edu>
#
###############################################################################
# Copyright (c) 2008, Andrey Kiselev <dron@ak4719.spb.edu>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal

###############################################################################
# Perform simple read tests.


def test_gsg_1():

    tst = gdaltest.GDALTest("gsbg", "gsg/gsg_binary.grd", 1, 4672)
    tst.testOpen(check_gt=(440720, 60, 0, 3751320, 0, -60))


def test_gsg_2():

    tst = gdaltest.GDALTest("gsag", "gsg/gsg_ascii.grd", 1, 4672)
    tst.testOpen(check_gt=(440720, 60, 0, 3751320, 0, -60))


def test_gsg_3():

    tst = gdaltest.GDALTest("gs7bg", "gsg/gsg_7binary.grd", 1, 4672)
    tst.testOpen(check_gt=(440720, 60, 0, 3751320, 0, -60))


###############################################################################
# Create simple copy and check.


def test_gsg_4(tmp_path):

    tst = gdaltest.GDALTest("gsbg", "gsg/gsg_binary.grd", 1, 4672, tmpdir=tmp_path)

    tst.testCreateCopy(check_gt=1)


def test_gsg_5(tmp_path):

    tst = gdaltest.GDALTest("gsag", "gsg/gsg_ascii.grd", 1, 4672, tmpdir=tmp_path)

    tst.testCreateCopy(check_gt=1)


def test_gsg_6():

    tst = gdaltest.GDALTest("gsbg", "gsg/gsg_binary.grd", 1, 4672)

    tst.testCreate(out_bands=1)


def test_gsg_7():

    tst = gdaltest.GDALTest("gs7bg", "gsg/gsg_7binary.grd", 1, 4672)

    tst.testCreate(out_bands=1)


def test_gsg_8(tmp_path):

    tst = gdaltest.GDALTest("gs7bg", "gsg/gsg_7binary.grd", 1, 4672, tmpdir=tmp_path)

    tst.testCreateCopy(check_gt=1)


###############################################################################


@pytest.mark.parametrize(
    "drv_name,xsize,ysize",
    [
        ("GSBG", 2, 1),
        ("GSBG", 1, 2),
        ("GSBG", 2, 32768),
        ("GSBG", 32768, 2),
        ("GS7BG", 2, 1),
        ("GS7BG", 1, 2),
        ("GS7BG", 2, (1 << 31) - 1),
        ("GS7BG", (1 << 31) - 1, 2),
    ],
)
def test_gsg_create_wrong_dims(tmp_vsimem, drv_name, xsize, ysize):

    with pytest.raises(Exception):
        gdal.GetDriverByName(drv_name).Create(tmp_vsimem / "out", xsize, ysize)


@pytest.mark.parametrize(
    "drv_name,xsize,ysize",
    [
        ("GSBG", 2, 1),
        ("GSBG", 1, 2),
        ("GSBG", 2, 32768),
        ("GSBG", 32768, 2),
        ("GS7BG", 2, 1),
        ("GS7BG", 1, 2),
    ],
)
def test_gsg_createcopy_wrong_dims(tmp_vsimem, drv_name, xsize, ysize):

    src_ds = gdal.GetDriverByName("MEM").Create("", xsize, ysize)
    with pytest.raises(Exception):
        gdal.GetDriverByName(drv_name).CreateCopy(tmp_vsimem / "out", src_ds)


@pytest.mark.require_driver("GS7BG")
def test_gs7bg_createcopy_non_square(tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 5)
    src_ds.SetGeoTransform([0, 1, 0, 5, 0, -1])
    src_ds.GetRasterBand(1).WriteRaster(0, 0, 3, 5, bytes(range(15)))
    gdal.GetDriverByName("GS7BG").CreateCopy(tmp_vsimem / "out.grd", src_ds)

    with gdal.Open(tmp_vsimem / "out.grd") as ds:
        assert ds.RasterYSize == 5
        assert ds.GetRasterBand(1).ReadRaster(buf_type=gdal.GDT_UInt8) == bytes(
            range(15)
        )

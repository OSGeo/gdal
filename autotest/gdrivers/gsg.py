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


def test_gsg_4():

    tst = gdaltest.GDALTest("gsbg", "gsg/gsg_binary.grd", 1, 4672)

    tst.testCreateCopy(check_gt=1)


def test_gsg_5():

    tst = gdaltest.GDALTest("gsag", "gsg/gsg_ascii.grd", 1, 4672)

    tst.testCreateCopy(check_gt=1)


def test_gsg_6():

    tst = gdaltest.GDALTest("gsbg", "gsg/gsg_binary.grd", 1, 4672)

    tst.testCreate(out_bands=1)


def test_gsg_7():

    tst = gdaltest.GDALTest("gs7bg", "gsg/gsg_7binary.grd", 1, 4672)

    tst.testCreate(out_bands=1)


def test_gsg_8():

    tst = gdaltest.GDALTest("gs7bg", "gsg/gsg_7binary.grd", 1, 4672)

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

#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for GIF driver.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2011, Even Rouault <even dot rouault at spatialys.com>
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

from osgeo import gdal

pytestmark = pytest.mark.require_driver("GIF")


@pytest.fixture(autouse=True)
def setup_and_cleanup_test():

    yield

    gdaltest.clean_tmp()


###############################################################################
# Get the GIF driver, and verify a few things about it.


def test_gif_1():

    gif_drv = gdal.GetDriverByName("GIF")
    drv_md = gif_drv.GetMetadata()
    assert drv_md["DMD_MIMETYPE"] == "image/gif"


###############################################################################
# Read test of simple byte reference data.


def test_gif_2():

    tst = gdaltest.GDALTest("GIF", "gif/bug407.gif", 1, 57921)
    tst.testOpen()


###############################################################################
# Test lossless copying.


def test_gif_3():

    tst = gdaltest.GDALTest(
        "GIF", "gif/bug407.gif", 1, 57921, options=["INTERLACING=NO"]
    )

    tst.testCreateCopy()


###############################################################################
# Verify the colormap, and nodata setting for test file.


def test_gif_4():

    ds = gdal.Open("data/gif/bug407.gif")
    cm = ds.GetRasterBand(1).GetRasterColorTable()
    assert (
        cm.GetCount() == 16
        and cm.GetColorEntry(0) == (255, 255, 255, 255)
        and cm.GetColorEntry(1) == (255, 255, 208, 255)
    ), "Wrong colormap entries"

    cm = None

    assert ds.GetRasterBand(1).GetNoDataValue() is None, "Wrong nodata value."

    md = ds.GetRasterBand(1).GetMetadata()
    assert (
        "GIF_BACKGROUND" in md and md["GIF_BACKGROUND"] == "0"
    ), "background metadata missing."


###############################################################################
# Test creating an in memory copy.


def test_gif_5():

    tst = gdaltest.GDALTest("GIF", "byte.tif", 1, 4672)

    tst.testCreateCopy(vsimem=1)


###############################################################################
# Verify nodata support


def test_gif_6():

    src_ds = gdal.Open("../gcore/data/nodata_byte.tif")

    new_ds = gdaltest.gif_drv.CreateCopy("tmp/nodata_byte.gif", src_ds)
    assert new_ds is not None, "Create copy operation failure"

    bnd = new_ds.GetRasterBand(1)
    assert bnd.Checksum() == 4440, "Wrong checksum"

    bnd = None
    new_ds = None
    src_ds = None

    new_ds = gdal.Open("tmp/nodata_byte.gif")

    bnd = new_ds.GetRasterBand(1)
    assert bnd.Checksum() == 4440, "Wrong checksum"

    # NOTE - mloskot: condition may fail as nodata is a float-point number
    nodata = bnd.GetNoDataValue()
    assert nodata == 0, "Got unexpected nodata value."

    bnd = None
    new_ds = None

    gdaltest.gif_drv.Delete("tmp/nodata_byte.gif")


###############################################################################
# Confirm reading with the BIGGIF driver.


def test_gif_7():

    # Move the GIF driver after the BIGGIF driver.
    try:
        drv = gdal.GetDriverByName("GIF")
        drv.Deregister()
        drv.Register()

        tst = gdaltest.GDALTest("BIGGIF", "gif/bug407.gif", 1, 57921)
        tst.testOpen()

        ds = gdal.Open("data/gif/bug407.gif")
        assert ds is not None

        assert ds.GetDriver().ShortName == "BIGGIF"
    finally:
        drv = gdal.GetDriverByName("BIGGIF")
        drv.Deregister()
        drv.Register()


###############################################################################
# Confirm that BIGGIF driver is selected for huge gifs


def test_gif_8():

    ds = gdal.Open("data/gif/fakebig.gif")
    assert ds is not None

    assert ds.GetDriver().ShortName == "BIGGIF"


###############################################################################
# Test writing to /vsistdout/


def test_gif_9():

    src_ds = gdal.Open("data/byte.tif")
    ds = gdal.GetDriverByName("GIF").CreateCopy(
        "/vsistdout_redirect//vsimem/tmp.gif", src_ds
    )
    assert ds.GetRasterBand(1).Checksum() == 0
    src_ds = None
    ds = None

    ds = gdal.Open("/vsimem/tmp.gif")
    assert ds is not None
    assert ds.GetRasterBand(1).Checksum() == 4672

    gdal.Unlink("/vsimem/tmp.gif")


###############################################################################
# Test interlacing


def test_gif_10():

    tst = gdaltest.GDALTest("GIF", "byte.tif", 1, 4672, options=["INTERLACING=YES"])

    tst.testCreateCopy(vsimem=1)

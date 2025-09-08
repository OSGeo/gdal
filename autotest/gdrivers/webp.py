#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test WEBP driver
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal

pytestmark = pytest.mark.require_driver("WEBP")

###############################################################################
# Open() test


def test_webp_2():

    ds = gdal.Open("data/webp/rgbsmall.webp")
    cs = ds.GetRasterBand(1).Checksum()
    assert (
        cs == 21464 or cs == 21450 or cs == 21459
    ), "did not get expected checksum on band 1"


###############################################################################
# CreateCopy() test


def test_webp_3():

    src_ds = gdal.Open("data/rgbsmall.tif")
    out_ds = gdaltest.webp_drv.CreateCopy(
        "/vsimem/webp_3.webp", src_ds, options=["QUALITY=80"]
    )
    src_ds = None
    cs1 = out_ds.GetRasterBand(1).Checksum()
    out_ds = None
    gdal.Unlink("/vsimem/webp_3.webp")
    gdal.Unlink("/vsimem/webp_3.webp.aux.xml")

    # 21502 is for libwebp 0.3.0
    # 21787 is for libwebp 1.0.3
    assert cs1 in (21464, 21502, 21695, 21700, 21787)


###############################################################################
# CreateCopy() on RGBA


@pytest.mark.require_creation_option("WEBP", "LOSSLESS")
def test_webp_4():

    src_ds = gdal.Open("../gcore/data/stefan_full_rgba.tif")
    out_ds = gdaltest.webp_drv.CreateCopy("/vsimem/webp_4.webp", src_ds)
    src_ds = None
    assert (
        out_ds.GetMetadataItem("COMPRESSION_REVERSIBILITY", "IMAGE_STRUCTURE")
        == "LOSSY"
    )
    cs1 = out_ds.GetRasterBand(1).Checksum()
    cs4 = out_ds.GetRasterBand(4).Checksum()
    out_ds = None
    gdal.Unlink("/vsimem/webp_4.webp")

    # 22849 is for libwebp 0.3.0
    # 29229 is for libwebp 1.0.3
    assert cs1 in (
        22001,
        22849,
        34422,
        36652,
        36658,
        45319,
        29229,
    ), "did not get expected checksum on band 1"

    assert cs4 == 10807, "did not get expected checksum on band 4"


###############################################################################
# CreateCopy() on RGBA with lossless compression


@pytest.mark.require_creation_option("WEBP", "LOSSLESS")
def test_webp_5():

    if gdaltest.is_travis_branch("alpine_32bit"):
        pytest.skip()

    src_ds = gdal.Open("../gcore/data/stefan_full_rgba.tif")
    out_ds = gdaltest.webp_drv.CreateCopy(
        "/vsimem/webp_5.webp", src_ds, options=["LOSSLESS=YES"]
    )
    src_ds = None
    assert (
        out_ds.GetMetadataItem("COMPRESSION_REVERSIBILITY", "IMAGE_STRUCTURE")
        == "LOSSLESS"
    )
    cs1 = out_ds.GetRasterBand(1).Checksum()
    cs4 = out_ds.GetRasterBand(4).Checksum()
    out_ds = None
    gdal.Unlink("/vsimem/webp_5.webp")

    assert (
        cs1 == 12603 or cs1 == 18536 or cs1 == 14800
    ), "did not get expected checksum on band 1"

    assert cs4 == 10807, "did not get expected checksum on band 4"


###############################################################################
# CreateCopy() on RGBA with lossless compression and exact rgb values


@pytest.mark.require_creation_option("WEBP", "LOSSLESS")
@pytest.mark.require_creation_option("WEBP", "EXACT")
def test_webp_6():

    src_ds = gdal.Open("../gcore/data/stefan_full_rgba.tif")
    out_ds = gdaltest.webp_drv.CreateCopy(
        "/vsimem/webp_6.webp", src_ds, options=["LOSSLESS=YES", "EXACT=1"]
    )
    src_ds = None
    cs1 = out_ds.GetRasterBand(1).Checksum()
    cs4 = out_ds.GetRasterBand(4).Checksum()
    out_ds = None
    gdal.Unlink("/vsimem/webp_6.webp")

    assert cs1 == 12603, "did not get expected checksum on band 1"

    assert cs4 == 10807, "did not get expected checksum on band 4"


###############################################################################
# CreateCopy() in lossless copy mode


def test_webp_lossless_copy():

    outfilename = "/vsimem/out.webp"
    src_ds = gdal.Open("data/webp/rgbsmall.webp")
    assert gdaltest.webp_drv.CreateCopy(outfilename, src_ds) is not None
    f = gdal.VSIFOpenL(outfilename, "rb")
    assert f
    data = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)
    assert data == open(src_ds.GetDescription(), "rb").read()
    gdaltest.webp_drv.Delete(outfilename)


###############################################################################
# CreateCopy() in lossless copy mode with XMP metadata and explicit LOSSLESS_COPY


def test_webp_lossless_copy_with_xmp():

    outfilename = "/vsimem/out.webp"
    src_ds = gdal.Open("data/webp/rgbsmall_with_xmp.webp")
    assert (
        gdaltest.webp_drv.CreateCopy(outfilename, src_ds, options=["LOSSLESS_COPY=YES"])
        is not None
    )
    f = gdal.VSIFOpenL(outfilename, "rb")
    assert f
    data = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)
    assert data == open(src_ds.GetDescription(), "rb").read()
    gdaltest.webp_drv.Delete(outfilename)


###############################################################################


def test_webp_create_copy_only_visible_at_close_time(tmp_path):

    src_ds = gdal.Open("data/rgbsmall.tif")
    out_filename = tmp_path / "tmp.webp"

    def my_callback(pct, msg, user_data):
        if pct < 1:
            assert gdal.VSIStatL(out_filename) is None
        return True

    drv = gdal.GetDriverByName("WEBP")
    assert drv.GetMetadataItem(gdal.DCAP_CREATE_ONLY_VISIBLE_AT_CLOSE_TIME) == "YES"
    drv.CreateCopy(
        out_filename,
        src_ds,
        options=["@CREATE_ONLY_VISIBLE_AT_CLOSE_TIME=YES"],
        callback=my_callback,
    )

    with gdal.Open(out_filename) as ds:
        ds.GetRasterBand(1).Checksum()

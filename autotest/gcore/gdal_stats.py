#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test core numeric operations and statistics calculations
# Author:   Mateusz Loskot <mateusz@loskot.net>
#
###############################################################################
# Copyright (c) 2007, Mateusz Loskot <mateusz@loskot.net>
# Copyright (c) 2009-2012, Even Rouault <even dot rouault at spatialys.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
###############################################################################

import math
import os
import shutil
import struct
import sys
from itertools import chain

import gdaltest
import pytest

from osgeo import gdal

###############################################################################
# Test handling NaN with GDT_Float32 data


def test_stats_nan_1():

    stats = (50.0, 58.0, 54.0, 2.5819888974716)

    shutil.copyfile("data/nan32.tif", "tmp/nan32.tif")

    t = gdaltest.GDALTest("GTiff", "tmp/nan32.tif", 1, 874, filename_absolute=1)
    t.testOpen(check_approx_stat=stats, check_stat=stats)

    gdal.GetDriverByName("GTiff").Delete("tmp/nan32.tif")


###############################################################################
# Test handling NaN with GDT_Float64 data


def test_stats_nan_2():

    stats = (50.0, 58.0, 54.0, 2.5819888974716)

    shutil.copyfile("data/nan64.tif", "tmp/nan64.tif")

    t = gdaltest.GDALTest("GTiff", "tmp/nan64.tif", 1, 4414, filename_absolute=1)
    t.testOpen(check_approx_stat=stats, check_stat=stats)

    gdal.GetDriverByName("GTiff").Delete("tmp/nan64.tif")


###############################################################################
# Test stats on signed byte (#3151)


@pytest.mark.require_driver("HFA")
def test_stats_signedbyte():

    stats = (-128.0, 127.0, -0.2, 80.64)

    shutil.copyfile("data/stats_signed_byte.img", "tmp/stats_signed_byte.img")

    t = gdaltest.GDALTest(
        "HFA", "tmp/stats_signed_byte.img", 1, 11, filename_absolute=1
    )
    t.testOpen(check_approx_stat=stats, check_stat=stats, skip_checksum=1)

    gdal.GetDriverByName("HFA").Delete("tmp/stats_signed_byte.img")


###############################################################################
# Test return of GetStatistics() when we don't have stats and don't
# force their computation (#3572)


def test_stats_dont_force():

    if os.path.exists("data/byte.tif.aux.xml"):
        gdal.Unlink("data/byte.tif.aux.xml")
    ds = gdal.Open("data/byte.tif")
    stats = ds.GetRasterBand(1).GetStatistics(0, 0)
    assert stats is None


###############################################################################
# Test statistics when stored nodata value doesn't accurately match the nodata
# value used in the imagery (#3573)


def test_stats_approx_nodata():

    shutil.copyfile("data/minfloat.tif", "tmp/minfloat.tif")
    try:
        os.remove("tmp/minfloat.tif.aux.xml")
    except OSError:
        pass

    ds = gdal.Open("tmp/minfloat.tif")
    stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    md = ds.GetRasterBand(1).GetMetadata()
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None

    os.remove("tmp/minfloat.tif.aux.xml")

    ds = gdal.Open("tmp/minfloat.tif")
    minmax = ds.GetRasterBand(1).ComputeRasterMinMax()
    ds = None

    os.remove("tmp/minfloat.tif")

    if nodata != -3.4028234663852886e38:
        print("%.17g" % nodata)
        pytest.fail("did not get expected nodata")

    assert stats == [-3.0, 5.0, 1.0, 4.0], "did not get expected stats"

    assert md == {
        "STATISTICS_MEAN": "1",
        "STATISTICS_MAXIMUM": "5",
        "STATISTICS_MINIMUM": "-3",
        "STATISTICS_STDDEV": "4",
        "STATISTICS_VALID_PERCENT": "50",
    }, "did not get expected metadata"

    assert minmax == (-3.0, 5.0), "did not get expected minmax"


###############################################################################
# Test read and copy of dataset with nan as nodata value (#3576)


@pytest.mark.require_driver("GTiff")
def test_stats_nan_3():

    src_ds = gdal.Open("data/nan32_nodata.tif")
    nodata = src_ds.GetRasterBand(1).GetNoDataValue()
    assert gdaltest.isnan(nodata), "expected nan, got %f" % nodata

    out_ds = gdal.GetDriverByName("GTiff").CreateCopy("tmp/nan32_nodata.tif", src_ds)
    del out_ds

    src_ds = None

    try:
        os.remove("tmp/nan32_nodata.tif.aux.xml")
    except OSError:
        pass

    ds = gdal.Open("tmp/nan32_nodata.tif")
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None

    gdal.GetDriverByName("GTiff").Delete("tmp/nan32_nodata.tif")
    assert gdaltest.isnan(nodata), "expected nan, got %f" % nodata


###############################################################################
# Test reading a VRT with a complex source that define nan as band nodata
# and complex source nodata (#3576)


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_stats_nan_4():

    ds = gdal.Open("data/nan32_nodata.vrt")
    cs = ds.GetRasterBand(1).Checksum()
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None

    assert cs == 874, "did not get expected checksum"

    assert gdaltest.isnan(nodata), "expected nan, got %f" % nodata


###############################################################################
# Test reading a VRT with a complex source that define 0 as band nodata
# and complex source nodata (nan must be translated to 0 then) (#3576)


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_stats_nan_5():

    ds = gdal.Open("data/nan32_nodata_nan_to_zero.vrt")
    cs = ds.GetRasterBand(1).Checksum()
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None

    assert cs == 978, "did not get expected checksum"

    assert nodata == 0, "expected nan, got %f" % nodata


###############################################################################
# Test reading a warped VRT with nan as src nodata and dest nodata (#3576)


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_stats_nan_6():

    ds = gdal.Open("data/nan32_nodata_warp.vrt")
    cs = ds.GetRasterBand(1).Checksum()
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None

    assert cs == 874, "did not get expected checksum"

    assert gdaltest.isnan(nodata), "expected nan, got %f" % nodata


###############################################################################
# Test reading a warped VRT with nan as src nodata and 0 as dest nodata (#3576)


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_stats_nan_7():

    ds = gdal.Open("data/nan32_nodata_warp_nan_to_zero.vrt")
    cs = ds.GetRasterBand(1).Checksum()
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None

    assert cs == 978, "did not get expected checksum"

    assert nodata == 0, "expected nan, got %f" % nodata


###############################################################################
# Test reading a warped VRT with zero as src nodata and nan as dest nodata (#3576)


@pytest.mark.skipif(
    not gdaltest.vrt_has_open_support(),
    reason="VRT driver open missing",
)
def test_stats_nan_8():

    ds = gdal.Open("data/nan32_nodata_warp_zero_to_nan.vrt")
    cs = ds.GetRasterBand(1).Checksum()
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None

    assert cs == 874, "did not get expected checksum"

    assert gdaltest.isnan(nodata), "expected nan, got %f" % nodata


###############################################################################
# Test statistics computation when nodata = +/- inf


def stats_nodata_inf_progress_cbk(value, string, extra):
    # pylint: disable=unused-argument
    extra[0] = value


@pytest.mark.require_driver("HFA")
def test_stats_nodata_inf():

    ds = gdal.GetDriverByName("HFA").Create(
        "/vsimem/stats_nodata_inf.img", 3, 1, 1, gdal.GDT_Float32
    )
    ds.GetRasterBand(1).SetNoDataValue(gdaltest.neginf())
    ds.GetRasterBand(1).WriteRaster(
        0, 0, 1, 1, struct.pack("f", gdaltest.neginf()), buf_type=gdal.GDT_Float32
    )
    ds.GetRasterBand(1).WriteRaster(
        1, 0, 1, 1, struct.pack("f", 1), buf_type=gdal.GDT_Float32
    )
    ds.GetRasterBand(1).WriteRaster(
        2, 0, 1, 1, struct.pack("f", -2), buf_type=gdal.GDT_Float32
    )

    ds.GetRasterBand(1).Checksum()
    user_data = [0]
    stats = ds.GetRasterBand(1).ComputeStatistics(
        False, stats_nodata_inf_progress_cbk, user_data
    )
    assert user_data[0] == 1.0, "did not get expected pct"
    ds = None

    gdal.GetDriverByName("HFA").Delete("/vsimem/stats_nodata_inf.img")

    assert stats == [-2.0, 1.0, -0.5, 1.5], "did not get expected stats"


###############################################################################
# Test deserialization of +inf/-inf values written by Linux and Windows


def stats_nodata_check(filename, expected_nodata):
    ds = gdal.Open(filename)
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None

    assert nodata == expected_nodata, "did not get expected nodata value"


def test_stats_nodata_neginf_linux():
    return stats_nodata_check("data/stats_nodata_neginf.tif", gdaltest.neginf())


def test_stats_nodata_neginf_msvc():
    return stats_nodata_check("data/stats_nodata_neginf_msvc.tif", gdaltest.neginf())


def test_stats_nodata_posinf_linux():
    return stats_nodata_check("data/stats_nodata_posinf.tif", gdaltest.posinf())


def test_stats_nodata_posinf_msvc():
    return stats_nodata_check("data/stats_nodata_posinf_msvc.tif", gdaltest.posinf())


###############################################################################
# Test standard deviation computation on huge values


@pytest.mark.require_driver("AAIGRID")
def test_stats_stddev_huge_values():

    gdal.FileFromMemBuffer(
        "/vsimem/stats_stddev_huge_values.asc",
        """ncols        4
nrows        4
xllcorner    0
yllcorner    0
cellsize     1
 100000000 100000002 100000000 100000002
 100000000 100000002 100000000 100000002
 100000000 100000002 100000000 100000002
 100000000 100000002 100000000 100000002""",
    )
    ds = gdal.Open("/vsimem/stats_stddev_huge_values.asc")
    stats = ds.GetRasterBand(1).ComputeStatistics(0)
    assert stats == [
        100000000.0,
        100000002.0,
        100000001.0,
        1.0,
    ], "did not get expected stats"
    ds = None
    gdal.GetDriverByName("AAIGRID").Delete("/vsimem/stats_stddev_huge_values.asc")


###############################################################################
# Test approximate statistics computation on a square shaped raster whose first column
# of blocks is nodata only


def test_stats_square_shape():

    ds = gdal.GetDriverByName("GTiff").Create(
        "/vsimem/stats_square_shape.tif",
        32,
        32,
        options=["TILED=YES", "BLOCKXSIZE=16", "BLOCKYSIZE=16"],
    )
    ds.GetRasterBand(1).SetNoDataValue(0)
    ds.GetRasterBand(1).WriteRaster(
        16, 0, 16, 32, struct.pack("B" * 1, 255), buf_xsize=1, buf_ysize=1
    )
    stats = ds.GetRasterBand(1).ComputeStatistics(True)
    hist = ds.GetRasterBand(1).GetHistogram(approx_ok=1)
    minmax = ds.GetRasterBand(1).ComputeRasterMinMax(1)
    ds = None

    gdal.GetDriverByName("GTiff").Delete("/vsimem/stats_square_shape.tif")

    assert stats == [255, 255, 255, 0], "did not get expected stats"
    assert hist[255] == 16 * 16, "did not get expected histogram"
    if minmax != (255, 255):
        print(hist)
        pytest.fail("did not get expected minmax")


###############################################################################
# Test when nodata = FLT_MIN (#6578)


def test_stats_flt_min():

    shutil.copyfile("data/flt_min.tif", "tmp/flt_min.tif")
    try:
        os.remove("tmp/flt_min.tif.aux.xml")
    except OSError:
        pass

    ds = gdal.Open("tmp/flt_min.tif")
    stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None

    os.remove("tmp/flt_min.tif.aux.xml")

    ds = gdal.Open("tmp/flt_min.tif")
    minmax = ds.GetRasterBand(1).ComputeRasterMinMax()
    ds = None

    os.remove("tmp/flt_min.tif")

    if nodata != 1.17549435082228751e-38:
        print("%.17g" % nodata)
        pytest.fail("did not get expected nodata")

    assert stats == [0.0, 1.0, 0.33333333333333337, 0.47140452079103168] or stats == [
        0.0,
        1.0,
        0.33333333333333331,
        0.47140452079103168,
    ], "did not get expected stats"

    assert minmax == (0.0, 1.0), "did not get expected minmax"


###############################################################################
# Test when nodata = DBL_MIN (#6578)


def test_stats_dbl_min():

    shutil.copyfile("data/dbl_min.tif", "tmp/dbl_min.tif")
    try:
        os.remove("tmp/dbl_min.tif.aux.xml")
    except OSError:
        pass

    ds = gdal.Open("tmp/dbl_min.tif")
    stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    ds = None

    os.remove("tmp/dbl_min.tif.aux.xml")

    ds = gdal.Open("tmp/dbl_min.tif")
    minmax = ds.GetRasterBand(1).ComputeRasterMinMax()
    ds = None

    os.remove("tmp/dbl_min.tif")

    if nodata != 2.22507385850720138e-308:
        print("%.17g" % nodata)
        pytest.fail("did not get expected nodata")

    assert stats == [0.0, 1.0, 0.33333333333333337, 0.47140452079103168] or stats == [
        0.0,
        1.0,
        0.33333333333333331,
        0.47140452079103168,
    ], "did not get expected stats"

    assert minmax == (0.0, 1.0), "did not get expected minmax"


###############################################################################
# Test stats on a tiled Byte with partial tiles


@pytest.mark.parametrize("GDAL_STATS_USE_INTEGER_STATS", [None, "NO"])
def test_stats_byte_partial_tiles(GDAL_STATS_USE_INTEGER_STATS):

    ds = gdal.Translate(
        "/vsimem/stats_byte_tiled.tif",
        "../gdrivers/data/small_world.tif",
        creationOptions=["TILED=YES", "BLOCKXSIZE=64", "BLOCKYSIZE=64"],
    )
    with gdal.config_option(
        "GDAL_STATS_USE_INTEGER_STATS", GDAL_STATS_USE_INTEGER_STATS
    ):
        stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    ds = None

    gdal.GetDriverByName("GTiff").Delete("/vsimem/stats_byte_tiled.tif")

    expected_stats = [0.0, 255.0, 50.22115, 67.119029288849973]
    if GDAL_STATS_USE_INTEGER_STATS == "NO":
        assert stats == pytest.approx(expected_stats, rel=1e-15)
    else:
        assert stats == expected_stats, "did not get expected stats"

    # Same but with nodata set
    ds = gdal.Translate(
        "/vsimem/stats_byte_tiled.tif",
        "../gdrivers/data/small_world.tif",
        creationOptions=["TILED=YES", "BLOCKXSIZE=64", "BLOCKYSIZE=64"],
    )
    ds.GetRasterBand(1).SetNoDataValue(0)
    with gdal.config_option(
        "GDAL_STATS_USE_INTEGER_STATS", GDAL_STATS_USE_INTEGER_STATS
    ):
        stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    ds = None

    gdal.GetDriverByName("GTiff").Delete("/vsimem/stats_byte_tiled.tif")

    expected_stats = [1.0, 255.0, 50.311081057390084, 67.14541389488096]
    assert stats == pytest.approx(
        expected_stats, rel=1e-10
    ), "did not get expected stats"

    # Same but with nodata set but untiled and with non power of 16 block size
    ds = gdal.Translate(
        "/vsimem/stats_byte_untiled.tif",
        "../gdrivers/data/small_world.tif",
        options="-srcwin 0 0 399 200",
    )
    ds.GetRasterBand(1).SetNoDataValue(0)
    with gdal.config_option(
        "GDAL_STATS_USE_INTEGER_STATS", GDAL_STATS_USE_INTEGER_STATS
    ):
        stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    ds = None

    gdal.GetDriverByName("GTiff").Delete("/vsimem/stats_byte_untiled.tif")

    expected_stats = [1.0, 255.0, 50.378183963744554, 67.184793517649453]
    assert stats == pytest.approx(
        expected_stats, rel=1e-10
    ), "did not get expected stats"

    ds = gdal.GetDriverByName("GTiff").Create(
        "/vsimem/stats_byte_tiled.tif",
        1000,
        512,
        options=["TILED=YES", "BLOCKXSIZE=512", "BLOCKYSIZE=512"],
    )
    ds.GetRasterBand(1).Fill(255)
    with gdal.config_option(
        "GDAL_STATS_USE_INTEGER_STATS", GDAL_STATS_USE_INTEGER_STATS
    ):
        stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    ds = None
    gdal.Unlink("/vsimem/stats_byte_tiled.tif")

    expected_stats = [255.0, 255.0, 255.0, 0.0]
    assert (
        max([abs(stats[i] - expected_stats[i]) for i in range(4)]) <= 1e-15
    ), "did not get expected stats"

    # Non optimized code path
    ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    ds.GetRasterBand(1).WriteRaster(0, 0, 1, 1, struct.pack("B" * 1, 1))
    with gdal.config_option(
        "GDAL_STATS_USE_INTEGER_STATS", GDAL_STATS_USE_INTEGER_STATS
    ):
        stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    ds = None

    expected_stats = [1.0, 1.0, 1.0, 0.0]
    assert (
        max([abs(stats[i] - expected_stats[i]) for i in range(4)]) <= 1e-15
    ), "did not get expected stats"

    ds = gdal.GetDriverByName("MEM").Create("", 3, 5)
    ds.GetRasterBand(1).WriteRaster(0, 0, 3, 1, struct.pack("B" * 3, 20, 30, 50))
    ds.GetRasterBand(1).WriteRaster(0, 1, 3, 1, struct.pack("B" * 3, 60, 10, 5))
    ds.GetRasterBand(1).WriteRaster(0, 2, 3, 1, struct.pack("B" * 3, 10, 20, 0))
    ds.GetRasterBand(1).WriteRaster(0, 3, 3, 1, struct.pack("B" * 3, 10, 20, 255))
    ds.GetRasterBand(1).WriteRaster(0, 4, 3, 1, struct.pack("B" * 3, 10, 20, 10))
    with gdal.config_option(
        "GDAL_STATS_USE_INTEGER_STATS", GDAL_STATS_USE_INTEGER_STATS
    ):
        stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    ds = None

    expected_stats = [0.0, 255.0, 35.333333333333336, 60.785597709398971]
    assert (
        max([abs(stats[i] - expected_stats[i]) for i in range(4)]) <= 1e-15
    ), "did not get expected stats"

    ds = gdal.GetDriverByName("MEM").Create("", 32 + 2, 2)
    ds.GetRasterBand(1).Fill(1)
    ds.GetRasterBand(1).WriteRaster(32, 1, 2, 1, struct.pack("B" * 2, 0, 255))
    with gdal.config_option(
        "GDAL_STATS_USE_INTEGER_STATS", GDAL_STATS_USE_INTEGER_STATS
    ):
        stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    ds = None

    expected_stats = [0.0, 255.0, 4.7205882352941178, 30.576733555893391]
    assert (
        max([abs(stats[i] - expected_stats[i]) for i in range(4)]) <= 1e-15
    ), "did not get expected stats"

    ds = gdal.GetDriverByName("MEM").Create("", 32 + 2, 2)
    ds.GetRasterBand(1).Fill(1)
    ds.GetRasterBand(1).SetNoDataValue(2)
    ds.GetRasterBand(1).WriteRaster(32, 1, 2, 1, struct.pack("B" * 2, 0, 255))
    with gdal.config_option(
        "GDAL_STATS_USE_INTEGER_STATS", GDAL_STATS_USE_INTEGER_STATS
    ):
        stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    ds = None

    expected_stats = [0.0, 255.0, 4.7205882352941178, 30.576733555893391]
    assert (
        max([abs(stats[i] - expected_stats[i]) for i in range(4)]) <= 1e-15
    ), "did not get expected stats"


###############################################################################
# Test stats on uint16


@pytest.mark.parametrize("GDAL_STATS_USE_INTEGER_STATS", [None, "NO"])
def test_stats_uint16(GDAL_STATS_USE_INTEGER_STATS):

    ds = gdal.Translate(
        "/vsimem/stats_uint16_tiled.tif",
        "../gdrivers/data/small_world.tif",
        outputType=gdal.GDT_UInt16,
        scaleParams=[[0, 255, 0, 65535]],
        creationOptions=["TILED=YES", "BLOCKXSIZE=64", "BLOCKYSIZE=64"],
    )
    with gdal.config_option(
        "GDAL_STATS_USE_INTEGER_STATS", GDAL_STATS_USE_INTEGER_STATS
    ):
        stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    ds = None

    gdal.GetDriverByName("GTiff").Delete("/vsimem/stats_uint16_tiled.tif")

    expected_stats = [
        0.0,
        65535.0,
        50.22115 * 65535 / 255,
        67.119029288849973 * 65535 / 255,
    ]
    assert stats == expected_stats, "did not get expected stats"

    ds = gdal.Translate(
        "/vsimem/stats_uint16_untiled.tif",
        "../gdrivers/data/small_world.tif",
        options="-srcwin 0 0 399 200 -scale 0 255 0 65535 -ot UInt16",
    )
    with gdal.config_option(
        "GDAL_STATS_USE_INTEGER_STATS", GDAL_STATS_USE_INTEGER_STATS
    ):
        stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    ds = None

    gdal.GetDriverByName("GTiff").Delete("/vsimem/stats_uint16_untiled.tif")

    expected_stats = [0.0, 65535.0, 12923.9921679198, 17259.703026841547]
    if GDAL_STATS_USE_INTEGER_STATS == "NO":
        assert stats == pytest.approx(expected_stats, rel=1e-14)
    else:
        assert stats == expected_stats, "did not get expected stats"

    # Same but with nodata set but untiled and with non power of 16 block size
    ds = gdal.Translate(
        "/vsimem/stats_uint16_untiled.tif",
        "../gdrivers/data/small_world.tif",
        options="-srcwin 0 0 399 200 -scale 0 255 0 65535 -ot UInt16",
    )
    ds.GetRasterBand(1).SetNoDataValue(0)
    with gdal.config_option(
        "GDAL_STATS_USE_INTEGER_STATS", GDAL_STATS_USE_INTEGER_STATS
    ):
        stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    ds = None

    gdal.GetDriverByName("GTiff").Delete("/vsimem/stats_uint16_untiled.tif")

    expected_stats = [
        257.0,
        65535.0,
        50.378183963744554 * 65535 / 255,
        67.184793517649453 * 65535 / 255,
    ]
    assert stats == pytest.approx(
        expected_stats, rel=1e-10
    ), "did not get expected stats"

    for fill_val in [0, 1, 32767, 32768, 65535]:
        ds = gdal.GetDriverByName("GTiff").Create(
            "/vsimem/stats_uint16_tiled.tif",
            1000,
            512,
            1,
            gdal.GDT_UInt16,
            options=["TILED=YES", "BLOCKXSIZE=512", "BLOCKYSIZE=512"],
        )
        ds.GetRasterBand(1).Fill(fill_val)
        with gdal.config_option(
            "GDAL_STATS_USE_INTEGER_STATS", GDAL_STATS_USE_INTEGER_STATS
        ):
            stats = ds.GetRasterBand(1).GetStatistics(0, 1)
        ds = None
        gdal.Unlink("/vsimem/stats_uint16_tiled.tif")

        expected_stats = [fill_val, fill_val, fill_val, 0.0]
        if max([abs(stats[i] - expected_stats[i]) for i in range(4)]) > 1e-15:
            print(fill_val)
            pytest.fail("did not get expected stats")

    # Test remaining pixels after multiple of 32
    ds = gdal.GetDriverByName("MEM").Create("", 32 + 2, 1, 1, gdal.GDT_UInt16)
    ds.GetRasterBand(1).Fill(1)
    ds.GetRasterBand(1).WriteRaster(32, 0, 2, 1, struct.pack("H" * 2, 0, 65535))
    with gdal.config_option(
        "GDAL_STATS_USE_INTEGER_STATS", GDAL_STATS_USE_INTEGER_STATS
    ):
        stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    ds = None

    expected_stats = [0.0, 65535.0, 1928.4411764705883, 11072.48066469611]
    assert (
        max([abs(stats[i] - expected_stats[i]) for i in range(4)]) <= 1e-15
    ), "did not get expected stats"

    # Non optimized code path
    for fill_val in [0, 1, 32767, 32768, 65535]:
        ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1, gdal.GDT_UInt16)
        ds.GetRasterBand(1).WriteRaster(0, 0, 1, 1, struct.pack("H" * 1, fill_val))
        with gdal.config_option(
            "GDAL_STATS_USE_INTEGER_STATS", GDAL_STATS_USE_INTEGER_STATS
        ):
            stats = ds.GetRasterBand(1).GetStatistics(0, 1)
        ds = None

        expected_stats = [fill_val, fill_val, fill_val, 0.0]
        if max([abs(stats[i] - expected_stats[i]) for i in range(4)]) > 1e-15:
            print(fill_val)
            pytest.fail("did not get expected stats")

    ds = gdal.GetDriverByName("MEM").Create("", 3, 5, 1, gdal.GDT_UInt16)
    ds.GetRasterBand(1).WriteRaster(0, 0, 3, 1, struct.pack("H" * 3, 20, 30, 50))
    ds.GetRasterBand(1).WriteRaster(0, 1, 3, 1, struct.pack("H" * 3, 60, 10, 5))
    ds.GetRasterBand(1).WriteRaster(0, 2, 3, 1, struct.pack("H" * 3, 10, 20, 0))
    ds.GetRasterBand(1).WriteRaster(0, 3, 3, 1, struct.pack("H" * 3, 10, 20, 65535))
    ds.GetRasterBand(1).WriteRaster(0, 4, 3, 1, struct.pack("H" * 3, 10, 20, 10))
    with gdal.config_option(
        "GDAL_STATS_USE_INTEGER_STATS", GDAL_STATS_USE_INTEGER_STATS
    ):
        stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    ds = None

    expected_stats = [0.0, 65535.0, 4387.333333333333, 16342.408927558861]
    assert stats == pytest.approx(expected_stats, rel=1e-15)

    ds = gdal.GetDriverByName("MEM").Create("", 2, 2, 1, gdal.GDT_UInt16)
    ds.GetRasterBand(1).WriteRaster(0, 0, 2, 1, struct.pack("H" * 2, 0, 65535))
    ds.GetRasterBand(1).WriteRaster(0, 1, 2, 1, struct.pack("H" * 2, 1, 65534))
    with gdal.config_option(
        "GDAL_STATS_USE_INTEGER_STATS", GDAL_STATS_USE_INTEGER_STATS
    ):
        stats = ds.GetRasterBand(1).GetStatistics(0, 1)
    ds = None

    expected_stats = [0.0, 65535.0, 32767.5, 32767.000003814814]
    assert stats == pytest.approx(expected_stats, abs=1e-15)


###############################################################################
# Test a case where the nodata value is almost the maximum value of float32


def test_stats_nodata_almost_max_float32():

    gdal.FileFromMemBuffer(
        "/vsimem/float32_almost_nodata_max_float32.tif",
        open("data/float32_almost_nodata_max_float32.tif", "rb").read(),
    )

    ds = gdal.Open("/vsimem/float32_almost_nodata_max_float32.tif")
    minmax = ds.GetRasterBand(1).ComputeRasterMinMax()
    assert minmax == (0, 0), "did not get expected minmax"
    stats = ds.GetRasterBand(1).ComputeStatistics(False)
    assert stats == [0, 0, 0, 0], "did not get expected stats"
    hist = ds.GetRasterBand(1).GetHistogram(approx_ok=0)
    assert hist[0] == 3, "did not get expected hist"
    ds = None

    gdal.GetDriverByName("GTiff").Delete(
        "/vsimem/float32_almost_nodata_max_float32.tif"
    )


###############################################################################
# Test STATISTICS_APPROXIMATE


def test_stats_approx_stats_flag(dt=gdal.GDT_UInt8, struct_frmt="B"):

    ds = gdal.GetDriverByName("MEM").Create("", 2000, 2000, 1, dt)
    ds.GetRasterBand(1).WriteRaster(1000, 1000, 1, 1, struct.pack(struct_frmt * 1, 20))

    approx_ok = 1
    force = 1
    stats = ds.GetRasterBand(1).GetStatistics(approx_ok, force)
    assert stats == [0.0, 0.0, 0.0, 0.0], "did not get expected stats"
    md = ds.GetRasterBand(1).GetMetadata()
    assert md == {
        "STATISTICS_MEAN": "0",
        "STATISTICS_MAXIMUM": "0",
        "STATISTICS_MINIMUM": "0",
        "STATISTICS_APPROXIMATE": "YES",
        "STATISTICS_STDDEV": "0",
        "STATISTICS_VALID_PERCENT": "100",
    }, "did not get expected metadata"

    approx_ok = 0
    force = 0
    stats = ds.GetRasterBand(1).GetStatistics(approx_ok, force)
    assert stats is None

    approx_ok = 0
    force = 1
    stats = ds.GetRasterBand(1).GetStatistics(approx_ok, force)
    assert stats[1] == 20.0, "did not get expected stats"
    md = ds.GetRasterBand(1).GetMetadata()
    assert "STATISTICS_APPROXIMATE" not in md, "did not get expected metadata"


def test_stats_approx_stats_flag_float():
    return test_stats_approx_stats_flag(dt=gdal.GDT_Float32, struct_frmt="f")


def test_stats_all_nodata():

    ds = gdal.GetDriverByName("MEM").Create("", 2000, 2000)
    ds.GetRasterBand(1).SetNoDataValue(0)

    with gdaltest.disable_exceptions(), gdaltest.error_handler():
        minmax = ds.GetRasterBand(1).ComputeRasterMinMax()
        assert math.isnan(minmax[0])
        assert math.isnan(minmax[1])

    with pytest.raises(Exception):
        ds.GetRasterBand(1).ComputeRasterMinMax()

    assert ds.GetRasterBand(1).ComputeRasterMinMax(can_return_none=True) is None

    with gdaltest.disable_exceptions(), gdal.quiet_errors():
        assert ds.GetRasterBand(1).ComputeRasterMinMax(can_return_none=True) is None

    # can_return_null also accepted for similarity with other methods
    assert ds.GetRasterBand(1).ComputeRasterMinMax(can_return_null=True) is None

    approx_ok = 1
    force = 1
    with pytest.raises(Exception):
        ds.GetRasterBand(1).GetStatistics(approx_ok, force)

    ds = gdal.GetDriverByName("MEM").Create("", 2000, 2000, 1, gdal.GDT_Float32)
    ds.GetRasterBand(1).SetNoDataValue(0)
    approx_ok = 1
    force = 1
    with pytest.raises(Exception):
        ds.GetRasterBand(1).GetStatistics(approx_ok, force)


def test_stats_float32_with_nodata_slightly_above_float_max():

    ds = gdal.Open("data/float32_with_nodata_slightly_above_float_max.tif")
    my_min, my_max = ds.GetRasterBand(1).ComputeRasterMinMax()
    assert (my_min, my_max) == (
        -1.0989999771118164,
        0.703338623046875,
    ), "did not get expected stats"


def test_stats_clear():

    filename = "/vsimem/out.tif"
    gdal.Translate(filename, "data/byte.tif")
    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).GetStatistics(False, False) is None
    assert ds.GetRasterBand(1).ComputeStatistics(False) is not None

    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).GetStatistics(False, False) is not None
    ds.ClearStatistics()

    ds = gdal.Open(filename)
    assert ds.GetRasterBand(1).GetStatistics(False, False) is None

    gdal.GetDriverByName("GTiff").Delete(filename)


@pytest.mark.parametrize(
    "datatype,minval,maxval",
    [
        (gdal.GDT_UInt8, 1, 254),
        (gdal.GDT_UInt8, -127, 127),
        (gdal.GDT_UInt16, 1, 65535),
        (gdal.GDT_Int16, -32767, 32766),
        (gdal.GDT_UInt32, 1, (1 << 32) - 2),
        (gdal.GDT_Int32, -(1 << 31) + 1, (1 << 31) - 2),
        (gdal.GDT_UInt64, 1, (1 << 60) - 1),
        (gdal.GDT_Int64, -(1 << 60) - 1, (1 << 60) - 1),
        (
            gdal.GDT_Float32,
            -struct.unpack("f", struct.pack("f", 1e20))[0],
            struct.unpack("f", struct.pack("f", 1e20))[0],
        ),
        (gdal.GDT_Float64, -1e100, 1e100),
        (gdal.GDT_CInt16, -32767, 32766),
        (gdal.GDT_CInt32, -(1 << 31) + 1, (1 << 31) - 2),
        (
            gdal.GDT_CFloat32,
            -struct.unpack("f", struct.pack("f", 1e20))[0],
            struct.unpack("f", struct.pack("f", 1e20))[0],
        ),
        (gdal.GDT_CFloat64, -1e100, 1e100),
    ],
)
@pytest.mark.parametrize("nodata", [None, 0, 1])
def test_stats_computeminmax(datatype, minval, maxval, nodata):
    ds = gdal.GetDriverByName("MEM").Create("", 64, 1, 1, datatype)
    minval_mod = minval
    expected_minval = minval
    if datatype == gdal.GDT_UInt8 and minval < 0:
        ds.GetRasterBand(1).SetMetadataItem(
            "PIXELTYPE", "SIGNEDBYTE", "IMAGE_STRUCTURE"
        )
        minval_mod = 256 + minval
    if nodata:
        ds.GetRasterBand(1).SetNoDataValue(nodata)
        if nodata == 1 and minval == 1:
            expected_minval = maxval
    ds.GetRasterBand(1).WriteRaster(
        0,
        0,
        64,
        1,
        struct.pack("d" * 2, minval_mod, maxval),
        buf_type=gdal.GDT_Float64,
        buf_xsize=2,
        buf_ysize=1,
    )
    if datatype in (gdal.GDT_Int64, gdal.GDT_UInt64):
        assert ds.GetRasterBand(1).ComputeRasterMinMax(0) == pytest.approx(
            (expected_minval, maxval), rel=1e-17
        )
    else:
        assert ds.GetRasterBand(1).ComputeRasterMinMax(0) == (expected_minval, maxval)


###############################################################################
# Test statistics on band with mask band


def test_stats_mask_band():

    src_ds = gdal.GetDriverByName("MEM").Create("", 3, 1, 1, gdal.GDT_Int16)
    src_ds.WriteRaster(0, 0, 3, 1, struct.pack("h" * 3, 1, 2, 3))
    src_ds.CreateMaskBand(gdal.GMF_PER_DATASET)
    mask_band = src_ds.GetRasterBand(1).GetMaskBand()
    mask_band.WriteRaster(0, 0, 3, 1, struct.pack("B" * 3, 0, 255, 255))
    assert src_ds.GetRasterBand(1).ComputeRasterMinMax(False) == (2, 3)
    assert src_ds.GetRasterBand(1).ComputeStatistics(False) == [2, 3, 2.5, 0.5]
    assert src_ds.GetRasterBand(1).GetHistogram(False) == [0, 0, 1, 1] + ([0] * 252)


###############################################################################
# Test statistics on a band with all values to large values

FLT_MAX = struct.unpack("f", struct.pack("f", 3.402823466e38))[0]


@pytest.mark.parametrize("value", [FLT_MAX, -FLT_MAX, float("inf"), -float("inf")])
def test_stats_all_large_values_float32(value):

    src_ds = gdal.GetDriverByName("MEM").Create("", 66, 1, 1, gdal.GDT_Float32)
    src_ds.WriteRaster(0, 0, 66, 1, struct.pack("f", value) * 66)
    assert src_ds.GetRasterBand(1).ComputeRasterMinMax(False) == (value, value)
    assert src_ds.GetRasterBand(1).ComputeStatistics(False) == [value, value, value, 0]


###############################################################################
# Test statistics on a band with all values to large values


@pytest.mark.parametrize(
    "value", [sys.float_info.max, -sys.float_info.max, float("inf"), -float("inf")]
)
def test_stats_all_large_values_float64(value):

    src_ds = gdal.GetDriverByName("MEM").Create("", 66, 1, 1, gdal.GDT_Float64)
    src_ds.WriteRaster(0, 0, 66, 1, struct.pack("d", value) * 66)
    assert src_ds.GetRasterBand(1).ComputeRasterMinMax(False) == (value, value)
    assert src_ds.GetRasterBand(1).ComputeStatistics(False) == [value, value, value, 0]


###############################################################################


def test_stats_int64():

    src_ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 1, gdal.GDT_Int64)
    src_ds.GetRasterBand(1).SetNoDataValue(-(1 << 60) - 1)
    src_ds.WriteRaster(0, 0, 2, 1, struct.pack("q" * 2, 1 << 50, -(1 << 60) - 1))
    assert src_ds.GetRasterBand(1).ComputeRasterMinMax(False) == (1 << 50, 1 << 50)
    assert src_ds.GetRasterBand(1).ComputeStatistics(False) == [
        1 << 50,
        1 << 50,
        1 << 50,
        0,
    ]


###############################################################################


def test_stats_uint64():

    src_ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 1, gdal.GDT_UInt64)
    src_ds.GetRasterBand(1).SetNoDataValue((1 << 60) - 1)
    src_ds.WriteRaster(0, 0, 2, 1, struct.pack("Q" * 2, 1 << 50, (1 << 60) - 1))
    assert src_ds.GetRasterBand(1).ComputeRasterMinMax(False) == (1 << 50, 1 << 50)
    assert src_ds.GetRasterBand(1).ComputeStatistics(False) == [
        1 << 50,
        1 << 50,
        1 << 50,
        0,
    ]


###############################################################################


@pytest.mark.parametrize("GDAL_STATS_USE_FLOAT32_OPTIM", [None, "NO"])
def test_stats_float32_nan(tmp_vsimem, GDAL_STATS_USE_FLOAT32_OPTIM):

    src_ds = gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "tmp.tif", 257, 257, 1, gdal.GDT_Float32, options={"TILED": "YES"}
    )
    src_ds.WriteRaster(1, 127, 1, 1, struct.pack("f", 1.0))
    src_ds.WriteRaster(1, 255, 1, 1, struct.pack("f", float("nan")))
    src_ds.WriteRaster(5, 255, 1, 1, struct.pack("f", float("nan")))
    with gdal.config_option(
        "GDAL_STATS_USE_FLOAT32_OPTIM", GDAL_STATS_USE_FLOAT32_OPTIM
    ):
        got_stats = src_ds.GetRasterBand(1).ComputeStatistics(False)
    expected_stats = [0.0, 1.0, 1.5140733114297485e-05, 0.0038910800393333316]
    if GDAL_STATS_USE_FLOAT32_OPTIM == "NO":
        assert got_stats == expected_stats
    else:
        assert got_stats == pytest.approx(expected_stats, rel=1e-15)


###############################################################################


@pytest.mark.parametrize("GDAL_STATS_USE_FLOAT32_OPTIM", [None, "NO"])
def test_stats_float32_nan_with_nodata(tmp_vsimem, GDAL_STATS_USE_FLOAT32_OPTIM):

    src_ds = gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "tmp.tif", 257, 257, 1, gdal.GDT_Float32, options={"TILED": "YES"}
    )
    src_ds.GetRasterBand(1).Fill(0)
    src_ds.GetRasterBand(1).SetNoDataValue(1.5)
    src_ds.WriteRaster(256, 0, 1, 1, struct.pack("f", 1.5))
    src_ds.WriteRaster(1, 1, 1, 1, struct.pack("f", 1.5))
    src_ds.WriteRaster(256, 1, 1, 1, struct.pack("f", 1.5))
    src_ds.WriteRaster(1, 127, 1, 1, struct.pack("f", 1.0))
    src_ds.WriteRaster(1, 255, 1, 1, struct.pack("f", float("nan")))
    src_ds.WriteRaster(5, 255, 1, 1, struct.pack("f", float("nan")))
    with gdal.config_option(
        "GDAL_STATS_USE_FLOAT32_OPTIM", GDAL_STATS_USE_FLOAT32_OPTIM
    ):
        got_stats = src_ds.GetRasterBand(1).ComputeStatistics(False)
    expected_stats = [0.0, 1.0, 1.514142087093462e-05, 0.0038911684117124336]
    if GDAL_STATS_USE_FLOAT32_OPTIM == "NO":
        assert got_stats == expected_stats
    else:
        assert got_stats == pytest.approx(expected_stats, rel=1e-15)


###############################################################################


@pytest.mark.parametrize("GDAL_STATS_USE_FLOAT32_OPTIM", [None, "NO"])
def test_stats_float32_check_bugfix_13543(tmp_vsimem, GDAL_STATS_USE_FLOAT32_OPTIM):

    gdaltest.importorskip_gdal_array()
    np = pytest.importorskip("numpy")

    nCols = 500
    nRows = 500
    ds = gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "tmp.tif",
        nCols,
        nRows,
        1,
        gdal.GDT_Float32,
        options={"TILED": "YES"},
    )

    # A ramp array
    (x, y) = np.mgrid[:nRows, :nCols]
    ramp = ((x + y) * 100.0 / (nRows - 1 + nCols - 1)).astype(np.float32)

    band = ds.GetRasterBand(1)
    band.WriteArray(ramp)

    with gdal.config_option(
        "GDAL_STATS_USE_FLOAT32_OPTIM", GDAL_STATS_USE_FLOAT32_OPTIM
    ):
        (minVal, maxVal, mean, stddev) = band.ComputeStatistics(approx_ok=False)
    npStd = ramp.std(dtype=np.float64)

    assert npStd == pytest.approx(20.45328026221, rel=1e-12)
    assert stddev == pytest.approx(20.45328026221, rel=1e-12)


###############################################################################


@pytest.mark.parametrize("GDAL_STATS_USE_FLOAT64_OPTIM", [None, "NO"])
def test_stats_float64_nan(tmp_vsimem, GDAL_STATS_USE_FLOAT64_OPTIM):

    src_ds = gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "tmp.tif", 257, 257, 1, gdal.GDT_Float64, options={"TILED": "YES"}
    )
    src_ds.WriteRaster(1, 127, 1, 1, struct.pack("d", 1.0))
    src_ds.WriteRaster(1, 255, 1, 1, struct.pack("d", float("nan")))
    src_ds.WriteRaster(5, 255, 1, 1, struct.pack("d", float("nan")))
    with gdal.config_option(
        "GDAL_STATS_USE_FLOAT64_OPTIM", GDAL_STATS_USE_FLOAT64_OPTIM
    ):
        got_stats = src_ds.GetRasterBand(1).ComputeStatistics(False)
    expected_stats = [0.0, 1.0, 1.5140733114297485e-05, 0.0038910800393333316]
    if GDAL_STATS_USE_FLOAT64_OPTIM == "NO":
        assert got_stats == expected_stats
    else:
        assert got_stats == pytest.approx(expected_stats, rel=1e-15)


###############################################################################


@pytest.mark.parametrize("GDAL_STATS_USE_FLOAT64_OPTIM", [None, "NO"])
def test_stats_float64_nan_with_nodata(tmp_vsimem, GDAL_STATS_USE_FLOAT64_OPTIM):

    src_ds = gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "tmp.tif", 257, 257, 1, gdal.GDT_Float64, options={"TILED": "YES"}
    )
    src_ds.GetRasterBand(1).Fill(0)
    src_ds.GetRasterBand(1).SetNoDataValue(1.5)
    src_ds.WriteRaster(256, 0, 1, 1, struct.pack("d", 1.5))
    src_ds.WriteRaster(1, 1, 1, 1, struct.pack("d", 1.5))
    src_ds.WriteRaster(256, 1, 1, 1, struct.pack("d", 1.5))
    src_ds.WriteRaster(1, 127, 1, 1, struct.pack("d", 1.0))
    src_ds.WriteRaster(1, 255, 1, 1, struct.pack("d", float("nan")))
    src_ds.WriteRaster(5, 255, 1, 1, struct.pack("d", float("nan")))
    with gdal.config_option(
        "GDAL_STATS_USE_FLOAT64_OPTIM", GDAL_STATS_USE_FLOAT64_OPTIM
    ):
        got_stats = src_ds.GetRasterBand(1).ComputeStatistics(False)
    expected_stats = [0.0, 1.0, 1.514142087093462e-05, 0.0038911684117124336]
    if GDAL_STATS_USE_FLOAT64_OPTIM == "NO":
        assert got_stats == expected_stats
    else:
        assert got_stats == pytest.approx(expected_stats, rel=1e-15)


###############################################################################


@pytest.mark.parametrize("GDAL_STATS_USE_FLOAT64_OPTIM", [None, "NO"])
def test_stats_float64_check_bugfix_13543(tmp_vsimem, GDAL_STATS_USE_FLOAT64_OPTIM):

    gdaltest.importorskip_gdal_array()
    np = pytest.importorskip("numpy")

    nCols = 500
    nRows = 500
    ds = gdal.GetDriverByName("GTiff").Create(
        tmp_vsimem / "tmp.tif",
        nCols,
        nRows,
        1,
        gdal.GDT_Float64,
        options={"TILED": "YES"},
    )

    # A ramp array
    (x, y) = np.mgrid[:nRows, :nCols]
    ramp = ((x + y) * 100.0 / (nRows - 1 + nCols - 1)).astype(np.float64)

    band = ds.GetRasterBand(1)
    band.WriteArray(ramp)

    with gdal.config_option(
        "GDAL_STATS_USE_FLOAT64_OPTIM", GDAL_STATS_USE_FLOAT64_OPTIM
    ):
        (minVal, maxVal, mean, stddev) = band.ComputeStatistics(approx_ok=False)
    npStd = ramp.std()

    assert npStd == pytest.approx(20.453280258841, rel=1e-14)
    assert stddev == pytest.approx(20.453280258841, rel=1e-14)


###############################################################################


@pytest.mark.parametrize(
    "datatype",
    [
        gdal.GDT_UInt8,
        gdal.GDT_Int8,
        gdal.GDT_UInt16,
        gdal.GDT_Int16,
        gdal.GDT_UInt32,
        gdal.GDT_Int32,
        gdal.GDT_UInt64,
        gdal.GDT_Int64,
        gdal.GDT_Float16,
        gdal.GDT_Float32,
        gdal.GDT_Float64,
    ],
)
def test_stats_minmax_one_two(datatype):

    ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 1, datatype)
    ds.GetRasterBand(1).WriteRaster(0, 0, 2, 1, b"\x01\x02", buf_type=gdal.GDT_UInt8)
    assert ds.GetRasterBand(1).ComputeRasterMinMax() == (1.0, 2.0)


###############################################################################


@pytest.mark.parametrize(
    "datatype",
    [
        gdal.GDT_UInt8,
        gdal.GDT_Int8,
        gdal.GDT_UInt16,
        gdal.GDT_Int16,
        gdal.GDT_UInt32,
        gdal.GDT_Int32,
        gdal.GDT_UInt64,
        gdal.GDT_Int64,
        gdal.GDT_Float16,
        gdal.GDT_Float32,
        gdal.GDT_Float64,
    ],
)
def test_stats_minmax_all_invalid_nodata(datatype):

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1, datatype)
    ds.GetRasterBand(1).SetNoDataValue(0)
    with pytest.raises(Exception, match="Failed to compute min/max"):
        ds.GetRasterBand(1).ComputeRasterMinMax()


###############################################################################


@pytest.mark.parametrize(
    "datatype",
    [
        gdal.GDT_UInt8,
        gdal.GDT_Int8,
        gdal.GDT_UInt16,
        gdal.GDT_Int16,
        gdal.GDT_UInt32,
        gdal.GDT_Int32,
        gdal.GDT_UInt64,
        gdal.GDT_Int64,
        gdal.GDT_Float16,
        gdal.GDT_Float32,
        gdal.GDT_Float64,
    ],
)
def test_stats_minmax_all_invalid_mask(datatype):

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1, datatype)
    ds.CreateMaskBand(gdal.GMF_PER_DATASET)
    with pytest.raises(Exception, match="Failed to compute min/max"):
        ds.GetRasterBand(1).ComputeRasterMinMax()


###############################################################################


def test_stats_GetInterBandCovarianceMatrix(tmp_vsimem):

    gdal.FileFromMemBuffer(
        tmp_vsimem / "test.tif", open("data/rgbsmall.tif", "rb").read()
    )

    with gdal.Open(tmp_vsimem / "test.tif") as ds:
        assert ds.GetInterBandCovarianceMatrix() is None
    assert gdal.VSIStatL(tmp_vsimem / "test.tif.aux.xml") is None

    expected_cov_matrix = [
        [2241.7045363745387, 2898.8196128051163, 1009.979953581434],
        [2898.8196128051163, 3900.269159023618, 1248.65396718687],
        [1009.979953581434, 1248.65396718687, 602.4703641456648],
    ]

    with gdal.Open(tmp_vsimem / "test.tif") as ds:
        assert list(
            chain.from_iterable(ds.GetInterBandCovarianceMatrix(force=True))
        ) == pytest.approx(list(chain.from_iterable(expected_cov_matrix)))
    assert gdal.VSIStatL(tmp_vsimem / "test.tif.aux.xml") is not None

    with gdal.Open(tmp_vsimem / "test.tif") as ds:
        assert list(
            chain.from_iterable(ds.GetInterBandCovarianceMatrix())
        ) == pytest.approx(list(chain.from_iterable(expected_cov_matrix)))

    gdal.Unlink(tmp_vsimem / "test.tif.aux.xml")

    tab_pct = [0]

    def my_progress(pct, msg, user_data):
        assert pct >= tab_pct[0]
        tab_pct[0] = pct
        return True

    with gdal.Open(tmp_vsimem / "test.tif") as ds:
        assert list(
            chain.from_iterable(
                ds.GetInterBandCovarianceMatrix(
                    force=True, write_into_metadata=False, callback=my_progress
                )
            )
        ) == pytest.approx(list(chain.from_iterable(expected_cov_matrix)))

        assert tab_pct[0] == 1.0

        res = ds.GetInterBandCovarianceMatrix(
            band_list=[2], force=True, write_into_metadata=False
        )
        assert res[0][0] == pytest.approx(expected_cov_matrix[1][1])

    assert gdal.VSIStatL(tmp_vsimem / "test.tif.aux.xml") is None


###############################################################################


def test_stats_GetInterBandCovarianceMatrix_edge_cases():

    ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0)
    assert ds.GetInterBandCovarianceMatrix(force=True) == []


###############################################################################


def test_stats_GetInterBandCovarianceMatrix_errors_on_band_list():

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 2)
    with pytest.raises(Exception, match=" invalid value"):
        ds.GetInterBandCovarianceMatrix(band_list=[0])
    with pytest.raises(Exception, match="invalid value"):
        ds.GetInterBandCovarianceMatrix(band_list=[3])
    with pytest.raises(Exception, match="nBandCount > nBands"):
        ds.GetInterBandCovarianceMatrix(band_list=[1, 1, 1])
    with pytest.raises(Exception, match="cannot write STATISTICS_COVARIANCES metadata"):
        ds.GetInterBandCovarianceMatrix(
            band_list=[1], force=True, write_into_metadata=True
        )
    with pytest.raises(Exception, match="cannot write STATISTICS_COVARIANCES metadata"):
        ds.GetInterBandCovarianceMatrix(
            band_list=[2, 1], force=True, write_into_metadata=True
        )


###############################################################################


def test_stats_ComputeInterBandCovarianceMatrix(tmp_vsimem):

    gdal.FileFromMemBuffer(
        tmp_vsimem / "test.tif", open("data/rgbsmall.tif", "rb").read()
    )

    expected_cov_matrix = [
        [2241.7045363745387, 2898.8196128051163, 1009.979953581434],
        [2898.8196128051163, 3900.269159023618, 1248.65396718687],
        [1009.979953581434, 1248.65396718687, 602.4703641456648],
    ]

    with gdal.Open(tmp_vsimem / "test.tif") as ds:
        assert list(
            chain.from_iterable(ds.ComputeInterBandCovarianceMatrix())
        ) == pytest.approx(list(chain.from_iterable(expected_cov_matrix)))

        # Check consistency with numpy.cov()
        try:
            import numpy

            from osgeo import gdal_array  # noqa

            has_numpy = True
        except ImportError:
            has_numpy = False
        if has_numpy:
            numpy_cov = numpy.cov(
                [
                    ds.GetRasterBand(n + 1).ReadAsArray().ravel()
                    for n in range(ds.RasterCount)
                ]
            )
            assert list(chain.from_iterable(numpy_cov)) == pytest.approx(
                list(chain.from_iterable(expected_cov_matrix))
            )

    assert gdal.VSIStatL(tmp_vsimem / "test.tif.aux.xml") is not None

    with gdal.Open(tmp_vsimem / "test.tif") as ds:
        assert list(
            chain.from_iterable(ds.GetInterBandCovarianceMatrix())
        ) == pytest.approx(list(chain.from_iterable(expected_cov_matrix)))

    tab_pct = [0]

    def my_progress(pct, msg, user_data):
        assert pct >= tab_pct[0]
        tab_pct[0] = pct
        return True

    gdal.Unlink(tmp_vsimem / "test.tif.aux.xml")

    with gdal.Open(tmp_vsimem / "test.tif") as ds:
        assert list(
            chain.from_iterable(
                ds.ComputeInterBandCovarianceMatrix(
                    write_into_metadata=False, callback=my_progress
                )
            )
        ) == pytest.approx(list(chain.from_iterable(expected_cov_matrix)))

        assert tab_pct[0] == 1.0

        res = ds.ComputeInterBandCovarianceMatrix(
            band_list=[2], write_into_metadata=False
        )
        assert res[0][0] == pytest.approx(expected_cov_matrix[1][1])

    assert gdal.VSIStatL(tmp_vsimem / "test.tif.aux.xml") is None


###############################################################################


def test_stats_ComputeInterBandCovarianceMatrix_overviews(tmp_vsimem):

    ds = gdal.Translate(tmp_vsimem / "test.tif", "data/rgbsmall.tif", width=1024)
    ds.BuildOverviews("NEAR", [2])
    ds.Close()

    expected_cov_matrix = [
        [2241.747462182087, 2898.723016609931, 1010.1568325853924],
        [2898.723016609931, 3900.2599077296286, 1248.6309543003213],
        [1010.1568325853924, 1248.6309543003213, 601.9344108053141],
    ]

    with gdal.Open(tmp_vsimem / "test.tif") as ds:
        cov_matrix = ds.ComputeInterBandCovarianceMatrix(approx_ok=True)
        assert list(chain.from_iterable(cov_matrix)) == pytest.approx(
            list(chain.from_iterable(expected_cov_matrix))
        )


###############################################################################


def test_stats_ComputeInterBandCovarianceMatrix_edge_cases():

    ds = gdal.GetDriverByName("MEM").Create("", 0, 0, 0)
    assert ds.ComputeInterBandCovarianceMatrix() == []


###############################################################################


def test_stats_ComputeInterBandCovarianceMatrix_errors_on_band_list():

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 2)
    with pytest.raises(Exception, match=" invalid value"):
        ds.ComputeInterBandCovarianceMatrix(band_list=[0])
    with pytest.raises(Exception, match="invalid value"):
        ds.ComputeInterBandCovarianceMatrix(band_list=[3])
    with pytest.raises(Exception, match="nBandCount > nBands"):
        ds.ComputeInterBandCovarianceMatrix(band_list=[1, 1, 1])
    with pytest.raises(Exception, match="cannot write STATISTICS_COVARIANCES metadata"):
        ds.ComputeInterBandCovarianceMatrix(band_list=[1], write_into_metadata=True)
    with pytest.raises(Exception, match="cannot write STATISTICS_COVARIANCES metadata"):
        ds.ComputeInterBandCovarianceMatrix(band_list=[2, 1], write_into_metadata=True)


###############################################################################


def test_stats_ComputeInterBandCovarianceMatrix_nodata():

    ds = gdal.GetDriverByName("MEM").Create("", 4, 1, 2)
    ds.GetRasterBand(1).WriteRaster(0, 0, 4, 1, b"\x01\x02\x03\xFF")
    ds.GetRasterBand(1).SetNoDataValue(255)
    ds.GetRasterBand(2).WriteRaster(0, 0, 4, 1, b"\x02\x01\xFE\x03")
    ds.GetRasterBand(2).SetNoDataValue(254)

    expected_cov_matrix = [[1, 0], [0, 1]]

    cov_matrix = ds.ComputeInterBandCovarianceMatrix()
    assert list(chain.from_iterable(cov_matrix)) == pytest.approx(
        list(chain.from_iterable(expected_cov_matrix))
    )


###############################################################################


def test_stats_ComputeInterBandCovarianceMatrix_nan_value():

    ds = gdal.GetDriverByName("MEM").Create("", 4, 1, 2, gdal.GDT_Float32)
    ds.GetRasterBand(1).WriteRaster(0, 0, 4, 1, struct.pack("f" * 4, 1, 2, 3, math.nan))
    ds.GetRasterBand(2).WriteRaster(0, 0, 4, 1, struct.pack("f" * 4, 2, 1, math.nan, 3))

    expected_cov_matrix = [[1, 0], [0, 1]]

    cov_matrix = ds.ComputeInterBandCovarianceMatrix()
    assert list(chain.from_iterable(cov_matrix)) == pytest.approx(
        list(chain.from_iterable(expected_cov_matrix))
    )


###############################################################################


def test_stats_ComputeInterBandCovarianceMatrix_nan_result():

    ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 2, gdal.GDT_Float32)
    ds.GetRasterBand(1).WriteRaster(0, 0, 2, 1, struct.pack("f" * 2, 1, math.nan))
    ds.GetRasterBand(2).WriteRaster(0, 0, 2, 1, struct.pack("f" * 2, math.nan, 1))

    cov_matrix = ds.ComputeInterBandCovarianceMatrix()
    assert math.isnan(cov_matrix[0][0])
    assert math.isnan(cov_matrix[0][1])
    assert math.isnan(cov_matrix[1][0])
    assert math.isnan(cov_matrix[1][1])


###############################################################################


def test_stats_ComputeInterBandCovarianceMatrix_whole_block_nodata():

    ds = gdal.GetDriverByName("MEM").Create("", 2, 3, 2, gdal.GDT_Float32)
    ds.GetRasterBand(1).WriteRaster(
        0, 0, 2, 1, struct.pack("f" * 2, math.nan, math.nan)
    )
    ds.GetRasterBand(1).WriteRaster(0, 1, 2, 1, struct.pack("f" * 2, 0, 1))
    ds.GetRasterBand(1).WriteRaster(
        0, 2, 2, 1, struct.pack("f" * 2, math.nan, math.nan)
    )
    ds.GetRasterBand(2).WriteRaster(
        0, 0, 2, 1, struct.pack("f" * 2, math.nan, math.nan)
    )
    ds.GetRasterBand(2).WriteRaster(0, 1, 2, 1, struct.pack("f" * 2, 1, 0))
    ds.GetRasterBand(2).WriteRaster(
        0, 2, 2, 1, struct.pack("f" * 2, math.nan, math.nan)
    )

    expected_cov_matrix = [[0.5, -0.5], [-0.5, 0.5]]

    cov_matrix = ds.ComputeInterBandCovarianceMatrix()
    assert list(chain.from_iterable(cov_matrix)) == pytest.approx(
        list(chain.from_iterable(expected_cov_matrix))
    )


###############################################################################


def test_stats_ComputeInterBandCovarianceMatrix_failed_to_compute_stats():

    ds = gdal.GetDriverByName("MEM").Create("", 2, 1, 1, gdal.GDT_Float32)
    ds.GetRasterBand(1).WriteRaster(
        0, 0, 2, 1, struct.pack("f" * 2, math.nan, math.nan)
    )

    cov_matrix = ds.ComputeInterBandCovarianceMatrix()
    assert math.isnan(cov_matrix[0][0])


###############################################################################


def test_stats_ComputeInterBandCovarianceMatrix_mask_band():

    ds = gdal.GetDriverByName("MEM").Create("", 4, 1, 2)
    ds.GetRasterBand(1).WriteRaster(0, 0, 4, 1, b"\x01\x02\x03\xFF")
    ds.GetRasterBand(1).CreateMaskBand(0)
    ds.GetRasterBand(1).GetMaskBand().WriteRaster(0, 0, 4, 1, b"\xFF\xFF\xFF\x00")
    ds.GetRasterBand(2).WriteRaster(0, 0, 4, 1, b"\x02\x01\xFE\x03")
    ds.GetRasterBand(2).CreateMaskBand(0)
    ds.GetRasterBand(2).GetMaskBand().WriteRaster(0, 0, 4, 1, b"\xFF\xFF\x00\xFF")

    expected_cov_matrix = [[1, 0], [0, 1]]

    cov_matrix = ds.ComputeInterBandCovarianceMatrix()
    assert list(chain.from_iterable(cov_matrix)) == pytest.approx(
        list(chain.from_iterable(expected_cov_matrix))
    )


###############################################################################


def test_stats_ComputeInterBandCovarianceMatrix_all_bands_same_mask():

    ds = gdal.GetDriverByName("MEM").Create("", 4, 1, 2)
    ds.CreateMaskBand(gdal.GMF_PER_DATASET)
    ds.GetRasterBand(1).WriteRaster(0, 0, 4, 1, b"\x01\x02\x03\xFF")
    ds.GetRasterBand(2).WriteRaster(0, 0, 4, 1, b"\x02\x01\xFE\x03")
    ds.GetRasterBand(1).GetMaskBand().WriteRaster(0, 0, 4, 1, b"\xFF\xFF\x00\x00")

    expected_cov_matrix = [[0.5, -0.5], [-0.5, 0.5]]

    cov_matrix = ds.ComputeInterBandCovarianceMatrix()
    assert list(chain.from_iterable(cov_matrix)) == pytest.approx(
        list(chain.from_iterable(expected_cov_matrix))
    )


###############################################################################


def test_stats_ComputeInterBandCovarianceMatrix_interrupt():

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1000)

    def my_progress(pct, msg, user_data):
        return pct < 1e-2

    with pytest.raises(Exception, match="User terminated"):
        ds.ComputeInterBandCovarianceMatrix(
            write_into_metadata=False, callback=my_progress
        )

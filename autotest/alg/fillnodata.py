#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  GDALFillNoData() testing
# Author:   Even Rouault <even.rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2019, Even Rouault <even.rouault @ spatialys.com>
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

import struct

import pytest

from osgeo import gdal


def test_fillnodata_1x1_no_nodata():

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    ar = struct.pack("B" * 1, 1)
    ds.WriteRaster(0, 0, ds.RasterXSize, ds.RasterYSize, ar)
    gdal.FillNodata(
        targetBand=ds.GetRasterBand(1),
        maxSearchDist=1,
        maskBand=None,
        smoothingIterations=0,
    )
    ar = ds.ReadRaster()
    ar = struct.unpack("B" * 1, ar)
    assert ar == (1,)


fillnodata_tests = {
    # (width, height, input_ar, maxSearchDist, expected, smoothingIterations):
    "1x1_nodata_but_pixel_valid": (1, 1, (1,), 1, (1,), 0),
    "1x1_nodata_pixel_invalid": (1, 1, (0,), 1, (0,), 0),
    "2x1_valid_invalid": (2, 1, (1, 0), 1, (1, 1), 0),
    "2x1_invalid_valid": (2, 1, (0, 1), 1, (1, 1), 0),
    "3x1_valid_invalid_valid": (3, 1, (2, 0, 4), 1, (2, 3, 4), 0),
    "4x1_valid_invalid_invalid_valid": (4, 1, (2, 0, 0, 4), 1, (2, 2, 4, 4), 0),
    "1x2_valid_invalid": (1, 2, (1, 0), 1, (1, 1), 0),
    "1x2_invalid_valid": (1, 2, (0, 1), 1, (1, 1), 0),
    "1x3_valid_invalid_valid": (1, 3, (2, 0, 4), 1, (2, 3, 4), 0),
    "1x4_valid_invalid_invalid_valid": (1, 4, (2, 0, 0, 4), 1, (2, 2, 4, 4), 0),
    "3x3_central_column_invalid": (
        3,
        3,
        (2, 0, 4, 4, 0, 6, 6, 0, 8),
        1,
        (2, 3, 4, 4, 5, 6, 6, 7, 8),
        0,
    ),
    "3x3_central_line_invalid": (
        3,
        3,
        (2, 3, 4, 0, 0, 0, 6, 7, 8),
        1,
        (2, 3, 4, 4, 5, 6, 6, 7, 8),
        0,
    ),
    "3x3_central_column_and_line_invalid": (
        3,
        3,
        (2, 0, 4, 0, 0, 0, 6, 0, 8),
        1,
        (2, 3, 4, 4, 0, 6, 6, 7, 8),
        0,
    ),
    # 1.5 > sqrt(2)
    "3x3_central_column_and_line_invalid_search_dist_1_5": (
        3,
        3,
        (2, 0, 4, 0, 0, 0, 6, 0, 8),
        1.5,
        (2, 3, 4, 4, 5, 6, 6, 7, 8),
        0,
    ),
    "4x4": (
        4,
        4,
        (20, 30, 40, 50, 30, 0, 0, 60, 40, 0, 0, 70, 50, 60, 70, 80),
        1,
        (20, 30, 40, 50, 30, 30, 50, 60, 40, 50, 70, 70, 50, 60, 70, 80),
        0,
    ),
    "4x4_smooth_1": (
        4,
        4,
        (20, 30, 40, 50, 30, 0, 0, 60, 40, 0, 0, 70, 50, 60, 70, 80),
        1,
        (20, 30, 40, 50, 30, 40, 50, 60, 40, 50, 60, 70, 50, 60, 70, 80),
        1,
    ),
}


@pytest.mark.parametrize(
    "width, height, input_ar, maxSearchDist, expected, smoothingIterations",
    fillnodata_tests.values(),
    ids=fillnodata_tests.keys(),
)
def test_fillnodata_nodata(
    width, height, input_ar, maxSearchDist, expected, smoothingIterations
):
    ds = gdal.GetDriverByName("MEM").Create("", width, height)
    npixels = ds.RasterXSize * ds.RasterYSize
    ds.GetRasterBand(1).SetNoDataValue(0)
    ar = struct.pack("B", input_ar[0])
    for i in range(1, len(input_ar)):
        ar += struct.pack("B", input_ar[i])
    ds.WriteRaster(0, 0, ds.RasterXSize, ds.RasterYSize, ar)
    gdal.FillNodata(
        targetBand=ds.GetRasterBand(1),
        maxSearchDist=maxSearchDist,
        maskBand=None,
        smoothingIterations=smoothingIterations,
    )
    ar = ds.ReadRaster()
    assert struct.unpack("B" * npixels, ar) == expected


def test_fillnodata_user_provided_mask_with_smoothing():

    ar = struct.pack(
        "f" * (5 * 5),
        5,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        10,
    )

    ds = gdal.GetDriverByName("MEM").Create("", 5, 5, 1, gdal.GDT_Float32)
    targetBand = ds.GetRasterBand(1)
    targetBand.WriteRaster(0, 0, ds.RasterXSize, ds.RasterYSize, ar)

    mask_ds = gdal.GetDriverByName("MEM").Create("", 5, 5)
    mask_ar = struct.pack(
        "B" * (5 * 5),
        255,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        255,
    )
    mask_ds.WriteRaster(0, 0, ds.RasterXSize, ds.RasterYSize, mask_ar)

    expected = [
        5.0,
        5.833333492279053,
        6.5450849533081055,
        7.105823040008545,
        8.333333015441895,
        6.325798988342285,
        6.566854476928711,
        7.038447856903076,
        7.557196140289307,
        7.811311721801758,
        7.0352678298950195,
        7.2065935134887695,
        7.561786651611328,
        7.926154613494873,
        8.114609718322754,
        7.453090190887451,
        7.642454147338867,
        8.04526424407959,
        8.50459098815918,
        8.746294975280762,
        7.5,
        7.894176959991455,
        8.454915046691895,
        9.166666984558105,
        10.0,
    ]

    gdal.FillNodata(
        targetBand=targetBand,
        maskBand=mask_ds.GetRasterBand(1),
        maxSearchDist=100,
        smoothingIterations=10,
    )
    got = [x for x in struct.unpack("f" * (5 * 5), targetBand.ReadRaster())]
    assert got == pytest.approx(expected, 1e-5)

    # Check with get the same result with mask band not explicitly set, and thus
    # defaulting to the implicit mask band with nodata==0

    ds = gdal.GetDriverByName("MEM").Create("", 5, 5, 1, gdal.GDT_Float32)
    targetBand = ds.GetRasterBand(1)
    targetBand.SetNoDataValue(0)
    targetBand.WriteRaster(0, 0, ds.RasterXSize, ds.RasterYSize, ar)

    gdal.FillNodata(
        targetBand=targetBand, maskBand=None, maxSearchDist=100, smoothingIterations=10
    )
    got = [x for x in struct.unpack("f" * (5 * 5), targetBand.ReadRaster())]
    assert got == pytest.approx(expected, 1e-5)

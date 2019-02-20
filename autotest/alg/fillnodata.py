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

from osgeo import gdal

import struct

def test_fillnodata_1x1_no_nodata():

    ds = gdal.GetDriverByName('MEM').Create('', 1, 1)
    ar = struct.pack('B' * 1, 1)
    ds.WriteRaster(0, 0, ds.RasterXSize, ds.RasterYSize, ar)
    gdal.FillNodata(targetBand = ds.GetRasterBand(1),
                    maxSearchDist = 1, maskBand = None, smoothingIterations = 0)
    ar = ds.ReadRaster()
    ar = struct.unpack('B' * 1, ar)
    assert ar == (1,)


def _test_fillnodata_nodata_zero(width, height, input_ar, maxSearchDist = 1):
    ds = gdal.GetDriverByName('MEM').Create('', width, height)
    npixels = ds.RasterXSize * ds.RasterYSize
    ds.GetRasterBand(1).SetNoDataValue(0)
    ar = struct.pack('B', input_ar[0])
    for i in range(1, len(input_ar)):
        ar += struct.pack('B', input_ar[i])
    ds.WriteRaster(0, 0, ds.RasterXSize, ds.RasterYSize, ar)
    gdal.FillNodata(targetBand = ds.GetRasterBand(1),
                    maxSearchDist = maxSearchDist,
                    maskBand = None, smoothingIterations = 0)
    ar = ds.ReadRaster()
    return struct.unpack('B' * npixels, ar)


def test_fillnodata_1x1_nodata_but_pixel_valid():

    assert _test_fillnodata_nodata_zero(1, 1, (1,)) == (1,)


def test_fillnodata_1x1_nodata_pixel_invalid():

    assert _test_fillnodata_nodata_zero(1, 1, (0,)) == (0,)


def test_fillnodata_2x1_valid_invalid():

    assert _test_fillnodata_nodata_zero(2, 1, (1, 0)) == (1, 1)


def test_fillnodata_2x1_invalid_valid():

    assert _test_fillnodata_nodata_zero(2, 1, (0, 1)) == (1, 1)


def test_fillnodata_3x1_valid_invalid_valid():

    assert _test_fillnodata_nodata_zero(3, 1, (2, 0, 4)) == (2, 3, 4)


def test_fillnodata_4x1_valid_invalid_invalid_valid():

    assert _test_fillnodata_nodata_zero(4, 1, (2, 0, 0, 4)) == (2, 2, 4, 4)


def test_fillnodata_1x2_valid_invalid():

    assert _test_fillnodata_nodata_zero(1, 2, (1,
                                               0)) == (1,
                                                       1)


def test_fillnodata_1x2_invalid_valid():

    assert _test_fillnodata_nodata_zero(1, 2, (0,
                                               1)) == (1,
                                                       1)

def test_fillnodata_1x3_valid_invalid_valid():

    assert _test_fillnodata_nodata_zero(1, 3, (2,
                                               0,
                                               4)) == (2,
                                                       3,
                                                       4)

def test_fillnodata_1x4_valid_invalid_invalid_valid():

    assert _test_fillnodata_nodata_zero(1, 4, (2,
                                               0,
                                               0,
                                               4)) == (2,
                                                       2,
                                                       4,
                                                       4)


def test_fillnodata_3x3_central_column_invalid():

    assert _test_fillnodata_nodata_zero(3, 3, (2, 0, 4,
                                               4, 0, 6,
                                               6, 0, 8)) == (2, 3, 4,
                                                             4, 5, 6,
                                                             6, 7, 8)


def test_fillnodata_3x3_central_line_invalid():

    assert _test_fillnodata_nodata_zero(3, 3, (2, 3, 4,
                                               0, 0, 0,
                                               6, 7, 8)) == (2, 3, 4,
                                                             4, 5, 6,
                                                             6, 7, 8)

def test_fillnodata_3x3_central_column_and_line_invalid():

    assert _test_fillnodata_nodata_zero(3, 3, (2, 0, 4,
                                               0, 0, 0,
                                               6, 0, 8)) == (2, 3, 4,
                                                             4, 0, 6,
                                                             6, 7, 8)

# 1.5 > sqrt(2)
def test_fillnodata_3x3_central_column_and_line_invalid_search_dist_1_5():

    assert _test_fillnodata_nodata_zero(3, 3, (2, 0, 4,
                                               0, 0, 0,
                                               6, 0, 8), maxSearchDist = 1.5) \
                == (2, 3, 4,
                    4, 5, 6,
                    6, 7, 8)

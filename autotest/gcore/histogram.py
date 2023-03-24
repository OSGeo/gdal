#!/usr/bin/env pytest
###############################################################################
# $Id: colortable.py 11065 2007-03-24 09:35:32Z mloskot $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GetHistogram() and GetDefaultHistogram() handling.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009, Even Rouault <even dot rouault at spatialys.com>
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

import math
import os
import shutil
import struct
import sys

import gdaltest
import pytest

from osgeo import gdal


###############################################################################
@pytest.fixture(autouse=True, scope="module")
def module_disable_exceptions():
    with gdaltest.disable_exceptions():
        yield


###############################################################################
# Fetch simple histogram.


def test_histogram_1():

    ds = gdal.Open("data/utmsmall.tif")
    hist = ds.GetRasterBand(1).GetHistogram()

    exp_hist = [
        2,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        8,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        23,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        29,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        46,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        69,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        99,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        120,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        178,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        193,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        212,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        281,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        365,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        460,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        533,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        544,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        626,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        653,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        673,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        629,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        586,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        541,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        435,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        348,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        341,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        284,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        225,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        237,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        172,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        159,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        105,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        824,
    ]

    assert hist == exp_hist, "did not get expected histogram."


###############################################################################
# Fetch histogram with specified sampling, using keywords.


def test_histogram_2():

    ds = gdal.Open("data/utmsmall.tif")
    hist = ds.GetRasterBand(1).GetHistogram(buckets=16, max=255.5, min=-0.5)

    exp_hist = [
        10,
        52,
        115,
        219,
        371,
        493,
        825,
        1077,
        1279,
        1302,
        1127,
        783,
        625,
        462,
        331,
        929,
    ]

    assert hist == exp_hist, "did not get expected histogram."


###############################################################################
# try on a different data type with out of range values included.


@pytest.mark.require_driver("AAIGRID")
def test_histogram_3():

    ds = gdal.Open("data/int32_withneg.grd")
    hist = ds.GetRasterBand(1).GetHistogram(
        buckets=21, max=100, min=-100, include_out_of_range=1, approx_ok=0
    )

    exp_hist = [0, 0, 0, 0, 0, 1, 0, 1, 1, 3, 3, 2, 0, 5, 3, 4, 0, 1, 1, 2, 3]

    assert hist == exp_hist, "did not get expected histogram."


###############################################################################
# try on a different data type without out of range values included.


@pytest.mark.require_driver("AAIGRID")
def test_histogram_4():

    ds = gdal.Open("data/int32_withneg.grd")
    hist = ds.GetRasterBand(1).GetHistogram(
        buckets=21, max=100, min=-100, include_out_of_range=0, approx_ok=0
    )

    exp_hist = [0, 0, 0, 0, 0, 1, 0, 1, 1, 3, 3, 2, 0, 5, 3, 4, 0, 1, 1, 2, 0]

    assert hist == exp_hist, "did not get expected histogram."

    ds = None

    gdal.Unlink("data/int32_withneg.grd.aux.xml")


###############################################################################
# Test GetDefaultHistogram() on the file.


def test_histogram_5():

    ds = gdal.Open("data/utmsmall.tif")
    hist = ds.GetRasterBand(1).GetDefaultHistogram(force=1)

    exp_hist = (
        -0.5,
        255.5,
        256,
        [
            2,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            8,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            23,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            29,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            46,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            69,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            99,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            120,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            178,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            193,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            212,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            281,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            365,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            460,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            533,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            544,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            626,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            653,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            673,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            629,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            586,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            541,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            435,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            348,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            341,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            284,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            225,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            237,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            172,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            159,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            105,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            824,
        ],
    )

    assert hist == exp_hist, "did not get expected histogram."

    ds = None

    gdal.Unlink("data/utmsmall.tif.aux.xml")


###############################################################################
# Test GetDefaultHistogram( force = 0 ) on a JPG file (#3304)


@pytest.mark.require_driver("JPEG")
def test_histogram_6():

    shutil.copy("../gdrivers/data/jpeg/albania.jpg", "tmp/albania.jpg")
    ds = gdal.Open("tmp/albania.jpg")
    hist = ds.GetRasterBand(1).GetDefaultHistogram(force=0)
    assert hist is None, "did not get expected histogram."
    ds = None
    os.unlink("tmp/albania.jpg")


###############################################################################
# Test GetHistogram() with infinity values


def test_histogram_inf_values():

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1, gdal.GDT_Float64)
    ds.WriteRaster(0, 0, 1, 1, struct.pack("d", math.inf))
    hist = ds.GetRasterBand(1).GetHistogram(
        buckets=2, min=-0.5, max=1.5, include_out_of_range=1, approx_ok=0
    )
    assert hist == [0, 1]

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1, gdal.GDT_Float64)
    ds.WriteRaster(0, 0, 1, 1, struct.pack("d", -math.inf))
    hist = ds.GetRasterBand(1).GetHistogram(
        buckets=2, min=-0.5, max=1.5, include_out_of_range=1, approx_ok=0
    )
    assert hist == [1, 0]


###############################################################################
# Test GetHistogram() error


def test_histogram_errors():

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    # Invalid bucket count
    with pytest.raises(Exception):
        ds.GetRasterBand(1).GetHistogram(
            buckets=0, min=-0.5, max=1.5, include_out_of_range=1, approx_ok=0
        )


###############################################################################
# Test GetHistogram() invalid min max bounds


@pytest.mark.parametrize(
    "min,max",
    [
        [math.nan, 1.5],
        [-math.inf, 1.5],
        [math.inf, 1.5],
        [-0.5, math.nan],
        [-0.5, -math.inf],
        [-0.5, math.inf],
        [-math.inf, math.inf],
        [-sys.float_info.max, sys.float_info.max],  # leads to dfScale == 0
        [0, 1e-309],  # leads to dfScale == inf
        [0, 0],
    ],
)
def test_histogram_invalid_min_max(min, max):

    ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    with gdaltest.error_handler():
        gdal.ErrorReset()
        ret = ds.GetRasterBand(1).GetHistogram(
            buckets=2, min=min, max=max, include_out_of_range=1, approx_ok=0
        )
        if (min, max) == (-sys.float_info.max, sys.float_info.max) and ret == [1, 0]:
            # Happens on i386 since 2. / (sys.float_info.max - -sys.float_info.max) == 5.56268464626800346e-309
            # when using i387 coprocessor (long double)
            pass
        else:
            ret == [0, 0]
            assert gdal.GetLastErrorMsg() != ""

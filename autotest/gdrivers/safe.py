#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test SAFE driver
# Author:   Delfim Rego <delfimrego@gmail.com>
#
###############################################################################
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


from osgeo import gdal
import gdaltest

###############################################################################
# Test reading a - fake - SAFE dataset. Note: the tiff files are fake,
# reduced to 1% of their initial size and metadata stripped


def test_safe_1():

    tst = gdaltest.GDALTest(
        'SAFE',
        'SAFE_FAKE/test.SAFE/manifest.safe', 1, 65372)
    return tst.testOpen()


def test_safe_2():

    tst = gdaltest.GDALTest(
        'SAFE', 'SAFE_FAKE/test.SAFE/manifest.safe', 2, 3732)
    return tst.testOpen()


def test_safe_3():

    tst = gdaltest.GDALTest(
        'SAFE',
        'SENTINEL1_CALIB:UNCALIB:data/SAFE_FAKE/test.SAFE/manifest.safe:IW_VH:AMPLITUDE', 1, 65372, filename_absolute=1)
    return tst.testOpen()


def test_safe_4():

    tst = gdaltest.GDALTest(
        'SAFE',
        'SENTINEL1_CALIB:UNCALIB:data/SAFE_FAKE/test.SAFE/manifest.safe:IW_VV:AMPLITUDE', 1, 3732, filename_absolute=1)
    return tst.testOpen()


def test_safe_5():

    tst = gdaltest.GDALTest(
        'SAFE',
        'SENTINEL1_CALIB:UNCALIB:data/SAFE_FAKE/test.SAFE:IW:AMPLITUDE', 1, 65372, filename_absolute=1)
    return tst.testOpen()


def test_safe_WV():

    ds = gdal.Open('data/SAFE_FAKE_WV')
    assert ds is not None
    subds = ds.GetSubDatasets()
    assert len(subds) == 10

    assert 'SENTINEL1_CALIB:SIGMA0:data/SAFE_FAKE_WV/manifest.safe:WV1_VV_001:INTENSITY' in [x[0].replace('\\', '/') for x in subds]

    ds = gdal.Open('SENTINEL1_CALIB:SIGMA0:data/SAFE_FAKE_WV/manifest.safe:WV1_VV_001:INTENSITY')
    assert ds is not None
    assert len(ds.GetSubDatasets()) == 0
    assert len(ds.GetGCPs()) == 1

    assert 'SENTINEL1_CALIB:SIGMA0:data/SAFE_FAKE_WV/manifest.safe:WV2_VV_002:INTENSITY' in [x[0].replace('\\', '/') for x in subds]

    ds = gdal.Open('SENTINEL1_CALIB:SIGMA0:data/SAFE_FAKE_WV/manifest.safe:WV2_VV_002:INTENSITY')
    assert ds is not None
    assert len(ds.GetSubDatasets()) == 0
    assert len(ds.GetGCPs()) == 2

    with gdaltest.error_handler():
        assert gdal.Open(subds[0][0] + 'xxxx') is None


#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read/write functionality for INGR.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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

import os
from osgeo import gdal


import gdaltest

###############################################################################
# Read test of byte file.


def test_ingr_1():

    tst = gdaltest.GDALTest('INGR', 'ingr/8bit_rgb.cot', 2, 4855)
    return tst.testOpen()

###############################################################################
# Read uint32 file.


def test_ingr_2():

    tst = gdaltest.GDALTest('INGR', 'ingr/uint32.cot', 1, 4672)
    return tst.testOpen()

###############################################################################
# Test paletted file, including checking the palette (format 02 I think).


def test_ingr_3():

    tst = gdaltest.GDALTest('INGR', 'ingr/8bit_pal.cot', 1, 4855)
    tst.testOpen()

    ds = gdal.Open('data/ingr/8bit_pal.cot')
    ct = ds.GetRasterBand(1).GetRasterColorTable()
    assert ct.GetCount() == 256 and ct.GetColorEntry(8) == (8, 8, 8, 255), \
        'Wrong color table entry.'

###############################################################################
# frmt02 is a plain byte format


def test_ingr_4():

    tst = gdaltest.GDALTest('INGR', 'ingr/frmt02.cot', 1, 26968)
    return tst.testOpen()

###############################################################################
# Test creation.


def test_ingr_5():

    tst = gdaltest.GDALTest('INGR', 'ingr/frmt02.cot', 1, 26968)
    return tst.testCreate()

###############################################################################
# Test createcopy.


def test_ingr_6():

    tst = gdaltest.GDALTest('INGR', 'ingr/frmt02.cot', 1, 26968)
    return tst.testCreate()

###############################################################################
# JPEG 8bit


def test_ingr_7():

    tst = gdaltest.GDALTest('INGR', 'ingr/frmt30.cot', 1, 29718)
    return tst.testOpen()

###############################################################################
# Read simple RLE


def test_ingr_8():

    tst = gdaltest.GDALTest('INGR', 'ingr/frmt09.cot', 1, 23035)
    return tst.testOpen()

###############################################################################
# Read Simple RLE Variable


def test_ingr_9():

    tst = gdaltest.GDALTest('INGR', 'ingr/frmt10.cot', 1, 47031)
    return tst.testOpen()

###############################################################################
# CCITT bitonal


def test_ingr_10():

    tst = gdaltest.GDALTest('INGR', 'ingr/frmt24.cit', 1, 23035)
    return tst.testOpen()

###############################################################################
# Adaptive RLE - 24 bit.


def test_ingr_11():

    tst = gdaltest.GDALTest('INGR', 'ingr/frmt27.cot', 2, 45616)
    return tst.testOpen()

###############################################################################
# Uncompressed RGB


def test_ingr_12():

    tst = gdaltest.GDALTest('INGR', 'ingr/frmt28.cot', 2, 45616)
    return tst.testOpen()

###############################################################################
# Adaptive RLE 8bit.


def test_ingr_13():

    tst = gdaltest.GDALTest('INGR', 'ingr/frmt29.cot', 1, 26968)
    return tst.testOpen()

###############################################################################
# JPEG RGB


def test_ingr_14():

    ds = gdal.Open('data/ingr/frmt31.cot')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    assert cs == 11466 or cs == 11095

###############################################################################
# Same, but through vsimem all in memory.


def test_ingr_15():

    tst = gdaltest.GDALTest('INGR', 'ingr/frmt02.cot', 1, 26968)
    result = tst.testCreateCopy(vsimem=1)

    try:
        os.remove('data/ingr/frmt02.cot.aux.xml')
    except OSError:
        pass

    return result

###############################################################################
# Read simple RLE tiled


def test_ingr_16():

    tst = gdaltest.GDALTest('INGR', 'ingr/frmt09t.cot', 1, 3178)
    return tst.testOpen()

###############################################################################
# Test writing 9 RLE bitonal compression (#5030)


def test_ingr_17():

    src_ds = gdal.Open('data/ingr/frmt09.cot')
    out_ds = gdal.GetDriverByName('INGR').CreateCopy('/vsimem/ingr_17.rle', src_ds)
    del out_ds
    ref_cs = src_ds.GetRasterBand(1).Checksum()
    src_ds = None

    ds = gdal.Open('/vsimem/ingr_17.rle')
    got_cs = ds.GetRasterBand(1).Checksum()
    ds = None

    gdal.GetDriverByName('INGR').Delete('/vsimem/ingr_17.rle')

    assert got_cs == ref_cs

###############################################################################
# Test 'random access' in simple RLE


def test_ingr_18():

    ds = gdal.Open('data/ingr/frmt09.cot')
    for y in range(ds.RasterYSize):
        expected_data = ds.ReadRaster(0, y, ds.RasterXSize, 1)
    ds = None

    ds = gdal.Open('data/ingr/frmt09.cot')

    ds.ReadRaster(0, 5, ds.RasterXSize, 1)

    got_data = ds.ReadRaster(0, ds.RasterYSize - 1, ds.RasterXSize, 1)

    assert got_data == expected_data

    ds.FlushCache()

    got_data = ds.ReadRaster(0, ds.RasterYSize - 1, ds.RasterXSize, 1)

    assert got_data == expected_data


def test_ingr_cleanup():

    gdal.Unlink('data/ingr/frmt09.cot.aux.xml')




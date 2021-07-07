#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test WEBP driver
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
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
import pytest

###############################################################################
# Test if WEBP driver is present


def test_webp_1():

    gdaltest.webp_drv = gdal.GetDriverByName('WEBP')
    if gdaltest.webp_drv is None:
        pytest.skip()

    
###############################################################################
# Open() test


def test_webp_2():

    if gdaltest.webp_drv is None:
        pytest.skip()

    ds = gdal.Open('data/webp/rgbsmall.webp')
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 21464 or cs == 21450 or cs == 21459, \
        'did not get expected checksum on band 1'

###############################################################################
# CreateCopy() test


def test_webp_3():

    if gdaltest.webp_drv is None:
        pytest.skip()

    src_ds = gdal.Open('data/rgbsmall.tif')
    out_ds = gdaltest.webp_drv.CreateCopy('/vsimem/webp_3.webp', src_ds, options=['QUALITY=80'])
    src_ds = None
    cs1 = out_ds.GetRasterBand(1).Checksum()
    out_ds = None
    gdal.Unlink('/vsimem/webp_3.webp')
    gdal.Unlink('/vsimem/webp_3.webp.aux.xml')

    # 21502 is for libwebp 0.3.0
    # 21787 is for libwebp 1.0.3
    assert cs1 in (21464, 21502, 21695, 21700, 21787)

###############################################################################
# CreateCopy() on RGBA


def test_webp_4():

    if gdaltest.webp_drv is None:
        pytest.skip()

    md = gdaltest.webp_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('LOSSLESS') == -1:
        pytest.skip()

    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    out_ds = gdaltest.webp_drv.CreateCopy('/vsimem/webp_4.webp', src_ds)
    src_ds = None
    cs1 = out_ds.GetRasterBand(1).Checksum()
    cs4 = out_ds.GetRasterBand(4).Checksum()
    out_ds = None
    gdal.Unlink('/vsimem/webp_4.webp')

    # 22849 is for libwebp 0.3.0
    # 29229 is for libwebp 1.0.3
    assert cs1 in (22001, 22849, 34422, 36652, 36658, 45319, 29229), \
        'did not get expected checksum on band 1'

    assert cs4 == 10807, 'did not get expected checksum on band 4'

###############################################################################
# CreateCopy() on RGBA with lossless compression


def test_webp_5():

    if gdaltest.webp_drv is None:
        pytest.skip()

    md = gdaltest.webp_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('LOSSLESS') == -1:
        pytest.skip()

    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    out_ds = gdaltest.webp_drv.CreateCopy('/vsimem/webp_5.webp', src_ds, options=['LOSSLESS=YES'])
    src_ds = None
    cs1 = out_ds.GetRasterBand(1).Checksum()
    cs4 = out_ds.GetRasterBand(4).Checksum()
    out_ds = None
    gdal.Unlink('/vsimem/webp_5.webp')

    assert cs1 == 12603 or cs1 == 18536 or cs1 == 14800, \
        'did not get expected checksum on band 1'

    assert cs4 == 10807, 'did not get expected checksum on band 4'


###############################################################################
# CreateCopy() on RGBA with lossless compression and exact rgb values


def test_webp_6():

    if gdaltest.webp_drv is None:
        pytest.skip()

    md = gdaltest.webp_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('LOSSLESS') == -1 or md['DMD_CREATIONOPTIONLIST'].find('EXACT') == -1:
        pytest.skip()

    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    out_ds = gdaltest.webp_drv.CreateCopy('/vsimem/webp_6.webp', src_ds, options=['LOSSLESS=YES', 'EXACT=1'])
    src_ds = None
    cs1 = out_ds.GetRasterBand(1).Checksum()
    cs4 = out_ds.GetRasterBand(4).Checksum()
    out_ds = None
    gdal.Unlink('/vsimem/webp_6.webp')

    assert cs1 == 12603, 'did not get expected checksum on band 1'

    assert cs4 == 10807, 'did not get expected checksum on band 4'

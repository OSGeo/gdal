#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  test librarified gdalbuildvrt
# Author:   Even Rouault <even dot rouault @ spatialys dot com>
#
###############################################################################
# Copyright (c) 2016, Even Rouault <even dot rouault @ spatialys dot com>
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

###############################################################################
# Simple test


def test_gdalbuildvrt_lib_1():

    # Source = String
    ds = gdal.BuildVRT('', '../gcore/data/byte.tif')
    assert ds is not None, 'got error/warning'

    assert ds.GetRasterBand(1).Checksum() == 4672, 'Bad checksum'

    # Source = Array of string
    ds = gdal.BuildVRT('', ['../gcore/data/byte.tif'])
    assert ds is not None, 'got error/warning'

    assert ds.GetRasterBand(1).Checksum() == 4672, 'Bad checksum'

    # Source = Dataset
    ds = gdal.BuildVRT('', gdal.Open('../gcore/data/byte.tif'))
    assert ds is not None, 'got error/warning'

    assert ds.GetRasterBand(1).Checksum() == 4672, 'Bad checksum'

    # Source = Array of dataset
    ds = gdal.BuildVRT('', [gdal.Open('../gcore/data/byte.tif')])
    assert ds is not None, 'got error/warning'

    assert ds.GetRasterBand(1).Checksum() == 4672, 'Bad checksum'

###############################################################################
# Test callback


def mycallback(pct, msg, user_data):
    # pylint: disable=unused-argument
    user_data[0] = pct
    return 1


def test_gdalbuildvrt_lib_2():

    tab = [0]
    ds = gdal.BuildVRT('', '../gcore/data/byte.tif', callback=mycallback, callback_data=tab)
    assert ds is not None

    assert ds.GetRasterBand(1).Checksum() == 4672, 'Bad checksum'

    assert tab[0] == 1.0, 'Bad percentage'

    ds = None


###############################################################################
# Test creating overviews


def test_gdalbuildvrt_lib_ovr():

    tmpfilename = '/vsimem/my.vrt'
    ds = gdal.BuildVRT(tmpfilename, '../gcore/data/byte.tif')
    ds.BuildOverviews('NEAR', [2])
    ds = None
    ds = gdal.Open(tmpfilename)
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    ds = None
    gdal.GetDriverByName('VRT').Delete(tmpfilename)


###############################################################################
def test_gdalbuildvrt_lib_separate_nodata():

    src1_ds = gdal.GetDriverByName('MEM').Create('', 1000, 1000)
    src1_ds.SetGeoTransform([2,0.001,0,49,0,-0.001])
    src1_ds.GetRasterBand(1).SetNoDataValue(1)

    src2_ds = gdal.GetDriverByName('MEM').Create('', 1000, 1000)
    src2_ds.SetGeoTransform([2,0.001,0,49,0,-0.001])
    src2_ds.GetRasterBand(1).SetNoDataValue(2)

    gdal.BuildVRT('/vsimem/out.vrt', [src1_ds, src2_ds], separate=True)

    f = gdal.VSIFOpenL('/vsimem/out.vrt', 'rb')
    data = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)
    gdal.Unlink('/vsimem/out.vrt')

    assert b'<NoDataValue>1</NoDataValue>' in data
    assert b'<NODATA>1</NODATA>' in data
    assert b'<NoDataValue>2</NoDataValue>' in data
    assert b'<NODATA>2</NODATA>' in data


###############################################################################
def test_gdalbuildvrt_lib_separate_nodata_2():

    src1_ds = gdal.GetDriverByName('MEM').Create('', 1000, 1000)
    src1_ds.SetGeoTransform([2,0.001,0,49,0,-0.001])
    src1_ds.GetRasterBand(1).SetNoDataValue(1)

    src2_ds = gdal.GetDriverByName('MEM').Create('', 1000, 1000)
    src2_ds.SetGeoTransform([2,0.001,0,49,0,-0.001])
    src2_ds.GetRasterBand(1).SetNoDataValue(2)

    gdal.BuildVRT('/vsimem/out.vrt', [src1_ds, src2_ds], separate=True, srcNodata='3 4')

    f = gdal.VSIFOpenL('/vsimem/out.vrt', 'rb')
    data = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)
    gdal.Unlink('/vsimem/out.vrt')

    assert b'<NoDataValue>3</NoDataValue>' in data
    assert b'<NODATA>3</NODATA>' in data
    assert b'<NoDataValue>4</NoDataValue>' in data
    assert b'<NODATA>4</NODATA>' in data


###############################################################################
def test_gdalbuildvrt_lib_separate_nodata_3():

    src1_ds = gdal.GetDriverByName('MEM').Create('', 1000, 1000)
    src1_ds.SetGeoTransform([2,0.001,0,49,0,-0.001])
    src1_ds.GetRasterBand(1).SetNoDataValue(1)

    src2_ds = gdal.GetDriverByName('MEM').Create('', 1000, 1000)
    src2_ds.SetGeoTransform([2,0.001,0,49,0,-0.001])
    src2_ds.GetRasterBand(1).SetNoDataValue(2)

    gdal.BuildVRT('/vsimem/out.vrt', [src1_ds, src2_ds], separate=True, srcNodata='3 4', VRTNodata='5 6')

    f = gdal.VSIFOpenL('/vsimem/out.vrt', 'rb')
    data = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)
    gdal.Unlink('/vsimem/out.vrt')

    assert b'<NoDataValue>5</NoDataValue>' in data
    assert b'<NODATA>3</NODATA>' in data
    assert b'<NoDataValue>6</NoDataValue>' in data
    assert b'<NODATA>4</NODATA>' in data


###############################################################################
def test_gdalbuildvrt_lib_separate_nodata_4():

    src1_ds = gdal.GetDriverByName('MEM').Create('', 1000, 1000)
    src1_ds.SetGeoTransform([2,0.001,0,49,0,-0.001])
    src1_ds.GetRasterBand(1).SetNoDataValue(1)

    src2_ds = gdal.GetDriverByName('MEM').Create('', 1000, 1000)
    src2_ds.SetGeoTransform([2,0.001,0,49,0,-0.001])
    src2_ds.GetRasterBand(1).SetNoDataValue(2)

    gdal.BuildVRT('/vsimem/out.vrt', [src1_ds, src2_ds], separate=True, srcNodata='None', VRTNodata='None')

    f = gdal.VSIFOpenL('/vsimem/out.vrt', 'rb')
    data = gdal.VSIFReadL(1, 10000, f)
    gdal.VSIFCloseL(f)
    gdal.Unlink('/vsimem/out.vrt')

    assert b'<NoDataValue>' not in data
    assert b'<NODATA>' not in data

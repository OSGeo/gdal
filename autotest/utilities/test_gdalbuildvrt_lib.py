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


#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  nearblack testing
# Author:   Even Rouault <even dot rouault @ spatialys dot com>
# 
###############################################################################
# Copyright (c) 2015, Even Rouault <even dot rouault @ spatialys dot com>
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

import sys

sys.path.append( '../pymod' )

from osgeo import gdal
import gdaltest

###############################################################################
# Basic test

def test_nearblack_lib_1():

    src_ds = gdal.Open('../gdrivers/data/rgbsmall.tif')
    ds = gdal.Nearblack('', src_ds, format = 'MEM', maxNonBlack = 0, nearDist = 15)
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 21106:
        print(ds.GetRasterBand(1).Checksum())
        gdaltest.post_reason('Bad checksum band 1')
        return 'fail'

    if ds.GetRasterBand(2).Checksum() != 20736:
        print(ds.GetRasterBand(2).Checksum())
        gdaltest.post_reason('Bad checksum band 2')
        return 'fail'

    if ds.GetRasterBand(3).Checksum() != 21309:
        print(ds.GetRasterBand(3).Checksum())
        gdaltest.post_reason('Bad checksum band 3')
        return 'fail'

    src_gt = src_ds.GetGeoTransform()
    dst_gt = ds.GetGeoTransform()
    for i in range(6):
        if abs(src_gt[i] - dst_gt[i]) > 1e-10:
            gdaltest.post_reason('Bad geotransform')
            return 'fail'

    dst_wkt = ds.GetProjectionRef()
    if dst_wkt.find('AUTHORITY["EPSG","4326"]') == -1:
        gdaltest.post_reason('Bad projection')
        return 'fail'

    src_ds = None
    ds = None

    return 'success'

###############################################################################
# Add alpha band

def test_nearblack_lib_2():

    ds = gdal.Nearblack('', '../gdrivers/data/rgbsmall.tif', format = 'MEM', maxNonBlack = 0, setAlpha = True)
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(4).Checksum() != 22002:
        print(ds.GetRasterBand(4).Checksum())
        gdaltest.post_reason('Bad checksum band 0')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Set existing alpha band

def test_nearblack_lib_3():

    src_ds = gdal.Nearblack('', '../gdrivers/data/rgbsmall.tif', format = 'MEM', maxNonBlack = 0, setAlpha = True)
    ds = gdal.Nearblack('', src_ds, format = 'MEM', maxNonBlack = 0, setAlpha = True)
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(4).Checksum() != 22002:
        print(ds.GetRasterBand(4).Checksum())
        gdaltest.post_reason('Bad checksum band 0')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test -white

def test_nearblack_lib_4():

    src_ds = gdal.Warp('', '../gdrivers/data/rgbsmall.tif', format = 'MEM', warpOptions = ["INIT_DEST=255"], srcNodata = 0)
    ds = gdal.Nearblack('', src_ds, format = 'MEM', white = True, maxNonBlack = 0, setAlpha = True)
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(4).Checksum() != 24151:
        print(ds.GetRasterBand(4).Checksum())
        gdaltest.post_reason('Bad checksum band 0')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Add mask band

def test_nearblack_lib_5():

    ds = gdal.Nearblack('/vsimem/test_nearblack_lib_5.tif', '../gdrivers/data/rgbsmall.tif', format = 'GTiff', maxNonBlack = 0, setMask = True)
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).GetMaskBand().Checksum() != 22002:
        print(ds.GetRasterBand(1).GetMaskBand().Checksum())
        gdaltest.post_reason('Bad checksum mask band')
        return 'fail'

    ds = None

    gdal.Unlink('/vsimem/test_nearblack_lib_5.tif')
    gdal.Unlink('/vsimem/test_nearblack_lib_5.tif.msk')

    return 'success'

###############################################################################
# Test -color

def test_nearblack_lib_7():

    ds = gdal.Nearblack('', 'data/whiteblackred.tif', format = 'MEM', colors = ((0,0,0),(255,255,255)))
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 418 or \
       ds.GetRasterBand(2).Checksum() != 0 or \
       ds.GetRasterBand(3).Checksum() != 0 :
        print(ds.GetRasterBand(1).Checksum())
        print(ds.GetRasterBand(2).Checksum())
        print(ds.GetRasterBand(3).Checksum())
        gdaltest.post_reason('Bad checksum')
        return 'fail'

    ds = None

    return 'success'

gdaltest_list = [
    test_nearblack_lib_1,
    test_nearblack_lib_2,
    test_nearblack_lib_3,
    test_nearblack_lib_4,
    test_nearblack_lib_5,
    test_nearblack_lib_7,
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'test_nearblack_lib' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

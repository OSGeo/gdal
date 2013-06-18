#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  nearblack testing
# Author:   Even Rouault <even dot rouault @ mines-paris dot org>
# 
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault @ mines-paris dot org>
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
import os
import shutil

sys.path.append( '../pymod' )

import gdal
import gdaltest
import test_cli_utilities

###############################################################################
# Basic test

def test_nearblack_1():
    if test_cli_utilities.get_nearblack_path() is None:
        return 'skip'
    
    (ret, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_nearblack_path() + ' ../gdrivers/data/rgbsmall.tif -nb 0 -of GTiff -o tmp/nearblack1.tif')
    if not (err is None or err == '') :
        gdaltest.post_reason('got error/warning')
        print(err)
        return 'fail'

    src_ds = gdal.Open('../gdrivers/data/rgbsmall.tif')
    ds = gdal.Open('tmp/nearblack1.tif')
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

def test_nearblack_2():
    if test_cli_utilities.get_nearblack_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_nearblack_path() + ' ../gdrivers/data/rgbsmall.tif -setalpha -nb 0 -of GTiff -o tmp/nearblack2.tif -co TILED=YES')

    ds = gdal.Open('tmp/nearblack2.tif')
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

def test_nearblack_3():
    if test_cli_utilities.get_nearblack_path() is None:
        return 'skip'

    shutil.copy('tmp/nearblack2.tif','tmp/nearblack3.tif')
    gdaltest.runexternal(test_cli_utilities.get_nearblack_path() + ' -setalpha -nb 0 -of GTiff tmp/nearblack3.tif')

    ds = gdal.Open('tmp/nearblack3.tif')
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

def test_nearblack_4():
    if test_cli_utilities.get_nearblack_path() is None:
        return 'skip'
    if test_cli_utilities.get_gdalwarp_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdalwarp_path() + ' -wo "INIT_DEST=255" ../gdrivers/data/rgbsmall.tif  tmp/nearblack4_src.tif -srcnodata 0')
    gdaltest.runexternal(test_cli_utilities.get_nearblack_path() + ' -q -setalpha -white -nb 0 -of GTiff tmp/nearblack4_src.tif -o tmp/nearblack4.tif')

    ds = gdal.Open('tmp/nearblack4.tif')
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

def test_nearblack_5():
    if test_cli_utilities.get_nearblack_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_nearblack_path() + ' ../gdrivers/data/rgbsmall.tif --config GDAL_TIFF_INTERNAL_MASK NO -setmask -nb 0 -of GTiff -o tmp/nearblack5.tif -co TILED=YES')

    ds = gdal.Open('tmp/nearblack5.tif')
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).GetMaskBand().Checksum() != 22002:
        print(ds.GetRasterBand(1).GetMaskBand().Checksum())
        gdaltest.post_reason('Bad checksum mask band')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Set existing mask band

def test_nearblack_6():
    if test_cli_utilities.get_nearblack_path() is None:
        return 'skip'

    shutil.copy('tmp/nearblack5.tif','tmp/nearblack6.tif')
    shutil.copy('tmp/nearblack5.tif.msk','tmp/nearblack6.tif.msk')
    
    gdaltest.runexternal(test_cli_utilities.get_nearblack_path() + ' -setmask -nb 0 -of GTiff tmp/nearblack6.tif')

    ds = gdal.Open('tmp/nearblack6.tif')
    if ds is None:
        return 'fail'

    if ds.GetRasterBand(1).GetMaskBand().Checksum() != 22002:
        print(ds.GetRasterBand(1).GetMaskBand().Checksum())
        gdaltest.post_reason('Bad checksum mask band')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test -color

def test_nearblack_7():
    if test_cli_utilities.get_nearblack_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_nearblack_path() + ' data/whiteblackred.tif -o tmp/nearblack7.tif -color 0,0,0 -color 255,255,255 -of GTiff')

    ds = gdal.Open('tmp/nearblack7.tif')
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

###############################################################################
# Cleanup

def test_nearblack_cleanup():
    try:
        os.remove('tmp/nearblack1.tif')
    except:
        pass
    try:
        os.remove('tmp/nearblack2.tif')
    except:
        pass
    try:
        os.remove('tmp/nearblack3.tif')
    except:
        pass
    try:
        os.remove('tmp/nearblack4_src.tif')
    except:
        pass
    try:
        os.remove('tmp/nearblack4.tif')
    except:
        pass
    try:
        os.remove('tmp/nearblack5.tif')
    except:
        pass
    try:
        os.remove('tmp/nearblack5.tif.msk')
    except:
        pass
    try:
        os.remove('tmp/nearblack6.tif')
    except:
        pass
    try:
        os.remove('tmp/nearblack6.tif.msk')
    except:
        pass
    try:
        os.remove('tmp/nearblack7.tif')
    except:
        pass
    return 'success'

gdaltest_list = [
    test_nearblack_1,
    test_nearblack_2,
    test_nearblack_3,
    test_nearblack_4,
    test_nearblack_5,
    test_nearblack_6,
    test_nearblack_7,
    test_nearblack_cleanup
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'test_nearblack' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

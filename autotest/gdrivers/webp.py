#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test WEBP driver
# Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2011, Even Rouault
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
import sys
import gdal

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Test if WEBP driver is present

def webp_1():

    try:
        gdaltest.webp_drv = gdal.GetDriverByName( 'WEBP' )
    except:
        gdaltest.webp_drv = None
        return 'skip'

    return 'success'

###############################################################################
# Open() test

def webp_2():

    if gdaltest.webp_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'WEBP', 'rgbsmall.webp', 1, 21464 )
    return tst.testOpen()

###############################################################################
# CreateCopy() test

def webp_3():

    if gdaltest.webp_drv is None:
        return 'skip'

    src_ds = gdal.Open('data/rgbsmall.tif')
    out_ds = gdaltest.webp_drv.CreateCopy('/vsimem/webp_3.webp', src_ds, options = ['QUALITY=80'])
    src_ds = None
    cs1 = out_ds.GetRasterBand(1).Checksum()
    out_ds = None
    gdal.Unlink('/vsimem/webp_3.webp')

    # 21502 is for libwebp 0.3.0
    if cs1 != 21464 and cs1 != 21502:
        gdaltest.post_reason('did not get expected checksum on band 1')
        print(cs1)
        return 'fail'

    return 'success'

###############################################################################
# CreateCopy() on RGBA 

def webp_4():

    if gdaltest.webp_drv is None:
        return 'skip'

    md = gdaltest.webp_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('LOSSLESS') == -1:
        return 'skip'

    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    out_ds = gdaltest.webp_drv.CreateCopy('/vsimem/webp_4.webp', src_ds)
    src_ds = None
    cs1 = out_ds.GetRasterBand(1).Checksum()
    cs4 = out_ds.GetRasterBand(4).Checksum()
    out_ds = None
    gdal.Unlink('/vsimem/webp_4.webp')

    # 22849 is for libwebp 0.3.0
    if cs1 != 22001 and cs1 != 22849:
        gdaltest.post_reason('did not get expected checksum on band 1')
        print(cs1)
        return 'fail'

    if cs4 != 10807: # lossless alpha
        gdaltest.post_reason('did not get expected checksum on band 4')
        print(cs4)
        return 'fail'

    return 'success'

###############################################################################
# CreateCopy() on RGBA with lossless compression

def webp_5():

    if gdaltest.webp_drv is None:
        return 'skip'

    md = gdaltest.webp_drv.GetMetadata()
    if md['DMD_CREATIONOPTIONLIST'].find('LOSSLESS') == -1:
        return 'skip'

    src_ds = gdal.Open('../gcore/data/stefan_full_rgba.tif')
    out_ds = gdaltest.webp_drv.CreateCopy('/vsimem/webp_5.webp', src_ds, options = ['LOSSLESS=YES'])
    src_ds = None
    cs1 = out_ds.GetRasterBand(1).Checksum()
    cs4 = out_ds.GetRasterBand(4).Checksum()
    out_ds = None
    gdal.Unlink('/vsimem/webp_5.webp')

    if cs1 != 12603:
        gdaltest.post_reason('did not get expected checksum on band 1')
        print(cs1)
        return 'fail'

    if cs4 != 10807:
        gdaltest.post_reason('did not get expected checksum on band 4')
        print(cs4)
        return 'fail'

    return 'success'

gdaltest_list = [
    webp_1,
    webp_2,
    webp_3,
    webp_4,
    webp_5 ]

if __name__ == '__main__':

    gdaltest.setup_run( 'webp' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()


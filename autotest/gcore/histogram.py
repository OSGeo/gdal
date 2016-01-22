#!/usr/bin/env python
###############################################################################
# $Id: colortable.py 11065 2007-03-24 09:35:32Z mloskot $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GetHistogram() and GetDefaultHistogram() handling.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009, Even Rouault <even dot rouault at mines-paris dot org>
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
import shutil

sys.path.append( '../pymod' )

from osgeo import gdal

import gdaltest

###############################################################################
# Fetch simple histogram.

def histogram_1():

    ds = gdal.Open( 'data/utmsmall.tif' )
    hist = ds.GetRasterBand(1).GetHistogram()

    exp_hist = [2, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 23, 0, 0, 0, 0, 0, 0, 0, 0, 29, 0, 0, 0, 0, 0, 0, 0, 46, 0, 0, 0, 0, 0, 0, 0, 69, 0, 0, 0, 0, 0, 0, 0, 99, 0, 0, 0, 0, 0, 0, 0, 0, 120, 0, 0, 0, 0, 0, 0, 0, 178, 0, 0, 0, 0, 0, 0, 0, 193, 0, 0, 0, 0, 0, 0, 0, 212, 0, 0, 0, 0, 0, 0, 0, 281, 0, 0, 0, 0, 0, 0, 0, 0, 365, 0, 0, 0, 0, 0, 0, 0, 460, 0, 0, 0, 0, 0, 0, 0, 533, 0, 0, 0, 0, 0, 0, 0, 544, 0, 0, 0, 0, 0, 0, 0, 0, 626, 0, 0, 0, 0, 0, 0, 0, 653, 0, 0, 0, 0, 0, 0, 0, 673, 0, 0, 0, 0, 0, 0, 0, 629, 0, 0, 0, 0, 0, 0, 0, 0, 586, 0, 0, 0, 0, 0, 0, 0, 541, 0, 0, 0, 0, 0, 0, 0, 435, 0, 0, 0, 0, 0, 0, 0, 348, 0, 0, 0, 0, 0, 0, 0, 341, 0, 0, 0, 0, 0, 0, 0, 0, 284, 0, 0, 0, 0, 0, 0, 0, 225, 0, 0, 0, 0, 0, 0, 0, 237, 0, 0, 0, 0, 0, 0, 0, 172, 0, 0, 0, 0, 0, 0, 0, 0, 159, 0, 0, 0, 0, 0, 0, 0, 105, 0, 0, 0, 0, 0, 0, 0, 824]

    if hist != exp_hist:
        gdaltest.post_reason( 'did not get expected histogram.' )
        print(hist)
        return 'fail'
    
    return 'success' 

###############################################################################
# Fetch histogram with specified sampling, using keywords.

def histogram_2():

    ds = gdal.Open( 'data/utmsmall.tif' )
    hist = ds.GetRasterBand(1).GetHistogram( buckets=16, max=255.5, min=-0.5 )

    exp_hist = [10, 52, 115, 219, 371, 493, 825, 1077, 1279, 1302, 1127, 783, 625, 462, 331, 929]

    if hist != exp_hist:
        gdaltest.post_reason( 'did not get expected histogram.' )
        print(hist)
        return 'fail'
    
    return 'success' 

###############################################################################
# try on a different data type with out of range values included.

def histogram_3():

    ds = gdal.Open( 'data/int32_withneg.grd' )
    hist = ds.GetRasterBand(1).GetHistogram( buckets=21, max=100, min=-100,
                                             include_out_of_range = 1,
                                             approx_ok = 0 )

    exp_hist = [0, 0, 0, 0, 0, 1, 0, 1, 1, 3, 3, 2, 0, 5, 3, 4, 0, 1, 1, 2, 3]

    if hist != exp_hist:
        gdaltest.post_reason( 'did not get expected histogram.' )
        print(hist)
        return 'fail'
    
    return 'success' 

###############################################################################
# try on a different data type without out of range values included.

def histogram_4():

    ds = gdal.Open( 'data/int32_withneg.grd' )
    hist = ds.GetRasterBand(1).GetHistogram( buckets=21, max=100, min=-100,
                                             include_out_of_range = 0,
                                             approx_ok = 0 )

    exp_hist = [0, 0, 0, 0, 0, 1, 0, 1, 1, 3, 3, 2, 0, 5, 3, 4, 0, 1, 1, 2, 0]

    if hist != exp_hist:
        gdaltest.post_reason( 'did not get expected histogram.' )
        print(hist)
        return 'fail'

    ds = None

    try:
        os.unlink( 'data/int32_withneg.grd.aux.xml' )
    except:
        pass
    
    return 'success' 

###############################################################################
# Test GetDefaultHistogram() on the file.

def histogram_5():

    ds = gdal.Open( 'data/utmsmall.tif' )
    hist = ds.GetRasterBand(1).GetDefaultHistogram( force = 1 )

    exp_hist = (-0.5, 255.5, 256, [2, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 23, 0, 0, 0, 0, 0, 0, 0, 0, 29, 0, 0, 0, 0, 0, 0, 0, 46, 0, 0, 0, 0, 0, 0, 0, 69, 0, 0, 0, 0, 0, 0, 0, 99, 0, 0, 0, 0, 0, 0, 0, 0, 120, 0, 0, 0, 0, 0, 0, 0, 178, 0, 0, 0, 0, 0, 0, 0, 193, 0, 0, 0, 0, 0, 0, 0, 212, 0, 0, 0, 0, 0, 0, 0, 281, 0, 0, 0, 0, 0, 0, 0, 0, 365, 0, 0, 0, 0, 0, 0, 0, 460, 0, 0, 0, 0, 0, 0, 0, 533, 0, 0, 0, 0, 0, 0, 0, 544, 0, 0, 0, 0, 0, 0, 0, 0, 626, 0, 0, 0, 0, 0, 0, 0, 653, 0, 0, 0, 0, 0, 0, 0, 673, 0, 0, 0, 0, 0, 0, 0, 629, 0, 0, 0, 0, 0, 0, 0, 0, 586, 0, 0, 0, 0, 0, 0, 0, 541, 0, 0, 0, 0, 0, 0, 0, 435, 0, 0, 0, 0, 0, 0, 0, 348, 0, 0, 0, 0, 0, 0, 0, 341, 0, 0, 0, 0, 0, 0, 0, 0, 284, 0, 0, 0, 0, 0, 0, 0, 225, 0, 0, 0, 0, 0, 0, 0, 237, 0, 0, 0, 0, 0, 0, 0, 172, 0, 0, 0, 0, 0, 0, 0, 0, 159, 0, 0, 0, 0, 0, 0, 0, 105, 0, 0, 0, 0, 0, 0, 0, 824])

    if hist != exp_hist:
        gdaltest.post_reason( 'did not get expected histogram.' )
        print(hist)
        return 'fail'

    ds = None

    try:
        os.unlink( 'data/utmsmall.tif.aux.xml' )
    except:
        pass
    
    return 'success' 

###############################################################################
# Test GetDefaultHistogram( force = 0 ) on a JPG file (#3304)

def histogram_6():

    shutil.copy( '../gdrivers/data/albania.jpg', 'tmp/albania.jpg' )
    ds = gdal.Open( 'tmp/albania.jpg' )
    hist = ds.GetRasterBand(1).GetDefaultHistogram( force = 0 )
    if hist is not None:
        gdaltest.post_reason( 'did not get expected histogram.' )
        print(hist)
        return 'fail'
    ds = None
    os.unlink( 'tmp/albania.jpg' )
    
    return 'success' 
    
gdaltest_list = [
    histogram_1,
    histogram_2,
    histogram_3,
    histogram_4,
    histogram_5,
    histogram_6
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'histogram' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()


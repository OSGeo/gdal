#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test the image reprojection functions. Try to test as many
#           resamplers as possible (we have optimized resamplers for some
#           data types, test them too).
# Author:   Andrey Kiselev, dron16@ak4719.spb.edu
# 
###############################################################################
# Copyright (c) 2008, Andrey Kiselev <dron16@ak4719.spb.edu>
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

sys.path.append( '../pymod' )

try:
    from osgeo import gdal
except ImportError:
    import gdal

try:
    import numpy as Numeric
except ImportError:
    import Numeric

try:
    from osgeo import gdal_array as gdalnumeric
except ImportError:
    import gdalnumeric

import gdaltest

###############################################################################
# Verify that we always getting the same image when warping.
# Warp the image using the VRT file and compare result with reference image
# using the specified threshold.

threshold_byte = 1.0    # For Byte datatype

# Upsampling
def warp_1():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        return 'skip'

    test = gdalnumeric.LoadFile( 'data/utmsmall_near.vrt' )
    ref = gdalnumeric.LoadFile( 'data/utmsmall_near.tiff' )

    if Numeric.alltrue(Numeric.fabs(test-ref) <= threshold_byte):
        return 'success'
    else:
        return 'fail'

def warp_2():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'VRT', 'utmsmall_blinear.vrt', 1, 28772 )

    return tst.testOpen()

def warp_3():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'VRT', 'utmsmall_cubic.vrt', 1, 23824 )

    return tst.testOpen()

def warp_4():
 
    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        return 'skip'
 
    test = gdalnumeric.LoadFile( 'data/utmsmall_cubicspline.vrt' )
    ref = gdalnumeric.LoadFile( 'data/utmsmall_cubicspline.tiff' )
 
    ary = Numeric.fabs(test - ref)
    if Numeric.alltrue(ary <= threshold_byte):
        return 'success'
    else:
        gdaltest.post_reason( 'Logical AND on matrix values failed. Expected values <= %f in matrix with min=%f and max=%f' % (threshold_byte, Numeric.amin(ary), Numeric.amax(ary)) )
        return 'fail'

def warp_5():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'VRT', 'utmsmall_lanczos.vrt', 1, 25603 )

    return tst.testOpen()

# Downsampling
def warp_6():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'VRT', 'utmsmall_ds_near.vrt', 1, 4770 )

    return tst.testOpen()

def warp_7():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'VRT', 'utmsmall_ds_blinear.vrt', 1, 4755 )

    return tst.testOpen()

def warp_8():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'VRT', 'utmsmall_ds_cubic.vrt', 1, 4717 )

    return tst.testOpen()

def warp_9():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        return 'skip'

    test = gdalnumeric.LoadFile( 'data/utmsmall_ds_cubicspline.vrt' )
    ref = gdalnumeric.LoadFile( 'data/utmsmall_ds_cubicspline.tiff' )

    if Numeric.alltrue(Numeric.fabs(test-ref) <= threshold_byte):
        return 'success'
    else:
        return 'fail'

def warp_10():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'VRT', 'utmsmall_ds_lanczos.vrt', 1, 4758 )

    return tst.testOpen()

def warp_11():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'VRT', 'rgbsmall_dstalpha.vrt', 4, 30658)

    return tst.testOpen()


###############################################################################

gdaltest_list = [
    warp_1,
    #warp_2,
    #warp_3,
    warp_4,
    #warp_5,
    #warp_6,
    #warp_7,
    #warp_8,
    warp_9,
    #warp_10,
    warp_11
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'warp' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()


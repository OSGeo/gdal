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

import gdaltest

###############################################################################
# Verify that we always getting the same image when warping.
# Warp the image using the VRT file and compare result with reference image

# Upsampling
def warp_1():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        return 'skip'
    
    ds = gdal.Open( 'data/utmsmall_near.vrt' )
    ref_ds = gdal.Open( 'data/utmsmall_near.tiff' )
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None
 
    if maxdiff > 1:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    return 'success'

def warp_2():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/utmsmall_blinear.vrt' )
    ref_ds = gdal.Open( 'data/utmsmall_blinear.tiff' )
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None
 
    if maxdiff > 1:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    return 'success'

def warp_3():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/utmsmall_cubic.vrt' )
    ref_ds = gdal.Open( 'data/utmsmall_cubic.tiff' )
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None
 
    if maxdiff > 1:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    return 'success'

def warp_4():
 
    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        return 'skip'
    
    ds = gdal.Open( 'data/utmsmall_cubicspline.vrt' )
    ref_ds = gdal.Open( 'data/utmsmall_cubicspline.tiff' )
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None
 
    if maxdiff > 1:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    return 'success'

def warp_5():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/utmsmall_lanczos.vrt' )
    ref_ds = gdal.Open( 'data/utmsmall_lanczos.tiff' )
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None
 
    if maxdiff > 1:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    return 'success'

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
    
    ds = gdal.Open( 'data/utmsmall_ds_cubicspline.vrt' )
    ref_ds = gdal.Open( 'data/utmsmall_ds_cubicspline.tiff' )
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None
 
    if maxdiff > 1:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    return 'success'

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

# Test warping an empty RGBA with bilinear resampling
def warp_12():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        return 'skip'

    ds = gdaltest.tiff_drv.Create('tmp/empty.tif', 20, 20, 4)
    ds.GetRasterBand(1).Fill(0)
    ds.GetRasterBand(2).Fill(0)
    ds.GetRasterBand(3).Fill(0)
    ds.GetRasterBand(4).Fill(0)
    ds = None

    # The alpha channel must be empty
    tst = gdaltest.GDALTest( 'VRT', 'empty_rb.vrt', 4, 0)

    ret = tst.testOpen()

    gdaltest.tiff_drv.Delete('tmp/empty.tif')

    return ret

# Test warping an empty RGBA with cubic resampling
def warp_13():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        return 'skip'

    ds = gdaltest.tiff_drv.Create('tmp/empty.tif', 20, 20, 4)
    ds.GetRasterBand(1).Fill(0)
    ds.GetRasterBand(2).Fill(0)
    ds.GetRasterBand(3).Fill(0)
    ds.GetRasterBand(4).Fill(0)
    ds = None

    # The alpha channel must be empty
    tst = gdaltest.GDALTest( 'VRT', 'empty_rc.vrt', 4, 0)

    ret = tst.testOpen()

    gdaltest.tiff_drv.Delete('tmp/empty.tif')

    return ret

# Test warping an empty RGBA with cubic spline resampling
def warp_14():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        return 'skip'

    ds = gdaltest.tiff_drv.Create('tmp/empty.tif', 20, 20, 4)
    ds.GetRasterBand(1).Fill(0)
    ds.GetRasterBand(2).Fill(0)
    ds.GetRasterBand(3).Fill(0)
    ds.GetRasterBand(4).Fill(0)
    ds = None

    # The alpha channel must be empty
    tst = gdaltest.GDALTest( 'VRT', 'empty_rcs.vrt', 4, 0)

    ret = tst.testOpen()

    gdaltest.tiff_drv.Delete('tmp/empty.tif')

    return ret

###############################################################################

gdaltest_list = [
    warp_1,
    warp_2,
    warp_3,
    warp_4,
    warp_5,
    warp_6,
    warp_7,
    warp_8,
    warp_9,
    warp_10,
    warp_11,
    warp_12,
    warp_13,
    warp_14
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'warp' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()


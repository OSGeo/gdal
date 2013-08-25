#!/usr/bin/env python
# -*- coding: utf-8 -*-
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
import shutil

sys.path.append( '../pymod' )

from osgeo import gdal
from osgeo import osr

import gdaltest
import test_cli_utilities

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

def warp_1_short():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        return 'skip'
    
    ds = gdal.Open( 'data/utmsmall_near_short.vrt' )
    ref_ds = gdal.Open( 'data/utmsmall_near.tiff' )
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None
 
    if maxdiff > 1:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    return 'success'

def warp_1_float():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        return 'skip'
    
    ds = gdal.Open( 'data/utmsmall_near_float.vrt' )
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

def warp_2_short():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/utmsmall_blinear_short.vrt' )
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

def warp_3_short():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        return 'skip'

    ds = gdal.Open( 'data/utmsmall_cubic_short.vrt' )
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

def warp_4_short():
 
    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        return 'skip'
    
    ds = gdal.Open( 'data/utmsmall_cubicspline_short.vrt' )
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

    ds = gdal.Open( 'data/utmsmall_ds_lanczos.vrt' )
    ref_ds = gdal.Open( 'data/utmsmall_ds_lanczos.tiff' )
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None
 
    if maxdiff > 1:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    return 'success'

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

# Test GWKNearestFloat with transparent source alpha band
def warp_15():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        return 'skip'

    ds = gdaltest.tiff_drv.Create('tmp/test.tif', 20, 20, 4)
    ds.GetRasterBand(1).Fill(0)
    ds.GetRasterBand(2).Fill(0)
    ds.GetRasterBand(3).Fill(0)
    ds.GetRasterBand(4).Fill(0)
    ds = None

    # The alpha channel must be empty
    tst = gdaltest.GDALTest( 'VRT', 'test_nearest_float.vrt', 4, 0)

    ret = tst.testOpen()

    gdaltest.tiff_drv.Delete('tmp/test.tif')

    return ret

# Test GWKNearestFloat with opaque source alpha band
def warp_16():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        return 'skip'

    ds = gdaltest.tiff_drv.Create('tmp/test.tif', 20, 20, 4)
    ds.GetRasterBand(1).Fill(255)
    ds.GetRasterBand(2).Fill(0)
    ds.GetRasterBand(3).Fill(0)
    ds.GetRasterBand(4).Fill(255)
    ds = None

    # The alpha channel must be empty
    tst = gdaltest.GDALTest( 'VRT', 'test_nearest_float.vrt', 4, 4921)

    ret = tst.testOpen()

    gdaltest.tiff_drv.Delete('tmp/test.tif')

    return ret

# Test GWKNearestShort with transparent source alpha band
def warp_17():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        return 'skip'

    ds = gdaltest.tiff_drv.Create('tmp/test.tif', 20, 20, 4)
    ds.GetRasterBand(1).Fill(0)
    ds.GetRasterBand(2).Fill(0)
    ds.GetRasterBand(3).Fill(0)
    ds.GetRasterBand(4).Fill(0)
    ds = None

    # The alpha channel must be empty
    tst = gdaltest.GDALTest( 'VRT', 'test_nearest_short.vrt', 4, 0)

    ret = tst.testOpen()

    gdaltest.tiff_drv.Delete('tmp/test.tif')

    return ret

# Test GWKNearestShort with opaque source alpha band
def warp_18():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        return 'skip'

    ds = gdaltest.tiff_drv.Create('tmp/test.tif', 20, 20, 4)
    ds.GetRasterBand(1).Fill(255)
    ds.GetRasterBand(2).Fill(0)
    ds.GetRasterBand(3).Fill(0)
    ds.GetRasterBand(4).Fill(255)
    ds = None

    # The alpha channel must be empty
    tst = gdaltest.GDALTest( 'VRT', 'test_nearest_short.vrt', 4, 4921)

    ret = tst.testOpen()

    gdaltest.tiff_drv.Delete('tmp/test.tif')

    return ret


def warp_19_internal(size, datatype, resampling_string):

    ds = gdaltest.tiff_drv.Create('tmp/test.tif', size, size, 1, datatype)
    ds.SetGeoTransform( (10,5,0,30,0,-5) )
    ds.SetProjection('GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]]')
    ds.GetRasterBand(1).Fill(10.1, 20.1)
    ds = None

    gdaltest.runexternal(test_cli_utilities.get_gdalwarp_path() + ' -r ' + resampling_string + ' tmp/test.tif tmp/testwarp.tif')

    ref_ds = gdal.Open( 'tmp/test.tif' )
    ds = gdal.Open( 'tmp/testwarp.tif' )
    checksum = ds.GetRasterBand(1).Checksum()
    checksum_ref = ref_ds.GetRasterBand(1).Checksum()
    ds = None
    ref_ds = None

    gdaltest.tiff_drv.Delete('tmp/testwarp.tif')

    if checksum != checksum_ref:
        print('got %d, expected %d' % (checksum, checksum_ref))
        gdaltest.post_reason('Result different from source')
        return 'fail'

    gdaltest.tiff_drv.Delete('tmp/test.tif')

    return 'success'


# Test all data types and resampling methods for very small images
# to test edge behaviour
def warp_19():

    if test_cli_utilities.get_gdalwarp_path() is None:
        return 'skip'

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        return 'skip'

    datatypes = [ gdal.GDT_Byte,
                  gdal.GDT_Int16,
                  gdal.GDT_CInt16,
                  gdal.GDT_UInt16,
                  gdal.GDT_Int32,
                  gdal.GDT_CInt32,
                  gdal.GDT_UInt32,
                  gdal.GDT_Float32,
                  gdal.GDT_CFloat32,
                  gdal.GDT_Float64,
                  gdal.GDT_CFloat64 ]

    methods = [ 'near', 'bilinear', 'cubic', 'cubicspline', 'lanczos' ]

    sizes = [ 1, 2, 3, 7 ]

    for k in range(len(sizes)):
        print('Testing size = %d ...' % (sizes[k]))
        for j in range(len(methods)):
            for i in range(len(datatypes)):
                if warp_19_internal(sizes[k], datatypes[i], methods[j]) != 'success':
                    print('fail with size = %d, data type = %d and method %s' % (sizes[k], datatypes[i], methods[j]))
                    return 'fail'

    return 'success'


# Test fix for #2724 (initialization of destination area to nodata in warped VRT)
def warp_20():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        return 'skip'

    tst = gdaltest.GDALTest( 'VRT', 'white_nodata.vrt', 1, 1705 )

    return tst.testOpen()

###############################################################################
# Test overviews on warped VRT files

def warp_21():

    shutil.copy( 'data/utmsmall_near.vrt', 'tmp/warp_21.vrt' )

    ds = gdal.Open( 'tmp/warp_21.vrt', gdal.GA_Update )
    ds.BuildOverviews('NEAR', overviewlist = [ 2 ] )
    ds = None

    ds = gdal.Open( 'tmp/warp_21.vrt' )
    if ds.GetRasterBand(1).GetOverviewCount() != 1:
        return 'skip'

    ds.GetRasterBand(1).GetOverview(0).Checksum()

    ds = None

    os.remove( 'tmp/warp_21.vrt' )

    return 'success'

###############################################################################
# Test warping with datasets which are "bigger" than the wm parameter.
# Would have detected issue of #3458

def warp_22():

    if test_cli_utilities.get_gdalwarp_path() is None:
        return 'skip'

    # Generate source image with non uniform data
    w = 1001
    h = 1001
    ds = gdal.GetDriverByName('GTiff').Create("tmp/warp_22_src.tif", w,h, 1)
    ds.SetGeoTransform([2,0.01,0,49,0,-0.01])
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    ds.SetProjection(sr.ExportToWkt())

    for j in range(h):
        line = ''
        for i in range(w):
            line = line + '%c' % ((i*i+h*j/(i+1))%256)
        ds.GetRasterBand(1).WriteRaster(0,j,w,1,line)

    expected_cs = ds.GetRasterBand(1).Checksum()
    ds = None

    ret = 'success'
    failures = ''

    # warp with various options
    for option1 in ['', '-wo OPTIMIZE_SIZE=TRUE']:
        for option2 in ['', '-co TILED=YES', '-co TILED=YES -co BLOCKXSIZE=16 -co BLOCKYSIZE=16']:
            option = option1 + ' ' + option2
            try:
                os.remove('tmp/warp_22_dst.tif')
            except:
                pass
            # -wm should not be greater than 2 * w * h. Let's put it at its minimum value
            gdaltest.runexternal(test_cli_utilities.get_gdalwarp_path() + ' tmp/warp_22_src.tif tmp/warp_22_dst.tif -wm 100000 ' + option)
            ds = gdal.Open('tmp/warp_22_dst.tif')
            cs = ds.GetRasterBand(1).Checksum()
            if cs != expected_cs:
                if failures != '':
                    failures = failures + '\n'
                failures = failures + 'failed for %s. Checksum : got %d, expected %d' % (option, cs, expected_cs)
                ret = 'fail'
            ds = None

    if failures != '':
        gdaltest.post_reason(failures)

    os.remove('tmp/warp_22_src.tif')
    os.remove('tmp/warp_22_dst.tif')

    return ret

###############################################################################
# Test warping with datasets where some RasterIO() requests involve nBufXSize == 0 (#3582)

def warp_23():

    if test_cli_utilities.get_gdalwarp_path() is None:
        return 'skip'
    
    gcp1 = gdal.GCP()
    gcp1.GCPPixel = 3213
    gcp1.GCPLine = 2225
    gcp1.GCPX = -88.834495
    gcp1.GCPY = 29.979959
    
    gcp2 = gdal.GCP()
    gcp2.GCPPixel = 2804
    gcp2.GCPLine = 2236
    gcp2.GCPX = -88.836706
    gcp2.GCPY = 29.979516
    
    gcp3 = gdal.GCP()
    gcp3.GCPPixel = 3157
    gcp3.GCPLine = 4344
    gcp3.GCPX = -88.833389
    gcp3.GCPY = 29.969519
    
    gcp4 = gdal.GCP()
    gcp4.GCPPixel = 3768
    gcp4.GCPLine = 5247
    gcp4.GCPX = -88.830168
    gcp4.GCPY = 29.964958
    
    gcp5 = gdal.GCP()
    gcp5.GCPPixel = 2697
    gcp5.GCPLine = 9225
    gcp5.GCPX = -88.83516
    gcp5.GCPY = 29.945386
    
    gcp6 = gdal.GCP()
    gcp6.GCPPixel = 4087
    gcp6.GCPLine = 12360
    gcp6.GCPX = -88.827899
    gcp6.GCPY = 29.929807
    
    gcp7 = gdal.GCP()
    gcp7.GCPPixel = 4629
    gcp7.GCPLine = 11258
    gcp7.GCPX = -88.825102
    gcp7.GCPY = 29.93527
    
    gcp8 = gdal.GCP()
    gcp8.GCPPixel = 4480
    gcp8.GCPLine = 7602
    gcp8.GCPX = -88.826733
    gcp8.GCPY = 29.95304
    
    gcps = [gcp1,gcp2,gcp3,gcp4,gcp5,gcp6,gcp7,gcp8]
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)

    ds = gdal.GetDriverByName('GTiff').Create('tmp/test3582.tif', 70, 170, 4, options = ['SPARSE_OK=YES'])
    for i in range(len(gcps)):
        gcps[i].GCPPixel = gcps[i].GCPPixel / 10
        gcps[i].GCPLine = gcps[i].GCPLine / 10
    ds.SetGCPs(gcps, sr.ExportToWkt())
    ds = None
    
    gdaltest.runexternal(test_cli_utilities.get_gdalwarp_path() + ' tmp/test3582.tif tmp/test3582_warped.tif')
    
    ds = gdal.Open('tmp/test3582_warped.tif')
    ret = 'success'
    if ds is None:
        gdaltest.post_reason('could not open output dataset')
        ret = 'fail'
    ds = None
    
    os.remove('tmp/test3582.tif')
    try:
        os.remove('tmp/test3582_warped.tif')
    except:
        pass
    
    return ret

###############################################################################
# Test fix for #3658 (numerical imprecision with Ubuntu 8.10 GCC 4.4.3 -O2 leading to upper
# left pixel being not set in GWKBilinearResample() case)

def warp_24():

    if test_cli_utilities.get_gdalwarp_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdalwarp_path() + ' -r bilinear data/test3658.tif tmp/test3658.tif')
    
    ds_ref = gdal.Open('data/test3658.tif')
    cs_ref = ds_ref.GetRasterBand(1).Checksum()
    ds_ref = None
    ds = gdal.Open('tmp/test3658.tif')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    os.remove('tmp/test3658.tif')

    if cs != cs_ref:
        gdaltest.post_reason('did not get expected checksum')
        return 'fail'

    return 'success'

###############################################################################
# Test -refine_gcps (#4143)

def warp_25():

    ds = gdal.Open('data/refine_gcps.vrt')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    if cs != 4672:
        gdaltest.post_reason('did not get expected checksum')
        return 'fail'

    return 'success'

###############################################################################
# Test serializing and deserializing TPS transformer

def warp_26():

    if test_cli_utilities.get_gdal_translate_path() is None:
        return 'skip'

    if test_cli_utilities.get_gdalwarp_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -of VRT ../gcore/data/byte.tif tmp/warp_25_gcp.vrt -of VRT -gcp 0 0 0 20 -gcp 0 20 0  0 -gcp 20 0 20 20 -gcp 20 20 20 0')
    gdaltest.runexternal(test_cli_utilities.get_gdalwarp_path() + ' -of VRT -tps tmp/warp_25_gcp.vrt tmp/warp_25_warp.vrt')

    ds = gdal.Open('tmp/warp_25_warp.vrt')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    if cs != 4672:
        gdaltest.post_reason('did not get expected checksum')
        return 'fail'

    os.unlink('tmp/warp_25_gcp.vrt')
    os.unlink('tmp/warp_25_warp.vrt')

    return 'success'

###############################################################################
# Pure Python reprojection example. Nothing particular, just make use of existing
# API.

def warp_27_progress_callback(pct, message, user_data):
    #print(pct)
    return 1 # 1 to continue, 0 to stop

def warp_27():

    if test_cli_utilities.get_gdalwarp_path() is None:
        return 'skip'

    # Open source dataset
    src_ds = gdal.Open('../gcore/data/byte.tif')

    # Desfine target SRS
    dst_srs = osr.SpatialReference()
    dst_srs.ImportFromEPSG(4326)
    dst_wkt = dst_srs.ExportToWkt()

    error_threshold = 0.125  # error threshold --> use same value as in gdalwarp
    resampling = gdal.GRA_Bilinear

    # Call AutoCreateWarpedVRT() to fetch default values for target raster dimensions and geotransform
    tmp_ds = gdal.AutoCreateWarpedVRT( src_ds, \
                                       None, # src_wkt : left to default value --> will use the one from source \
                                       dst_wkt, \
                                       resampling, \
                                       error_threshold )
    dst_xsize = tmp_ds.RasterXSize
    dst_ysize = tmp_ds.RasterYSize
    dst_gt = tmp_ds.GetGeoTransform()
    tmp_ds = None

    # Now create the true target dataset
    dst_ds = gdal.GetDriverByName('GTiff').Create('tmp/warp_27.tif', dst_xsize, dst_ysize,
                                                  src_ds.RasterCount)
    dst_ds.SetProjection(dst_wkt)
    dst_ds.SetGeoTransform(dst_gt)

    # And run the reprojection

    cbk = warp_27_progress_callback
    cbk_user_data = None # value for last parameter of above warp_27_progress_callback

    gdal.ReprojectImage( src_ds, \
                         dst_ds, \
                         None, # src_wkt : left to default value --> will use the one from source \
                         None, # dst_wkt : left to default value --> will use the one from destination \
                         resampling, \
                         0, # WarpMemoryLimit : left to default value \
                         error_threshold,
                         cbk, # Progress callback : could be left to None or unspecified for silent progress
                         cbk_user_data)  # Progress callback user data

    # Done !
    dst_ds = None

    # Check that we have the same result as produced by 'gdalwarp -rb -t_srs EPSG:4326 ../gcore/data/byte.tif tmp/warp_27.tif'
    ds = gdal.Open('tmp/warp_27.tif')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    gdaltest.runexternal(test_cli_utilities.get_gdalwarp_path() + ' -rb -t_srs EPSG:4326 ../gcore/data/byte.tif tmp/warp_27_ref.tif')
    ds = gdal.Open('tmp/warp_27_ref.tif')
    ref_cs = ds.GetRasterBand(1).Checksum()
    ds = None

    if cs != ref_cs:
        return 'fail'

    gdal.Unlink('tmp/warp_27.tif')
    gdal.Unlink('tmp/warp_27_ref.tif')

    return 'success'

###############################################################################
# Test reading a VRT with a destination alpha band, but no explicit
# INIT_DEST setting

def warp_28():

    ds = gdal.Open( 'data/utm_alpha_noinit.vrt' )
    cs1 = ds.GetRasterBand(1).Checksum()
    cs2 = ds.GetRasterBand(2).Checksum()
    if cs1 == 0 or cs2 == 0:
        gdaltest.post_reason('bad checksum')
        print(cs1)
        print(cs2)
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test multi-thread computations

def warp_29():

    ds = gdal.Open( 'data/white_nodata.vrt' )
    cs_monothread = ds.GetRasterBand(1).Checksum()
    ds = None

    old_val = gdal.GetConfigOption('GDAL_NUM_THREADS')
    gdal.SetConfigOption('GDAL_NUM_THREADS', 'ALL_CPUS')
    ds = gdal.Open( 'data/white_nodata.vrt' )
    cs_multithread = ds.GetRasterBand(1).Checksum()
    ds = None
    gdal.SetConfigOption('GDAL_NUM_THREADS', old_val)

    if cs_monothread != cs_multithread:
        gdaltest.post_reason('failed')
        return 'fail'

    old_val = gdal.GetConfigOption('GDAL_NUM_THREADS')
    gdal.SetConfigOption('GDAL_NUM_THREADS', '2')
    ds = gdal.Open( 'data/white_nodata.vrt' )
    cs_multithread = ds.GetRasterBand(1).Checksum()
    ds = None
    gdal.SetConfigOption('GDAL_NUM_THREADS', old_val)

    if cs_monothread != cs_multithread:
        gdaltest.post_reason('failed')
        return 'fail'

    return 'success'

###############################################################################
# Test warping interruption

def warp_30_progress_callback(pct, message, user_data):
    if pct > 0.2:
        return 0 # stop
    else:
        return 1 # continue

def warp_30():

    # Open source dataset
    src_ds = gdal.Open('../gcore/data/byte.tif')

    # Desfine target SRS
    dst_srs = osr.SpatialReference()
    dst_srs.ImportFromEPSG(4326)
    dst_wkt = dst_srs.ExportToWkt()

    error_threshold = 0.125  # error threshold --> use same value as in gdalwarp
    resampling = gdal.GRA_Bilinear

    # Call AutoCreateWarpedVRT() to fetch default values for target raster dimensions and geotransform
    tmp_ds = gdal.AutoCreateWarpedVRT( src_ds, \
                                       None, # src_wkt : left to default value --> will use the one from source \
                                       dst_wkt, \
                                       resampling, \
                                       error_threshold )
    dst_xsize = tmp_ds.RasterXSize
    dst_ysize = tmp_ds.RasterYSize
    dst_gt = tmp_ds.GetGeoTransform()
    tmp_ds = None

    # Now create the true target dataset
    dst_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/warp_30.tif', dst_xsize, dst_ysize,
                                                  src_ds.RasterCount)
    dst_ds.SetProjection(dst_wkt)
    dst_ds.SetGeoTransform(dst_gt)

    # And run the reprojection

    cbk = warp_30_progress_callback
    cbk_user_data = None # value for last parameter of above warp_27_progress_callback

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = gdal.ReprojectImage( src_ds, \
                         dst_ds, \
                         None, # src_wkt : left to default value --> will use the one from source \
                         None, # dst_wkt : left to default value --> will use the one from destination \
                         resampling, \
                         0, # WarpMemoryLimit : left to default value \
                         error_threshold,
                         cbk, # Progress callback : could be left to None or unspecified for silent progress
                         cbk_user_data)  # Progress callback user data
    gdal.PopErrorHandler()

    if ret == 0:
        gdaltest.post_reason('failed')
        return 'fail'

    old_val = gdal.GetConfigOption('GDAL_NUM_THREADS')
    gdal.SetConfigOption('GDAL_NUM_THREADS', '2')
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = gdal.ReprojectImage( src_ds, \
                         dst_ds, \
                         None, # src_wkt : left to default value --> will use the one from source \
                         None, # dst_wkt : left to default value --> will use the one from destination \
                         resampling, \
                         0, # WarpMemoryLimit : left to default value \
                         error_threshold,
                         cbk, # Progress callback : could be left to None or unspecified for silent progress
                         cbk_user_data)  # Progress callback user data
    gdal.PopErrorHandler()
    gdal.SetConfigOption('GDAL_NUM_THREADS', old_val)

    if ret == 0:
        gdaltest.post_reason('failed')
        return 'fail'


    return 'success'

# Average (Byte)
def warp_31():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        return 'skip'
    
    ds = gdal.Open( 'data/utmsmall_average.vrt' )
    ref_ds = gdal.Open( 'data/utmsmall_average.tiff' )
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None
 
    if maxdiff > 1:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    return 'success'

# Average (Float)
def warp_32():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        return 'skip'
    
    ds = gdal.Open( 'data/utmsmall_average_float.vrt' )
    ref_ds = gdal.Open( 'data/utmsmall_average_float.tiff' )
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None
 
    if maxdiff > 1:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    return 'success'

# Mode (Byte)
def warp_33():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        return 'skip'
    
    ds = gdal.Open( 'data/utmsmall_mode.vrt' )
    ref_ds = gdal.Open( 'data/utmsmall_mode.tiff' )
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None
 
    if maxdiff > 1:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    return 'success'

# Mode (Int16)
def warp_34():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        return 'skip'
    
    ds = gdal.Open( 'data/utmsmall_mode_int16.vrt' )
    ref_ds = gdal.Open( 'data/utmsmall_mode_int16.tiff' )
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None
 
    if maxdiff > 1:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    return 'success'

# Mode (Int16 - signed with negative values)
def warp_35():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        return 'skip'
    
    ds = gdal.Open( 'data/utmsmall-int16-neg_mode.vrt' )
    ref_ds = gdal.Open( 'data/utmsmall-int16-neg_mode.tiff' )
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None
 
    if maxdiff > 1:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    return 'success'

# Mode (Int32) - this uses algo 2 (ineficient)
def warp_36():

    gdaltest.tiff_drv = gdal.GetDriverByName( 'GTiff' )
    if gdaltest.tiff_drv is None:
        return 'skip'
    
    ds = gdal.Open( 'data/utmsmall_mode_int32.vrt' )
    ref_ds = gdal.Open( 'data/utmsmall_mode_int32.tiff' )
    maxdiff = gdaltest.compare_ds(ds, ref_ds)
    ds = None
    ref_ds = None
 
    if maxdiff > 1:
        gdaltest.post_reason('Image too different from reference')
        return 'fail'

    return 'success'

###############################################################################
# Test a few error cases

def warp_37():

    # Open source dataset
    src_ds = gdal.Open('../gcore/data/byte.tif')

    # Dummy proj.4 method
    sr = osr.SpatialReference()
    sr.ImportFromProj4('+proj=dummy_method +wktext')
    dst_wkt = sr.ExportToWkt()

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    tmp_ds = gdal.AutoCreateWarpedVRT( src_ds, None, dst_wkt )
    gdal.PopErrorHandler()
    gdal.ErrorReset()
    if tmp_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Incompatible projection (UTM 40 is on the other side of the earth w.r.t UTM 11)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32640)
    dst_wkt = sr.ExportToWkt()

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    tmp_ds = gdal.AutoCreateWarpedVRT( src_ds, None, dst_wkt )
    gdal.PopErrorHandler()
    gdal.ErrorReset()
    if tmp_ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################

gdaltest_list = [
    warp_1,
    warp_1_short,
    warp_1_float,
    warp_2,
    warp_2_short,
    warp_3,
    warp_3_short,
    warp_4,
    warp_4_short,
    warp_5,
    warp_6,
    warp_7,
    warp_8,
    warp_9,
    warp_10,
    warp_11,
    warp_12,
    warp_13,
    warp_14,
    warp_15,
    warp_16,
    warp_17,
    warp_18,
    warp_19,
    warp_20,
    warp_21,
    warp_22,
    warp_23,
    warp_24,
    warp_25,
    warp_26,
    warp_27,
    warp_28,
    warp_29,
    warp_30,
    warp_31,
    warp_32,
    warp_33,
    warp_34,
    warp_35,
    warp_36,
    warp_37,
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'warp' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()


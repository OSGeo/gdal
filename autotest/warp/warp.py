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
import shutil

sys.path.append( '../pymod' )

try:
    from osgeo import gdal
    from osgeo import osr
except ImportError:
    import gdal
    import osr

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
    warp_22
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'warp' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()


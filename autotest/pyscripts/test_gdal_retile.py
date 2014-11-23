#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_retile.py testing
# Author:   Even Rouault <even dot rouault @ mines-paris dot org>
# 
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault at mines-paris dot org>
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

sys.path.append( '../pymod' )

from osgeo import gdal
from osgeo import osr
import gdaltest
import test_py_scripts

###############################################################################
# Test gdal_retile.py

def test_gdal_retile_1():

    script_path = test_py_scripts.get_py_script('gdal_retile')
    if script_path is None:
        return 'skip'

    try:
        os.mkdir('tmp/outretile')
    except:
        pass

    test_py_scripts.run_py_script(script_path, 'gdal_retile', '-v -levels 2 -r bilinear -targetDir tmp/outretile ../gcore/data/byte.tif' )

    ds = gdal.Open('tmp/outretile/byte_1_1.tif')
    if ds.GetRasterBand(1).Checksum() != 4672:
        print(ds.GetRasterBand(1).Checksum())
        return 'fail'
    ds = None

    ds = gdal.Open('tmp/outretile/1/byte_1_1.tif')
    if ds.RasterXSize != 10:
        print(ds.RasterXSize)
        return 'fail'
    #if ds.GetRasterBand(1).Checksum() != 1152:
    #    print(ds.GetRasterBand(1).Checksum())
    #    return 'fail'
    ds = None

    ds = gdal.Open('tmp/outretile/2/byte_1_1.tif')
    if ds.RasterXSize != 5:
        print(ds.RasterXSize)
        return 'fail'
    #if ds.GetRasterBand(1).Checksum() != 215:
    #    print(ds.GetRasterBand(1).Checksum())
    #    return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test gdal_retile.py with RGBA dataset

def test_gdal_retile_2():

    script_path = test_py_scripts.get_py_script('gdal_retile')
    if script_path is None:
        return 'skip'

    try:
        os.mkdir('tmp/outretile2')
    except:
        pass

    test_py_scripts.run_py_script(script_path, 'gdal_retile', '-v -levels 2 -r bilinear -targetDir tmp/outretile2 ../gcore/data/rgba.tif' )

    ds = gdal.Open('tmp/outretile2/2/rgba_1_1.tif')
    if ds.GetRasterBand(1).Checksum() != 35:
        gdaltest.post_reason('wrong checksum for band 1')
        print(ds.GetRasterBand(1).Checksum())
        return 'fail'
    if ds.GetRasterBand(4).Checksum() != 35:
        gdaltest.post_reason('wrong checksum for band 4')
        print(ds.GetRasterBand(4).Checksum())
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test gdal_retile.py with input images of different pixel sizes

def test_gdal_retile_3():

    script_path = test_py_scripts.get_py_script('gdal_retile')
    if script_path is None:
        return 'skip'

    drv = gdal.GetDriverByName('GTiff')
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS( 'WGS84' )
    wkt = srs.ExportToWkt()

    # Create two images to tile together. The images will cover the geographic
    # range 0E-30E and 0-60N, split horizontally at 30N. The pixel size in the
    # second image will be twice that of the first time. If the make the first
    # image black and the second gray, then the result of tiling these two
    # together should be gray square stacked on top of a black square.
    #
    # 60 N ---------------  
    #      |             | \
    #      |    50x50    |  \ Image 2
    #      |             |  /
    #      |             | /
    # 30 N ---------------
    #      |             | \
    #      |  100x100    |  \ Image 1
    #      |             |  /
    #      |             | /
    #  0 N ---------------
    #      0 E           30 E
    
    ds = drv.Create('tmp/in1.tif', 100, 100, 1)
    px1_x = 30.0 / ds.RasterXSize
    px1_y = 30.0 / ds.RasterYSize
    ds.SetProjection( wkt )
    ds.SetGeoTransform( [ 0, px1_x, 0, 30, 0, -px1_y ] )
    ds.GetRasterBand(1).Fill(0)
    ds = None

    ds = drv.Create('tmp/in2.tif', 50, 50, 1)
    px2_x = 30.0 / ds.RasterXSize
    px2_y = 30.0 / ds.RasterYSize
    ds.SetProjection( wkt )
    ds.SetGeoTransform( [ 0, px2_x, 0, 60, 0, -px2_y ] )
    ds.GetRasterBand(1).Fill(42)
    ds = None

    try:
        os.mkdir('tmp/outretile3')
    except:
        pass

    test_py_scripts.run_py_script(script_path, 'gdal_retile', '-v -levels 2 -r bilinear -targetDir tmp/outretile3 tmp/in1.tif tmp/in2.tif' )

    ds = gdal.Open('tmp/outretile3/in1_1_1.tif')
    if ds.GetProjectionRef().find('WGS 84') == -1:
        gdaltest.post_reason('Expected WGS 84\nGot : %s' % (ds.GetProjectionRef()) )
        return 'fail'

    gt = ds.GetGeoTransform()
    expected_gt = [ 0, px1_x, 0, 60, 0, -px1_y ]
    for i in range(6):
        if abs(gt[i] - expected_gt[i] > 1e-5):
            gdaltest.post_reason('Expected : %s\nGot : %s' % (expected_gt, gt) )
            return 'fail'

    if ds.RasterXSize != 100 or ds.RasterYSize != 200:
        gdaltest.post_reason('Wrong raster dimensions : %d x %d' % (ds.RasterXSize, ds.RasterYSize) )
        return 'fail'

    if ds.RasterCount != 1:
        gdaltest.post_reason('Wrong raster count : %d ' % (ds.RasterCount) )
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 38999:
        gdaltest.post_reason('Wrong checksum')
        return 'fail'

    return 'success'


###############################################################################
# Cleanup

def test_gdal_retile_cleanup():

    lst = [ 'tmp/outretile/1/byte_1_1.tif',
            'tmp/outretile/2/byte_1_1.tif',
            'tmp/outretile/byte_1_1.tif',
            'tmp/outretile/1',
            'tmp/outretile/2',
            'tmp/outretile',
            'tmp/outretile2/1/rgba_1_1.tif',
            'tmp/outretile2/2/rgba_1_1.tif',
            'tmp/outretile2/1',
            'tmp/outretile2/2',
            'tmp/outretile2/rgba_1_1.tif',
            'tmp/outretile2',
            'tmp/in1.tif',
            'tmp/in2.tif',
            'tmp/outretile3/1/in1_1_1.tif',
            'tmp/outretile3/2/in1_1_1.tif',
            'tmp/outretile3/1',
            'tmp/outretile3/2',
            'tmp/outretile3/in1_1_1.tif',
            'tmp/outretile3' ]
    for filename in lst:
        try:
            os.remove(filename)
        except:
            try:
                os.rmdir(filename)
            except:
                pass

    return 'success'

gdaltest_list = [
    test_gdal_retile_1,
    test_gdal_retile_2,
    test_gdal_retile_3,
    test_gdal_retile_cleanup
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'gdal_retile' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

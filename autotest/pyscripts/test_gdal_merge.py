#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_merge.py testing
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

sys.path.append( '../pymod' )

import gdal
import osr
import gdaltest
import test_py_scripts

###############################################################################
# Basic test

def test_gdal_merge_1():

    script_path = test_py_scripts.get_py_script('gdal_merge')
    if script_path is None:
        return 'skip'

    test_py_scripts.run_py_script(script_path, 'gdal_merge', '-o tmp/test_gdal_merge_1.tif ../gcore/data/byte.tif')

    ds = gdal.Open('tmp/test_gdal_merge_1.tif')
    if ds.GetRasterBand(1).Checksum() != 4672:
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Merge 4 tiles

def test_gdal_merge_2():

    script_path = test_py_scripts.get_py_script('gdal_merge')
    if script_path is None:
        return 'skip'

    drv = gdal.GetDriverByName('GTiff')
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS( 'WGS84' )
    wkt = srs.ExportToWkt()

    ds = drv.Create('tmp/in1.tif', 10, 10, 1)
    ds.SetProjection( wkt )
    ds.SetGeoTransform( [ 2, 0.1, 0, 49, 0, -0.1 ] )
    ds.GetRasterBand(1).Fill(0)
    ds = None

    ds = drv.Create('tmp/in2.tif', 10, 10, 1)
    ds.SetProjection( wkt )
    ds.SetGeoTransform( [ 3, 0.1, 0, 49, 0, -0.1 ] )
    ds.GetRasterBand(1).Fill(63)
    ds = None

    ds = drv.Create('tmp/in3.tif', 10, 10, 1)
    ds.SetProjection( wkt )
    ds.SetGeoTransform( [ 2, 0.1, 0, 48, 0, -0.1 ] )
    ds.GetRasterBand(1).Fill(127)
    ds = None

    ds = drv.Create('tmp/in4.tif', 10, 10, 1)
    ds.SetProjection( wkt )
    ds.SetGeoTransform( [ 3, 0.1, 0, 48, 0, -0.1 ] )
    ds.GetRasterBand(1).Fill(255)
    ds = None

    test_py_scripts.run_py_script(script_path, 'gdal_merge', '-q -o tmp/test_gdal_merge_2.tif tmp/in1.tif tmp/in2.tif tmp/in3.tif tmp/in4.tif')

    ds = gdal.Open('tmp/test_gdal_merge_2.tif')
    if ds.GetProjectionRef().find('WGS 84') == -1:
        gdaltest.post_reason('Expected WGS 84\nGot : %s' % (ds.GetProjectionRef()) )
        return 'fail'

    gt = ds.GetGeoTransform()
    expected_gt = [ 2, 0.1, 0, 49, 0, -0.1 ]
    for i in range(6):
        if abs(gt[i] - expected_gt[i] > 1e-5):
            gdaltest.post_reason('Expected : %s\nGot : %s' % (expected_gt, gt) )
            return 'fail'

    if ds.RasterXSize != 20 or ds.RasterYSize != 20:
        gdaltest.post_reason('Wrong raster dimensions : %d x %d' % (ds.RasterXSize, ds.RasterYSize) )
        return 'fail'

    if ds.RasterCount != 1:
        gdaltest.post_reason('Wrong raster count : %d ' % (ds.RasterCount) )
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 3508:
        gdaltest.post_reason('Wrong checksum')
        return 'fail'

    return 'success'

###############################################################################
# Test -separate option

def test_gdal_merge_3():

    script_path = test_py_scripts.get_py_script('gdal_merge')
    if script_path is None:
        return 'skip'

    test_py_scripts.run_py_script(script_path, 'gdal_merge', '-separate -o tmp/test_gdal_merge_3.tif tmp/in1.tif tmp/in2.tif tmp/in3.tif tmp/in4.tif' )

    ds = gdal.Open('tmp/test_gdal_merge_3.tif')
    if ds.GetProjectionRef().find('WGS 84') == -1:
        gdaltest.post_reason('Expected WGS 84\nGot : %s' % (ds.GetProjectionRef()) )
        return 'fail'

    gt = ds.GetGeoTransform()
    expected_gt = [ 2, 0.1, 0, 49, 0, -0.1 ]
    for i in range(6):
        if abs(gt[i] - expected_gt[i] > 1e-5):
            gdaltest.post_reason('Expected : %s\nGot : %s' % (expected_gt, gt) )
            return 'fail'

    if ds.RasterXSize != 20 or ds.RasterYSize != 20:
        gdaltest.post_reason('Wrong raster dimensions : %d x %d' % (ds.RasterXSize, ds.RasterYSize) )
        return 'fail'

    if ds.RasterCount != 4:
        gdaltest.post_reason('Wrong raster count : %d ' % (ds.RasterCount) )
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 0:
        gdaltest.post_reason('Wrong checksum')
        return 'fail'

    return 'success'

###############################################################################
# Test -init option

def test_gdal_merge_4():

    script_path = test_py_scripts.get_py_script('gdal_merge')
    if script_path is None:
        return 'skip'

    test_py_scripts.run_py_script(script_path, 'gdal_merge', '-init 255 -o tmp/test_gdal_merge_4.tif tmp/in2.tif tmp/in3.tif' )

    ds = gdal.Open('tmp/test_gdal_merge_4.tif')

    if ds.GetRasterBand(1).Checksum() != 4725:
        print(ds.GetRasterBand(1).Checksum())
        gdaltest.post_reason('Wrong checksum')
        return 'fail'

    return 'success'

###############################################################################
# Cleanup

def test_gdal_merge_cleanup():

    lst = [ 'tmp/test_gdal_merge_1.tif',
            'tmp/test_gdal_merge_2.tif',
            'tmp/test_gdal_merge_3.tif',
            'tmp/test_gdal_merge_4.tif',
            'tmp/in1.tif',
            'tmp/in2.tif',
            'tmp/in3.tif',
            'tmp/in4.tif' ]
    for filename in lst:
        try:
            os.remove(filename)
        except:
            pass

    return 'success'

gdaltest_list = [
    test_gdal_merge_1,
    test_gdal_merge_2,
    test_gdal_merge_3,
    test_gdal_merge_4,
    test_gdal_merge_cleanup
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'test_gdal_merge' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdalbuildvrt testing
# Author:   Even Rouault <even dot rouault @ mines-paris dot org>
# 
###############################################################################
# Copyright (c) 2008, Even Rouault <even dot rouault @ mines-paris dot org>
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
import ogr
import osr
import gdaltest
import test_cli_utilities


###############################################################################
def test_gdalbuildvrt_check():

    ds = gdal.Open('tmp/mosaic.vrt')
    if ds.GetProjectionRef().find('WGS 84') == -1:
        gdaltest.post_reason('Expected WGS 84\nGot : %s' % (ds.GetProjectionRef()) )
        return 'fail'

    gt = ds.GetGeoTransform()
    expected_gt = [ 2, 0.1, 0, 49, 0, -0.1 ]
    for i in range(6):
        if abs(gt[i] - expected_gt[i] > 1e-5):
            gdaltest.post_reason('Expected : %s\nGot : %s' % (gt, expected_gt) )
            return 'fail'

    if ds.RasterXSize != 20 or ds.RasterYSize != 20:
        gdaltest.post_reason('Wrong raster dimensions : %d x %d' % (ds.RasterXSize, ds.RasterYSize) )
        return 'fail'

    if ds.GetRasterBand(1).Checksum() != 3508:
        gdaltest.post_reason('Wrong checksum')
        return 'fail'

    return 'success'


###############################################################################
# Simple test

def test_gdalbuildvrt_1():
    if test_cli_utilities.get_gdalbuildvrt_path() is None:
        return 'skip'

    drv = gdal.GetDriverByName('GTiff')
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS( 'WGS84' )
    wkt = srs.ExportToWkt()

    ds = drv.Create('tmp/gdalbuildvrt1.tif', 10, 10, 1)
    ds.SetProjection( wkt )
    ds.SetGeoTransform( [ 2, 0.1, 0, 49, 0, -0.1 ] )
    ds.GetRasterBand(1).Fill(0)
    ds = None

    ds = drv.Create('tmp/gdalbuildvrt2.tif', 10, 10, 1)
    ds.SetProjection( wkt )
    ds.SetGeoTransform( [ 3, 0.1, 0, 49, 0, -0.1 ] )
    ds.GetRasterBand(1).Fill(63)
    ds = None

    ds = drv.Create('tmp/gdalbuildvrt3.tif', 10, 10, 1)
    ds.SetProjection( wkt )
    ds.SetGeoTransform( [ 2, 0.1, 0, 48, 0, -0.1 ] )
    ds.GetRasterBand(1).Fill(127)
    ds = None

    ds = drv.Create('tmp/gdalbuildvrt4.tif', 10, 10, 1)
    ds.SetProjection( wkt )
    ds.SetGeoTransform( [ 3, 0.1, 0, 48, 0, -0.1 ] )
    ds.GetRasterBand(1).Fill(255)
    ds = None

    os.popen(test_cli_utilities.get_gdalbuildvrt_path() + ' tmp/mosaic.vrt tmp/gdalbuildvrt1.tif tmp/gdalbuildvrt2.tif tmp/gdalbuildvrt3.tif tmp/gdalbuildvrt4.tif').read()

    return test_gdalbuildvrt_check()

###############################################################################
# Test with tile index

def test_gdalbuildvrt_2():
    if test_cli_utilities.get_gdalbuildvrt_path() is None:
        return 'skip'
    if test_cli_utilities.get_gdaltindex_path() is None:
        return 'skip'

    try:
        os.remove('tmp/tileindex.shp')
    except:
        pass
    try:
        os.remove('tmp/tileindex.dbf')
    except:
        pass
    try:
        os.remove('tmp/tileindex.shx')
    except:
        pass
    try:
        os.remove('tmp/mosaic.vrt')
    except:
        pass

    os.popen(test_cli_utilities.get_gdaltindex_path() + ' tmp/tileindex.shp tmp/gdalbuildvrt1.tif tmp/gdalbuildvrt2.tif tmp/gdalbuildvrt3.tif tmp/gdalbuildvrt4.tif').read()

    os.popen(test_cli_utilities.get_gdalbuildvrt_path() + ' tmp/mosaic.vrt tmp/tileindex.shp').read()

    return test_gdalbuildvrt_check()

###############################################################################
# Test with file list

def test_gdalbuildvrt_3():
    if test_cli_utilities.get_gdalbuildvrt_path() is None:
        return 'skip'

    open('tmp/filelist.txt', 'wt').write('tmp/gdalbuildvrt1.tif\ntmp/gdalbuildvrt2.tif\ntmp/gdalbuildvrt3.tif\ntmp/gdalbuildvrt4.tif')

    os.popen(test_cli_utilities.get_gdalbuildvrt_path() + ' -input_file_list tmp/filelist.txt tmp/mosaic.vrt').read()

    return test_gdalbuildvrt_check()


###############################################################################
# Try adding a raster in another projection

def test_gdalbuildvrt_4():
    if test_cli_utilities.get_gdalbuildvrt_path() is None:
        return 'skip'

    drv = gdal.GetDriverByName('GTiff')
    wkt = 'GEOGCS[\"WGS 72\",DATUM[\"WGS_1972\"]]'

    ds = drv.Create('tmp/gdalbuildvrt5.tif', 10, 10, 1)
    ds.SetProjection( wkt )
    ds.SetGeoTransform( [ 47, 0.1, 0, 2, 0, -0.1 ] )
    ds = None

    os.popen(test_cli_utilities.get_gdalbuildvrt_path() + ' tmp/mosaic.vrt tmp/gdalbuildvrt1.tif tmp/gdalbuildvrt2.tif tmp/gdalbuildvrt3.tif tmp/gdalbuildvrt4.tif tmp/gdalbuildvrt5.tif').read()

    return test_gdalbuildvrt_check()

###############################################################################
# Try adding a raster with different band count

def test_gdalbuildvrt_5():
    if test_cli_utilities.get_gdalbuildvrt_path() is None:
        return 'skip'

    drv = gdal.GetDriverByName('GTiff')
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS( 'WGS84' )
    wkt = srs.ExportToWkt()

    ds = drv.Create('tmp/gdalbuildvrt5.tif', 10, 10, 2)
    ds.SetProjection( wkt )
    ds.SetGeoTransform( [ 47, 0.1, 0, 2, 0, -0.1 ] )
    ds = None

    os.popen(test_cli_utilities.get_gdalbuildvrt_path() + ' tmp/mosaic.vrt tmp/gdalbuildvrt1.tif tmp/gdalbuildvrt2.tif tmp/gdalbuildvrt3.tif tmp/gdalbuildvrt4.tif tmp/gdalbuildvrt5.tif').read()

    return test_gdalbuildvrt_check()

###############################################################################
# Cleanup

def test_gdalbuildvrt_cleanup():

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/tileindex.shp')

    gdal.GetDriverByName('GTiff').Delete('tmp/mosaic.vrt')

    drv = gdal.GetDriverByName('GTiff')

    drv.Delete('tmp/gdalbuildvrt1.tif')
    drv.Delete('tmp/gdalbuildvrt2.tif')
    drv.Delete('tmp/gdalbuildvrt3.tif')
    drv.Delete('tmp/gdalbuildvrt4.tif')
    drv.Delete('tmp/gdalbuildvrt5.tif')
    try:
        os.remove('tmp/filelist.txt')
    except:
        pass

    return 'success'


gdaltest_list = [
    test_gdalbuildvrt_1,
    test_gdalbuildvrt_2,
    test_gdalbuildvrt_3,
    test_gdalbuildvrt_4,
    test_gdalbuildvrt_5,
    test_gdalbuildvrt_cleanup
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'test_gdalbuildvrt' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()






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

    gdaltest.runexternal(test_cli_utilities.get_gdalbuildvrt_path() + ' tmp/mosaic.vrt tmp/gdalbuildvrt1.tif tmp/gdalbuildvrt2.tif tmp/gdalbuildvrt3.tif tmp/gdalbuildvrt4.tif')

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

    gdaltest.runexternal(test_cli_utilities.get_gdaltindex_path() + ' tmp/tileindex.shp tmp/gdalbuildvrt1.tif tmp/gdalbuildvrt2.tif tmp/gdalbuildvrt3.tif tmp/gdalbuildvrt4.tif')

    gdaltest.runexternal(test_cli_utilities.get_gdalbuildvrt_path() + ' tmp/mosaic.vrt tmp/tileindex.shp')

    return test_gdalbuildvrt_check()

###############################################################################
# Test with file list

def test_gdalbuildvrt_3():
    if test_cli_utilities.get_gdalbuildvrt_path() is None:
        return 'skip'

    open('tmp/filelist.txt', 'wt').write('tmp/gdalbuildvrt1.tif\ntmp/gdalbuildvrt2.tif\ntmp/gdalbuildvrt3.tif\ntmp/gdalbuildvrt4.tif')

    gdaltest.runexternal(test_cli_utilities.get_gdalbuildvrt_path() + ' -input_file_list tmp/filelist.txt tmp/mosaic.vrt')

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

    gdaltest.runexternal(test_cli_utilities.get_gdalbuildvrt_path() + ' tmp/mosaic.vrt tmp/gdalbuildvrt1.tif tmp/gdalbuildvrt2.tif tmp/gdalbuildvrt3.tif tmp/gdalbuildvrt4.tif tmp/gdalbuildvrt5.tif')

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

    gdaltest.runexternal(test_cli_utilities.get_gdalbuildvrt_path() + ' tmp/mosaic.vrt tmp/gdalbuildvrt1.tif tmp/gdalbuildvrt2.tif tmp/gdalbuildvrt3.tif tmp/gdalbuildvrt4.tif tmp/gdalbuildvrt5.tif')

    return test_gdalbuildvrt_check()

###############################################################################
# Test -separate option

def test_gdalbuildvrt_6():
    if test_cli_utilities.get_gdalbuildvrt_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdalbuildvrt_path() + ' -separate tmp/stacked.vrt tmp/gdalbuildvrt1.tif tmp/gdalbuildvrt2.tif tmp/gdalbuildvrt3.tif tmp/gdalbuildvrt4.tif')

    ds = gdal.Open('tmp/stacked.vrt')
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
# Test source rasters with nodata

def test_gdalbuildvrt_7():
    if test_cli_utilities.get_gdalbuildvrt_path() is None:
        return 'skip'

    out_ds = gdal.GetDriverByName('GTiff').Create('tmp/vrtnull1.tif', 20, 10, 3, gdal.GDT_UInt16)
    out_ds.SetGeoTransform([2,0.1,0,49,0,-0.1])
    srs = osr.SpatialReference()
    srs.SetFromUserInput('EPSG:4326')
    out_ds.SetProjection(srs.ExportToWkt())
    out_ds.GetRasterBand(1).SetRasterColorInterpretation(gdal.GCI_RedBand)
    out_ds.GetRasterBand(2).SetRasterColorInterpretation(gdal.GCI_GreenBand)
    out_ds.GetRasterBand(3).SetRasterColorInterpretation(gdal.GCI_BlueBand)
    out_ds.GetRasterBand(1).SetNoDataValue(256)
    
    try:
        ff = '\xff'.encode('latin1')
    except:
        ff = '\xff'

    out_ds.GetRasterBand(1).WriteRaster( 0, 0, 10, 10, ff, buf_type = gdal.GDT_Byte, buf_xsize = 1, buf_ysize = 1 )
    out_ds.GetRasterBand(2).WriteRaster( 0, 0, 10, 10, '\x00', buf_type = gdal.GDT_Byte, buf_xsize = 1, buf_ysize = 1 )
    out_ds.GetRasterBand(3).WriteRaster( 0, 0, 10, 10, '\x00', buf_type = gdal.GDT_Byte, buf_xsize = 1, buf_ysize = 1 )
    out_ds = None

    out_ds = gdal.GetDriverByName('GTiff').Create('tmp/vrtnull2.tif', 20, 10, 3, gdal.GDT_UInt16)
    out_ds.SetGeoTransform([2,0.1,0,49,0,-0.1])
    srs = osr.SpatialReference()
    srs.SetFromUserInput('EPSG:4326')
    out_ds.SetProjection(srs.ExportToWkt())
    out_ds.GetRasterBand(1).SetRasterColorInterpretation(gdal.GCI_RedBand)
    out_ds.GetRasterBand(2).SetRasterColorInterpretation(gdal.GCI_GreenBand)
    out_ds.GetRasterBand(3).SetRasterColorInterpretation(gdal.GCI_BlueBand)
    out_ds.GetRasterBand(1).SetNoDataValue(256)

    out_ds.GetRasterBand(1).WriteRaster( 10, 0, 10, 10, '\x00', buf_type = gdal.GDT_Byte, buf_xsize = 1, buf_ysize = 1 )
    out_ds.GetRasterBand(2).WriteRaster( 10, 0, 10, 10, ff, buf_type = gdal.GDT_Byte, buf_xsize = 1, buf_ysize = 1 )
    out_ds.GetRasterBand(3).WriteRaster( 10, 0, 10, 10, '\x00', buf_type = gdal.GDT_Byte, buf_xsize = 1, buf_ysize = 1 )
    out_ds = None

    gdaltest.runexternal(test_cli_utilities.get_gdalbuildvrt_path() + ' tmp/gdalbuildvrt7.vrt tmp/vrtnull1.tif tmp/vrtnull2.tif')

    ds = gdal.Open('tmp/gdalbuildvrt7.vrt')

    if ds.GetRasterBand(1).Checksum() != 1217:
        gdaltest.post_reason('Wrong checksum')
        return 'fail'

    if ds.GetRasterBand(2).Checksum() != 1218:
        gdaltest.post_reason('Wrong checksum')
        return 'fail'

    if ds.GetRasterBand(3).Checksum() != 0:
        gdaltest.post_reason('Wrong checksum')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test -tr option

def test_gdalbuildvrt_8():
    if test_cli_utilities.get_gdalbuildvrt_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdalbuildvrt_path() + ' -tr 0.05 0.05 tmp/mosaic2.vrt tmp/gdalbuildvrt1.tif tmp/gdalbuildvrt2.tif tmp/gdalbuildvrt3.tif tmp/gdalbuildvrt4.tif')
    
    ds = gdal.Open('tmp/mosaic2.vrt')

    gt = ds.GetGeoTransform()
    expected_gt = [ 2, 0.05, 0, 49, 0, -0.05 ]
    for i in range(6):
        if abs(gt[i] - expected_gt[i] > 1e-5):
            gdaltest.post_reason('Expected : %s\nGot : %s' % (expected_gt, gt) )
            return 'fail'

    if ds.RasterXSize != 40 or ds.RasterYSize != 40:
        gdaltest.post_reason('Wrong raster dimensions : %d x %d' % (ds.RasterXSize, ds.RasterYSize) )
        return 'fail'
    
    gdaltest.runexternal(test_cli_utilities.get_gdalbuildvrt_path() + ' -tr 0.1 0.1 tmp/mosaic.vrt tmp/mosaic2.vrt')

    return test_gdalbuildvrt_check()

###############################################################################
# Test -te option

def test_gdalbuildvrt_9():
    if test_cli_utilities.get_gdalbuildvrt_path() is None:
        return 'skip'

    gdaltest.runexternal(test_cli_utilities.get_gdalbuildvrt_path() + ' -te 1 46 5 50 tmp/mosaic2.vrt tmp/gdalbuildvrt1.tif tmp/gdalbuildvrt2.tif tmp/gdalbuildvrt3.tif tmp/gdalbuildvrt4.tif')

    ds = gdal.Open('tmp/mosaic2.vrt')

    gt = ds.GetGeoTransform()
    expected_gt = [ 1, 0.1, 0, 50, 0, -0.1 ]
    for i in range(6):
        if abs(gt[i] - expected_gt[i] > 1e-5):
            gdaltest.post_reason('Expected : %s\nGot : %s' % (expected_gt, gt) )
            return 'fail'

    if ds.RasterXSize != 40 or ds.RasterYSize != 40:
        gdaltest.post_reason('Wrong raster dimensions : %d x %d' % (ds.RasterXSize, ds.RasterYSize) )
        return 'fail'
   
    gdaltest.runexternal(test_cli_utilities.get_gdalbuildvrt_path() + ' -te 2 47 4 49 tmp/mosaic.vrt tmp/mosaic2.vrt')

    return test_gdalbuildvrt_check()
    
###############################################################################
# Test explicit nodata setting (#3254)

def test_gdalbuildvrt_10():
    if test_cli_utilities.get_gdalbuildvrt_path() is None:
        return 'skip'

    out_ds = gdal.GetDriverByName('GTiff').Create('tmp/test_gdalbuildvrt_10_1.tif', 10, 10, 1, gdal.GDT_Byte, options = ['NBITS=1', 'PHOTOMETRIC=MINISWHITE'])
    out_ds.SetGeoTransform([2,0.1,0,49,0,-0.1])
    srs = osr.SpatialReference()
    srs.SetFromUserInput('EPSG:4326')
    out_ds.SetProjection(srs.ExportToWkt())

    out_ds.GetRasterBand(1).WriteRaster( 1, 1, 3, 3, '\x01', buf_type = gdal.GDT_Byte, buf_xsize = 1, buf_ysize = 1 )
    out_ds = None

    out_ds = gdal.GetDriverByName('GTiff').Create('tmp/test_gdalbuildvrt_10_2.tif', 10, 10, 1, gdal.GDT_Byte, options = ['NBITS=1', 'PHOTOMETRIC=MINISWHITE'])
    out_ds.SetGeoTransform([2,0.1,0,49,0,-0.1])
    srs = osr.SpatialReference()
    srs.SetFromUserInput('EPSG:4326')
    out_ds.SetProjection(srs.ExportToWkt())

    out_ds.GetRasterBand(1).WriteRaster( 6, 6, 3, 3, '\x01', buf_type = gdal.GDT_Byte, buf_xsize = 1, buf_ysize = 1 )
    out_ds = None

    gdaltest.runexternal(test_cli_utilities.get_gdalbuildvrt_path() + ' -srcnodata 0 tmp/gdalbuildvrt10.vrt tmp/test_gdalbuildvrt_10_1.tif tmp/test_gdalbuildvrt_10_2.tif')

    ds = gdal.Open('tmp/gdalbuildvrt10.vrt')

    if ds.GetRasterBand(1).Checksum() != 18:
        print(ds.GetRasterBand(1).Checksum())
        gdaltest.post_reason('Wrong checksum')
        return 'fail'

    ds = None

    return 'success'
    
###############################################################################
# Test that we can stack ungeoreference single band images with -separate (#3432)

def test_gdalbuildvrt_11():
    if test_cli_utilities.get_gdalbuildvrt_path() is None:
        return 'skip'

    out_ds = gdal.GetDriverByName('GTiff').Create('tmp/test_gdalbuildvrt_11_1.tif', 10, 10, 1)
    out_ds.GetRasterBand(1).Fill(255)
    cs1 = out_ds.GetRasterBand(1).Checksum()
    out_ds = None
    
    out_ds = gdal.GetDriverByName('GTiff').Create('tmp/test_gdalbuildvrt_11_2.tif', 10, 10, 1)
    out_ds.GetRasterBand(1).Fill(127)
    cs2 = out_ds.GetRasterBand(1).Checksum()
    out_ds = None

    gdaltest.runexternal(test_cli_utilities.get_gdalbuildvrt_path() + ' -separate tmp/gdalbuildvrt11.vrt tmp/test_gdalbuildvrt_11_1.tif tmp/test_gdalbuildvrt_11_2.tif')

    ds = gdal.Open('tmp/gdalbuildvrt11.vrt')

    if ds.GetRasterBand(1).Checksum() != cs1:
        print(ds.GetRasterBand(1).Checksum())
        print(cs1)
        gdaltest.post_reason('Wrong checksum')
        return 'fail'

    if ds.GetRasterBand(2).Checksum() != cs2:
        print(ds.GetRasterBand(2).Checksum())
        print(cs2)
        gdaltest.post_reason('Wrong checksum')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Cleanup

def test_gdalbuildvrt_cleanup():

    if test_cli_utilities.get_gdalbuildvrt_path() is None:
        return 'skip'

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/tileindex.shp')

    gdal.GetDriverByName('VRT').Delete('tmp/mosaic.vrt')
    gdal.GetDriverByName('VRT').Delete('tmp/mosaic2.vrt')
    gdal.GetDriverByName('VRT').Delete('tmp/stacked.vrt')
    gdal.GetDriverByName('VRT').Delete('tmp/gdalbuildvrt7.vrt')
    gdal.GetDriverByName('VRT').Delete('tmp/gdalbuildvrt10.vrt')
    gdal.GetDriverByName('VRT').Delete('tmp/gdalbuildvrt11.vrt')

    drv = gdal.GetDriverByName('GTiff')

    drv.Delete('tmp/gdalbuildvrt1.tif')
    drv.Delete('tmp/gdalbuildvrt2.tif')
    drv.Delete('tmp/gdalbuildvrt3.tif')
    drv.Delete('tmp/gdalbuildvrt4.tif')
    drv.Delete('tmp/gdalbuildvrt5.tif')
    drv.Delete('tmp/vrtnull1.tif')
    drv.Delete('tmp/vrtnull2.tif')
    drv.Delete('tmp/test_gdalbuildvrt_10_1.tif')
    drv.Delete('tmp/test_gdalbuildvrt_10_2.tif')
    drv.Delete('tmp/test_gdalbuildvrt_11_1.tif')
    drv.Delete('tmp/test_gdalbuildvrt_11_2.tif')
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
    test_gdalbuildvrt_6,
    test_gdalbuildvrt_7,
    test_gdalbuildvrt_8,
    test_gdalbuildvrt_9,
    test_gdalbuildvrt_10,
    test_gdalbuildvrt_11,
    test_gdalbuildvrt_cleanup
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'test_gdalbuildvrt' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()






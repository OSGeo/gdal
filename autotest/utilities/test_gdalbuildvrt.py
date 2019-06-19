#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdalbuildvrt testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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


from osgeo import gdal
from osgeo import ogr
from osgeo import osr
import gdaltest
import test_cli_utilities
import pytest


###############################################################################
def gdalbuildvrt_check():

    ds = gdal.Open('tmp/mosaic.vrt')
    try:
        assert ds.GetProjectionRef().find('WGS 84') != -1, \
            ('Expected WGS 84\nGot : %s' % (ds.GetProjectionRef()))

        gt = ds.GetGeoTransform()
        expected_gt = [2, 0.1, 0, 49, 0, -0.1]
        for i in range(6):
            assert not abs(gt[i] - expected_gt[i] > 1e-5), \
                ('Expected : %s\nGot : %s' % (expected_gt, gt))

        assert ds.RasterXSize == 20 and ds.RasterYSize == 20, \
            ('Wrong raster dimensions : %d x %d' % (ds.RasterXSize, ds.RasterYSize))

        assert ds.RasterCount == 1, ('Wrong raster count : %d ' % (ds.RasterCount))

        assert ds.GetRasterBand(1).Checksum() == 3508, 'Wrong checksum'
    finally:
        del ds

###############################################################################
# Simple test

def test_gdalbuildvrt_1():
    if test_cli_utilities.get_gdalbuildvrt_path() is None:
        pytest.skip()

    drv = gdal.GetDriverByName('GTiff')
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS('WGS84')
    wkt = srs.ExportToWkt()

    ds = drv.Create('tmp/gdalbuildvrt1.tif', 10, 10, 1)
    ds.SetProjection(wkt)
    ds.SetGeoTransform([2, 0.1, 0, 49, 0, -0.1])
    ds.GetRasterBand(1).Fill(0)
    ds = None

    ds = drv.Create('tmp/gdalbuildvrt2.tif', 10, 10, 1)
    ds.SetProjection(wkt)
    ds.SetGeoTransform([3, 0.1, 0, 49, 0, -0.1])
    ds.GetRasterBand(1).Fill(63)
    ds = None

    ds = drv.Create('tmp/gdalbuildvrt3.tif', 10, 10, 1)
    ds.SetProjection(wkt)
    ds.SetGeoTransform([2, 0.1, 0, 48, 0, -0.1])
    ds.GetRasterBand(1).Fill(127)
    ds = None

    ds = drv.Create('tmp/gdalbuildvrt4.tif', 10, 10, 1)
    ds.SetProjection(wkt)
    ds.SetGeoTransform([3, 0.1, 0, 48, 0, -0.1])
    ds.GetRasterBand(1).Fill(255)
    ds = None

    (_, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdalbuildvrt_path() + ' tmp/mosaic.vrt tmp/gdalbuildvrt1.tif tmp/gdalbuildvrt2.tif tmp/gdalbuildvrt3.tif tmp/gdalbuildvrt4.tif')
    assert (err is None or err == ''), 'got error/warning'

    return gdalbuildvrt_check()

###############################################################################
# Test with tile index


def test_gdalbuildvrt_2():
    if test_cli_utilities.get_gdalbuildvrt_path() is None:
        pytest.skip()
    if test_cli_utilities.get_gdaltindex_path() is None:
        pytest.skip()

    try:
        os.remove('tmp/tileindex.shp')
    except OSError:
        pass
    try:
        os.remove('tmp/tileindex.dbf')
    except OSError:
        pass
    try:
        os.remove('tmp/tileindex.shx')
    except OSError:
        pass
    try:
        os.remove('tmp/mosaic.vrt')
    except OSError:
        pass

    gdaltest.runexternal(test_cli_utilities.get_gdaltindex_path() + ' tmp/tileindex.shp tmp/gdalbuildvrt1.tif tmp/gdalbuildvrt2.tif tmp/gdalbuildvrt3.tif tmp/gdalbuildvrt4.tif')

    gdaltest.runexternal(test_cli_utilities.get_gdalbuildvrt_path() + ' tmp/mosaic.vrt tmp/tileindex.shp')

    return gdalbuildvrt_check()

###############################################################################
# Test with file list


def test_gdalbuildvrt_3():
    if test_cli_utilities.get_gdalbuildvrt_path() is None:
        pytest.skip()

    open('tmp/filelist.txt', 'wt').write('tmp/gdalbuildvrt1.tif\ntmp/gdalbuildvrt2.tif\ntmp/gdalbuildvrt3.tif\ntmp/gdalbuildvrt4.tif')

    gdaltest.runexternal(test_cli_utilities.get_gdalbuildvrt_path() + ' -input_file_list tmp/filelist.txt tmp/mosaic.vrt')

    return gdalbuildvrt_check()


###############################################################################
# Try adding a raster in another projection

def test_gdalbuildvrt_4():
    if test_cli_utilities.get_gdalbuildvrt_path() is None:
        pytest.skip()

    drv = gdal.GetDriverByName('GTiff')
    wkt = """GEOGCS["WGS 72",
    DATUM["WGS_1972",
        SPHEROID["WGS 72",6378135,298.26],
        TOWGS84[0,0,4.5,0,0,0.554,0.2263]],
    PRIMEM["Greenwich",0],
    UNIT["degree",0.0174532925199433]]"""

    ds = drv.Create('tmp/gdalbuildvrt5.tif', 10, 10, 1)
    ds.SetProjection(wkt)
    ds.SetGeoTransform([47, 0.1, 0, 2, 0, -0.1])
    ds = None

    gdaltest.runexternal(test_cli_utilities.get_gdalbuildvrt_path() + ' tmp/mosaic.vrt tmp/gdalbuildvrt1.tif tmp/gdalbuildvrt2.tif tmp/gdalbuildvrt3.tif tmp/gdalbuildvrt4.tif tmp/gdalbuildvrt5.tif')

    return gdalbuildvrt_check()

###############################################################################
# Try adding a raster with different band count


# NOTE: fails. commented out originally in 4ef886421c99a4451f8873cb6e094d45ecc86d3f, not sure why
@pytest.mark.skip()
def test_gdalbuildvrt_5():
    if test_cli_utilities.get_gdalbuildvrt_path() is None:
        pytest.skip()

    drv = gdal.GetDriverByName('GTiff')
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS('WGS84')
    wkt = srs.ExportToWkt()

    ds = drv.Create('tmp/gdalbuildvrt5.tif', 10, 10, 2)
    ds.SetProjection(wkt)
    ds.SetGeoTransform([47, 0.1, 0, 2, 0, -0.1])
    ds = None

    gdaltest.runexternal(test_cli_utilities.get_gdalbuildvrt_path() + ' tmp/mosaic.vrt tmp/gdalbuildvrt1.tif tmp/gdalbuildvrt2.tif tmp/gdalbuildvrt3.tif tmp/gdalbuildvrt4.tif tmp/gdalbuildvrt5.tif')

    return gdalbuildvrt_check()

###############################################################################
# Test -separate option


def test_gdalbuildvrt_6():
    if test_cli_utilities.get_gdalbuildvrt_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdalbuildvrt_path() + ' -separate tmp/stacked.vrt tmp/gdalbuildvrt1.tif tmp/gdalbuildvrt2.tif tmp/gdalbuildvrt3.tif tmp/gdalbuildvrt4.tif')

    ds = gdal.Open('tmp/stacked.vrt')
    assert ds.GetProjectionRef().find('WGS 84') != -1, \
        ('Expected WGS 84\nGot : %s' % (ds.GetProjectionRef()))

    gt = ds.GetGeoTransform()
    expected_gt = [2, 0.1, 0, 49, 0, -0.1]
    for i in range(6):
        assert not abs(gt[i] - expected_gt[i] > 1e-5), \
            ('Expected : %s\nGot : %s' % (expected_gt, gt))

    assert ds.RasterXSize == 20 and ds.RasterYSize == 20, \
        ('Wrong raster dimensions : %d x %d' % (ds.RasterXSize, ds.RasterYSize))

    assert ds.RasterCount == 4, ('Wrong raster count : %d ' % (ds.RasterCount))

    assert ds.GetRasterBand(1).Checksum() == 0, 'Wrong checksum'

###############################################################################
# Test source rasters with nodata


def test_gdalbuildvrt_7():
    if test_cli_utilities.get_gdalbuildvrt_path() is None:
        pytest.skip()

    out_ds = gdal.GetDriverByName('GTiff').Create('tmp/vrtnull1.tif', 20, 10, 3, gdal.GDT_UInt16)
    out_ds.SetGeoTransform([2, 0.1, 0, 49, 0, -0.1])
    srs = osr.SpatialReference()
    srs.SetFromUserInput('EPSG:4326')
    out_ds.SetProjection(srs.ExportToWkt())
    out_ds.GetRasterBand(1).SetRasterColorInterpretation(gdal.GCI_RedBand)
    out_ds.GetRasterBand(2).SetRasterColorInterpretation(gdal.GCI_GreenBand)
    out_ds.GetRasterBand(3).SetRasterColorInterpretation(gdal.GCI_BlueBand)
    out_ds.GetRasterBand(1).SetNoDataValue(256)

    try:
        ff = '\xff'.encode('latin1')
    except UnicodeDecodeError:
        ff = '\xff'

    out_ds.GetRasterBand(1).WriteRaster(0, 0, 10, 10, ff, buf_type=gdal.GDT_Byte, buf_xsize=1, buf_ysize=1)
    out_ds.GetRasterBand(2).WriteRaster(0, 0, 10, 10, '\x00', buf_type=gdal.GDT_Byte, buf_xsize=1, buf_ysize=1)
    out_ds.GetRasterBand(3).WriteRaster(0, 0, 10, 10, '\x00', buf_type=gdal.GDT_Byte, buf_xsize=1, buf_ysize=1)
    out_ds = None

    out_ds = gdal.GetDriverByName('GTiff').Create('tmp/vrtnull2.tif', 20, 10, 3, gdal.GDT_UInt16)
    out_ds.SetGeoTransform([2, 0.1, 0, 49, 0, -0.1])
    srs = osr.SpatialReference()
    srs.SetFromUserInput('EPSG:4326')
    out_ds.SetProjection(srs.ExportToWkt())
    out_ds.GetRasterBand(1).SetRasterColorInterpretation(gdal.GCI_RedBand)
    out_ds.GetRasterBand(2).SetRasterColorInterpretation(gdal.GCI_GreenBand)
    out_ds.GetRasterBand(3).SetRasterColorInterpretation(gdal.GCI_BlueBand)
    out_ds.GetRasterBand(1).SetNoDataValue(256)

    out_ds.GetRasterBand(1).WriteRaster(10, 0, 10, 10, '\x00', buf_type=gdal.GDT_Byte, buf_xsize=1, buf_ysize=1)
    out_ds.GetRasterBand(2).WriteRaster(10, 0, 10, 10, ff, buf_type=gdal.GDT_Byte, buf_xsize=1, buf_ysize=1)
    out_ds.GetRasterBand(3).WriteRaster(10, 0, 10, 10, '\x00', buf_type=gdal.GDT_Byte, buf_xsize=1, buf_ysize=1)
    out_ds = None

    gdaltest.runexternal(test_cli_utilities.get_gdalbuildvrt_path() + ' tmp/gdalbuildvrt7.vrt tmp/vrtnull1.tif tmp/vrtnull2.tif')

    ds = gdal.Open('tmp/gdalbuildvrt7.vrt')

    assert ds.GetRasterBand(1).Checksum() == 1217, 'Wrong checksum'

    assert ds.GetRasterBand(2).Checksum() == 1218, 'Wrong checksum'

    assert ds.GetRasterBand(3).Checksum() == 0, 'Wrong checksum'

    ds = None

###############################################################################
# Test -tr option


def test_gdalbuildvrt_8():
    if test_cli_utilities.get_gdalbuildvrt_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdalbuildvrt_path() + ' -tr 0.05 0.05 tmp/mosaic2.vrt tmp/gdalbuildvrt1.tif tmp/gdalbuildvrt2.tif tmp/gdalbuildvrt3.tif tmp/gdalbuildvrt4.tif')

    ds = gdal.Open('tmp/mosaic2.vrt')

    gt = ds.GetGeoTransform()
    expected_gt = [2, 0.05, 0, 49, 0, -0.05]
    for i in range(6):
        assert not abs(gt[i] - expected_gt[i] > 1e-5), \
            ('Expected : %s\nGot : %s' % (expected_gt, gt))

    assert ds.RasterXSize == 40 and ds.RasterYSize == 40, \
        ('Wrong raster dimensions : %d x %d' % (ds.RasterXSize, ds.RasterYSize))

    gdaltest.runexternal(test_cli_utilities.get_gdalbuildvrt_path() + ' -tr 0.1 0.1 tmp/mosaic.vrt tmp/mosaic2.vrt')

    return gdalbuildvrt_check()

###############################################################################
# Test -te option


def test_gdalbuildvrt_9():
    if test_cli_utilities.get_gdalbuildvrt_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdalbuildvrt_path() + ' -te 1 46 5 50 tmp/mosaic2.vrt tmp/gdalbuildvrt1.tif tmp/gdalbuildvrt2.tif tmp/gdalbuildvrt3.tif tmp/gdalbuildvrt4.tif')

    ds = gdal.Open('tmp/mosaic2.vrt')

    gt = ds.GetGeoTransform()
    expected_gt = [1, 0.1, 0, 50, 0, -0.1]
    for i in range(6):
        assert not abs(gt[i] - expected_gt[i] > 1e-5), \
            ('Expected : %s\nGot : %s' % (expected_gt, gt))

    assert ds.RasterXSize == 40 and ds.RasterYSize == 40, \
        ('Wrong raster dimensions : %d x %d' % (ds.RasterXSize, ds.RasterYSize))

    gdaltest.runexternal(test_cli_utilities.get_gdalbuildvrt_path() + ' -te 2 47 4 49 tmp/mosaic.vrt tmp/mosaic2.vrt')

    return gdalbuildvrt_check()

###############################################################################
# Test explicit nodata setting (#3254)


def test_gdalbuildvrt_10():
    if test_cli_utilities.get_gdalbuildvrt_path() is None:
        pytest.skip()

    out_ds = gdal.GetDriverByName('GTiff').Create('tmp/test_gdalbuildvrt_10_1.tif', 10, 10, 1, gdal.GDT_Byte, options=['NBITS=1', 'PHOTOMETRIC=MINISWHITE'])
    out_ds.SetGeoTransform([2, 0.1, 0, 49, 0, -0.1])
    srs = osr.SpatialReference()
    srs.SetFromUserInput('EPSG:4326')
    out_ds.SetProjection(srs.ExportToWkt())

    out_ds.GetRasterBand(1).WriteRaster(1, 1, 3, 3, '\x01', buf_type=gdal.GDT_Byte, buf_xsize=1, buf_ysize=1)
    out_ds = None

    out_ds = gdal.GetDriverByName('GTiff').Create('tmp/test_gdalbuildvrt_10_2.tif', 10, 10, 1, gdal.GDT_Byte, options=['NBITS=1', 'PHOTOMETRIC=MINISWHITE'])
    out_ds.SetGeoTransform([2, 0.1, 0, 49, 0, -0.1])
    srs = osr.SpatialReference()
    srs.SetFromUserInput('EPSG:4326')
    out_ds.SetProjection(srs.ExportToWkt())

    out_ds.GetRasterBand(1).WriteRaster(6, 6, 3, 3, '\x01', buf_type=gdal.GDT_Byte, buf_xsize=1, buf_ysize=1)
    out_ds = None

    gdaltest.runexternal(test_cli_utilities.get_gdalbuildvrt_path() + ' -srcnodata 0 tmp/gdalbuildvrt10.vrt tmp/test_gdalbuildvrt_10_1.tif tmp/test_gdalbuildvrt_10_2.tif')

    ds = gdal.Open('tmp/gdalbuildvrt10.vrt')

    assert ds.GetRasterBand(1).Checksum() == 18, 'Wrong checksum'

    ds = None

###############################################################################
# Test that we can stack ungeoreference single band images with -separate (#3432)


def test_gdalbuildvrt_11():
    if test_cli_utilities.get_gdalbuildvrt_path() is None:
        pytest.skip()

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

    assert ds.GetRasterBand(1).Checksum() == cs1, 'Wrong checksum'

    assert ds.GetRasterBand(2).Checksum() == cs2, 'Wrong checksum'

    ds = None

###############################################################################
# Test -tap option


def test_gdalbuildvrt_12():
    if test_cli_utilities.get_gdalbuildvrt_path() is None:
        pytest.skip()

    (_, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdalbuildvrt_path() + ' -tap tmp/gdalbuildvrt12.vrt ../gcore/data/byte.tif',
                                                check_memleak=False)
    assert err.find('-tap option cannot be used without using -tr') != -1, \
        'expected error'

    gdaltest.runexternal(test_cli_utilities.get_gdalbuildvrt_path() + ' -tr 100 50 -tap tmp/gdalbuildvrt12.vrt ../gcore/data/byte.tif')

    ds = gdal.Open('tmp/gdalbuildvrt12.vrt')

    gt = ds.GetGeoTransform()
    expected_gt = [440700.0, 100.0, 0.0, 3751350.0, 0.0, -50.0]
    for i in range(6):
        assert not abs(gt[i] - expected_gt[i] > 1e-5), \
            ('Expected : %s\nGot : %s' % (expected_gt, gt))

    assert ds.RasterXSize == 13 and ds.RasterYSize == 25, \
        ('Wrong raster dimensions : %d x %d' % (ds.RasterXSize, ds.RasterYSize))

###############################################################################
# Test -a_srs


def test_gdalbuildvrt_13():
    if test_cli_utilities.get_gdalbuildvrt_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdalbuildvrt_path() + ' tmp/gdalbuildvrt13.vrt ../gcore/data/byte.tif -a_srs EPSG:4326')

    ds = gdal.Open('tmp/gdalbuildvrt13.vrt')
    assert ds.GetProjectionRef().find('4326') != -1
    ds = None

###############################################################################
# Test -r


def test_gdalbuildvrt_14():
    if test_cli_utilities.get_gdalbuildvrt_path() is None:
        pytest.skip()
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdalbuildvrt_path() + ' tmp/test_gdalbuildvrt_14.vrt ../gcore/data/byte.tif -r cubic -tr 30 30')

    gdaltest.runexternal(test_cli_utilities.get_gdal_translate_path() + ' -of VRT ../gcore/data/byte.tif tmp/test_gdalbuildvrt_14_ref.vrt -r cubic -outsize 40 40')

    ds = gdal.Open('tmp/test_gdalbuildvrt_14.vrt')
    ds_ref = gdal.Open('tmp/test_gdalbuildvrt_14_ref.vrt')
    cs = ds.GetRasterBand(1).Checksum()
    cs_ref = ds_ref.GetRasterBand(1).Checksum()
    ds = None
    ds_ref = None

    assert cs == cs_ref

###############################################################################
# Test -b


def test_gdalbuildvrt_15():
    if test_cli_utilities.get_gdalbuildvrt_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_gdalbuildvrt_path() + ' tmp/test_gdalbuildvrt_15.vrt ../gcore/data/byte.tif -b 1')

    ds = gdal.Open('tmp/test_gdalbuildvrt_15.vrt')
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    assert cs == 4672

###############################################################################
# Test output to non writable file


def test_gdalbuildvrt_16():
    if test_cli_utilities.get_gdalbuildvrt_path() is None:
        pytest.skip()

    (out, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdalbuildvrt_path() + ' /non_existing_dir/non_existing_subdir/out.vrt ../gcore/data/byte.tif')

    if not gdaltest.is_travis_branch('mingw'):
        assert 'ERROR ret code = 1' in err, out
    else:
        # We don't get the error code on Travis mingw
        assert 'ERROR' in err, out

    
###############################################################################
# Cleanup


def test_gdalbuildvrt_cleanup():

    if test_cli_utilities.get_gdalbuildvrt_path() is None:
        pytest.skip()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/tileindex.shp')

    gdal.GetDriverByName('VRT').Delete('tmp/mosaic.vrt')
    gdal.GetDriverByName('VRT').Delete('tmp/mosaic2.vrt')
    gdal.GetDriverByName('VRT').Delete('tmp/stacked.vrt')
    gdal.GetDriverByName('VRT').Delete('tmp/gdalbuildvrt7.vrt')
    gdal.GetDriverByName('VRT').Delete('tmp/gdalbuildvrt10.vrt')
    gdal.GetDriverByName('VRT').Delete('tmp/gdalbuildvrt11.vrt')
    gdal.GetDriverByName('VRT').Delete('tmp/gdalbuildvrt12.vrt')
    gdal.GetDriverByName('VRT').Delete('tmp/gdalbuildvrt13.vrt')
    gdal.GetDriverByName('VRT').Delete('tmp/test_gdalbuildvrt_14.vrt')
    gdal.GetDriverByName('VRT').Delete('tmp/test_gdalbuildvrt_14_ref.vrt')
    gdal.GetDriverByName('VRT').Delete('tmp/test_gdalbuildvrt_15.vrt')

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
    except OSError:
        pass

    




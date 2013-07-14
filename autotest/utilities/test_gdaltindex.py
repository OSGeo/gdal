#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdaltindex testing
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
# Simple test

def test_gdaltindex_1():
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
        os.remove('tmp/tileindex.prj')
    except:
        pass

    drv = gdal.GetDriverByName('GTiff')
    wkt = 'GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9108\"]],AUTHORITY[\"EPSG\",\"4326\"]]'

    ds = drv.Create('tmp/gdaltindex1.tif', 10, 10, 1)
    ds.SetProjection( wkt )
    ds.SetGeoTransform( [ 49, 0.1, 0, 2, 0, -0.1 ] )
    ds = None

    ds = drv.Create('tmp/gdaltindex2.tif', 10, 10, 1)
    ds.SetProjection( wkt )
    ds.SetGeoTransform( [ 49, 0.1, 0, 3, 0, -0.1 ] )
    ds = None

    ds = drv.Create('tmp/gdaltindex3.tif', 10, 10, 1)
    ds.SetProjection( wkt )
    ds.SetGeoTransform( [ 48, 0.1, 0, 2, 0, -0.1 ] )
    ds = None

    ds = drv.Create('tmp/gdaltindex4.tif', 10, 10, 1)
    ds.SetProjection( wkt )
    ds.SetGeoTransform( [ 48, 0.1, 0, 3, 0, -0.1 ] )
    ds = None

    (out, err) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdaltindex_path() + ' tmp/tileindex.shp tmp/gdaltindex1.tif tmp/gdaltindex2.tif')
    if not (err is None or err == '') :
        gdaltest.post_reason('got error/warning')
        print(err)
        return 'fail'
    gdaltest.runexternal(test_cli_utilities.get_gdaltindex_path() + ' tmp/tileindex.shp tmp/gdaltindex3.tif tmp/gdaltindex4.tif')

    ds = ogr.Open('tmp/tileindex.shp')
    if ds.GetLayer(0).GetFeatureCount() != 4:
        return 'fail'
    tileindex_wkt = ds.GetLayer(0).GetSpatialRef().ExportToWkt()
    if tileindex_wkt.find('GCS_WGS_1984') == -1:
        return 'fail'

    expected_wkts =['POLYGON ((49 2,50 2,50 1,49 1,49 2))',
                    'POLYGON ((49 3,50 3,50 2,49 2,49 3))',
                    'POLYGON ((48 2,49 2,49 1,48 1,48 2))',
                    'POLYGON ((48 3,49 3,49 2,48 2,48 3))' ]
    i = 0
    feat = ds.GetLayer(0).GetNextFeature()
    while feat is not None:
        if feat.GetGeometryRef().ExportToWkt() != expected_wkts[i]:
            print('i=%d, wkt=%s' % (i, feat.GetGeometryRef().ExportToWkt()))
            return 'fail'
        i = i + 1
        feat = ds.GetLayer(0).GetNextFeature()
    ds.Destroy()

    return 'success'

###############################################################################
# Try adding the same rasters again

def test_gdaltindex_2():
    if test_cli_utilities.get_gdaltindex_path() is None:
        return 'skip'

    (ret_stdout, ret_stderr) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdaltindex_path() + ' tmp/tileindex.shp tmp/gdaltindex1.tif tmp/gdaltindex2.tif tmp/gdaltindex3.tif tmp/gdaltindex4.tif')

    if ret_stderr.find('File tmp/gdaltindex1.tif is already in tileindex. Skipping it.') == -1 or \
       ret_stderr.find('File tmp/gdaltindex2.tif is already in tileindex. Skipping it.') == -1 or \
       ret_stderr.find('File tmp/gdaltindex3.tif is already in tileindex. Skipping it.') == -1 or \
       ret_stderr.find('File tmp/gdaltindex4.tif is already in tileindex. Skipping it.') == -1:
        print(ret_stderr)
        gdaltest.post_reason( 'got unexpected error messages.' )
        return 'fail'

    ds = ogr.Open('tmp/tileindex.shp')
    if ds.GetLayer(0).GetFeatureCount() != 4:
        return 'fail'
    ds.Destroy()

    return 'success'


###############################################################################
# Try adding a raster in another projection with -skip_different_projection
# 5th tile should NOT be inserted

def test_gdaltindex_3():
    if test_cli_utilities.get_gdaltindex_path() is None:
        return 'skip'

    drv = gdal.GetDriverByName('GTiff')
    wkt = 'GEOGCS[\"WGS 72\",DATUM[\"WGS_1972\"]]'

    ds = drv.Create('tmp/gdaltindex5.tif', 10, 10, 1)
    ds.SetProjection( wkt )
    ds.SetGeoTransform( [ 47, 0.1, 0, 2, 0, -0.1 ] )
    ds = None

    (ret_stdout, ret_stderr) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdaltindex_path() + ' -skip_different_projection tmp/tileindex.shp tmp/gdaltindex5.tif')

    expected = 'Warning : tmp/gdaltindex5.tif is not using the same projection system as other files in the tileindex.\nThis may cause problems when using it in MapServer for example.\nUse -t_srs option to set target projection system (not supported by MapServer).'
    if ret_stderr.find(expected) == -1:
        print(ret_stderr)
        gdaltest.post_reason( 'got unexpected error message \n[%s]\nexpecting\n[%s]' % (ret_stderr,expected))
        return 'fail'

    ds = ogr.Open('tmp/tileindex.shp')
    if ds.GetLayer(0).GetFeatureCount() != 4:
        return 'fail'
    ds.Destroy()

    return 'success'

###############################################################################
# Try adding a raster in another projection with -t_srs
# 5th tile should be inserted, will not be if there is a srs transformation error

def test_gdaltindex_4():
    if test_cli_utilities.get_gdaltindex_path() is None:
        return 'skip'

    drv = gdal.GetDriverByName('GTiff')
    wkt = 'GEOGCS[\"WGS 72\",DATUM[\"WGS_1972\"]]'

    ds = drv.Create('tmp/gdaltindex5.tif', 10, 10, 1)
    ds.SetProjection( wkt )
    ds.SetGeoTransform( [ 47, 0.1, 0, 2, 0, -0.1 ] )
    ds = None

    (ret_stdout, ret_stderr) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdaltindex_path() + ' -t_srs EPSG:4326 tmp/tileindex.shp tmp/gdaltindex5.tif')

    ds = ogr.Open('tmp/tileindex.shp')
    if ds.GetLayer(0).GetFeatureCount() != 5:
        gdaltest.post_reason( 'got %d features, expecting 4' %  ds.GetLayer(0).GetFeatureCount() )
        return 'fail'
    ds.Destroy()

    return 'success'

###############################################################################
# Test -src_srs_name, -src_srs_format options

def test_gdaltindex_5():
    if test_cli_utilities.get_gdaltindex_path() is None:
        return 'skip'

    drv = gdal.GetDriverByName('GTiff')
    wkt = 'GEOGCS[\"WGS 72\",DATUM[\"WGS_1972\"]]'

    ds = drv.Create('tmp/gdaltindex6.tif', 10, 10, 1)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4322)
    ds.SetProjection( sr.ExportToWkt() )
    ds.SetGeoTransform( [ 47, 0.1, 0, 2, 0, -0.1 ] )
    ds = None

    for src_srs_format in [ '', '-src_srs_format AUTO', '-src_srs_format EPSG', '-src_srs_format PROJ', '-src_srs_format WKT']:
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_gdaltindex_5.shp')
        gdal.PopErrorHandler()
        (ret_stdout, ret_stderr) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdaltindex_path() + ' -src_srs_name src_srs %s -t_srs EPSG:4326 tmp/test_gdaltindex_5.shp tmp/gdaltindex1.tif tmp/gdaltindex6.tif' % src_srs_format)

        ds = ogr.Open('tmp/test_gdaltindex_5.shp')
        lyr = ds.GetLayer(0)
        if lyr.GetFeatureCount() != 2:
            gdaltest.post_reason( 'got %d features, expecting 2' %  ds.GetLayer(0).GetFeatureCount() )
            return 'fail'
        feat = lyr.GetNextFeature()
        feat = lyr.GetNextFeature()
        if src_srs_format == '-src_srs_format PROJ':
            if feat.GetField('src_srs').find('+proj=longlat +ellps=WGS72') != 0:
                gdaltest.post_reason('fail')
                feat.DumpReadable()
                return 'fail'
        elif src_srs_format == '-src_srs_format WKT':
            if feat.GetField('src_srs').find('GEOGCS["WGS 72"') != 0:
                gdaltest.post_reason('fail')
                feat.DumpReadable()
                return 'fail'
        else:
            if feat.GetField('src_srs') != 'EPSG:4322':
                gdaltest.post_reason('fail')
                feat.DumpReadable()
                return 'fail'
        ds = None

    return 'success'

###############################################################################
# Test -f, -lyr_name

def test_gdaltindex_6():
    if test_cli_utilities.get_gdaltindex_path() is None:
        return 'skip'

    for option in [ '', '-lyr_name tileindex']:
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_gdaltindex_6.mif')
        gdal.PopErrorHandler()
        (ret_stdout, ret_stderr) = gdaltest.runexternal_out_and_err(test_cli_utilities.get_gdaltindex_path() + ' -f "MapInfo File" %s tmp/test_gdaltindex_6.mif tmp/gdaltindex1.tif' % option)
        ds = ogr.Open('tmp/test_gdaltindex_6.mif')
        lyr = ds.GetLayer(0)
        if lyr.GetFeatureCount() != 1:
            gdaltest.post_reason( 'got %d features, expecting 1' %  lyr.GetFeatureCount() )
            return 'fail'
        ds = None

    return 'success'

###############################################################################
# Cleanup

def test_gdaltindex_cleanup():

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/tileindex.shp')
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/test_gdaltindex_5.shp')
    ogr.GetDriverByName('MapInfo File').DeleteDataSource('tmp/test_gdaltindex_6.mif')

    drv = gdal.GetDriverByName('GTiff')

    drv.Delete('tmp/gdaltindex1.tif')
    drv.Delete('tmp/gdaltindex2.tif')
    drv.Delete('tmp/gdaltindex3.tif')
    drv.Delete('tmp/gdaltindex4.tif')
    drv.Delete('tmp/gdaltindex5.tif')
    drv.Delete('tmp/gdaltindex6.tif')

    return 'success'


gdaltest_list = [
    test_gdaltindex_1,
    test_gdaltindex_2,
    test_gdaltindex_3,
    test_gdaltindex_4,
    test_gdaltindex_5,
    test_gdaltindex_6,
    test_gdaltindex_cleanup
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'test_gdaltindex' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()






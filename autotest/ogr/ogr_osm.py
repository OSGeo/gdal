#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR OSM driver.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
# 
###############################################################################
# Copyright (c) 2012, Even Rouault <even dot rouault at mines dash paris dot org>
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
from osgeo import ogr
from osgeo import osr
from osgeo import gdal

sys.path.append( '../pymod' )

import gdaltest
import ogrtest

###############################################################################
# Test .pbf

def ogr_osm_1(filename = 'data/test.pbf'):

    try:
        ogrtest.osm_drv = ogr.GetDriverByName('OSM')
    except:
        ogrtest.osm_drv = None
    if ogrtest.osm_drv is None:
        return 'skip'

    ds = ogr.Open(filename)
    if ds is None:
        if filename == 'data/test.osm':
            ogrtest.osm_drv_parse_osm = False
            if gdal.GetLastErrorMsg().find('OSM XML detected, but Expat parser not available') == 0:
                return 'skip'

        gdaltest.post_reason('fail')
        return 'fail'
    else:
        if filename == 'data/test.osm':
            ogrtest.osm_drv_parse_osm = True

    # Test points
    lyr = ds.GetLayer('points')
    if lyr.GetGeomType() != ogr.wkbPoint:
        gdaltest.post_reason('fail')
        return 'fail'

    sr = lyr.GetSpatialRef()
    if sr.ExportToWkt().find('GEOGCS["WGS 84",DATUM["WGS_1984",') != 0 and \
       sr.ExportToWkt().find('GEOGCS["GCS_WGS_1984",DATUM["WGS_1984"') != 0 :
        gdaltest.post_reason('fail')
        print(sr.ExportToWkt())
        return 'fail'

    if filename == 'data/test.osm':
        if lyr.GetExtent() != (2.0, 3.0, 49.0, 50.0):
            gdaltest.post_reason('fail')
            print(lyr.GetExtent())
            return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('osm_id') != '3' or \
       feat.GetFieldAsString('name') != 'Some interesting point' or \
       feat.GetFieldAsString('other_tags') != '"foo"=>"bar","bar"=>"baz"' :
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('POINT (3.0 49.5)')) != 0:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat is not None:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    # Test lines
    lyr = ds.GetLayer('lines')
    if lyr.GetGeomType() != ogr.wkbLineString:
        gdaltest.post_reason('fail')
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('osm_id') != '1' or \
       feat.GetFieldAsString('highway') != 'motorway' or \
       feat.GetFieldAsString('other_tags') != '"foo"=>"bar"' :
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('LINESTRING (2 49,3 50)')) != 0:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('osm_id') != '6':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('LINESTRING (2 49,3 49,3 50,2 50,2 49)')) != 0:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat is not None:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    # Test multipolygons
    lyr = ds.GetLayer('multipolygons')
    if filename == 'tmp/ogr_osm_3':
        if lyr.GetGeomType() != ogr.wkbPolygon:
            gdaltest.post_reason('fail')
            return 'fail'
    else:
        if lyr.GetGeomType() != ogr.wkbMultiPolygon:
            gdaltest.post_reason('fail')
            return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('osm_id') != '1' or \
       feat.GetFieldAsString('type') != 'multipolygon' or \
       feat.GetFieldAsString('natural') != 'forest':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    if filename == 'tmp/ogr_osm_3':
        if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('POLYGON ((2 49,2 50,3 50,3 49,2 49),(2.1 49.1,2.2 49.1,2.2 49.2,2.1 49.2,2.1 49.1))')) != 0:
            gdaltest.post_reason('fail')
            feat.DumpReadable()
            return 'fail'
    else:
        if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('MULTIPOLYGON (((2 49,3 49,3 50,2 50,2 49),(2.1 49.1,2.2 49.1,2.2 49.2,2.1 49.2,2.1 49.1)))')) != 0:
            gdaltest.post_reason('fail')
            feat.DumpReadable()
            return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('osm_id') != '5' or \
       feat.GetFieldAsString('type') != 'multipolygon' or \
       feat.GetFieldAsString('natural') != 'wood':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('osm_way_id') != '8' or \
       feat.GetFieldAsString('name') != 'standalone_polygon':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat is not None:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    # Test multilinestrings
    lyr = ds.GetLayer('multilinestrings')
    if filename == 'tmp/ogr_osm_3':
        if lyr.GetGeomType() != ogr.wkbLineString:
            gdaltest.post_reason('fail')
            return 'fail'
    else:
        if lyr.GetGeomType() != ogr.wkbMultiLineString:
            gdaltest.post_reason('fail')
            return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('osm_id') != '3' or \
       feat.GetFieldAsString('type') != 'route':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    if filename == 'tmp/ogr_osm_3':
        if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('LINESTRING (2 49,3 50)')) != 0:
            gdaltest.post_reason('fail')
            feat.DumpReadable()
            return 'fail'
    else:
        if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('MULTILINESTRING ((2 49,3 50))')) != 0:
            gdaltest.post_reason('fail')
            feat.DumpReadable()
            return 'fail'

    feat = lyr.GetNextFeature()
    if feat is not None:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    # Test other_relations
    lyr = ds.GetLayer('other_relations')
    if filename == 'tmp/ogr_osm_3':
        if lyr is not None:
            gdaltest.post_reason('fail')
            return 'fail'
    else:
        if lyr.GetGeomType() != ogr.wkbGeometryCollection:
            gdaltest.post_reason('fail')
            return 'fail'

        feat = lyr.GetNextFeature()
        if feat.GetFieldAsString('osm_id') != '4' or \
           feat.GetFieldAsString('type') != 'other_type':
            gdaltest.post_reason('fail')
            feat.DumpReadable()
            return 'fail'

        if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION (POINT (2 49),LINESTRING (2 49,3 50))')) != 0:
            gdaltest.post_reason('fail')
            feat.DumpReadable()
            return 'fail'

        feat = lyr.GetNextFeature()
        if feat is not None:
            gdaltest.post_reason('fail')
            feat.DumpReadable()
            return 'fail'

    if ds.GetDriver().GetName() == 'OSM':
        sql_lyr = ds.ExecuteSQL("GetBytesRead()")
        if sql_lyr is None:
            gdaltest.post_reason('fail')
            return 'fail'
        feat = sql_lyr.GetNextFeature()
        if feat is None:
            gdaltest.post_reason('fail')
            return 'fail'
        feat = sql_lyr.GetNextFeature()
        if feat is not None:
            gdaltest.post_reason('fail')
            return 'fail'
        sql_lyr.ResetReading()
        feat = sql_lyr.GetNextFeature()
        if feat is None:
            gdaltest.post_reason('fail')
            return 'fail'
        sql_lyr.GetLayerDefn()
        sql_lyr.TestCapability("foo")
        ds.ReleaseResultSet(sql_lyr)

    ds = None

    return 'success'

###############################################################################
# Test .osm

def ogr_osm_2():
    return ogr_osm_1('data/test.osm')

###############################################################################
# Test ogr2ogr

def ogr_osm_3(options = None):

    if ogrtest.osm_drv is None:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_ogr2ogr_path() is None:
        return 'skip'

    try:
        os.stat('tmp/ogr_osm_3')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/ogr_osm_3')
    except:
        pass

    if options is not None:
        options = ' ' + options
    else:
        options = ''
    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' tmp/ogr_osm_3 data/test.pbf points lines polygons multipolygons multilinestrings -progress' + options)

    ret = ogr_osm_1('tmp/ogr_osm_3')

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/ogr_osm_3')

    return ret

###############################################################################
# Test ogr2ogr with --config OSM_USE_CUSTOM_INDEXING NO

def ogr_osm_3_sqlite_nodes():
    return ogr_osm_3(options = '--config OSM_USE_CUSTOM_INDEXING NO')

###############################################################################
# Test ogr2ogr with --config OSM_COMPRESS_NODES YES

def ogr_osm_3_custom_compress_nodes():
    return ogr_osm_3(options = '--config OSM_COMPRESS_NODES YES')

###############################################################################
# Test optimization when reading only the points layer through a SQL request

def ogr_osm_4():

    if ogrtest.osm_drv is None:
        return 'skip'

    ds = ogr.Open( 'data/test.pbf' )
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    sql_lyr = ds.ExecuteSQL('SELECT * FROM points')

    feat = sql_lyr.GetNextFeature()
    is_none = feat is None

    ds.ReleaseResultSet(sql_lyr)

    if is_none:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test spatial filter

    lyr = ds.GetLayerByName('points')
    lyr.SetSpatialFilterRect(0,0,0,0)
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    is_none = feat is None

    if not is_none:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr.SetSpatialFilter(None)

    # Change layer
    sql_lyr = ds.ExecuteSQL('SELECT * FROM lines')

    feat = sql_lyr.GetNextFeature()
    is_none = feat is None

    ds.ReleaseResultSet(sql_lyr)

    if is_none:
        gdaltest.post_reason('fail')
        return 'fail'

    # Change layer
    sql_lyr = ds.ExecuteSQL('SELECT * FROM points')

    feat = sql_lyr.GetNextFeature()
    is_none = feat is None

    ds.ReleaseResultSet(sql_lyr)

    if is_none:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test optimizations for early attribute filter evaluation

def ogr_osm_5():

    if ogrtest.osm_drv is None:
        return 'skip'

    ds = ogr.Open( 'data/test.pbf' )
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    tests = [ [ 'points', '3', True ],
              [ 'points', 'foo', False ],
              [ 'lines', '1', True ],
              [ 'lines', 'foo', False ],
              [ 'multipolygons', '1', True ],
              [ 'multipolygons', 'foo', False ],
              [ 'multilinestrings', '3', True ],
              [ 'multilinestrings', 'foo', False ] ]

    for test in tests:
        sql_lyr = ds.ExecuteSQL("SELECT * FROM %s WHERE osm_id = '%s'" % (test[0], test[1]))
        feat = sql_lyr.GetNextFeature()
        is_none = feat is None
        feat = None
        ds.ReleaseResultSet(sql_lyr)

        if not (test[2] ^ is_none):
            gdaltest.post_reason('fail')
            print(test)
            return 'fail'

    sql_lyr = ds.ExecuteSQL("select * from multipolygons where type = 'multipolygon'")
    feat = sql_lyr.GetNextFeature()
    is_none = feat is None
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    if is_none:
        gdaltest.post_reason('fail')
        print(test)
        return 'fail'

    return 'success'

###############################################################################
# Test ogr2ogr -sql

def ogr_osm_6():

    if ogrtest.osm_drv is None:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_ogr2ogr_path() is None:
        return 'skip'

    try:
        os.stat('tmp/ogr_osm_6')
        ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/ogr_osm_6')
    except:
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' tmp/ogr_osm_6 data/test.pbf -sql "select * from multipolygons" -progress')

    ds = ogr.Open('tmp/ogr_osm_6')
    lyr = ds.GetLayer(0)
    count = lyr.GetFeatureCount()
    ds = None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/ogr_osm_6')

    if count != 3:
        gdaltest.post_reason('fail')
        print(count)
        return 'fail'

    return 'success'

###############################################################################
# Test optimization when reading only the points layer through a SQL request
# with SQLite dialect (#4825)

def ogr_osm_7():

    if ogrtest.osm_drv is None:
        return 'skip'

    ds = ogr.Open( 'data/test.pbf' )
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    sql_lyr = ds.ExecuteSQL('SELECT * FROM points LIMIT 10', dialect = 'SQLite')
    if sql_lyr is None and gdal.GetLastErrorMsg().find('automatic extension loading failed') != 0:
        return 'skip'
    count = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)

    if count != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test 64-bit ids

def ogr_osm_8():

    if ogrtest.osm_drv is None:
        return 'skip'

    ds = ogr.Open( 'data/base-64.osm.pbf' )
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr = ds.GetLayerByName( 'points')
    lyr.SetAttributeFilter("osm_id = '4294967934'")
    feat = lyr.GetNextFeature()

    if feat.GetField('name') != 'Treetops' or \
       ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('POINT (-61.7964321 17.1498319)')) != 0:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    lyr = ds.GetLayerByName( 'multipolygons')
    feat = lyr.GetFeature(1113)

    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('MULTIPOLYGON (((-61.7780345 17.140634,-61.7777002 17.1406069,-61.7776854 17.1407739,-61.7779131 17.1407923,-61.7779158 17.1407624,-61.7780224 17.140771,-61.7780345 17.140634)))')) != 0:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################
# Same as ogr_osm_8 but with OSM_USE_CUSTOM_INDEXING=NO

def ogr_osm_9():

    if ogrtest.osm_drv is None:
        return 'skip'

    old_val = gdal.GetConfigOption('OSM_USE_CUSTOM_INDEXING')
    gdal.SetConfigOption('OSM_USE_CUSTOM_INDEXING', 'NO')
    ret = ogr_osm_8()
    gdal.SetConfigOption('OSM_USE_CUSTOM_INDEXING', old_val)
    
    return ret

###############################################################################
# Some error conditions

def ogr_osm_10():

    if ogrtest.osm_drv is None:
        return 'skip'

    # Non existing file
    ds = ogr.Open('/nonexisting/foo.osm')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Empty .osm file
    f = gdal.VSIFOpenL('/vsimem/foo.osm', 'wb')
    gdal.VSIFCloseL(f)

    ds = ogr.Open('/vsimem/foo.osm')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.Unlink('/vsimem/foo.osm')

    # Empty .pbf file
    f = gdal.VSIFOpenL('/vsimem/foo.pbf', 'wb')
    gdal.VSIFCloseL(f)

    ds = ogr.Open('/vsimem/foo.pbf')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.Unlink('/vsimem/foo.pbf')

    if ogrtest.osm_drv_parse_osm:
        # Invalid .osm file
        f = gdal.VSIFOpenL('/vsimem/foo.osm', 'wb')
        data = "<osm>"
        gdal.VSIFWriteL(data, 1, len(data), f)
        gdal.VSIFCloseL(f)

        ds = ogr.Open('/vsimem/foo.osm')
        lyr = ds.GetLayer(0)
        gdal.ErrorReset()
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        feat = lyr.GetNextFeature()
        gdal.PopErrorHandler()
        if gdal.GetLastErrorMsg() == '':
            gdaltest.post_reason('fail')
            return 'fail'
        ds = None

        gdal.Unlink('/vsimem/foo.osm')

    # Invalid .pbf file
    f = gdal.VSIFOpenL('/vsimem/foo.pbf', 'wb')
    data = "OSMHeader\n"
    gdal.VSIFWriteL(data, 1, len(data), f)
    gdal.VSIFCloseL(f)

    ds = ogr.Open('/vsimem/foo.pbf')
    lyr = ds.GetLayer(0)
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    feat = lyr.GetNextFeature()
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/foo.pbf')

    # Test million laugh pattern
    if ogrtest.osm_drv_parse_osm:
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        ds = ogr.Open('data/billionlaugh.osm')
        gdal.PopErrorHandler()
        if ds is not None:
            gdaltest.post_reason('fail')
            return 'fail'

    return 'success'

gdaltest_list = [
    ogr_osm_1,
    ogr_osm_2,
    ogr_osm_3,
    ogr_osm_3_sqlite_nodes,
    ogr_osm_3_custom_compress_nodes,
    ogr_osm_4,
    ogr_osm_5,
    ogr_osm_6,
    ogr_osm_7,
    ogr_osm_8,
    ogr_osm_9,
    ogr_osm_10,
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_osm' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

#!/usr/bin/env python
###############################################################################
# $Id $
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
import ogr
import osr
import gdal

sys.path.append( '../pymod' )

import gdaltest
import ogrtest

###############################################################################
# Test .pbf

def ogr_osm_1(filename = 'data/test.pbf'):

    try:
        drv = ogr.GetDriverByName('OSM')
    except:
        drv = None
    if drv is None:
        return 'skip'

    ds = ogr.Open(filename)
    if ds is None:
        if filename == 'data/test.osm' and gdal.GetLastErrorMsg().find('OSM XML detected, but Expat parser not available') == 0:
            return 'skip'

        gdaltest.post_reason('fail')
        return 'fail'

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
    if feat.GetFieldAsString('osm_id') != '3':
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
        return 'fail'

    # Test lines
    lyr = ds.GetLayer('lines')
    if lyr.GetGeomType() != ogr.wkbLineString:
        gdaltest.post_reason('fail')
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('osm_id') != '1':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('LINESTRING (2 49,3 50)')) != 0:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test polygons
    lyr = ds.GetLayer('polygons')
    if lyr.GetGeomType() != ogr.wkbPolygon:
        gdaltest.post_reason('fail')
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('osm_id') != '2':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    if filename == 'tmp/ogr_osm_3':
        if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('POLYGON ((2 49,2 50,3 50,3 49,2 49))')) != 0 :
            gdaltest.post_reason('fail')
            feat.DumpReadable()
            return 'fail'
    else:
        if ogrtest.check_feature_geometry(feat, ogr.CreateGeometryFromWkt('POLYGON ((2 49,3 49,3 50,2 50,2 49))')) != 0 :
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
    if feat.GetFieldAsString('osm_id') != '1':
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

    ds = None

    return 'success'

###############################################################################
# Test .osm

def ogr_osm_2():
    return ogr_osm_1('data/test.osm')

###############################################################################
# Test ogr2ogr

def ogr_osm_3():

    try:
        drv = ogr.GetDriverByName('OSM')
    except:
        drv = None
    if drv is None:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_ogr2ogr_path() is None:
        return 'skip'

    try:
        ogr.GetDriverByName('ESRI Shapefile').Delete('tmp/ogr_osm_3')
    except:
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' tmp/ogr_osm_3 data/test.pbf -progress')

    ret = ogr_osm_1('tmp/ogr_osm_3')

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/ogr_osm_3')

    return ret

###############################################################################
# Test optimization when reading only the points layer through a SQL request

def ogr_osm_4():

    try:
        drv = ogr.GetDriverByName('OSM')
    except:
        drv = None
    if drv is None:
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

gdaltest_list = [
    ogr_osm_1,
    ogr_osm_2,
    ogr_osm_3,
    ogr_osm_4,
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_osm' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  ElasticSearch driver testing (with fake server)
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2012, Even Rouault <even dot rouault at mines-paris dot org>
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

sys.path.append( '../pymod' )

import ogrtest
import gdaltest
from osgeo import gdal
from osgeo import ogr
from osgeo import osr

###############################################################################
# Test driver availability
#

def ogr_elasticsearch_init():

    ogrtest.elasticsearch_drv = None
    ogrtest.srs_wgs84 = osr.SpatialReference()
    ogrtest.srs_wgs84.SetFromUserInput('WGS84')

    try:
        ogrtest.elasticsearch_drv = ogr.GetDriverByName('ElasticSearch')
    except:
        pass

    if ogrtest.elasticsearch_drv is None:
        return 'skip'

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    return 'success'

###############################################################################
# Test writing into an nonexistent ElasticSearch datastore.

def ogr_elasticsearch_nonexistent_server():
    if ogrtest.elasticsearch_drv is None:
        return 'skip'

    with gdaltest.error_handler():
        ds = ogrtest.elasticsearch_drv.CreateDataSource(
            '/vsimem/nonexistent_host')
    if ds is not None:
        gdaltest.post_reason(
            'managed to open nonexistent ElasticSearch datastore.')
        return 'fail'

    return 'success'

###############################################################################
# Simple test

def ogr_elasticsearch_1():
    if ogrtest.elasticsearch_drv is None:
        return 'skip'

    gdal.FileFromMemBuffer("/vsimem/fakeelasticsearch/_status", """{"_shards":{"total":0,"successful":0,"failed":0},"indices":{}}""")

    ds = ogrtest.elasticsearch_drv.CreateDataSource("/vsimem/fakeelasticsearch")
    if ds is None:
        gdaltest.post_reason('did not managed to open ElasticSearch datastore')
        return 'fail'

    if ds.TestCapability(ogr.ODsCCreateLayer) == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.TestCapability(ogr.ODsCDeleteLayer) == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.TestCapability(ogr.ODsCCreateGeomFieldAfterCreateLayer) == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Failed index creation
    with gdaltest.error_handler():
        lyr = ds.CreateLayer('foo', srs = ogrtest.srs_wgs84, options = ['FID='])
    if lyr is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.GetLastErrorType() != gdal.CE_Failure:
        gdaltest.post_reason('fail')
        return 'fail'

    # Successful index creation
    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/foo&POSTFIELDS=', '{}')
    lyr = ds.CreateLayer('foo', srs = ogrtest.srs_wgs84, options = ['FID='])
    if lyr is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.GetLastErrorType() != gdal.CE_None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer(
        '/vsimem/fakeelasticsearch/foo/FeatureCollection/_mapping&POSTFIELDS'
        '={ "FeatureCollection": { "properties": { "type": '
        '{ "type": "string" }, "properties": { } } } }', '{}')

    # OVERWRITE an nonexistent layer.
    lyr = ds.CreateLayer('foo', geom_type = ogr.wkbNone,
                         options = ['OVERWRITE=TRUE', 'FID='])
    if gdal.GetLastErrorType() != gdal.CE_None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Simulate failed overwrite
    gdal.FileFromMemBuffer(
        '/vsimem/fakeelasticsearch/foo/_mapping/FeatureCollection',
        '{"foo":{"mappings":{"FeatureCollection":{}}}}')
    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/foo', '{}')
    with gdaltest.error_handler():
        lyr = ds.CreateLayer('foo', geom_type = ogr.wkbNone, options = ['OVERWRITE=TRUE'])
    if gdal.GetLastErrorType() != gdal.CE_Failure:
        gdaltest.post_reason('fail')
        return 'fail'

    # Successful overwrite
    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/foo/_mapping/FeatureCollection&CUSTOMREQUEST=DELETE', '{}')
    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/foo/FeatureCollection/&POSTFIELDS={ }', '{}')
    lyr = ds.CreateLayer('foo', geom_type = ogr.wkbNone, options = ['OVERWRITE=TRUE', 'BULK_INSERT=NO', 'FID='])
    if gdal.GetLastErrorType() != gdal.CE_None:
        gdaltest.post_reason('fail')
        return 'fail'

    if lyr.TestCapability(ogr.OLCFastFeatureCount) == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.TestCapability(ogr.OLCStringsAsUTF8) == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.TestCapability(ogr.OLCSequentialWrite) == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.TestCapability(ogr.OLCCreateField) == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.TestCapability(ogr.OLCCreateGeomField) == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    feat = ogr.Feature(lyr.GetLayerDefn())

    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/foo/FeatureCollection/&POSTFIELDS={ "properties": { } }', '{}')
    ret = lyr.CreateFeature(feat)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None

    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/foo&POSTFIELDS=', '{"error":"IndexAlreadyExistsException[[foo] already exists]","status":400}')
    with gdaltest.error_handler():
        lyr = ds.CreateLayer('foo', srs = ogrtest.srs_wgs84)
    if gdal.GetLastErrorType() != gdal.CE_Failure:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/foo/FeatureCollection/_mapping&POSTFIELDS={ "FeatureCollection": { "properties": { "type": { "type": "string" }, "properties": { "properties": { } }, "geometry": { "type": "geo_shape" } } } }""", "")

    ds.DeleteLayer(-1)
    ds.DeleteLayer(10)
    ret = ds.DeleteLayer(0)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/foo2&POSTFIELDS=', '{}')
    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/foo2/FeatureCollection/_mapping&POSTFIELDS={ "FeatureCollection": { "properties": { "type": { "type": "string" }, "properties": { "properties": { "str_field": { "type": "string", "index": "not_analyzed" }, "int_field": { "type": "integer", "store": "yes" }, "int64_field": { "type": "long", "index": "no" }, "real_field": { "type": "double" }, "real_field_unset": { "type": "double" }, "boolean_field": { "type": "boolean" }, "strlist_field": { "type": "string" }, "intlist_field": { "type": "integer" }, "int64list_field": { "type": "long" }, "reallist_field": { "type": "double" }, "date_field": { "type": "date", "format": "yyyy\/MM\/dd HH:mm:ss.SSSZZ||yyyy\/MM\/dd HH:mm:ss.SSS||yyyy\/MM\/dd" }, "datetime_field": { "type": "date", "format": "yyyy\/MM\/dd HH:mm:ss.SSSZZ||yyyy\/MM\/dd HH:mm:ss.SSS||yyyy\/MM\/dd" }, "time_field": { "type": "date", "format": "HH:mm:ss.SSS" }, "binary_field": { "type": "binary" } } }, "geometry": { "properties": { "type": { "type": "string" }, "coordinates": { "type": "geo_point" } } } }, "_meta": { "fields": { "strlist_field": "StringList", "intlist_field": "IntegerList", "int64list_field": "Integer64List", "reallist_field": "RealList" } } } }', '{}')
    lyr = ds.CreateLayer('foo2', srs = ogrtest.srs_wgs84, geom_type = ogr.wkbPoint, \
        options = ['BULK_INSERT=NO', 'FID=', 'STORED_FIELDS=int_field', 'NOT_ANALYZED_FIELDS=str_field', 'NOT_INDEXED_FIELDS=int64_field'])
    lyr.CreateField(ogr.FieldDefn('str_field', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('int_field', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('int64_field', ogr.OFTInteger64))
    lyr.CreateField(ogr.FieldDefn('real_field', ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn('real_field_unset', ogr.OFTReal))
    fld_defn = ogr.FieldDefn('boolean_field', ogr.OFTInteger)
    fld_defn.SetSubType(ogr.OFSTBoolean)
    lyr.CreateField(fld_defn)
    lyr.CreateField(ogr.FieldDefn('strlist_field', ogr.OFTStringList))
    lyr.CreateField(ogr.FieldDefn('intlist_field', ogr.OFTIntegerList))
    lyr.CreateField(ogr.FieldDefn('int64list_field', ogr.OFTInteger64List))
    lyr.CreateField(ogr.FieldDefn('reallist_field', ogr.OFTRealList))
    lyr.CreateField(ogr.FieldDefn('date_field', ogr.OFTDate))
    lyr.CreateField(ogr.FieldDefn('datetime_field', ogr.OFTDateTime))
    lyr.CreateField(ogr.FieldDefn('time_field', ogr.OFTTime))
    lyr.CreateField(ogr.FieldDefn('binary_field', ogr.OFTBinary))

    ret = lyr.SyncToDisk()
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('str_field', 'a')
    feat.SetField('int_field', 1)
    feat.SetField('int64_field', 123456789012)
    feat.SetField('real_field', 2.34)
    feat.SetField('boolean_field', 1)
    feat['strlist_field'] = ['a', 'b']
    feat['intlist_field'] =  [1,2]
    feat['int64list_field'] = [123456789012,2]
    feat['reallist_field'] = [1.23,4.56]
    feat['date_field'] = '2015/08/12'
    feat['datetime_field'] = '2015/08/12 12:34:56.789'
    feat['time_field'] = '12:34:56.789'
    feat.SetFieldBinaryFromHexString( 'binary_field', '0123465789ABCDEF' )
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(0 1)'))

    # Simulate server error
    with gdaltest.error_handler():
        ret = lyr.CreateFeature(feat)
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Success
    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/foo2/FeatureCollection/&POSTFIELDS={ "geometry": { "type": "POINT", "coordinates": [ 0.0, 1.0 ] }, "type": "Feature", "properties": { "str_field": "a", "int_field": 1, "int64_field": 123456789012, "real_field": 2.340000, "boolean_field": true, "strlist_field": [ "a", "b" ], "intlist_field": [ 1, 2 ], "int64list_field": [ 123456789012, 2 ], "reallist_field": [ 1.230000, 4.560000 ], "date_field": "2015\/08\/12", "datetime_field": "2015\/08\/12 12:34:56.789", "time_field": "12:34:56.789", "binary_field": "ASNGV4mrze8=" } }', '{ "_id": "my_id" }')
    ret = lyr.CreateFeature(feat)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if feat['_id'] != 'my_id':
        gdaltest.post_reason('fail')
        return 'fail'

    # DateTime with TZ
    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/foo2/FeatureCollection/&POSTFIELDS={ "properties": { "datetime_field": "2015\/08\/12 12:34:56.789+03:00" } }', '{}')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat['datetime_field'] = '2015/08/12 12:34:56.789+0300'
    ret = lyr.CreateFeature(feat)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # CreateFeature() with _id set
    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/foo2/FeatureCollection/my_id2&POSTFIELDS={ "properties": { } }', '{}')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat['_id'] = 'my_id2'
    ret = lyr.CreateFeature(feat)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Failed SetFeature because of missing _id
    feat = ogr.Feature(lyr.GetLayerDefn())
    with gdaltest.error_handler():
        ret = lyr.SetFeature(feat)
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Simulate server error
    feat['_id'] = 'my_id'
    with gdaltest.error_handler():
        ret = lyr.SetFeature(feat)
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/foo2/FeatureCollection/my_id&POSTFIELDS={ "properties": { } }', '{}')
    ret = lyr.SetFeature(feat)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # With explicit GEOM_MAPPING_TYPE=GEO_POINT
    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/foo3&POSTFIELDS=', '{}')
    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/foo3/FeatureCollection/_mapping&POSTFIELDS={ "FeatureCollection": { "properties": { "type": { "type": "string" }, "properties": { "properties": { } }, "geometry": { "properties": { "type": { "type": "string" }, "coordinates": { "type": "geo_point", "fielddata": { "format": "compressed", "precision": "1m" } } } } }, "_meta": { "fid": "ogc_fid" } } }', '{}')
    lyr = ds.CreateLayer('foo3', srs = ogrtest.srs_wgs84, options = ['GEOM_MAPPING_TYPE=GEO_POINT', 'GEOM_PRECISION=1m', 'BULK_INSERT=NO'])

    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/foo3/FeatureCollection/&POSTFIELDS={ "ogc_fid": 1, "geometry": { "type": "POINT", "coordinates": [ 0.5, 0.5 ] }, "type": "Feature", "properties": { } }', '{}')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING(0 0,1 1)'))
    ret = lyr.CreateFeature(feat)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None

    # Test explicit MAPPING first with error case
    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/foo4&POSTFIELDS=', '{}')
    with gdaltest.error_handler():
        lyr = ds.CreateLayer('foo4', srs = ogrtest.srs_wgs84, options = ['MAPPING={ "FeatureCollection": { "properties": {} }}'])
    if lyr is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test successful explicit MAPPING with inline JSon mapping
    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/foo4/FeatureCollection/_mapping&POSTFIELDS={ "FeatureCollection": { "properties": {} }}', '{}')
    lyr = ds.CreateLayer('foo4', srs = ogrtest.srs_wgs84, options = ['MAPPING={ "FeatureCollection": { "properties": {} }}'])
    if lyr is None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test successful explicit MAPPING with reference to file with mapping
    gdal.FileFromMemBuffer('/vsimem/map.txt', '{ "FeatureCollection": { "properties": { "foo": { "type": "string" } } }}')
    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/foo4/FeatureCollection/_mapping&POSTFIELDS={ "FeatureCollection": { "properties": { "foo": { "type": "string" } } }}', '{}')
    lyr = ds.CreateLayer('foo4', srs = ogrtest.srs_wgs84, options = ['MAPPING=/vsimem/map.txt'])
    gdal.Unlink('/vsimem/map.txt')
    if lyr is None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Geo_shape geometries

def ogr_elasticsearch_2():
    if ogrtest.elasticsearch_drv is None:
        return 'skip'

    ds = ogrtest.elasticsearch_drv.CreateDataSource("/vsimem/fakeelasticsearch")
    if ds is None:
        gdaltest.post_reason('did not managed to open ElasticSearch datastore')
        return 'fail'

    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/foo&POSTFIELDS=', '{}')
    gdal.Unlink('/vsimem/fakeelasticsearch/foo/_mapping/FeatureCollection')
    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/foo/FeatureCollection/_mapping&POSTFIELDS={ "FeatureCollection": { "properties": { "type": { "type": "string" }, "properties": { }, "geometry": { "type": "geo_shape" } } } }', '{}')

    lyr = ds.CreateLayer('foo', srs = ogrtest.srs_wgs84, options = ['BULK_INSERT=NO', 'FID='])
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION(POINT(0 1),LINESTRING(0 1,2 3),POLYGON((0 0,0 10,10 10,0 0),(1 1,1 9,9 9,1 1)),MULTIPOINT(0 1, 2 3),MULTILINESTRING((0 1,2 3),(4 5,6 7)),MULTIPOLYGON(((0 0,0 10,10 10,0 0),(1 1,1 9,9 9,1 1)),((-1 -1,-1 -9,-9 -9,-1 -1))))'))

    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/foo/FeatureCollection/&POSTFIELDS={ "geometry": { "type": "geometrycollection", "geometries": [ { "type": "point", "coordinates": [ 0.0, 1.0 ] }, { "type": "linestring", "coordinates": [ [ 0.0, 1.0 ], [ 2.0, 3.0 ] ] }, { "type": "polygon", "coordinates": [ [ [ 0.0, 0.0 ], [ 0.0, 10.0 ], [ 10.0, 10.0 ], [ 0.0, 0.0 ] ], [ [ 1.0, 1.0 ], [ 1.0, 9.0 ], [ 9.0, 9.0 ], [ 1.0, 1.0 ] ] ] }, { "type": "multipoint", "coordinates": [ [ 0.0, 1.0 ], [ 2.0, 3.0 ] ] }, { "type": "multilinestring", "coordinates": [ [ [ 0.0, 1.0 ], [ 2.0, 3.0 ] ], [ [ 4.0, 5.0 ], [ 6.0, 7.0 ] ] ] }, { "type": "multipolygon", "coordinates": [ [ [ [ 0.0, 0.0 ], [ 0.0, 10.0 ], [ 10.0, 10.0 ], [ 0.0, 0.0 ] ], [ [ 1.0, 1.0 ], [ 1.0, 9.0 ], [ 9.0, 9.0 ], [ 1.0, 1.0 ] ] ], [ [ [ -1.0, -1.0 ], [ -1.0, -9.0 ], [ -9.0, -9.0 ], [ -1.0, -1.0 ] ] ] ] } ] }, "type": "Feature", "properties": { } }', '{}')
    ret = lyr.CreateFeature(feat)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None

    # Same but with explicit GEOM_MAPPING_TYPE=GEO_SHAPE
    lyr = ds.CreateLayer('foo', srs = ogrtest.srs_wgs84, options = ['GEOM_MAPPING_TYPE=GEO_SHAPE', 'GEOM_PRECISION=1m', 'BULK_INSERT=NO', 'FID='])
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION(POINT(0 1),LINESTRING(0 1,2 3),POLYGON((0 0,0 10,10 10,0 0),(1 1,1 9,9 9,1 1)),MULTIPOINT(0 1, 2 3),MULTILINESTRING((0 1,2 3),(4 5,6 7)),MULTIPOLYGON(((0 0,0 10,10 10,0 0),(1 1,1 9,9 9,1 1)),((-1 -1,-1 -9,-9 -9,-1 -1))))'))

    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/foo/FeatureCollection/_mapping&POSTFIELDS={ "FeatureCollection": { "properties": { "type": { "type": "string" }, "properties": { "properties": { } }, "geometry": { "type": "geo_shape", "precision": "1m" } } } }', '{}')

    ret = lyr.CreateFeature(feat)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None


    return 'success'

###############################################################################
# Test bulk insert and layer name laundering

def ogr_elasticsearch_3():
    if ogrtest.elasticsearch_drv is None:
        return 'skip'

    ds = ogrtest.elasticsearch_drv.CreateDataSource("/vsimem/fakeelasticsearch")
    if ds is None:
        gdaltest.post_reason('did not managed to open ElasticSearch datastore')
        return 'fail'

    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/name_laundering&POSTFIELDS=', '{}')
    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/name_laundering/FeatureCollection/_mapping&POSTFIELDS={ "FeatureCollection": { "properties": { "type": { "type": "string" }, "properties": { "properties": { } }, "geometry": { "type": "geo_shape" } } } }', '{}')

    lyr = ds.CreateLayer('NAME/laundering', srs = ogrtest.srs_wgs84, options = ['FID='])
    feat = ogr.Feature(lyr.GetLayerDefn())
    ret = lyr.CreateFeature(feat)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None

    with gdaltest.error_handler():
        ret = lyr.SyncToDisk()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    feat = ogr.Feature(lyr.GetLayerDefn())
    ret = lyr.CreateFeature(feat)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None

    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/_bulk&POSTFIELDS={"index" :{"_index":"name_laundering", "_type":"FeatureCollection"}}
{ "properties": { } }

""", '{}')
    ret = lyr.SyncToDisk()
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test basic read functionality

def ogr_elasticsearch_4():
    if ogrtest.elasticsearch_drv is None:
        return 'skip'

    with gdaltest.error_handler():
        ds = ogr.Open('ES:/vsimem/fakeelasticsearch')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test case where there's no index and _status is not responding
    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/_cat/indices?h=i""", '')
    gdal.FileFromMemBuffer("/vsimem/fakeelasticsearch/_status", "")
    with gdaltest.error_handler():
        ds = ogr.Open('ES:/vsimem/fakeelasticsearch')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test case where there's no index
    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/_cat/indices?h=i""", '')
    gdal.FileFromMemBuffer("/vsimem/fakeelasticsearch/_status", """{"_shards":{"total":0,"successful":0,"failed":0},"indices":{}}""")
    ds = ogr.Open('ES:/vsimem/fakeelasticsearch')
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetLayerCount() != 0:
        gdaltest.post_reason('fail')
        print(ds.GetLayerCount())
        return 'fail'
    ds = None
    gdal.Unlink("""/vsimem/fakeelasticsearch/_status""")

    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/_cat/indices?h=i""", '\n')
    ds = ogr.Open('ES:/vsimem/fakeelasticsearch')
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetLayerCount() != 0:
        gdaltest.post_reason('fail')
        print(ds.GetLayerCount())
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/_cat/indices?h=i""", 'a_layer  \n')
    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/a_layer?pretty""", """
{
    "a_layer":
    {
        "mappings":
        {
            "FeatureCollection":
            {
                "_meta": {
                    "fid": "my_fid",
                    "geomfields": {
                            "a_geoshape": "LINESTRING"
                    },
                    "fields": {
                            "strlist_field": "StringList",
                            "intlist_field": "IntegerList",
                            "int64list_field": "Integer64List",
                            "doublelist_field": "RealList"
                    }
                },
                "properties":
                {
                    "type": { "type": "string" },
                    "a_geoshape":
                    {
                        "type": "geo_shape",
                    },
                    "a_geopoint":
                    {
                        "properties":
                        {
                            "coordinates":
                            {
                                "type": "geo_point"
                            }
                        }
                    },
                    "my_fid": { "type": "long" },
                    "properties" :
                    {
                        "properties":
                        {
                            "str_field": { "type": "string"},
                            "int_field": { "type": "integer"},
                            "int64_field": { "type": "long"},
                            "double_field": { "type": "double"},
                            "float_field": { "type": "float"},
                            "boolean_field": { "type": "boolean"},
                            "binary_field": { "type": "binary"},
                            "dt_field": { "type": "date"},
                            "date_field": { "type": "date", "format": "yyyy\/MM\/dd"},
                            "time_field": { "type": "date", "format": "HH:mm:ss.SSS"},
                            "strlist_field": { "type": "string"},
                            "intlist_field": { "type": "integer"},
                            "int64list_field": { "type": "long"},
                            "doublelist_field": { "type": "double"}
                        }
                    }
                }
            }
        }
    }
}
""")
    ds = ogr.Open('ES:/vsimem/fakeelasticsearch')
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if ds.GetLayerCount() != 1:
        gdaltest.post_reason('fail')
        print(ds.GetLayerCount())
        return 'fail'
    lyr = ds.GetLayer(0)

    with gdaltest.error_handler():
        lyr_defn = lyr.GetLayerDefn()
    idx = lyr_defn.GetFieldIndex("strlist_field")
    if lyr_defn.GetFieldDefn(idx).GetType() != ogr.OFTStringList:
        gdaltest.post_reason('fail')
        return 'fail'
    idx = lyr_defn.GetGeomFieldIndex("a_geoshape")
    if lyr_defn.GetGeomFieldDefn(idx).GetType() != ogr.wkbLineString:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetFIDColumn() != 'my_fid':
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/a_layer/FeatureCollection/_search?search_type=count&pretty""", """{
}""")
    with gdaltest.error_handler():
        lyr.GetFeatureCount()

    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/a_layer/FeatureCollection/_search?search_type=count&pretty""", """{
    "hits": null
}""")
    with gdaltest.error_handler():
        lyr.GetFeatureCount()

    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/a_layer/FeatureCollection/_search?search_type=count&pretty""", """{
    "hits": { "count": null }
}""")
    with gdaltest.error_handler():
        lyr.GetFeatureCount()

    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/a_layer/FeatureCollection/_search?search_type=count&pretty""", """{
    "hits":
    {
        "count": 3
    }
}""")
    fc = lyr.GetFeatureCount()
    if fc != 3:
        gdaltest.post_reason('fail')
        return 'fail'

    with gdaltest.error_handler():
        f = lyr.GetNextFeature()
    if f is not None:
        gdaltest.post_reason('fail')
        return 'fail'


    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/a_layer/FeatureCollection/_search?scroll=1m&size=100""", """{

}""")
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/a_layer/FeatureCollection/_search?scroll=1m&size=100""", """{
    "hits": null
}""")
    lyr.ResetReading()
    lyr.GetNextFeature()

    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/a_layer/FeatureCollection/_search?scroll=1m&size=100""", """{
    "hits": { "hits": null }
}""")
    lyr.ResetReading()
    lyr.GetNextFeature()

    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/a_layer/FeatureCollection/_search?scroll=1m&size=100""", """{
    "hits": { "hits": [ null, {}, { "_source":null } ] }
}""")
    lyr.ResetReading()
    lyr.GetNextFeature()

    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/a_layer/FeatureCollection/_search?scroll=1m&size=100""", """{
    "_scroll_id": "my_scrollid",
    "hits":
    {
        "hits":[
        {
            "_id": "my_id",
            "_source": {
                "type": "Feature",
                "my_fid": 5,
                "a_geopoint" : {
                    "type": "POINT",
                    "coordinates": [2,49]
                },
                "a_geoshape": {
                    "type": "linestring",
                    "coordinates": [[2,49],[3,50]]
                },
                "properties": {
                    "str_field": "foo",
                    "int_field": 1,
                    "int64_field": 123456789012,
                    "double_field": 1.23,
                    "float_field": 3.45,
                    "boolean_field": true,
                    "binary_field": "ASNGV4mrze8=",
                    "dt_field": "2015\/08\/12 12:34:56.789",
                    "date_field": "2015\/08\/12",
                    "time_field": "12:34:56.789",
                    "strlist_field": ["foo"],
                    "intlist_field": [1],
                    "int64list_field": [123456789012],
                    "doublelist_field": [1.23]
                }
            },
        }]
    }
}""")
    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/_search/scroll?scroll=1m&size=100&scroll_id=my_scrollid""", "{}")
    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/_search/scroll?scroll_id=my_scrollid&CUSTOMREQUEST=DELETE""", '{}')

    ds = ogr.Open('ES:/vsimem/fakeelasticsearch')
    lyr = ds.GetLayer(0)

    if lyr.GetLayerDefn().GetFieldCount() != 15:
        gdaltest.post_reason('fail')
        print(lyr.GetLayerDefn().GetFieldCount())
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/a_layer/FeatureCollection/_search?scroll=1m&size=100""", """{
    "_scroll_id": "my_scrollid",
    "hits":
    {
        "hits":[
        {
            "_id": "my_id",
            "_source": {
                "type": "Feature",
                "a_geopoint" : {
                    "type": "POINT",
                    "coordinates": [2,49]
                },
                "a_geoshape": {
                    "type": "linestring",
                    "coordinates": [[2,49],[3,50]]
                },
                "my_fid": 5,
                "properties": {
                    "str_field": "foo",
                    "int_field": 1,
                    "int64_field": 123456789012,
                    "double_field": 1.23,
                    "float_field": 3.45,
                    "boolean_field": true,
                    "binary_field": "ASNGV4mrze8=",
                    "dt_field": "2015\/08\/12 12:34:56.789",
                    "date_field": "2015\/08\/12",
                    "time_field": "12:34:56.789",
                    "strlist_field": ["foo"],
                    "intlist_field": [1],
                    "int64list_field": [123456789012],
                    "doublelist_field": [1.23]
                }
            },
        },
        {
            "_source": {
                "type": "Feature",
                "properties": {
                    "non_existing": "foo"
                }
            },
        }
        ]
    }
}""")

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if f.GetFID() != 5 or f['_id'] != 'my_id' or f['str_field'] != 'foo' or f['int_field'] != 1 or f['int64_field'] != 123456789012 or \
       f['double_field'] != 1.23 or f['float_field'] != 3.45 or f['boolean_field'] != 1 or \
       f['binary_field'] != '0123465789ABCDEF' or f['dt_field'] != '2015/08/12 12:34:56.789' or \
       f['date_field'] != '2015/08/12' or f['time_field'] != '12:34:56.789' or \
       f['strlist_field'] != ['foo'] or \
       f['intlist_field'] != [1] or \
       f['int64list_field'] != [123456789012] or \
       f['doublelist_field'] != [1.23] or \
       f['a_geopoint'].ExportToWkt() != 'POINT (2 49)' or \
       f['a_geoshape'].ExportToWkt() != 'LINESTRING (2 49,3 50)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    lyr.ResetReading()
    lyr.GetNextFeature()
    f = lyr.GetNextFeature()
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/_search/scroll?scroll=1m&size=100&scroll_id=my_scrollid""", """{
    "hits":
    {
        "hits":[
        {
            "_source": {
                "type": "Feature",
                "properties": {
                    "int_field": 2,
                }
            },
        }
        ]
    }
}""")
    f = lyr.GetNextFeature()
    if f['int_field'] != 2:
        gdaltest.post_reason('fail')
        return 'fail'


    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/_search/scroll?scroll=1m&size=100&scroll_id=my_scrollid""", """{
    "hits":
    {
        "hits":[]
    }
}""")
    f = lyr.GetNextFeature()
    if f is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr.SetSpatialFilterRect(1,48,3,50)
    lyr.ResetReading()
    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/a_layer/FeatureCollection/_search?scroll=1m&size=100&POSTFIELDS={ "query": { "filtered" : { "query" : { "match_all" : {} }, "filter": { "geo_shape": { "a_geoshape": { "shape": { "type": "envelope", "coordinates": [ [ 1.000000, 50.000000 ], [ 3.000000, 48.000000 ] ] } } } } } } }""", """{
    "hits":
    {
        "hits":[
        {
            "_source": {
                "type": "Feature",
                "properties": {
                    "int_field": 3,
                }
            },
        }
        ]
    }
}""")
    f = lyr.GetNextFeature()
    if f['int_field'] != 3:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr.SetSpatialFilterRect(1,1,48,3,50)
    lyr.ResetReading()
    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/a_layer/FeatureCollection/_search?scroll=1m&size=100&POSTFIELDS={ "query": { "filtered" : { "query" : { "match_all" : {} }, "filter": { "geo_bounding_box": { "a_geopoint.coordinates": { "top_left": { "lat": 50.000000, "lon": 1.000000 }, "bottom_right": { "lat": 48.000000, "lon": 3.000000 } } } } } } }""", """{
    "hits":
    {
        "hits":[
        {
            "_source": {
                "type": "Feature",
                "properties": {
                    "int_field": 4,
                }
            },
        }
        ]
    }
}""")
    f = lyr.GetNextFeature()
    if f['int_field'] != 4:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/a_layer/FeatureCollection/_search?search_type=count&pretty&POSTFIELDS={ "query": { "filtered" : { "query" : { "match_all" : {} }, "filter": { "geo_bounding_box": { "a_geopoint.coordinates": { "top_left": { "lat": 50.000000, "lon": 1.000000 }, "bottom_right": { "lat": 48.000000, "lon": 3.000000 } } } } } } }""","""{
    "hits":
    {
        "total": 10
    }
}""")
    fc = lyr.GetFeatureCount()
    if fc != 10:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr.SetSpatialFilter(None)
    lyr.SetSpatialFilterRect(-180,-90,180,90)
    with gdaltest.error_handler():
        lyr.SetSpatialFilter(-1, None)
        lyr.SetSpatialFilter(2, None)

    lyr.SetAttributeFilter("{ 'FOO' : 'BAR' }")
    lyr.ResetReading()
    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/a_layer/FeatureCollection/_search?scroll=1m&size=100&POSTFIELDS={ 'FOO' : 'BAR' }""", """{
    "hits":
    {
        "hits":[
        {
            "_source": {
                "type": "Feature",
                "properties": {
                    "int_field": 5,
                }
            },
        }
        ]
    }
}""")
    f = lyr.GetNextFeature()
    if f['int_field'] != 5:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr.SetAttributeFilter(None)

    sql_lyr = ds.ExecuteSQL("{ 'FOO' : 'BAR' }", dialect = 'ES')
    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/_search?scroll=1m&size=100&POSTFIELDS={ 'FOO' : 'BAR' }""", """{
    "hits":
    {
        "hits":[
        {
            "_index": "some_layer",
            "_type": "some_type",
            "_source": {
                "some_field": 5
            },
        }
        ]
    }
}""")
    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/some_layer/_mapping/some_type?pretty""", """
{
    "some_layer":
    {
        "mappings":
        {
            "some_type":
            {
                "properties":
                {
                    "some_field": { "type": "string"}
                }
            }
        }
    }
}
""")
    f = sql_lyr.GetNextFeature()
    if f['some_field'] != '5':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)

    # Invalid index
    with gdaltest.error_handler():
        bbox = lyr.GetExtent(geom_field = -1)

    # geo_shape
    bbox = lyr.GetExtent(geom_field = 0)

    # Invalid index
    with gdaltest.error_handler():
        bbox = lyr.GetExtent(geom_field = 2)

    # No response
    with gdaltest.error_handler():
        bbox = lyr.GetExtent(geom_field = 1)

    # Invalid response
    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/a_layer/FeatureCollection/_search?search_type=count&pretty&POSTFIELDS={ "aggs" : { "bbox" : { "geo_bounds" : { "field" : "a_geopoint.coordinates" } } } }""",
"""{
  "aggregations" : {
    "bbox" : {
      "bounds" : {
        "top_left" : {

        },
        "bottom_right" : {

        }
      }
    }
  }
}""")

    with gdaltest.error_handler():
        bbox = lyr.GetExtent(geom_field = 1)

    # Valid response
    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/a_layer/FeatureCollection/_search?search_type=count&pretty&POSTFIELDS={ "aggs" : { "bbox" : { "geo_bounds" : { "field" : "a_geopoint.coordinates" } } } }""",
"""{
  "aggregations" : {
    "bbox" : {
      "bounds" : {
        "top_left" : {
          "lat" : 10,
          "lon" : 1
        },
        "bottom_right" : {
          "lat" : 9,
          "lon" : 2
        }
      }
    }
  }
}""")
    bbox = lyr.GetExtent(geom_field = 1)
    if bbox != (1.0, 2.0, 9.0, 10.0):
        gdaltest.post_reason('fail')
        print(bbox)
        return 'fail'

    return 'success'

###############################################################################
# Write documents with non geojson structure

def ogr_elasticsearch_5():
    if ogrtest.elasticsearch_drv is None:
        return 'skip'

    gdal.FileFromMemBuffer("/vsimem/fakeelasticsearch/_status", """{"_shards":{"total":0,"successful":0,"failed":0},"indices":{}}""")
    ds = ogrtest.elasticsearch_drv.CreateDataSource("/vsimem/fakeelasticsearch")

    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/non_geojson&POSTFIELDS=', '')
    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/non_geojson/my_mapping/_mapping&POSTFIELDS={ "my_mapping": { "properties": { "str": { "type": "string", "store": "yes" }, "geometry": { "type": "geo_shape" } }, "_meta": { "fid": "ogc_fid" } } }', '{}')

    lyr = ds.CreateLayer('non_geojson', srs = ogrtest.srs_wgs84, options = ['MAPPING_NAME=my_mapping', 'BULK_INSERT=NO', 'STORE_FIELDS=YES'])
    lyr.CreateField(ogr.FieldDefn('str', ogr.OFTString))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetFID(5)
    feat['str'] = 'foo'

    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/non_geojson/my_mapping/&POSTFIELDS={ "ogc_fid": 5, "str": "foo" }', '{}')
    ret = lyr.CreateFeature(feat)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None

    ds = None

    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/_cat/indices?h=i""", 'non_geojson\n')
    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/non_geojson?pretty""", """
{
    "non_geojson":
    {
        "mappings":
        {
            "my_mapping":
            {
                "properties":
                {
                    "a_geoshape":
                    {
                        "type": "geo_shape",
                    },
                    "a_geopoint":
                    {
                        "properties":
                        {
                            "type": "string",
                            "coordinates":
                            {
                                "type": "geo_point"
                            }
                        }
                    },
                    "another_geopoint": { "type": "geo_point" },
                    "str_field": { "type": "string"},
                    "superobject": {
                        "properties": {
                            "subfield": { "type": "string" },
                            "subobject": {
                                "properties": {
                                    "subfield": { "type": "string" }
                                }
                            },
                            "another_geoshape": { "type": "geo_shape" }
                        }
                    }
                }
            }
        }
    }
}
""")
    ds = gdal.OpenEx("ES:/vsimem/fakeelasticsearch", gdal.OF_UPDATE, open_options = ['BULK_INSERT=NO'])
    lyr = ds.GetLayer(0)

    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/non_geojson/my_mapping/_search?scroll=1m&size=100""", """{
    "hits":
    {
        "hits":[
        {
            "_source": {
                "a_geopoint" : {
                    "type": "POINT",
                    "coordinates": [2,49]
                },
                "a_geoshape": {
                    "type": "linestring",
                    "coordinates": [[2,49],[3,50]]
                },
                "another_geopoint": "49.5,2.5",
                "str_field": "foo",
                "superobject": {
                    "subfield": 5,
                    "subobject":
                    {
                        "subfield": "foo",
                        "another_subfield": 6
                    },
                    "another_geoshape": {
                        "type": "point",
                        "coordinates": [3,50]
                    },
                    "another_geoshape2": {
                        "type": "point",
                        "coordinates": [2,50]
                    }
                }
            }
        },
        {
            "_source": {
                "another_field": "foo",
                "another_geopoint": { "lat": 49.1, "lon": 2.1 }
            }
        },
        {
            "_source": {
                "another_geopoint": "49.2,2.2"
            }
        },
        {
            "_source": {""" +
                # "this is the geohash format",
"""                "another_geopoint": "u09qv80meqh16ve02equ"
            }
        }]
    }
}""")
    index = lyr.GetLayerDefn().GetFieldIndex('another_field')
    if index < 0:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f['str_field'] != 'foo' or \
       f['superobject.subfield'] != '5' or \
       f['superobject.subobject.subfield'] != 'foo' or \
       f['superobject.subobject.another_subfield'] != 6 or \
       f['a_geopoint'].ExportToWkt() != 'POINT (2 49)' or \
       f['another_geopoint'].ExportToWkt() != 'POINT (2.5 49.5)' or \
       f['a_geoshape'].ExportToWkt() != 'LINESTRING (2 49,3 50)' or \
       f['superobject.another_geoshape'].ExportToWkt() != 'POINT (3 50)' or \
       f['superobject.another_geoshape2'].ExportToWkt() != 'POINT (2 50)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    f['_id'] = 'my_id'
    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/non_geojson/my_mapping/my_id&POSTFIELDS={ "a_geoshape": { "type": "linestring", "coordinates": [ [ 2.0, 49.0 ], [ 3.0, 50.0 ] ] }, "a_geopoint": { "type": "POINT", "coordinates": [ 2.0, 49.0 ] }, "another_geopoint": [ 2.5, 49.5 ], "superobject": { "another_geoshape": { "type": "point", "coordinates": [ 3.0, 50.0 ] }, "another_geoshape2": { "type": "point", "coordinates": [ 2.0, 50.0 ] }, "subfield": "5", "subobject": { "subfield": "foo", "another_subfield": 6 } }, "str_field": "foo" }""", "{}")
    ret = lyr.SetFeature(f)
    if ret != 0:
        gdaltest.post_reason('failure')
        return 'fail'

    f = lyr.GetNextFeature()
    if f['another_geopoint'].ExportToWkt() != 'POINT (2.1 49.1)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    f = lyr.GetNextFeature()
    if f['another_geopoint'].ExportToWkt() != 'POINT (2.2 49.2)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    # Test geohash
    f = lyr.GetNextFeature()
    if ogrtest.check_feature_geometry(f['another_geopoint'], 'POINT (2 49)') != 0:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    f = None
    lyr.CreateField(ogr.FieldDefn('superobject.subfield2', ogr.OFTString))
    with gdaltest.error_handler():
        lyr.CreateGeomField(ogr.GeomFieldDefn('superobject.another_geoshape3', ogr.wkbPoint))
    f = ogr.Feature(lyr.GetLayerDefn())
    f['superobject.subfield2'] = 'foo'
    f['superobject.another_geoshape3'] = ogr.CreateGeometryFromWkt('POINT (3 50)')
    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/non_geojson/my_mapping/_mapping&POSTFIELDS={ "my_mapping": { "properties": { "str_field": { "type": "string" }, "superobject": { "properties": { "subfield": { "type": "string" }, "subobject": { "properties": { "subfield": { "type": "string" }, "another_subfield": { "type": "integer" } } }, "subfield2": { "type": "string" }, "another_geoshape": { "type": "geo_shape" }, "another_geoshape2": { "type": "geo_shape" }, "another_geoshape3": { "type": "geo_shape" } } }, "another_field": { "type": "string" }, "a_geoshape": { "type": "geo_shape" }, "a_geopoint": { "properties": { "type": { "type": "string" }, "coordinates": { "type": "geo_point" } } }, "another_geopoint": { "type": "geo_point" } }, "_meta": { "geomfields": { "superobject.another_geoshape2": "POINT", "superobject.another_geoshape3": "POINT" } } } }""", '{}')
    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/non_geojson/my_mapping/&POSTFIELDS={ "superobject": { "another_geoshape3": { "type": "point", "coordinates": [ 3.0, 50.0 ] }, "subfield2": "foo" } }""", "{}")
    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/non_geojson/my_mapping/_search?search_type=count&pretty""", "{}")
    lyr.CreateFeature(f)

    ds = gdal.OpenEx("ES:/vsimem/fakeelasticsearch", open_options = ['FEATURE_COUNT_TO_ESTABLISH_FEATURE_DEFN=0'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['str_field'] != 'foo' or \
       f['superobject.subfield'] != '5' or \
       f['a_geopoint'].ExportToWkt() != 'POINT (2 49)' or \
       f['a_geoshape'].ExportToWkt() != 'LINESTRING (2 49,3 50)' or \
       f['superobject.another_geoshape'].ExportToWkt() != 'POINT (3 50)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    ds = gdal.OpenEx("ES:/vsimem/fakeelasticsearch", open_options = ['FEATURE_COUNT_TO_ESTABLISH_FEATURE_DEFN=0', 'FLATTEN_NESTED_ATTRIBUTES=FALSE'])
    lyr = ds.GetLayer(0)
    index = lyr.GetLayerDefn().GetFieldIndex('another_field')
    if index >= 0:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f['str_field'] != 'foo' or \
       f['superobject'] != '{ "subfield": 5, "subobject": { "subfield": "foo", "another_subfield": 6 }, "another_geoshape": { "type": "point", "coordinates": [ 3, 50 ] }, "another_geoshape2": { "type": "point", "coordinates": [ 2, 50 ] } }' or \
       f['a_geopoint'].ExportToWkt() != 'POINT (2 49)' or \
       f['a_geoshape'].ExportToWkt() != 'LINESTRING (2 49,3 50)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    ds = gdal.OpenEx("ES:/vsimem/fakeelasticsearch", gdal.OF_UPDATE, open_options = ['JSON_FIELD=YES'])
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['str_field'] != 'foo' or \
       f['superobject.subfield'] != '5' or \
       f['_json'].find('{') != 0 or \
       f['a_geopoint'].ExportToWkt() != 'POINT (2 49)' or \
       f['a_geoshape'].ExportToWkt() != 'LINESTRING (2 49,3 50)' or \
       f['superobject.another_geoshape'].ExportToWkt() != 'POINT (3 50)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    f['_id'] = 'my_id'
    f['_json'] = '{ "foo": "bar" }'
    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/non_geojson/my_mapping/my_id&POSTFIELDS={ "foo": "bar" }""", "{}")
    ret = lyr.SetFeature(f)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test reading circle and envelope geometries

def ogr_elasticsearch_6():
    if ogrtest.elasticsearch_drv is None:
        return 'skip'

    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/_cat/indices?h=i""", 'non_standard_geometries\n')
    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/non_standard_geometries?pretty""", """
{
    "non_standard_geometries":
    {
        "mappings":
        {
            "my_mapping":
            {
                "properties":
                {
                    "geometry":
                    {
                        "type": "geo_shape",
                    }
                }
            }
        }
    }
}
""")
    ds = gdal.OpenEx("ES:/vsimem/fakeelasticsearch")
    lyr = ds.GetLayer(0)

    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/non_standard_geometries/my_mapping/_search?scroll=1m&size=100""", """{
    "hits":
    {
        "hits":[
        {
            "_source": {
                "geometry": {
                    "type": "envelope",
                    "coordinates": [[2,49],[3,50]]
                }
            }
        },
        {
            "_source": {
                "geometry": {
                    "type": "circle",
                    "coordinates": [2,49],
                    "radius": 100
                }
            }
        },
        {
            "_source": {
                "geometry": {
                    "type": "circle",
                    "coordinates": [2,49],
                    "radius": "100m"
                }
            }
        },
        {
            "_source": {
                "geometry": {
                    "type": "circle",
                    "coordinates": [2,49],
                    "radius": "0.1km"
                }
            }
        }]
    }
}""")
    f = lyr.GetNextFeature()
    if f['geometry'].ExportToWkt() != 'POLYGON ((2 49,3 49,3 50,2 50,2 49))':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    f = lyr.GetNextFeature()
    ref_txt = f['geometry'].ExportToWkt()
    if ref_txt.find('POLYGON ((') != 0:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    f = lyr.GetNextFeature()
    if f['geometry'].ExportToWkt() != ref_txt:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    f = lyr.GetNextFeature()
    if f['geometry'].ExportToWkt() != ref_txt:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################
# Test WRITE_MAPPING option

def ogr_elasticsearch_7():
    if ogrtest.elasticsearch_drv is None:
        return 'skip'

    gdal.FileFromMemBuffer("/vsimem/fakeelasticsearch/_status", """{"_shards":{"total":0,"successful":0,"failed":0},"indices":{}}""")

    ds = ogrtest.elasticsearch_drv.CreateDataSource("/vsimem/fakeelasticsearch")

    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/test_write_mapping&POSTFIELDS=', '{}')
    lyr = ds.CreateLayer('test_write_mapping', srs = ogrtest.srs_wgs84, options = ['WRITE_MAPPING=/vsimem/map.txt', 'FID='])
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    ds = None

    f = gdal.VSIFOpenL('/vsimem/map.txt', 'rb')
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
    gdal.VSIFCloseL(f)

    gdal.Unlink('/vsimem/map.txt')

    if data != '{ "FeatureCollection": { "properties": { "type": { "type": "string" }, "properties": { "properties": { } }, "geometry": { "type": "geo_shape" } } } }':
        gdaltest.post_reason('fail')
        print(data)
        return 'fail'

    return 'success'

###############################################################################
# Test SRS support

def ogr_elasticsearch_8():
    if ogrtest.elasticsearch_drv is None:
        return 'skip'

    gdal.FileFromMemBuffer("/vsimem/fakeelasticsearch/_status", """{"_shards":{"total":0,"successful":0,"failed":0},"indices":{}}""")

    ds = ogrtest.elasticsearch_drv.CreateDataSource("/vsimem/fakeelasticsearch")

    # No SRS
    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/no_srs&POSTFIELDS=', '{}')
    # Will emit a warning
    gdal.ErrorReset()
    with gdaltest.error_handler():
        lyr = ds.CreateLayer('no_srs')
    if gdal.GetLastErrorType() != gdal.CE_Warning:
        gdaltest.post_reason('warning expected')
        return 'fail'
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (-100 -200)'))
    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/no_srs/FeatureCollection/_mapping&POSTFIELDS={ "FeatureCollection": { "properties": { "type": { "type": "string" }, "properties": { "properties": { } }, "geometry": { "type": "geo_shape" } }, "_meta": { "fid": "ogc_fid" } } }""", '{}')
    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/_bulk&POSTFIELDS={"index" :{"_index":"no_srs", "_type":"FeatureCollection"}}
{ "ogc_fid": 1, "geometry": { "type": "point", "coordinates": [ -100.0, -200.0 ] }, "type": "Feature", "properties": { } }

""", "{}")
    # Will emit a warning
    gdal.ErrorReset()
    with gdaltest.error_handler():
        ret = lyr.CreateFeature(f)
    if gdal.GetLastErrorType() != gdal.CE_Warning:
        gdaltest.post_reason('warning expected')
        return 'fail'
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Non EPSG-4326 SRS
    other_srs = osr.SpatialReference()
    other_srs.ImportFromEPSG(32631)
    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/other_srs&POSTFIELDS=""", "{}")
    lyr = ds.CreateLayer('other_srs', srs = other_srs)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (500000 0)'))
    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/other_srs/FeatureCollection/_mapping&POSTFIELDS={ "FeatureCollection": { "properties": { "type": { "type": "string" }, "properties": { "properties": { } }, "geometry": { "type": "geo_shape" } }, "_meta": { "fid": "ogc_fid" } } }""", '{}')
    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/_bulk&POSTFIELDS={"index" :{"_index":"other_srs", "_type":"FeatureCollection"}}
{ "ogc_fid": 1, "geometry": { "type": "point", "coordinates": [ 3.0, 0.0 ] }, "type": "Feature", "properties": { } }

""", "{}")
    ret = lyr.CreateFeature(f)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Cleanup

def ogr_elasticsearch_cleanup():
    if ogrtest.elasticsearch_drv is None:
        return 'skip'

    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', None)

    for f in gdal.ReadDir('/vsimem/fakeelasticsearch'):
        gdal.Unlink('/vsimem/fakeelasticsearch/' + f)

    return 'success'


gdaltest_list = [
    ogr_elasticsearch_init,
    ogr_elasticsearch_nonexistent_server,
    ogr_elasticsearch_1,
    ogr_elasticsearch_2,
    ogr_elasticsearch_3,
    ogr_elasticsearch_4,
    ogr_elasticsearch_5,
    ogr_elasticsearch_6,
    ogr_elasticsearch_7,
    ogr_elasticsearch_8,
    ogr_elasticsearch_cleanup,
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_elasticsearch' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()


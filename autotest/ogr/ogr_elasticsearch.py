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

###############################################################################
# Test driver availability
#

def ogr_elasticsearch_init():

    ogrtest.elasticsearch_drv = None

    try:
        ogrtest.elasticsearch_drv = ogr.GetDriverByName('ElasticSearch')
    except:
        pass
        
    if ogrtest.elasticsearch_drv is None:
        return 'skip'
    
    gdal.SetConfigOption('CPL_CURL_ENABLE_VSIMEM', 'YES')

    return 'success'

###############################################################################
# Test writing into an inexisting ElasticSearch datastore !

def ogr_elasticsearch_unexisting_server():
    if ogrtest.elasticsearch_drv is None:
        return 'skip'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogrtest.elasticsearch_drv.CreateDataSource("/vsimem/non_existing_host")
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('managed to open inexisting ElasticSearch datastore !')
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
    
    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/foo&POSTFIELDS=', '{}')
    lyr = ds.CreateLayer('foo')
    if lyr is None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/foo/FeatureCollection/_mapping&POSTFIELDS={ "FeatureCollection": { "properties": { "type": { "store": "yes", "type": "string" }, "properties": { } } } }', '{}')

    # OVERWRITE an inexisting layer
    lyr = ds.CreateLayer('foo', geom_type = ogr.wkbNone, options = ['OVERWRITE=TRUE'])

    # Simulate failed overwrite
    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/foo', '{}')
    gdal.PushErrorHandler()
    lyr = ds.CreateLayer('foo', geom_type = ogr.wkbNone, options = ['OVERWRITE=TRUE'])
    gdal.PopErrorHandler()
    
    # Successful overwrite
    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/foo&CUSTOMREQUEST=DELETE', '{}')
    lyr = ds.CreateLayer('foo', geom_type = ogr.wkbNone, options = ['OVERWRITE=TRUE'])
    
    feat = ogr.Feature(lyr.GetLayerDefn())

    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/foo/FeatureCollection/&POSTFIELDS={ "properties": { } }', '')
    ret = lyr.CreateFeature(feat)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None
    
    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/foo&POSTFIELDS=', '{"error":"IndexAlreadyExistsException[[foo] already exists]","status":400}')
    gdal.PushErrorHandler()
    lyr = ds.CreateLayer('foo')
    gdal.PopErrorHandler()
    if lyr is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    
    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/foo2&POSTFIELDS=', '{}')
    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/foo2/FeatureCollection/_mapping&POSTFIELDS={ "FeatureCollection": { "properties": { "type": { "store": "yes", "type": "string" }, "properties": { "properties": { "str_field": { "store": "yes", "type": "string" }, "int_field": { "store": "yes", "type": "integer" }, "int64_field": { "store": "yes", "type": "long" }, "real_field": { "store": "yes", "type": "double" }, "real_field_unset": { "store": "yes", "type": "double" }, "boolean_field": { "store": "yes", "type": "boolean" }, "strlist_field": { "store": "yes", "type": "string" }, "intlist_field": { "store": "yes", "type": "integer" }, "int64list_field": { "store": "yes", "type": "long" }, "reallist_field": { "store": "yes", "type": "double" }, "date_field": { "store": "yes", "type": "date", "format": "yyyy\/MM\/dd HH:mm:ss.SSS||yyyy\/MM\/dd" }, "datetime_field": { "store": "yes", "type": "date", "format": "yyyy\/MM\/dd HH:mm:ss.SSS||yyyy\/MM\/dd" }, "time_field": { "store": "yes", "type": "date", "format": "HH:mm:ss.SSS" }, "binary_field": { "store": "yes", "type": "binary" } } }, "geometry": { "properties": { "type": { "store": "yes", "type": "string" }, "coordinates": { "store": "yes", "type": "geo_point" } } } } } }', '{}')
    lyr = ds.CreateLayer('foo2', geom_type = ogr.wkbPoint)
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

    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/foo2/FeatureCollection/&POSTFIELDS={ "geometry": { "type": "POINT", "coordinates": [ 0.000000, 1.000000 ] }, "type": "Feature", "properties": { "str_field": "a", "int_field": 1, "int64_field": 123456789012, "real_field": 2.340000, "boolean_field": true, "strlist_field": [ "a", "b" ], "intlist_field": [ 1, 2 ], "int64list_field": [ 123456789012, 2 ], "reallist_field": [ 1.230000, 4.560000 ], "date_field": "2015\/08\/12", "datetime_field": "2015\/08\/12 12:34:56.789", "time_field": "12:34:56.789", "binary_field": "ASNGV4mrze8=" } }', '{}')
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
    ret = lyr.CreateFeature(feat)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None

    # With explicit GEOM_MAPPING_TYPE=GEO_POINT
    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/foo3&POSTFIELDS=', '{}')
    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/foo3/FeatureCollection/_mapping&POSTFIELDS={ "FeatureCollection": { "properties": { "type": { "store": "yes", "type": "string" }, "properties": { }, "geometry": { "properties": { "type": { "store": "yes", "type": "string" }, "coordinates": { "store": "yes", "type": "geo_point", "fielddata": { "format": "compressed", "precision": "1m" } } } } } } }', '{}')
    lyr = ds.CreateLayer('foo3', options = ['GEOM_MAPPING_TYPE=GEO_POINT', 'GEOM_PRECISION=1m'])

    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/foo3/FeatureCollection/&POSTFIELDS={ "geometry": { "type": "POINT", "coordinates": [ 0.500000, 0.500000 ] }, "type": "Feature", "properties": { } }', '{}')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING(0 0,1 1)'))
    ret = lyr.CreateFeature(feat)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None
    
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
    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/foo/FeatureCollection/_mapping&POSTFIELDS={ "FeatureCollection": { "properties": { "type": { "store": "yes", "type": "string" }, "properties": { }, "geometry": { "type": "geo_shape" } } } }', '{}')
   
    lyr = ds.CreateLayer('foo')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION(POINT(0 1),LINESTRING(0 1,2 3),POLYGON((0 0,0 10,10 10,0 0),(1 1,1 9,9 9,1 1)),MULTIPOINT(0 1, 2 3),MULTILINESTRING((0 1,2 3),(4 5,6 7)),MULTIPOLYGON(((0 0,0 10,10 10,0 0),(1 1,1 9,9 9,1 1)),((-1 -1,-1 -9,-9 -9,-1 -1))))'))

    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/foo/FeatureCollection/&POSTFIELDS={ "geometry": { "type": "geometrycollection", "geometries": [ { "type": "point", "coordinates": [ 0.0, 1.0 ] }, { "type": "linestring", "coordinates": [ [ 0.0, 1.0 ], [ 2.0, 3.0 ] ] }, { "type": "polygon", "coordinates": [ [ [ 0.0, 0.0 ], [ 0.0, 10.0 ], [ 10.0, 10.0 ], [ 0.0, 0.0 ] ], [ [ 1.0, 1.0 ], [ 1.0, 9.0 ], [ 9.0, 9.0 ], [ 1.0, 1.0 ] ] ] }, { "type": "multipoint", "coordinates": [ [ 0.0, 1.0 ], [ 2.0, 3.0 ] ] }, { "type": "multilinestring", "coordinates": [ [ [ 0.0, 1.0 ], [ 2.0, 3.0 ] ], [ [ 4.0, 5.0 ], [ 6.0, 7.0 ] ] ] }, { "type": "multipolygon", "coordinates": [ [ [ [ 0.0, 0.0 ], [ 0.0, 10.0 ], [ 10.0, 10.0 ], [ 0.0, 0.0 ] ], [ [ 1.0, 1.0 ], [ 1.0, 9.0 ], [ 9.0, 9.0 ], [ 1.0, 1.0 ] ] ], [ [ [ -1.0, -1.0 ], [ -1.0, -9.0 ], [ -9.0, -9.0 ], [ -1.0, -1.0 ] ] ] ] } ] }, "properties": { } }', '{}')
    ret = lyr.CreateFeature(feat)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None

    # Same but with explicit GEOM_MAPPING_TYPE=GEO_SHAPE
    lyr = ds.CreateLayer('foo', options = ['GEOM_MAPPING_TYPE=GEO_SHAPE', 'GEOM_PRECISION=1m'])
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION(POINT(0 1),LINESTRING(0 1,2 3),POLYGON((0 0,0 10,10 10,0 0),(1 1,1 9,9 9,1 1)),MULTIPOINT(0 1, 2 3),MULTILINESTRING((0 1,2 3),(4 5,6 7)),MULTIPOLYGON(((0 0,0 10,10 10,0 0),(1 1,1 9,9 9,1 1)),((-1 -1,-1 -9,-9 -9,-1 -1))))'))

    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/foo/FeatureCollection/_mapping&POSTFIELDS={ "FeatureCollection": { "properties": { "type": { "store": "yes", "type": "string" }, "properties": { }, "geometry": { "type": "geo_shape", "precision": "1m" } } } }', '{}')

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
    gdal.FileFromMemBuffer('/vsimem/fakeelasticsearch/name_laundering/FeatureCollection/_mapping&POSTFIELDS={ "FeatureCollection": { "properties": { "type": { "store": "yes", "type": "string" }, "properties": { }, "geometry": { "type": "geo_shape" } } } }', '{}')
   
    lyr = ds.CreateLayer('NAME/laundering', options = ['BULK_INSERT=YES'])
    feat = ogr.Feature(lyr.GetLayerDefn())
    ret = lyr.CreateFeature(feat)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = None

    gdal.PushErrorHandler()
    ret = lyr.SyncToDisk()
    gdal.PopErrorHandler()
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

    gdal.PushErrorHandler()
    ds = ogr.Open('ES:/vsimem/fakeelasticsearch')
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

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
                            "coordinates":
                            {
                                "type": "geo_point"
                            }
                        }
                    },
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

    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/a_layer/FeatureCollection/_search?search_type=count&pretty""", """{
}""")
    gdal.PushErrorHandler()
    lyr.GetFeatureCount()
    gdal.PopErrorHandler()

    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/a_layer/FeatureCollection/_search?search_type=count&pretty""", """{
    "hits": null
}""")
    gdal.PushErrorHandler()
    lyr.GetFeatureCount()
    gdal.PopErrorHandler()

    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/a_layer/FeatureCollection/_search?search_type=count&pretty""", """{
    "hits": { "count": null }
}""")
    gdal.PushErrorHandler()
    lyr.GetFeatureCount()
    gdal.PopErrorHandler()
    
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

    gdal.PushErrorHandler()
    f = lyr.GetNextFeature()
    gdal.PopErrorHandler()
    if f is not None:
        gdaltest.post_reason('fail')
        return 'fail'


    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/a_layer/FeatureCollection/_search?scroll=1m&size=100&pretty""", """{

}""")
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/a_layer/FeatureCollection/_search?scroll=1m&size=100&pretty""", """{
    "hits": null
}""")
    lyr.ResetReading()
    lyr.GetNextFeature()

    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/a_layer/FeatureCollection/_search?scroll=1m&size=100&pretty""", """{
    "hits": { "hits": null }
}""")
    lyr.ResetReading()
    lyr.GetNextFeature()

    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/a_layer/FeatureCollection/_search?scroll=1m&size=100&pretty""", """{
    "hits": { "hits": [ null, {}, { "_source":null } ] }
}""")
    lyr.ResetReading()
    lyr.GetNextFeature()

    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/a_layer/FeatureCollection/_search?scroll=1m&size=100&pretty""", """{
    "_scroll_id": "my_scrollid",
    "hits":
    {
        "hits":[
        {
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
                    "time_field": "12:34:56.789"
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
    if f['str_field'] != 'foo' or f['int_field'] != 1 or f['int64_field'] != 123456789012 or \
       f['double_field'] != 1.23 or f['float_field'] != 3.45 or f['boolean_field'] != 1 or \
       f['binary_field'] != '0123465789ABCDEF' or f['dt_field'] != '2015/08/12 12:34:56.789' or \
       f['date_field'] != '2015/08/12' or f['time_field'] != '12:34:56.789' or \
       f['a_geopoint'].ExportToWkt() != 'POINT (2 49)' or \
       f['a_geoshape'].ExportToWkt() != 'LINESTRING (2 49,3 50)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    
    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/_search/scroll?scroll_id=my_scrollid&CUSTOMREQUEST=DELETE""", '{}')
    lyr.ResetReading()
    lyr.GetNextFeature()
    f = lyr.GetNextFeature()
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/_search/scroll?scroll=1m&size=100&scroll_id=my_scrollid&pretty""", """{
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


    gdal.FileFromMemBuffer("""/vsimem/fakeelasticsearch/_search/scroll?scroll=1m&size=100&scroll_id=my_scrollid&pretty""", """{
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
    ogr_elasticsearch_1,
    ogr_elasticsearch_2,
    ogr_elasticsearch_3,
    ogr_elasticsearch_4,
    ogr_elasticsearch_cleanup,
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_elasticsearch' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()


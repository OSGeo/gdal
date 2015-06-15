#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  MongoDB driver testing.
# Author:   Even Rouault <even dot rouault at spatialys dot com>
#
###############################################################################
# Copyright (c) 2015, Even Rouault <even dot rouault at spatialys dot com>
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
import uuid

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import gdal, ogr, osr

###############################################################################
# Test if driver is available

def ogr_mongodb_init():

    ogrtest.mongodb_drv = None

    try:
        ogrtest.mongodb_drv = ogr.GetDriverByName('MongoDB')
    except:
        pass

    if ogrtest.mongodb_drv is None:
        return 'skip'

    if 'MONGODB_TEST_HOST' in os.environ:
        ogrtest.mongodb_test_host = os.environ['MONGODB_TEST_HOST']
        if 'MONGODB_TEST_PORT' in os.environ:
            ogrtest.mongodb_test_port = os.environ['MONGODB_TEST_PORT']
        else:
            ogrtest.mongodb_test_port = 27017
        if 'MONGODB_TEST_DBNAME' in os.environ:
            ogrtest.mongodb_test_dbname = os.environ['MONGODB_TEST_DBNAME']
        else:
            ogrtest.mongodb_test_dbname = 'gdalautotest'
        if 'MONGODB_TEST_USER' in os.environ:
            ogrtest.mongodb_test_user = os.environ['MONGODB_TEST_USER']
        else:
            ogrtest.mongodb_test_user = None
        if 'MONGODB_TEST_PASSWORD' in os.environ:
            ogrtest.mongodb_test_password = os.environ['MONGODB_TEST_PASSWORD']
        else:
            ogrtest.mongodb_test_password = None
    else:
        ogrtest.mongodb_test_host = 'ds047612.mongolab.com'
        ogrtest.mongodb_test_port = 47612
        ogrtest.mongodb_test_dbname = 'gdalautotest'
        ogrtest.mongodb_test_user = 'user'
        ogrtest.mongodb_test_password = 'password'

    if ogrtest.mongodb_test_user is not None:
        ogrtest.mongodb_test_uri = "mongodb://%s:%s@%s:%d/%s" % (ogrtest.mongodb_test_user, ogrtest.mongodb_test_password, ogrtest.mongodb_test_host, ogrtest.mongodb_test_port, ogrtest.mongodb_test_dbname)
    else:
        ogrtest.mongodb_test_uri = "mongodb://%s:%d/%s" % (ogrtest.mongodb_test_host, ogrtest.mongodb_test_port, ogrtest.mongodb_test_dbname)

    ogrtest.mongodb_layer_name = None
    ogrtest.mongodb_layer_name_no_ogr_metadata = None
    ogrtest.mongodb_layer_name_guess_types = None
    ogrtest.mongodb_layer_name_with_2d_index = None
    ogrtest.mongodb_layer_name_no_spatial_index = None

    ds = ogr.Open(ogrtest.mongodb_test_uri)
    if ds is None:
        print('cannot open %s' % ogrtest.mongodb_test_uri)
        ogrtest.mongodb_drv = None
        return 'skip'

    return 'success'

###############################################################################
# Test various open methods

def ogr_mongodb_1():
    if ogrtest.mongodb_drv is None:
        return 'skip'

    # The below options must be used the very first time mongoDB is initialized
    # otherwise they will get ignored
    open_options = []
    open_options += ['SSL_PEM_KEY_FILE=bla']
    open_options += ['SSL_PEM_KEY_PASSWORD=bla']
    open_options += ['SSL_CA_FILE=bla']
    open_options += ['SSL_CRL_FILE=bla']
    open_options += ['SSL_ALLOW_INVALID_CERTIFICATES=YES']
    open_options += ['SSL_ALLOW_INVALID_HOSTNAMES=YES']
    open_options += ['FIPS_MODE=YES']
    gdal.PushErrorHandler()
    ds = gdal.OpenEx('mongodb:', open_options = open_options)
    gdal.PopErrorHandler()

    # Might work or not depending on how the db is set up
    gdal.PushErrorHandler()
    ds = ogr.Open("mongodb:")
    gdal.PopErrorHandler()

    # Wrong URI
    gdal.PushErrorHandler()
    ds = ogr.Open("mongodb://")
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # URI to non exhisting host
    gdal.PushErrorHandler()
    ds = ogr.Open("mongodb://non_existing")
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Connect to non exhisting host
    gdal.PushErrorHandler()
    ds = gdal.OpenEx('mongodb:', open_options = ['HOST=non_existing'] )
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # All arguments split up
    open_options = []
    open_options += ['HOST=' + ogrtest.mongodb_test_host ]
    open_options += ['PORT=' + str(ogrtest.mongodb_test_port) ]
    open_options += ['DBNAME=' + ogrtest.mongodb_test_dbname ]
    if ogrtest.mongodb_test_user is not None:
        open_options += ['USER=' + ogrtest.mongodb_test_user ]
        open_options += ['PASSWORD=' + ogrtest.mongodb_test_password ]
    ds = gdal.OpenEx('mongodb:', open_options = open_options)
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Without DBNAME
    open_options = []
    open_options += ['HOST=' + ogrtest.mongodb_test_host ]
    open_options += ['PORT=' + str(ogrtest.mongodb_test_port) ]
    if ogrtest.mongodb_test_user is not None:
        open_options += ['AUTH_DBNAME=' + ogrtest.mongodb_test_dbname ]
        open_options += ['USER=' + ogrtest.mongodb_test_user ]
        open_options += ['PASSWORD=' + ogrtest.mongodb_test_password ]
    gdal.PushErrorHandler()
    # Will succeed only against server in single mode
    ds = gdal.OpenEx('mongodb:', open_options = open_options)
    gdal.PopErrorHandler()

    # A few error cases with authentication
    if ogrtest.mongodb_test_user is not None:
        open_options = []
        open_options += ['HOST=' + ogrtest.mongodb_test_host ]
        open_options += ['PORT=' + str(ogrtest.mongodb_test_port) ]
        open_options += ['DBNAME=' + ogrtest.mongodb_test_dbname ]
        # Missing user and password
        gdal.PushErrorHandler()
        ds = gdal.OpenEx('mongodb:', open_options = open_options)
        gdal.PopErrorHandler()
        if ds is not None:
            gdaltest.post_reason('fail')
            return 'fail'

        open_options = []
        open_options += ['HOST=' + ogrtest.mongodb_test_host ]
        open_options += ['PORT=' + str(ogrtest.mongodb_test_port) ]
        open_options += ['DBNAME=' + ogrtest.mongodb_test_dbname ]
        open_options += ['USER=' + ogrtest.mongodb_test_user ]
        # Missing password
        gdal.PushErrorHandler()
        ds = gdal.OpenEx('mongodb:', open_options = open_options)
        gdal.PopErrorHandler()
        if ds is not None:
            gdaltest.post_reason('fail')
            return 'fail'

        open_options = []
        open_options += ['HOST=' + ogrtest.mongodb_test_host ]
        open_options += ['PORT=' + str(ogrtest.mongodb_test_port) ]
        open_options += ['USER=' + ogrtest.mongodb_test_user ]
        open_options += ['PASSWORD=' + ogrtest.mongodb_test_password ]
        # Missing DBNAME
        gdal.PushErrorHandler()
        ds = gdal.OpenEx('mongodb:', open_options = open_options)
        gdal.PopErrorHandler()
        if ds is not None:
            gdaltest.post_reason('fail')
            return 'fail'

        open_options = []
        open_options += ['HOST=' + ogrtest.mongodb_test_host ]
        open_options += ['PORT=' + str(ogrtest.mongodb_test_port) ]
        open_options += ['DBNAME=' + ogrtest.mongodb_test_dbname ]
        open_options += ['USER=' + ogrtest.mongodb_test_user ]
        open_options += ['PASSWORD=' + ogrtest.mongodb_test_password + '_wrong' ]
        # Wrong password
        gdal.PushErrorHandler()
        ds = gdal.OpenEx('mongodb:', open_options = open_options)
        gdal.PopErrorHandler()
        if ds is not None:
            gdaltest.post_reason('fail')
            return 'fail'

    # Test AUTH_JSON: invalid JSon
    gdal.PushErrorHandler()
    open_options = []
    open_options += ['HOST=' + ogrtest.mongodb_test_host ]
    open_options += ['PORT=' + str(ogrtest.mongodb_test_port) ]
    open_options += ['DBNAME=' + ogrtest.mongodb_test_dbname ]
    open_options += ['AUTH_JSON={']
    ds = gdal.OpenEx('mongodb:', open_options = open_options)
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test AUTH_JSON: missing mechanism
    gdal.PushErrorHandler()
    open_options = []
    open_options += ['HOST=' + ogrtest.mongodb_test_host ]
    open_options += ['PORT=' + str(ogrtest.mongodb_test_port) ]
    open_options += ['DBNAME=' + ogrtest.mongodb_test_dbname ]
    open_options += ['AUTH_JSON={}']
    ds = gdal.OpenEx('mongodb:', open_options = open_options)
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Successful AUTH_JSON use
    if ogrtest.mongodb_test_user is not None:
        open_options = []
        open_options += ['HOST=' + ogrtest.mongodb_test_host ]
        open_options += ['PORT=' + str(ogrtest.mongodb_test_port) ]
        open_options += ['DBNAME=' + ogrtest.mongodb_test_dbname ]
        open_options += ['AUTH_JSON={ "mechanism" : "MONGODB-CR", "db": "%s", "user": "%s", "pwd": "%s" }' % \
            (ogrtest.mongodb_test_dbname, ogrtest.mongodb_test_user, ogrtest.mongodb_test_password)]
        ds = gdal.OpenEx('mongodb:', open_options = open_options)
        if ds is None:
            gdaltest.post_reason('fail')
            return 'fail'

    return 'success'

###############################################################################
# Basic tests

def ogr_mongodb_2():
    if ogrtest.mongodb_drv is None:
        return 'skip'

    ogrtest.mongodb_ds = ogr.Open(ogrtest.mongodb_test_uri, update = 1)
    if ogrtest.mongodb_ds.GetLayerByName('not_existing') is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Create layer
    a_uuid = str(uuid.uuid1()).replace('-', '_')
    ogrtest.mongodb_layer_name = 'test_' + a_uuid
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4258) # ETRS 89 will reproject identically to EPSG:4326
    lyr = ogrtest.mongodb_ds.CreateLayer(ogrtest.mongodb_layer_name, geom_type = ogr.wkbPolygon, srs = srs, options = ['GEOMETRY_NAME=location.mygeom', 'FID='])
    lyr.CreateField(ogr.FieldDefn('str', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('location.name', ogr.OFTString))
    bool_field = ogr.FieldDefn('bool', ogr.OFTInteger)
    bool_field.SetSubType(ogr.OFSTBoolean)
    lyr.CreateField(bool_field)
    lyr.CreateField(ogr.FieldDefn('int', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('int64', ogr.OFTInteger64))
    lyr.CreateField(ogr.FieldDefn('real', ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn('dt', ogr.OFTDateTime))
    lyr.CreateField(ogr.FieldDefn('embed.str', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('binary', ogr.OFTBinary))
    lyr.CreateField(ogr.FieldDefn('strlist', ogr.OFTStringList))
    lyr.CreateField(ogr.FieldDefn('intlist', ogr.OFTIntegerList))
    lyr.CreateField(ogr.FieldDefn('int64list', ogr.OFTInteger64List))
    lyr.CreateField(ogr.FieldDefn('realist', ogr.OFTRealList))
    lyr.CreateField(ogr.FieldDefn('embed.int', ogr.OFTInteger))
    
    # Test CreateFeature()
    f = ogr.Feature(lyr.GetLayerDefn())
    f['str'] = 'str'
    f['location.name'] = 'Paris'
    f['bool'] = 1
    f['int'] = 1
    f['int64'] = 1234567890123456 # put a number larger than 1 << 40 so that fromjson() doesn't pick double
    f['real'] = 1.23
    f['dt'] = '1234/12/31 23:59:59.123+00'
    f.SetFieldBinaryFromHexString('binary', '00FF')
    f['strlist'] = ['a', 'b']
    f['intlist'] = [1, 2]
    f['int64list'] = [1234567890123456, 1234567890123456]
    f['realist'] = [1.23, 4.56]
    f['embed.str'] = 'foo'
    f['embed.int'] = 3
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POLYGON((2 49,2 50,3 50,3 49,2 49))'))
    if lyr.CreateFeature(f) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if f['_id'] is None:
        gdaltest.post_reason('fail')
        return 'fail'
    f_ref = f.Clone()

    # Test GetFeatureCount()
    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test GetNextFeature()
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if not f.Equal(f_ref):
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f = lyr.GetNextFeature()
    if f is not None:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    # Test GetFeature()
    f = lyr.GetFeature(1)
    if not f.Equal(f_ref):
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    # Test SetFeature()
    f['bool'] = 0
    if lyr.SetFeature(f) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    f_ref = f.Clone()
    f = lyr.GetFeature(1)
    if f['bool'] != 0:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    # Test (not working) DeleteFeature()
    gdal.PushErrorHandler()
    ret = lyr.DeleteFeature(1)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test Mongo filter
    lyr.SetAttributeFilter('{ "int": 1 }')
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if not f.Equal(f_ref):
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    lyr.SetAttributeFilter('{ "int": 2 }')
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f is not None:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    # Test OGR filter
    lyr.SetAttributeFilter('int = 1')
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if not f.Equal(f_ref):
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    lyr.SetAttributeFilter('int = 2')
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f is not None:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
        
    # Test geometry filter
    lyr.SetAttributeFilter(None)
    lyr.SetSpatialFilterRect(2.1,49.1,2.9,49.9)
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if not f.Equal(f_ref):
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
        
    lyr.SetSpatialFilterRect(1.1,49.1,1.9,49.9)
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f is not None:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    f = f_ref.Clone()
    f.SetFID(-1)
    if lyr.CreateFeature(f) != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Duplicate key
    gdal.PushErrorHandler()
    ret = lyr.SyncToDisk()
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    f['_id'] = None
    lyr.CreateFeature(f)
    ret = lyr.SyncToDisk()
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # MongoDB dialect of ExecuteSQL() with invalid JSON
    gdal.PushErrorHandler()
    sql_lyr = ogrtest.mongodb_ds.ExecuteSQL('{', dialect = 'MongoDB')
    gdal.PopErrorHandler()
    
    # MongoDB dialect of ExecuteSQL() with inexisting command
    sql_lyr = ogrtest.mongodb_ds.ExecuteSQL('{ "foo": 1 }', dialect = 'MongoDB')
    if sql_lyr is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ogrtest.mongodb_ds.ReleaseResultSet(sql_lyr)

    # MongoDB dialect of ExecuteSQL() with existing commnand
    sql_lyr = ogrtest.mongodb_ds.ExecuteSQL('{ "listCommands" : 1 }', dialect = 'MongoDB')
    if sql_lyr is None:
        gdaltest.post_reason('fail')
        return 'fail'
    f = sql_lyr.GetNextFeature()
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    f = sql_lyr.GetNextFeature()
    if f is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    sql_lyr.GetLayerDefn()
    sql_lyr.ResetReading()
    sql_lyr.TestCapability('')
    ogrtest.mongodb_ds.ReleaseResultSet(sql_lyr)
    
    # Regular ExecuteSQL()
    sql_lyr = ogrtest.mongodb_ds.ExecuteSQL('SELECT * FROM ' + ogrtest.mongodb_layer_name)
    if sql_lyr is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ogrtest.mongodb_ds.ReleaseResultSet(sql_lyr)

    # Test CreateLayer again with same name
    gdal.PushErrorHandler()
    lyr = ogrtest.mongodb_ds.CreateLayer(ogrtest.mongodb_layer_name)
    gdal.PopErrorHandler()
    if lyr is not None:
        return 'fail'

    ogrtest.mongodb_ds = gdal.OpenEx(ogrtest.mongodb_test_uri, gdal.OF_UPDATE,
        open_options = ['FEATURE_COUNT_TO_ESTABLISH_FEATURE_DEFN=-1', 'BULK_INSERT=NO', 'JSON_FIELD=TRUE'])

    # Check after reopening
    lyr = ogrtest.mongodb_ds.GetLayerByName(ogrtest.mongodb_layer_name)
    if lyr.TestCapability(ogr.OLCFastSpatialFilter) == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    json_field = f['_json']
    # We cannot use feature.Equal() has the C++ layer defn has changed
    for i in range(f_ref.GetDefnRef().GetFieldCount()):
        if f.GetField(i) != f_ref.GetField(i) or \
           f.GetFieldDefnRef(i).GetType() != f_ref.GetFieldDefnRef(i).GetType() or \
           f.GetFieldDefnRef(i).GetSubType() != f_ref.GetFieldDefnRef(i).GetSubType():
            gdaltest.post_reason('fail')
            f.DumpReadable()
            f_ref.DumpReadable()
            return 'fail'
    for i in range(f_ref.GetDefnRef().GetGeomFieldCount()):
        if not f.GetGeomFieldRef(i).Equals(f_ref.GetGeomFieldRef(i)) or \
               f.GetGeomFieldDefnRef(i).GetName() != f_ref.GetGeomFieldDefnRef(i).GetName() or \
               f.GetGeomFieldDefnRef(i).GetType() != f_ref.GetGeomFieldDefnRef(i).GetType():
            gdaltest.post_reason('fail')
            f.DumpReadable()
            f_ref.DumpReadable()
            return 'fail'

    lyr.SetSpatialFilterRect(2.1,49.1,2.9,49.9)
    lyr.ResetReading()
    if f is None:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    # Create a feature only from its _json content and do not store any ogr metadata related to the layer
    ogrtest.mongodb_layer_name_no_ogr_metadata = ogrtest.mongodb_layer_name + "_no_ogr_metadata"
    lyr = ogrtest.mongodb_ds.CreateLayer(ogrtest.mongodb_layer_name_no_ogr_metadata, options = ['GEOMETRY_NAME=location.mygeom', 'FID=', 'WRITE_OGR_METADATA=NO'])
    f = ogr.Feature(lyr.GetLayerDefn())
    f['_json'] = json_field
    if lyr.CreateFeature(f) != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    ogrtest.mongodb_layer_name_guess_types = ogrtest.mongodb_layer_name + "_guess_types"
    lyr = ogrtest.mongodb_ds.CreateLayer(ogrtest.mongodb_layer_name_guess_types, geom_type = ogr.wkbNone, options = ['FID=', 'WRITE_OGR_METADATA=NO'])
    f = ogr.Feature(lyr.GetLayerDefn())
    f['_json'] = '{'
    f['_json'] += '"int": 2, '
    f['_json'] += '"int64": { "$numberLong" : "1234567890123456" }, '
    f['_json'] += '"real": 2.34, '
    f['_json'] += '"intlist" : [2], '
    f['_json'] += '"reallist" : [2.34], '
    f['_json'] += '"int64list" : [{ "$numberLong" : "1234567890123456" }], '
    f['_json'] += '"int_str" : 2, '
    f['_json'] += '"str_int" : "2", '
    f['_json'] += '"int64_str" : { "$numberLong" : "1234567890123456" }, '
    f['_json'] += '"str_int64" : "2", '
    f['_json'] += '"int_int64": 2, '
    f['_json'] += '"int64_int": { "$numberLong" : "1234567890123456" }, '
    f['_json'] += '"int_real": 2, '
    f['_json'] += '"real_int": 3.45, '
    f['_json'] += '"int64_real": { "$numberLong" : "1234567890123456" }, '
    f['_json'] += '"real_int64": 3.45, '
    f['_json'] += '"real_str": 3.45, '
    f['_json'] += '"str_real": "3.45", '
    f['_json'] += '"int_bool" : 2, '
    f['_json'] += '"bool_int" : true, '
    f['_json'] += '"intlist_strlist" : [2], '
    f['_json'] += '"strlist_intlist" : ["2"], '
    f['_json'] += '"intlist_int64list": [2], '
    f['_json'] += '"int64list_intlist": [{ "$numberLong" : "1234567890123456" }], '
    f['_json'] += '"intlist_reallist": [2], '
    f['_json'] += '"reallist_intlist": [3.45], '
    f['_json'] += '"int64list_reallist": [{ "$numberLong" : "1234567890123456" }], '
    f['_json'] += '"reallist_int64list": [3.45], '
    f['_json'] += '"intlist_boollist" : [2], '
    f['_json'] += '"boollist_intlist" : [true], '
    f['_json'] += '"mixedlist": [true,1,{ "$numberLong" : "1234567890123456" },3.45],'
    f['_json'] += '"mixedlist2": [true,1,{ "$numberLong" : "1234567890123456" },3.45,"str"]'
    f['_json'] += '}'
    if lyr.CreateFeature(f) != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    f = ogr.Feature(lyr.GetLayerDefn())
    f['_json'] = '{'
    f['_json'] += '"int_str" : "3", '
    f['_json'] += '"str_int" : 3, '
    f['_json'] += '"int64_str" : "2", '
    f['_json'] += '"str_int64" : { "$numberLong" : "1234567890123456" }, '
    f['_json'] += '"int_int64": { "$numberLong" : "1234567890123456" }, '
    f['_json'] += '"int64_int": 2, '
    f['_json'] += '"int_real" : 3.45, '
    f['_json'] += '"real_int": 2, '
    f['_json'] += '"int64_real": 3.45, '
    f['_json'] += '"real_int64": { "$numberLong" : "1234567890123456" }, '
    f['_json'] += '"real_str": "3.45", '
    f['_json'] += '"str_real": 3.45, '
    f['_json'] += '"int_bool" : true, '
    f['_json'] += '"bool_int" : 2, '
    f['_json'] += '"intlist_strlist" : ["3"], '
    f['_json'] += '"strlist_intlist" : [3], '
    f['_json'] += '"intlist_int64list": [{ "$numberLong" : "1234567890123456" }], '
    f['_json'] += '"int64list_intlist": [2], '
    f['_json'] += '"intlist_reallist": [3.45], '
    f['_json'] += '"reallist_intlist": [2], '
    f['_json'] += '"int64list_reallist": [3.45], '
    f['_json'] += '"reallist_int64list": [{ "$numberLong" : "1234567890123456" }], '
    f['_json'] += '"intlist_boollist" : [true], '
    f['_json'] += '"boollist_intlist" : [2]'
    f['_json'] += '}'
    if lyr.CreateFeature(f) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
        
    # This new features will not be taken into account by below the FEATURE_COUNT_TO_ESTABLISH_FEATURE_DEFN=2
    f = ogr.Feature(lyr.GetLayerDefn())
    f['_json'] = '{'
    f['_json'] += '"int": { "$minKey": 1 }, '
    f['_json'] += '"int64": { "$minKey": 1 }, '
    f['_json'] += '"real": { "$minKey": 1 }, '
    f['_json'] += '"intlist" : [1, "1", { "$minKey": 1 },{ "$maxKey": 1 },{ "$numberLong" : "-1234567890123456" }, { "$numberLong" : "1234567890123456" }, -1234567890123456.1, 1234567890123456.1], '
    f['_json'] += '"int64list" : [1, { "$numberLong" : "1234567890123456" }, "1", { "$minKey": 1 },{ "$maxKey": 1 }, -1e300, 1e300 ], '
    f['_json'] += '"reallist" : [1, { "$numberLong" : "1234567890123456" }, 1.0, "1", { "$minKey": 1 },{ "$maxKey": 1 }, { "$numberLong" : "1234567890123456" } ] '
    f['_json'] += '}'
    if lyr.CreateFeature(f) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
        
    f = ogr.Feature(lyr.GetLayerDefn())
    f['_json'] = '{'
    f['_json'] += '"int": { "$maxKey": 1 }, '
    f['_json'] += '"int64": { "$maxKey": 1 }, '
    f['_json'] += '"real": { "$maxKey": 1 } '
    f['_json'] += '}'
    if lyr.CreateFeature(f) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
        
    ogrtest.mongodb_layer_name_with_2d_index = ogrtest.mongodb_layer_name + "_with_2d_index"
    gdal.SetConfigOption('OGR_MONGODB_SPAT_INDEX_TYPE', '2d')
    lyr = ogrtest.mongodb_ds.CreateLayer(ogrtest.mongodb_layer_name_with_2d_index, geom_type = ogr.wkbPoint, options = ['FID=', 'WRITE_OGR_METADATA=NO'])
    gdal.SetConfigOption('OGR_MONGODB_SPAT_INDEX_TYPE', None)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT(2 49)'))
    if lyr.CreateFeature(f) != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    ogrtest.mongodb_layer_name_no_spatial_index = ogrtest.mongodb_layer_name + "_no_spatial_index"
    for i in range(2):
        lyr = ogrtest.mongodb_ds.CreateLayer(ogrtest.mongodb_layer_name_no_spatial_index, options = ['SPATIAL_INDEX=NO', 'OVERWRITE=YES'])
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT(2 49)'))
        if lyr.CreateFeature(f) != 0:
            gdaltest.post_reason('fail')
            return 'fail'
        ogrtest.mongodb_ds.ExecuteSQL('WRITE_OGR_METADATA ' + ogrtest.mongodb_layer_name_no_spatial_index)

    # Open "ghost" layer
    lyr = ogrtest.mongodb_ds.GetLayerByName('_ogr_metadata')
    if lyr is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetFeatureCount() != 2:
        print(lyr.GetFeatureCount())
        gdaltest.post_reason('fail')
        return 'fail'

    if ogrtest.mongodb_ds.DeleteLayer(-1) == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    ogrtest.mongodb_ds = None

    # Reopen in read-only
    ogrtest.mongodb_ds = gdal.OpenEx(ogrtest.mongodb_test_uri, 0, open_options = ['FEATURE_COUNT_TO_ESTABLISH_FEATURE_DEFN=2', 'JSON_FIELD=TRUE'])

    lyr = ogrtest.mongodb_ds.GetLayerByName(ogrtest.mongodb_layer_name_no_ogr_metadata)
    if lyr.TestCapability(ogr.OLCFastSpatialFilter) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    for i in range(f_ref.GetDefnRef().GetFieldCount()):
        # Order might be a bit different...
        j = f.GetDefnRef().GetFieldIndex(f_ref.GetFieldDefnRef(i).GetNameRef())
        if f.GetField(j) != f_ref.GetField(i) or \
        f.GetFieldDefnRef(j).GetType() != f_ref.GetFieldDefnRef(i).GetType() or \
        f.GetFieldDefnRef(j).GetSubType() != f_ref.GetFieldDefnRef(i).GetSubType():
            gdaltest.post_reason('fail')
            f.DumpReadable()
            f_ref.DumpReadable()
            return 'fail'
    for i in range(f_ref.GetDefnRef().GetGeomFieldCount()):
        # Order might be a bit different...
        j = f.GetDefnRef().GetGeomFieldIndex(f_ref.GetGeomFieldDefnRef(i).GetNameRef())
        if not f.GetGeomFieldRef(j).Equals(f_ref.GetGeomFieldRef(i)) or \
            f.GetGeomFieldDefnRef(j).GetName() != f_ref.GetGeomFieldDefnRef(i).GetName() or \
            f.GetGeomFieldDefnRef(j).GetType() != f_ref.GetGeomFieldDefnRef(i).GetType():
            gdaltest.post_reason('fail')
            f.DumpReadable()
            f_ref.DumpReadable()
            print(f.GetGeomFieldDefnRef(j).GetType())
            print(f_ref.GetGeomFieldDefnRef(i).GetType())
            return 'fail'

    lyr.SetSpatialFilterRect(2.1,49.1,2.9,49.9)
    lyr.ResetReading()
    if f is None:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    lyr = ogrtest.mongodb_ds.GetLayerByName(ogrtest.mongodb_layer_name_guess_types)
    
    expected_fields = [
  ("int", ogr.OFTInteger),
  ("int64", ogr.OFTInteger64),
  ("real", ogr.OFTReal),
  ("intlist", ogr.OFTIntegerList),
  ("reallist", ogr.OFTRealList),
  ("int64list", ogr.OFTInteger64List),
  ("int_str", ogr.OFTString),
  ("str_int", ogr.OFTString),
  ("int64_str", ogr.OFTString),
  ("str_int64", ogr.OFTString),
  ("int_int64", ogr.OFTInteger64),
  ("int64_int", ogr.OFTInteger64),
  ("int_real", ogr.OFTReal),
  ("real_int", ogr.OFTReal),
  ("int64_real", ogr.OFTReal),
  ("real_int64", ogr.OFTReal),
  ("real_str", ogr.OFTString),
  ("str_real", ogr.OFTString),
  ("int_bool", ogr.OFTInteger),
  ("bool_int", ogr.OFTInteger),
  ("intlist_strlist", ogr.OFTStringList),
  ("strlist_intlist", ogr.OFTStringList),
  ("intlist_int64list", ogr.OFTInteger64List),
  ("int64list_intlist", ogr.OFTInteger64List),
  ("intlist_reallist", ogr.OFTRealList),
  ("reallist_intlist", ogr.OFTRealList),
  ("int64list_reallist", ogr.OFTRealList),
  ("reallist_int64list", ogr.OFTRealList),
  ("intlist_boollist", ogr.OFTIntegerList),
  ("boollist_intlist", ogr.OFTIntegerList),
  ("mixedlist", ogr.OFTRealList),
  ("mixedlist2", ogr.OFTStringList) ]
    for (fieldname, fieldtype) in expected_fields:
        fld_defn = lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex(fieldname))
        if fld_defn.GetType() != fieldtype:
            gdaltest.post_reason('fail')
            print(fieldname)
            print(fld_defn.GetType())
            return 'fail'
        if fld_defn.GetSubType() != ogr.OFSTNone:
            gdaltest.post_reason('fail')
            return 'fail'

    f = lyr.GetNextFeature()
    f = lyr.GetNextFeature()
    f = lyr.GetNextFeature()
    if f['intlist'] != [1,1,-2147483648,2147483647,-2147483648,2147483647,-2147483648,2147483647] or \
       f['int64list'] != [1,1234567890123456,1,-9223372036854775808,9223372036854775807,-9223372036854775808,9223372036854775807] or \
       f['int'] != -2147483648 or f['int64'] != -9223372036854775808 or f['real'] - 1 != f['real']:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f = lyr.GetNextFeature()
    if f['int'] != 2147483647 or f['int64'] != 9223372036854775807 or f['real'] +1 != f['real']:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    lyr = ogrtest.mongodb_ds.GetLayerByName(ogrtest.mongodb_layer_name_with_2d_index)
    if lyr.TestCapability(ogr.OLCFastSpatialFilter) == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr.SetSpatialFilterRect(1.9,48.9,2.1,49.1)
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr.SetSpatialFilterRect(1.9,48.9,1.95,48.95)
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    
    lyr = ogrtest.mongodb_ds.GetLayerByName(ogrtest.mongodb_layer_name_no_spatial_index)
    if lyr.TestCapability(ogr.OLCFastSpatialFilter) != 0:
        gdaltest.post_reason('fail')
        print(lyr.TestCapability(ogr.OLCFastSpatialFilter))
        return 'fail'
    lyr.SetSpatialFilterRect(1.9,48.9,2.1,49.1)
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.PushErrorHandler()
    lyr = ogrtest.mongodb_ds.CreateLayer('foo')
    gdal.PopErrorHandler()
    if lyr is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.ErrorReset()
    gdal.PushErrorHandler()
    ogrtest.mongodb_ds.ExecuteSQL('WRITE_OGR_METADATA ' + ogrtest.mongodb_layer_name)
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'

    lyr_count_before = ogrtest.mongodb_ds.GetLayerCount()
    gdal.PushErrorHandler()
    ogrtest.mongodb_ds.ExecuteSQL('DELLAYER:' + ogrtest.mongodb_layer_name)
    gdal.PopErrorHandler()
    if ogrtest.mongodb_ds.GetLayerCount() != lyr_count_before:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr = ogrtest.mongodb_ds.GetLayerByName(ogrtest.mongodb_layer_name)

    gdal.PushErrorHandler()
    ret = lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.PushErrorHandler()
    ret = lyr.CreateGeomField(ogr.GeomFieldDefn('foo', ogr.wkbPoint))
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    
    f = ogr.Feature(lyr.GetLayerDefn())
    gdal.PushErrorHandler()
    ret = lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    
    gdal.PushErrorHandler()
    ret = lyr.SetFeature(f)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    
    gdal.PushErrorHandler()
    ret = lyr.DeleteFeature(1)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# test_ogrsf

def ogr_mongodb_3():
    if ogrtest.mongodb_drv is None:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro ' + ogrtest.mongodb_test_uri)
    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Cleanup

def ogr_mongodb_cleanup():
    if ogrtest.mongodb_drv is None:
        return 'skip'

    ogrtest.mongodb_ds = None

    # Reopen in read-write
    ogrtest.mongodb_ds = ogr.Open(ogrtest.mongodb_test_uri, update = 1)

    if ogrtest.mongodb_layer_name is not None:
        ogrtest.mongodb_ds.ExecuteSQL('DELLAYER:' + ogrtest.mongodb_layer_name)
    if ogrtest.mongodb_layer_name_no_ogr_metadata is not None:
        ogrtest.mongodb_ds.ExecuteSQL('DELLAYER:' + ogrtest.mongodb_layer_name_no_ogr_metadata)
    if ogrtest.mongodb_layer_name_guess_types is not None:
        ogrtest.mongodb_ds.ExecuteSQL('DELLAYER:' + ogrtest.mongodb_layer_name_guess_types)
    if ogrtest.mongodb_layer_name_with_2d_index is not None:
        ogrtest.mongodb_ds.ExecuteSQL('DELLAYER:' + ogrtest.mongodb_layer_name_with_2d_index)
    if ogrtest.mongodb_layer_name_no_spatial_index is not None:
        ogrtest.mongodb_ds.ExecuteSQL('DELLAYER:' + ogrtest.mongodb_layer_name_no_spatial_index)

    ogrtest.mongodb_ds = None
    
    return 'success'

gdaltest_list = [ 
    ogr_mongodb_init,
    ogr_mongodb_1,
    ogr_mongodb_2,
    ogr_mongodb_3,
    ogr_mongodb_cleanup,
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_mongodb' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

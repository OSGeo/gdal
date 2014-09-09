#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  OpenFileGDB driver testing.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2014, Even Rouault <even dot rouault at mines-paris dot org>
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
import string
import shutil

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import gdal
from osgeo import ogr
from osgeo import osr

ogrtest.openfilegdb_datalist = [ [ "none", ogr.wkbNone, None],
                [ "point", ogr.wkbPoint, "POINT (1 2)" ],
                [ "multipoint", ogr.wkbMultiPoint, "MULTIPOINT (1 2,3 4)" ],
                [ "linestring", ogr.wkbLineString, "LINESTRING (1 2,3 4)", "MULTILINESTRING ((1 2,3 4))" ],
                [ "multilinestring", ogr.wkbMultiLineString, "MULTILINESTRING ((1 2,3 4))" ],
                [ "polygon", ogr.wkbPolygon, "POLYGON ((0 0,0 1,1 1,1 0,0 0))", "MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0)))" ],
                [ "multipolygon", ogr.wkbMultiPolygon, "MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0),(0.25 0.25,0.75 0.25,0.75 0.75,0.25 0.75,0.25 0.25)),((2 0,2 1,3 1,3 0,2 0)))" ],
                [ "point25D", ogr.wkbPoint25D, "POINT (1 2 3)" ],
                [ "multipoint25D", ogr.wkbMultiPoint25D, "MULTIPOINT (1 2 -10,3 4 -20)" ],
                [ "linestring25D", ogr.wkbLineString25D, "LINESTRING (1 2 -10,3 4 -20)", "MULTILINESTRING ((1 2 -10,3 4 -20))" ],
                [ "multilinestring25D", ogr.wkbMultiLineString25D, "MULTILINESTRING ((1 2 -10,3 4 -20))" ],
                [ "polygon25D", ogr.wkbPolygon25D, "POLYGON ((0 0 -10,0 1 -10,1 1 -10,1 0 -10,0 0 -10))", "MULTIPOLYGON (((0 0 -10,0 1 -10,1 1 -10,1 0 -10,0 0 -10)))" ],
                [ "multipolygon25D", ogr.wkbMultiPolygon25D, "MULTIPOLYGON (((0 0 -10,0 1 -10,1 1 -10,1 0 -10,0 0 -10)))" ],
                [ "multipatch", ogr.wkbMultiPolygon25D, "MULTIPOLYGON (((0 0 0,0 1 0,1 0 0,0 0 0)),((0 1 0,1 0 0,1 1 0,0 1 0)),((10 0 0,10 1 0,11 0 0,10 0 0)),((10 0 0,11 0 0,10 -1 0,10 0 0)),((5 0 0,5 1 0,6 0 0,5 0 0)),((100 0 0,100 1 0,101 1 0,101 0 0,100 0 0),(100.25 0.25 0,100.75 0.25 0,100.75 0.75 0,100.75 0.25 0,100.25 0.25 0)))" ],
                [ "null_polygon", ogr.wkbPolygon, None],
                [ "empty_polygon", ogr.wkbPolygon, "POLYGON EMPTY", None],
                [ "empty_multipoint", ogr.wkbMultiPoint, "MULTIPOINT EMPTY", None],
            ]

###############################################################################
# Disable FileGDB driver

def ogr_openfilegdb_init():

    ogrtest.fgdb_drv = ogr.GetDriverByName('FileGDB')
    if ogrtest.fgdb_drv is not None:
        ogrtest.fgdb_drv.Deregister()

    return 'success'

###############################################################################
# Make test data

def ogr_openfilegdb_make_test_data():

    try:
        shutil.rmtree("data/testopenfilegdb.gdb")
    except:
        pass
    ds = ogrtest.fgdb_drv.CreateDataSource('data/testopenfilegdb.gdb')

    srs = osr.SpatialReference()
    srs.SetFromUserInput("WGS84")

    options = ['COLUMN_TYPES=smallint=esriFieldTypeSmallInteger,float=esriFieldTypeSingle,guid=esriFieldTypeGUID,xml=esriFieldTypeXML']

    for data in ogrtest.openfilegdb_datalist:
        if data[1] == ogr.wkbNone:
            lyr = ds.CreateLayer(data[0], geom_type=data[1], options = options )
        elif data[0] == 'multipatch':
            lyr = ds.CreateLayer(data[0], geom_type=data[1], srs=srs, options = [ 'CREATE_MULTIPATCH=YES', options[0] ] )
        else:
            lyr = ds.CreateLayer(data[0], geom_type=data[1], srs=srs, options = options )
        lyr.CreateField(ogr.FieldDefn("id", ogr.OFTInteger))
        lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))
        lyr.CreateField(ogr.FieldDefn("smallint", ogr.OFTInteger))
        lyr.CreateField(ogr.FieldDefn("int", ogr.OFTInteger))
        lyr.CreateField(ogr.FieldDefn("float", ogr.OFTReal))
        lyr.CreateField(ogr.FieldDefn("real", ogr.OFTReal))
        lyr.CreateField(ogr.FieldDefn("adate", ogr.OFTDateTime))
        lyr.CreateField(ogr.FieldDefn("guid", ogr.OFTString))
        lyr.CreateField(ogr.FieldDefn("xml", ogr.OFTString))
        lyr.CreateField(ogr.FieldDefn("binary", ogr.OFTBinary))
        lyr.CreateField(ogr.FieldDefn("nullint", ogr.OFTInteger))
        lyr.CreateField(ogr.FieldDefn("binary2", ogr.OFTBinary))

        # We need at least 5 features so that test_ogrsf can test SetFeature()
        for i in range(5):
            feat = ogr.Feature(lyr.GetLayerDefn())
            if data[1] != ogr.wkbNone and data[2] != None:
                feat.SetGeometry(ogr.CreateGeometryFromWkt(data[2]))
            feat.SetField("id", i + 1)
            feat.SetField("str", "foo_\xc3\xa9")
            feat.SetField("smallint", -13)
            feat.SetField("int", 123)
            feat.SetField("float", 1.5)
            feat.SetField("real", 4.56)
            feat.SetField("adate", "2013/12/26 12:34:56")
            feat.SetField("guid", "{12345678-9abc-DEF0-1234-567890ABCDEF}")
            feat.SetFieldBinaryFromHexString("binary", "00FF7F")
            feat.SetField("xml", "<foo></foo>")
            feat.SetFieldBinaryFromHexString("binary2", "123456")
            lyr.CreateFeature(feat)

        if data[0] is 'none':
            # Create empty feature
            feat = ogr.Feature(lyr.GetLayerDefn())
            lyr.CreateFeature(feat)

    if False:
        lyr = ds.CreateLayer('sparse_layer', geom_type = ogr.wkbPoint )
        for i in range(4096):
            feat = ogr.Feature(lyr.GetLayerDefn())
            lyr.CreateFeature(feat)
            lyr.DeleteFeature(feat.GetFID())
        feat = ogr.Feature(lyr.GetLayerDefn())
        lyr.CreateFeature(feat)

    if True:
        lyr = ds.CreateLayer('big_layer', geom_type = ogr.wkbNone )
        lyr.CreateField(ogr.FieldDefn("real", ogr.OFTReal))
        gdal.SetConfigOption('FGDB_BULK_LOAD', 'YES')
        #for i in range(340*341+1):
        for i in range(340+1):
            feat = ogr.Feature(lyr.GetLayerDefn())
            feat.SetField(0, i % 4)
            lyr.CreateFeature(feat)
        gdal.SetConfigOption('FGDB_BULK_LOAD', None)

    if True:
        lyr = ds.CreateLayer('hole', geom_type = ogr.wkbPoint, srs = None)
        lyr.CreateField(ogr.FieldDefn('str', ogr.OFTString))
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetField('str', 'f1')
        lyr.CreateFeature(feat)

        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetField('str', 'fid2')
        lyr.CreateFeature(feat)
        
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetField('str', 'fid3')
        lyr.CreateFeature(feat)
        feat = None

        lyr.CreateField(ogr.FieldDefn('int0', ogr.OFTInteger))
        lyr.CreateField(ogr.FieldDefn('str2', ogr.OFTString))

        for i in range(8):
            feat = ogr.Feature(lyr.GetLayerDefn())
            feat.SetField('str', 'fid%d' % (4+i))
            feat.SetField('int0', 4+i)
            feat.SetField('str2', '                                            ')
            lyr.CreateFeature(feat)
        feat = None

        for i in range(8):
            lyr.CreateField(ogr.FieldDefn('int%d' % (i+1), ogr.OFTInteger))

        lyr.DeleteFeature(1)
        
        feat = ogr.Feature(lyr.GetLayerDefn())
        lyr.CreateFeature(feat)
        feat = None
        
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetField('str', 'fid13')
        lyr.CreateFeature(feat)
        feat = None

    if True:
        lyr = ds.CreateLayer('no_field', geom_type = ogr.wkbNone, srs = None)
        for i in range(5):
            feat = ogr.Feature(lyr.GetLayerDefn())
            lyr.CreateFeature(feat)
            feat = None

    if True:
        lyr = ds.CreateLayer('several_polygons', geom_type = ogr.wkbPolygon, srs = None)
        for i in range(3):
            for j in range(3):
                feat = ogr.Feature(lyr.GetLayerDefn())
                x1 = 2 * i
                x2 = 2 * i + 1
                y1 = 2 * j
                y2 = 2 * j + 1
                geom = ogr.CreateGeometryFromWkt('POLYGON((%d %d,%d %d,%d %d,%d %d,%d %d))' % (x1,y1,x1,y2,x2,y2,x2,y1,x1,y1))
                feat.SetGeometry(geom)
                lyr.CreateFeature(feat)
                feat = None

    for fld_name in [ 'id', 'str', 'smallint', 'int', 'float', 'real', 'adate', 'guid', 'nullint' ]:
        ds.ExecuteSQL('CREATE INDEX idx_%s ON point(%s)' % (fld_name, fld_name))
    ds.ExecuteSQL('CREATE INDEX idx_id ON none(id)')
    ds.ExecuteSQL('CREATE INDEX idx_real ON big_layer(real)')
    ds = None

    try:
        os.unlink('data/testopenfilegdb.gdb.zip')
    except:
        pass
    os.chdir('data')
    os.system('zip -r -9 testopenfilegdb.gdb.zip testopenfilegdb.gdb')
    os.chdir('..')
    shutil.rmtree('data/testopenfilegdb.gdb')

    return 'success'

###############################################################################
# Basic tests
def ogr_openfilegdb_1():

    srs = osr.SpatialReference()
    srs.SetFromUserInput("WGS84")

    ds = ogr.Open('data/testopenfilegdb.gdb.zip')

    for data in ogrtest.openfilegdb_datalist:
        lyr = ds.GetLayerByName(data[0])
        expected_geom_type = data[1]
        if expected_geom_type == ogr.wkbLineString:
            expected_geom_type = ogr.wkbMultiLineString
        elif expected_geom_type == ogr.wkbLineString25D:
            expected_geom_type = ogr.wkbMultiLineString25D
        elif expected_geom_type == ogr.wkbPolygon:
            expected_geom_type = ogr.wkbMultiPolygon
        elif expected_geom_type == ogr.wkbPolygon25D:
            expected_geom_type = ogr.wkbMultiPolygon25D
        if lyr.GetGeomType() != expected_geom_type:
            print(lyr.GetName())
            print(lyr.GetGeomType())
            return 'fail'
        if data[1] != ogr.wkbNone:
            if lyr.GetSpatialRef().IsSame(srs) != 1:
                print(lyr.GetSpatialRef())
                return 'fail'
        feat = lyr.GetNextFeature()
        if data[1] != ogr.wkbNone:
            try:
                expected_wkt = data[3]
            except:
                expected_wkt = data[2]
            geom = feat.GetGeometryRef()
            if geom:
                geom = geom.ExportToWkt()
            if geom != expected_wkt:
                feat.DumpReadable()
                return 'fail'

        if feat.GetField('id') != 1 or \
           feat.GetField('smallint') != -13 or \
           feat.GetField('int') != 123 or \
           feat.GetField('float') != 1.5 or \
           feat.GetField('real') != 4.56 or \
           feat.GetField('adate') != "2013/12/26 12:34:56" or \
           feat.GetField('guid') != "{12345678-9ABC-DEF0-1234-567890ABCDEF}" or \
           feat.GetField('xml') != "<foo></foo>" or \
           feat.GetField('binary') != "00FF7F" or \
           feat.GetField('binary2') != "123456":
            feat.DumpReadable()
            return 'fail'

        sql_lyr = ds.ExecuteSQL("GetLayerDefinition %s" % lyr.GetName())
        if sql_lyr is None:
            gdaltest.post_reason('failure')
            return 'fail'
        feat = sql_lyr.GetNextFeature()
        if feat is None:
            gdaltest.post_reason('failure')
            return 'fail'
        feat = sql_lyr.GetNextFeature()
        if feat is not None:
            gdaltest.post_reason('failure')
            return 'fail'
        lyr.ResetReading()
        lyr.TestCapability("foo")
        ds.ReleaseResultSet(sql_lyr)

        sql_lyr = ds.ExecuteSQL("GetLayerMetadata %s" % lyr.GetName())
        if sql_lyr is None:
            gdaltest.post_reason('failure')
            return 'fail'
        feat = sql_lyr.GetNextFeature()
        if feat is None:
            gdaltest.post_reason('failure')
            return 'fail'
        ds.ReleaseResultSet(sql_lyr)

    sql_lyr = ds.ExecuteSQL("GetLayerDefinition foo")
    if sql_lyr is not None:
        gdaltest.post_reason('failure')
        return 'fail'

    sql_lyr = ds.ExecuteSQL("GetLayerMetadata foo")
    if sql_lyr is not None:
        gdaltest.post_reason('failure')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Run test_ogrsf

def ogr_openfilegdb_2():

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/testopenfilegdb.gdb.zip')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Open a .gdbtable directly

def ogr_openfilegdb_3():

    ds = ogr.Open('/vsizip/data/testopenfilegdb.gdb.zip/testopenfilegdb.gdb/a00000009.gdbtable')
    if ds.GetLayerCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr = ds.GetLayer(0)
    if lyr.GetName() != 'none':
        gdaltest.post_reason('fail')
        return 'fail'

    # Try opening a system table
    lyr = ds.GetLayerByName('GDB_SystemCatalog')
    if lyr.GetName() != 'GDB_SystemCatalog':
        gdaltest.post_reason('fail')
        return 'fail'
    feat = lyr.GetNextFeature()
    if feat.GetField('Name') != 'GDB_SystemCatalog':
        gdaltest.post_reason('fail')
        return 'fail'
    lyr = ds.GetLayerByName('GDB_SystemCatalog')
    if lyr.GetName() != 'GDB_SystemCatalog':
        gdaltest.post_reason('fail')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test use of attribute indexes

def ogr_openfilegdb_4():
    
    ds = ogr.Open('/vsizip/data/testopenfilegdb.gdb.zip/testopenfilegdb.gdb')

    lyr = ds.GetLayerByName('point')
    tests = [ ('id = 1', [1]),
              ('1 = id', [1]),
              ('id = 5', [5]),
              ('id = 0', []),
              ('id = 6', []),
              ('id <= 1', [1]),
              ('1 >= id', [1]),
              ('id >= 5', [5]),
              ('5 <= id', [5]),
              ('id < 1', []),
              ('1 > id', []),
              ('id >= 1', [1,2,3,4,5]),
              ('id > 0', [1,2,3,4,5]),
              ('0 < id', [1,2,3,4,5]),
              ('id <= 5', [1,2,3,4,5]),
              ('id < 6', [1,2,3,4,5]),
              ('id <> 0', [1,2,3,4,5]),
              ('id IS NOT NULL', [1,2,3,4,5]),
              ('id IS NULL', []),
              ('nullint IS NOT NULL', []),
              ('nullint IS NULL', [1,2,3,4,5]),
              ("str = 'foo_e'", []),
              ("str = 'foo_é'", [1,2,3,4,5]),
              ("str <= 'foo_é'", [1,2,3,4,5]),
              ("str >= 'foo_é'", [1,2,3,4,5]),
              ("str <> 'foo_é'", []),
              ("str < 'foo_é'", []),
              ("str > 'foo_é'", []),
              ('smallint = -13', [1,2,3,4,5]),
              ('smallint <= -13', [1,2,3,4,5]),
              ('smallint >= -13', [1,2,3,4,5]),
              ('smallint < -13', []),
              ('smallint > -13', []),
              ('int = 123', [1,2,3,4,5]),
              ('int <= 123', [1,2,3,4,5]),
              ('int >= 123', [1,2,3,4,5]),
              ('int < 123', []),
              ('int > 123', []),
              ('float = 1.5', [1,2,3,4,5]),
              ('float <= 1.5', [1,2,3,4,5]),
              ('float >= 1.5', [1,2,3,4,5]),
              ('float < 1.5', []),
              ('float > 1.5', []),
              ('real = 4.56', [1,2,3,4,5]),
              ('real <= 4.56', [1,2,3,4,5]),
              ('real >= 4.56', [1,2,3,4,5]),
              ('real < 4.56', []),
              ('real > 4.56', []),
              ("adate = '2013/12/26 12:34:56'", [1,2,3,4,5]),
              ("adate <= '2013/12/26 12:34:56'", [1,2,3,4,5]),
              ("adate >= '2013/12/26 12:34:56'", [1,2,3,4,5]),
              ("adate < '2013/12/26 12:34:56'", []),
              ("adate > '2013/12/26 12:34:56'", []),
              ("guid = '{12345678-9ABC-DEF0-1234-567890ABCDEF}'", [1,2,3,4,5]),
              ("guid <= '{12345678-9ABC-DEF0-1234-567890ABCDEF}'", [1,2,3,4,5]),
              ("guid >= '{12345678-9ABC-DEF0-1234-567890ABCDEF}'", [1,2,3,4,5]),
              ("guid < '{12345678-9ABC-DEF0-1234-567890ABCDEF}'", []),
              ("guid > '{12345678-9ABC-DEF0-1234-567890ABCDEF}'", []),
              ("guid = '{'", []),
              ("guid > '{'", [1,2,3,4,5]),
              ("NOT(id = 1)", [2,3,4,5]),
              ("id = 1 OR id = -1", [1]),
              ("id = -1 OR id = 1", [1]),
              ("id = 1 OR id = 1", [1]),
              ("id = 1 OR id = 2", [1,2]), # exclusive branches
              ("id < 3 OR id > 3", [1,2,4,5]), # exclusive branches
              ("id > 3 OR id < 3", [1,2,4,5]), # exclusive branches
              ("id <= 3 OR id >= 4", [1,2,3,4,5]), # exclusive branches
              ("id >= 4 OR id <= 3", [1,2,3,4,5]), # exclusive branches
              ("id < 3 OR id >= 3", [1,2,3,4,5]),
              ("id <= 3 OR id >= 3", [1,2,3,4,5]),
              ("id <= 5 OR id >= 1", [1,2,3,4,5]),
              ("id <= 1.5 OR id >= 2", [1,2,3,4,5]),
              ('id IS NULL OR id IS NOT NULL', [1,2,3,4,5]),
              ('float < 1.5 OR float > 1.5', []),
              ('float <= 1.5 OR float >= 1.5', [1,2,3,4,5]),
              ('float < 1.5 OR float > 2', []),
              ('float < 1 OR float > 2.5', []),
              ("str < 'foo_é' OR str > 'z'", []),
              ("adate < '2013/12/26 12:34:56' OR adate > '2014/01/01'", []),
              ("id = 1 AND id = -1", []),
              ("id = -1 AND id = 1", []),
              ("id = 1 AND id = 1", [1]),
              ("id = 1 AND id = 2", []),
              ("id <= 5 AND id >= 1", [1,2,3,4,5]),
              ("id <= 3 AND id >= 3", [3]),
              ("id = 1 AND float = 1.5", [1]),
              ("id BETWEEN 1 AND 5", [1,2,3,4,5]),
              ("id IN (1)", [1]),
              ("id IN (5,4,3,2,1)", [1,2,3,4,5]),
              ('fid = 1', [1], 0), # no index used
              ('fid BETWEEN 1 AND 1', [1], 0), # no index used
              ('fid IN (1)', [1], 0), # no index used
              ('fid IS NULL', [], 0), # no index used
              ('fid IS NOT NULL', [1,2,3,4,5], 0), # no index used
              ("xml <> ''", [1,2,3,4,5], 0), # no index used
              ("id = 1 AND xml <> ''", [1], 1), # index partially used
              ("xml <> '' AND id = 1", [1], 1), # index partially used
              ("NOT(id = 1 AND xml <> '')", [2,3,4,5], 0), # no index used
              ("id = 1 OR xml <> ''", [1,2,3,4,5], 0), # no index used
              ('id = id', [1,2,3,4,5], 0), # no index used
              ('id = 1 + 0', [1], 0), # no index used (currently...)
             ]
    for test in tests:

        if len(test) == 2:
            (where_clause, fids) = test
            expected_attr_index_use = 2
        else:
            (where_clause, fids, expected_attr_index_use) = test

        lyr.SetAttributeFilter(where_clause)
        sql_lyr = ds.ExecuteSQL('GetLayerAttrIndexUse %s' % lyr.GetName())
        attr_index_use = int(sql_lyr.GetNextFeature().GetField(0))
        ds.ReleaseResultSet(sql_lyr)
        if attr_index_use != expected_attr_index_use:
            print(where_clause, fids, expected_attr_index_use)
            print(attr_index_use)
            gdaltest.post_reason('fail')
            return 'fail'
        if lyr.GetFeatureCount() != len(fids):
            print(where_clause, fids)
            print(lyr.GetFeatureCount())
            gdaltest.post_reason('fail')
            return 'fail'
        for fid in fids:
            feat = lyr.GetNextFeature()
            if feat.GetFID() != fid:
                print(where_clause, fids)
                gdaltest.post_reason('fail')
                return 'fail'
        feat = lyr.GetNextFeature()
        if feat is not None:
            print(where_clause, fids)
            gdaltest.post_reason('fail')
            return 'fail'


    lyr = ds.GetLayerByName('none')
    tests = [ ('id = 1', [1]),
              ('id IS NULL', [6]),
              ('id IS NOT NULL', [1,2,3,4,5]),
              ('id IS NULL OR id IS NOT NULL', [1,2,3,4,5,6]),
              ('id = 1 OR id IS NULL', [1, 6]),
              ('id IS NULL OR id = 1', [1, 6]),
            ]
    for test in tests:

        if len(test) == 2:
            (where_clause, fids) = test
            expected_attr_index_use = 2
        else:
            (where_clause, fids, expected_attr_index_use) = test

        lyr.SetAttributeFilter(where_clause)
        sql_lyr = ds.ExecuteSQL('GetLayerAttrIndexUse %s' % lyr.GetName())
        attr_index_use = int(sql_lyr.GetNextFeature().GetField(0))
        ds.ReleaseResultSet(sql_lyr)
        if attr_index_use != expected_attr_index_use:
            print(where_clause, fids, expected_attr_index_use)
            print(attr_index_use)
            gdaltest.post_reason('fail')
            return 'fail'
        if lyr.GetFeatureCount() != len(fids):
            print(where_clause, fids)
            print(lyr.GetFeatureCount())
            gdaltest.post_reason('fail')
            return 'fail'
        for fid in fids:
            feat = lyr.GetNextFeature()
            if feat.GetFID() != fid:
                print(where_clause, fids)
                gdaltest.post_reason('fail')
                return 'fail'
        feat = lyr.GetNextFeature()
        if feat is not None:
            print(where_clause, fids)
            gdaltest.post_reason('fail')
            return 'fail'

    lyr = ds.GetLayerByName('big_layer')
    tests = [ ('real = 0', 86, 1),
              ('real = 1', 85, 2),
              ('real = 2', 85, 3),
              ('real = 3', 85, 4),
              ('real >= 0', 86 + 3 * 85, None),
              ('real < 4', 86 + 3 * 85, None),
              ('real > 1 AND real < 2', 0, None),
              ('real < 0', 0, None),
            ]
    for (where_clause, count, start) in tests:

        lyr.SetAttributeFilter(where_clause)
        if lyr.GetFeatureCount() != count:
            print(where_clause, count)
            print(lyr.GetFeatureCount())
            gdaltest.post_reason('fail')
            return 'fail'
        for i in range(count):
            feat = lyr.GetNextFeature()
            if feat is None or \
               (start is not None and \
                feat.GetFID() != i * 4 + start):
                print(where_clause, count)
                gdaltest.post_reason('fail')
                return 'fail'
        feat = lyr.GetNextFeature()
        if feat is not None:
            print(where_clause, count)
            gdaltest.post_reason('fail')
            return 'fail'

    ds = None
    return 'success'

###############################################################################
# Test opening an unzipped dataset

def ogr_openfilegdb_5():

    try:
        shutil.rmtree('tmp/testopenfilegdb.gdb')
    except:
        pass
    try:
        gdaltest.unzip( 'tmp/', 'data/testopenfilegdb.gdb.zip')
    except:
        return 'skip'
    try:
        os.stat('tmp/testopenfilegdb.gdb')
    except:
        return 'skip'

    ds = ogr.Open('tmp/testopenfilegdb.gdb')
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test special SQL processing for min/max/count/sum/avg values

def ogr_openfilegdb_6():

    ds = ogr.Open('data/testopenfilegdb.gdb.zip')
    sql_lyr = ds.ExecuteSQL("select min(id), max(id), count(id), sum(id), avg(id), min(str), min(smallint), " \
                            "avg(smallint), min(float), avg(float), min(real), avg(real), min(adate), avg(adate), min(guid), min(nullint), avg(nullint) from point")
    if sql_lyr is None:
        return 'fail'
    feat = sql_lyr.GetNextFeature()
    if feat.GetField('MIN_id') != 1 or \
       feat.GetField('MAX_id') != 5 or \
       feat.GetField('COUNT_id') != 5 or \
       feat.GetField('SUM_id') != 15.0 or \
       feat.GetField('AVG_id') != 3.0 or \
       feat.GetField('MIN_str')[0:4] != 'foo_' or \
       feat.GetField('MIN_smallint') != -13 or \
       feat.GetField('AVG_smallint') != -13 or \
       feat.GetField('MIN_float') != 1.5 or \
       feat.GetField('AVG_float') != 1.5 or \
       feat.GetField('MIN_real') != 4.56 or \
       feat.GetField('AVG_real') != 4.56 or \
       feat.GetField('MIN_adate') != '2013/12/26 12:34:56' or \
       feat.GetField('AVG_adate') != '2013/12/26 12:34:56' or \
       feat.GetField('MIN_guid') != '{12345678-9ABC-DEF0-1234-567890ABCDEF}' or \
       feat.IsFieldSet('MIN_nullint') or \
       feat.IsFieldSet('AVG_nullint'):
        feat.DumpReadable()
        ds.ReleaseResultSet(sql_lyr)
        return 'fail'

    ds.ReleaseResultSet(sql_lyr)
    return 'success'

###############################################################################
# Test special SQL processing for ORDER BY

def ogr_openfilegdb_7():

    ds = ogr.Open('data/testopenfilegdb.gdb.zip')
    
    tests = [ # Optimized:
              ( "select * from point order by id", 5, 1, 1),
              ( "select id, str from point order by id desc", 5, 5, 1),
              ( "select * from point where id = 1 order by id", 1, 1, 1),
              ( "select * from big_layer order by real", 86 + 3 * 85, 1, 1),
              ( "select * from big_layer order by real desc", 86 + 3 * 85, 4 * 85, 1),
              # Invalid :
              ( "select foo from", None, None, None),
              ( "select foo from bar", None, None, None),
              ( "select * from point order by foo", None, None, None),
              # Non-optimized :
              ( "select * from point order by xml", None, None, 0),
              ( "select fid from point order by id", None, None, 0),
              ( "select cast(id as float) from point order by id", None, None, 0),
              ( "select distinct id from point order by id", None, None, 0),
              ( "select 1 from point order by id", None, None, 0),
              ( "select count(*) from point order by id", None, None, 0),
              ( "select * from point order by nullint", None, None, 0),
              ( "select * from point where id = 1 or id = 2 order by id", None, None, 0),
              ( "select * from point where id = 1 order by id, float", None, None, 0),
              ( "select * from point where float > 0 order by id", None, None, 0),
            ]

    for (sql, feat_count, first_fid, expected_optimized) in tests:
        if expected_optimized is None:
            gdal.PushErrorHandler('CPLQuietErrorHandler')
        sql_lyr = ds.ExecuteSQL(sql)
        if expected_optimized is None:
            gdal.PopErrorHandler()
        if expected_optimized is None:
            if sql_lyr is not None:
                ds.ReleaseResultSet(sql_lyr)
                print(sql, feat_count, first_fid)
                gdaltest.post_reason('fail')
                return 'fail'
            continue
        if sql_lyr is None:
            print(sql, feat_count, first_fid)
            gdaltest.post_reason('fail')
            return 'fail'
        if expected_optimized:
            if sql_lyr.GetFeatureCount() != feat_count:
                print(sql, feat_count, first_fid)
                ds.ReleaseResultSet(sql_lyr)
                gdaltest.post_reason('fail')
                return 'fail'
            feat = sql_lyr.GetNextFeature()
            if feat.GetFID() != first_fid:
                print(sql, feat_count, first_fid)
                ds.ReleaseResultSet(sql_lyr)
                feat.DumpReadable()
                gdaltest.post_reason('fail')
                return 'fail'
        ds.ReleaseResultSet(sql_lyr)

        sql_lyr = ds.ExecuteSQL('GetLastSQLUsedOptimizedImplementation')
        optimized = int(sql_lyr.GetNextFeature().GetField(0))
        ds.ReleaseResultSet(sql_lyr)
        if optimized != expected_optimized:
            print(sql, feat_count, first_fid)
            gdaltest.post_reason('fail')
            return 'fail'

        if optimized and sql.find('big_layer') < 0:
            import test_cli_utilities
            if test_cli_utilities.get_test_ogrsf_path() is not None:
                ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/testopenfilegdb.gdb.zip -sql "%s"' % sql)
                if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
                    print(sql, feat_count, first_fid)
                    print(ret)
                    gdaltest.post_reason('fail')
                    return 'fail'

    return 'success'

###############################################################################
# Test reading a .gdbtable without .gdbtablx

def ogr_openfilegdb_8():

    ds = ogr.Open('data/testopenfilegdb.gdb.zip')
    dict_feat_count = {}
    for i in range(ds.GetLayerCount()):
        lyr = ds.GetLayer(i)
        dict_feat_count[lyr.GetName()] = lyr.GetFeatureCount()
    ds = None

    dict_feat_count2 = {}
    gdal.SetConfigOption('OPENFILEGDB_IGNORE_GDBTABLX', 'YES')
    ds = ogr.Open('data/testopenfilegdb.gdb.zip')
    for i in range(ds.GetLayerCount()):
        lyr = ds.GetLayer(i)
        dict_feat_count2[lyr.GetName()] = lyr.GetFeatureCount()
    gdal.SetConfigOption('OPENFILEGDB_IGNORE_GDBTABLX', None)

    if dict_feat_count != dict_feat_count2:
        print(dict_feat_count)
        print(dict_feat_count2)
        return 'fail'

    lyr = ds.GetLayerByName('hole')
    # Not exactly in the order that one might expect, but logical when
    # looking at the structure of the .gdbtable
    expected_str = [ 'fid13', 'fid2', 'fid3', 'fid4', 'fid5', 'fid6', 'fid7', 'fid8', 'fid9', 'fid10', 'fid11', None ]
    i = 0
    feat = lyr.GetNextFeature()
    while feat is not None:
        if feat.GetField('str') != expected_str[i]:
            feat.DumpReadable()
            return 'fail'
        i = i + 1
        feat = lyr.GetNextFeature()

    return 'success'

###############################################################################
# Test reading a .gdbtable outside a .gdb

def ogr_openfilegdb_9():

    try:
        os.stat('tmp/testopenfilegdb.gdb')
    except:
        return 'skip'
 
    shutil.copy('tmp/testopenfilegdb.gdb/a00000009.gdbtable', 'tmp/a00000009.gdbtable')
    shutil.copy('tmp/testopenfilegdb.gdb/a00000009.gdbtablx', 'tmp/a00000009.gdbtablx')
    ds = ogr.Open('tmp/a00000009.gdbtable')
    if ds is None:
        return 'fail'
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    if feat is None:
        return 'fail'

    return 'success'

###############################################################################
# Test various error conditions

def fuzz(filename, offset):
    f = open(filename, "rb+")
    f.seek(offset, 0)
    v = ord(f.read(1))
    f.seek(offset, 0)
    import sys
    if sys.version_info >= (3,0,0):
        f.write(('%c' % (255 - v)).encode('ISO-8859-1'))
    else:
        f.write('%c' % (255 - v))
    f.close()
    return (filename, offset, v)

def unfuzz(backup):
    (filename, offset, v) = backup
    f = open(filename, "rb+")
    f.seek(offset, 0)
    import sys
    if sys.version_info >= (3,0,0):
        f.write(('%c' % (v)).encode('ISO-8859-1'))
    else:
        f.write('%c' % (v))
    f.close()

def ogr_openfilegdb_10():

    try:
        os.stat('tmp/testopenfilegdb.gdb')
    except:
        return 'skip'
 
    shutil.copytree('tmp/testopenfilegdb.gdb', 'tmp/testopenfilegdb_fuzzed.gdb')

    if False:
        for filename in ['tmp/testopenfilegdb_fuzzed.gdb/a00000001.gdbtable',
                        'tmp/testopenfilegdb_fuzzed.gdb/a00000001.gdbtablx']:
            errors = set()
            offsets = []
            last_error_msg = ''
            last_offset = -1
            for offset in range(os.stat(filename).st_size):
                #print(offset)
                backup = fuzz(filename, offset)
                gdal.ErrorReset()
                print(offset)
                ds = ogr.Open('tmp/testopenfilegdb_fuzzed.gdb')
                error_msg = gdal.GetLastErrorMsg()
                feat = None
                if ds is not None:
                    gdal.ErrorReset()
                    lyr = ds.GetLayerByName('GDB_SystemCatalog')
                    if error_msg == '':
                        error_msg = gdal.GetLastErrorMsg()
                    if lyr is not None:
                        gdal.ErrorReset()
                        feat = lyr.GetNextFeature()
                        if error_msg == '':
                            error_msg = gdal.GetLastErrorMsg()
                if feat is None or error_msg != '':
                    if offset - last_offset >= 4 or last_error_msg != error_msg:
                        if error_msg != '' and not error_msg in errors:
                            errors.add(error_msg)
                            offsets.append(offset)
                        else:
                            offsets.append(offset)
                    last_offset = offset
                    last_error_msg = error_msg
                ds = None
                unfuzz(backup)
            print(offsets)

        for filename in ['tmp/testopenfilegdb_fuzzed.gdb/a00000004.gdbindexes',
                         'tmp/testopenfilegdb_fuzzed.gdb/a00000004.CatItemsByPhysicalName.atx']:
            errors = set()
            offsets = []
            last_error_msg = ''
            last_offset = -1
            for offset in range(os.stat(filename).st_size):
                #print(offset)
                backup = fuzz(filename, offset)
                gdal.ErrorReset()
                print(offset)
                ds = ogr.Open('tmp/testopenfilegdb_fuzzed.gdb')
                error_msg = gdal.GetLastErrorMsg()
                feat = None
                if ds is not None:
                    gdal.ErrorReset()
                    lyr = ds.GetLayerByName('GDB_Items')
                    lyr.SetAttributeFilter("PhysicalName = 'NO_FIELD'")
                    if error_msg == '':
                        error_msg = gdal.GetLastErrorMsg()
                    if lyr is not None:
                        gdal.ErrorReset()
                        feat = lyr.GetNextFeature()
                        if error_msg == '':
                            error_msg = gdal.GetLastErrorMsg()
                if feat is None or error_msg != '':
                    if offset - last_offset >= 4 or last_error_msg != error_msg:
                        if error_msg != '' and not error_msg in errors:
                            errors.add(error_msg)
                            offsets.append(offset)
                        else:
                            offsets.append(offset)
                    last_offset = offset
                    last_error_msg = error_msg
                ds = None
                unfuzz(backup)
            print(offsets)

    else:

        for (filename, offsets) in [ ('tmp/testopenfilegdb_fuzzed.gdb/a00000001.gdbtable', [4, 7, 32, 33, 41, 42, 52, 59, 60, 63, 64, 72, 73, 77, 78, 79, 80, 81, 101, 102, 104, 105, 111, 180]),
                          ('tmp/testopenfilegdb_fuzzed.gdb/a00000001.gdbtablx', [4, 7, 11, 16, 31, 5136, 5140, 5142, 5144])]:
            for offset in offsets:
                backup = fuzz(filename, offset)
                gdal.PushErrorHandler('CPLQuietErrorHandler')
                gdal.ErrorReset()
                ds = ogr.Open('tmp/testopenfilegdb_fuzzed.gdb')
                error_msg = gdal.GetLastErrorMsg()
                feat = None
                if ds is not None:
                    gdal.ErrorReset()
                    lyr = ds.GetLayerByName('GDB_SystemCatalog')
                    if error_msg == '':
                        error_msg = gdal.GetLastErrorMsg()
                    if lyr is not None:
                        gdal.ErrorReset()
                        feat = lyr.GetNextFeature()
                        if error_msg == '':
                            error_msg = gdal.GetLastErrorMsg()
                if feat is not None and error_msg == '':
                    print('%s: expected problem at offset %d, but did not find' % (filename, offset))
                ds = None
                gdal.PopErrorHandler()
                unfuzz(backup)

        for (filename, offsets) in [ ('tmp/testopenfilegdb_fuzzed.gdb/a00000004.gdbindexes', [0, 4, 5, 44, 45, 66, 67, 100, 101, 116, 117, 148, 149, 162, 163, 206, 207, 220, 221, 224, 280, 281]),
                          ('tmp/testopenfilegdb_fuzzed.gdb/a00000004.CatItemsByPhysicalName.atx', [4, 108, 268, 428, 588, 748, 908, 1068, 1228, 1388, 1548, 1708, 1868, 2028, 2188, 2348, 4096, 4098, 4102, 4106]) ]:
            for offset in offsets:
                #print(offset)
                backup = fuzz(filename, offset)
                gdal.PushErrorHandler('CPLQuietErrorHandler')
                gdal.ErrorReset()
                ds = ogr.Open('tmp/testopenfilegdb_fuzzed.gdb')
                error_msg = gdal.GetLastErrorMsg()
                feat = None
                if ds is not None:
                    gdal.ErrorReset()
                    lyr = ds.GetLayerByName('GDB_Items')
                    lyr.SetAttributeFilter("PhysicalName = 'NO_FIELD'")
                    if error_msg == '':
                        error_msg = gdal.GetLastErrorMsg()
                    if lyr is not None:
                        gdal.ErrorReset()
                        feat = lyr.GetNextFeature()
                        if error_msg == '':
                            error_msg = gdal.GetLastErrorMsg()
                if feat is not None and error_msg == '':
                    print('%s: expected problem at offset %d, but did not find' % (filename, offset))
                ds = None
                gdal.PopErrorHandler()
                unfuzz(backup)

    return 'success'

###############################################################################
# Test spatial filtering

SPI_IN_BUILDING = 0
SPI_COMPLETED = 1
SPI_INVALID = 2

def get_spi_state(ds, lyr):
    sql_lyr = ds.ExecuteSQL('GetLayerSpatialIndexState %s' % lyr.GetName())
    value = int(sql_lyr.GetNextFeature().GetField(0))
    ds.ReleaseResultSet(sql_lyr)
    return value

def ogr_openfilegdb_11():

    # Test building spatial index with GetFeatureCount()
    ds = ogr.Open('data/testopenfilegdb.gdb.zip')
    lyr = ds.GetLayerByName('several_polygons')
    if get_spi_state(ds, lyr) != SPI_IN_BUILDING:
        gdaltest.post_reason('failure')
        return 'fail'
    lyr.ResetReading()
    if get_spi_state(ds, lyr) != SPI_IN_BUILDING:
        gdaltest.post_reason('failure')
        return 'fail'
    if lyr.TestCapability(ogr.OLCFastFeatureCount) != 1:
        gdaltest.post_reason('failure')
        return 'fail'
    lyr.SetSpatialFilterRect(0.25,0.25,0.5,0.5)
    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('failure')
        return 'fail'
    if get_spi_state(ds, lyr) != SPI_COMPLETED:
        gdaltest.post_reason('failure')
        return 'fail'
    # Should return cached value
    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('failure')
        return 'fail'
    # Should use index
    c = 0
    feat = lyr.GetNextFeature()
    while feat is not None:
        c = c + 1
        feat = lyr.GetNextFeature()
    if c != 1:
        gdaltest.post_reason('failure')
        return 'fail'
    feat = None
    lyr = None
    ds = None

    # Test iterating without spatial index already built
    ds = ogr.Open('data/testopenfilegdb.gdb.zip')
    lyr = ds.GetLayerByName('several_polygons')
    lyr.SetSpatialFilterRect(0.25,0.25,0.5,0.5)
    c = 0
    feat = lyr.GetNextFeature()
    if get_spi_state(ds, lyr) != SPI_IN_BUILDING:
        gdaltest.post_reason('failure')
        return 'fail'
    while feat is not None:
        c = c + 1
        feat = lyr.GetNextFeature()
    if c != 1:
        gdaltest.post_reason('failure')
        return 'fail'
    if get_spi_state(ds, lyr) != SPI_COMPLETED:
        gdaltest.post_reason('failure')
        return 'fail'
    feat = None
    lyr = None
    ds = None
    
    # Test GetFeatureCount() without spatial index already built, with no matching feature
    # when GEOS is available
    if ogrtest.have_geos():
        expected_count = 0
    else:
        expected_count = 5

    ds = ogr.Open('data/testopenfilegdb.gdb.zip')
    lyr = ds.GetLayerByName('multipolygon')
    lyr.SetSpatialFilterRect(1.4,0.4,1.6,0.6)
    if lyr.GetFeatureCount() != expected_count:
        gdaltest.post_reason('failure')
        return 'fail'
    lyr = None
    ds = None

    # Test iterating without spatial index already built, with no matching feature
    # when GEOS is available
    ds = ogr.Open('data/testopenfilegdb.gdb.zip')
    lyr = ds.GetLayerByName('multipolygon')
    lyr.SetSpatialFilterRect(1.4,0.4,1.6,0.6)
    c = 0
    feat = lyr.GetNextFeature()
    while feat is not None:
        c = c + 1
        feat = lyr.GetNextFeature()
    if c != expected_count:
        gdaltest.post_reason('failure')
        return 'fail'
    if lyr.GetFeatureCount() != expected_count:
        gdaltest.post_reason('failure')
        return 'fail'
    feat = None
    lyr = None
    ds = None

    # GetFeature() should not impact spatial index building
    ds = ogr.Open('data/testopenfilegdb.gdb.zip')
    lyr = ds.GetLayerByName('several_polygons')
    lyr.SetSpatialFilterRect(0.25,0.25,0.5,0.5)
    feat = lyr.GetFeature(1)
    feat = lyr.GetFeature(1)
    if get_spi_state(ds, lyr) != SPI_IN_BUILDING:
        gdaltest.post_reason('failure')
        return 'fail'
    feat = lyr.GetNextFeature()
    while feat is not None:
        feat = lyr.GetNextFeature()
    if get_spi_state(ds, lyr) != SPI_COMPLETED:
        gdaltest.post_reason('failure')
        return 'fail'
    lyr.ResetReading()
    c = 0
    feat = lyr.GetNextFeature()
    while feat is not None:
        c = c + 1
        feat = lyr.GetNextFeature()
    if c != 1:
        gdaltest.post_reason('failure')
        return 'fail'
    feat = None
    lyr = None
    ds = None

    # but SetNextByIndex() does
    ds = ogr.Open('data/testopenfilegdb.gdb.zip')
    lyr = ds.GetLayerByName('multipolygon')
    lyr.SetNextByIndex(3)
    if get_spi_state(ds, lyr) != SPI_INVALID:
        gdaltest.post_reason('failure')
        return 'fail'
    feat = None
    lyr = None
    ds = None

    # and ResetReading() as well
    ds = ogr.Open('data/testopenfilegdb.gdb.zip')
    lyr = ds.GetLayerByName('multipolygon')
    feat = lyr.GetNextFeature()
    lyr.ResetReading()
    if get_spi_state(ds, lyr) != SPI_INVALID:
        gdaltest.post_reason('failure')
        return 'fail'
    feat = None
    lyr = None
    ds = None

    # and SetAttributeFilter() with an index too
    ds = ogr.Open('data/testopenfilegdb.gdb.zip')
    lyr = ds.GetLayerByName('point')
    lyr.SetAttributeFilter('id = 1')
    if get_spi_state(ds, lyr) != SPI_INVALID:
        gdaltest.post_reason('failure')
        return 'fail'
    feat = None
    lyr = None
    ds = None
    return 'success'

###############################################################################
# Test opening a FGDB with both SRID and LatestSRID set (#5638)

def ogr_openfilegdb_12():

    ds = ogr.Open('/vsizip/data/test3005.gdb.zip')
    lyr = ds.GetLayer(0)
    got_wkt = lyr.GetSpatialRef().ExportToWkt()
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(3005)
    expected_wkt = sr.ExportToWkt()
    if got_wkt != expected_wkt:
        gdaltest.post_reason('fail')
        print(got_wkt)
        print(expected_wkt)
        return 'fail'
    ds = None

    return 'success'
    
###############################################################################
# Cleanup

def ogr_openfilegdb_cleanup():

    if ogrtest.fgdb_drv is not None:
        ogrtest.fgdb_drv.Register()

    try:
        shutil.rmtree('tmp/testopenfilegdb.gdb')
    except:
        pass
    try:
        os.remove('tmp/a00000009.gdbtable')
        os.remove('tmp/a00000009.gdbtablx')
    except:
        pass
    try:
        shutil.rmtree('tmp/testopenfilegdb_fuzzed.gdb')
    except:
        pass

    return 'success'

gdaltest_list = [
    ogr_openfilegdb_init,
    #ogr_openfilegdb_make_test_data,
    ogr_openfilegdb_1,
    ogr_openfilegdb_2,
    ogr_openfilegdb_3,
    ogr_openfilegdb_4,
    ogr_openfilegdb_5,
    ogr_openfilegdb_6,
    ogr_openfilegdb_7,
    ogr_openfilegdb_8,
    ogr_openfilegdb_9,
    ogr_openfilegdb_10,
    ogr_openfilegdb_11,
    ogr_openfilegdb_12,
    ogr_openfilegdb_cleanup,
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_openfilegdb' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()


#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  FGDB driver testing.
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2011-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
import shutil

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import gdal
from osgeo import ogr
from osgeo import osr

###############################################################################
# Test if driver is available

def ogr_fgdb_init():

    ogrtest.fgdb_drv = None

    try:
        ogrtest.fgdb_drv = ogr.GetDriverByName('FileGDB')
    except:
        pass

    if ogrtest.fgdb_drv is None:
        return 'skip'

    try:
        ogrtest.openfilegdb_drv = ogr.GetDriverByName('OpenFileGDB')
    except:
        ogrtest.openfilegdb_drv = None
    if ogrtest.openfilegdb_drv is not None:
        ogrtest.openfilegdb_drv.Deregister()

    try:
        shutil.rmtree("tmp/test.gdb")
    except:
        pass

    return 'success'

###############################################################################
def ogr_fgdb_is_sdk_1_4_or_later():

    if ogrtest.fgdb_drv is None:
        return False

    if hasattr(ogrtest, 'fgdb_is_sdk_1_4'):
        return ogrtest.fgdb_is_sdk_1_4

    ogrtest.fgdb_is_sdk_1_4 = False

    try:
        shutil.rmtree("tmp/ogr_fgdb_is_sdk_1_4_or_later.gdb")
    except:
        pass

    ds = ogrtest.fgdb_drv.CreateDataSource("tmp/ogr_fgdb_is_sdk_1_4_or_later.gdb")
    srs = osr.SpatialReference()
    srs.ImportFromProj4('+proj=tmerc +datum=WGS84 +no_defs')
    with gdaltest.error_handler():
        lyr = ds.CreateLayer('test', srs = srs, geom_type = ogr.wkbPoint)
    if lyr is not None:
        ogrtest.fgdb_is_sdk_1_4 = True
    ds = None
    shutil.rmtree("tmp/ogr_fgdb_is_sdk_1_4_or_later.gdb")
    return ogrtest.fgdb_is_sdk_1_4


###############################################################################
# Write and read back various geometry types

def ogr_fgdb_1():
    if ogrtest.fgdb_drv is None:
        return 'skip'

    srs = osr.SpatialReference()
    srs.SetFromUserInput("WGS84")

    ds = ogrtest.fgdb_drv.CreateDataSource("tmp/test.gdb")

    datalist = [ [ "none", ogr.wkbNone, None],
                 [ "point", ogr.wkbPoint, "POINT (1 2)" ],
                 [ "multipoint", ogr.wkbMultiPoint, "MULTIPOINT (1 2,3 4)" ],
                 [ "linestring", ogr.wkbLineString, "LINESTRING (1 2,3 4)", "MULTILINESTRING ((1 2,3 4))" ],
                 [ "multilinestring", ogr.wkbMultiLineString, "MULTILINESTRING ((1 2,3 4),(5 6,7 8))" ],
                 [ "polygon", ogr.wkbPolygon, "POLYGON ((0 0,0 1,1 1,1 0,0 0))", "MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0)))" ],
                 [ "multipolygon", ogr.wkbMultiPolygon, "MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0),(0.25 0.25,0.75 0.25,0.75 0.75,0.25 0.75,0.25 0.25)),((2 0,2 1,3 1,3 0,2 0)))" ],
                 [ "point25D", ogr.wkbPoint25D, "POINT (1 2 3)" ],
                 [ "multipoint25D", ogr.wkbMultiPoint25D, "MULTIPOINT (1 2 -10,3 4 -20)" ],
                 [ "linestring25D", ogr.wkbLineString25D, "LINESTRING (1 2 -10,3 4 -20)", "MULTILINESTRING ((1 2 -10,3 4 -20))" ],
                 [ "multilinestring25D", ogr.wkbMultiLineString25D, "MULTILINESTRING ((1 2 -10,3 4 -20))" ],
                 [ "polygon25D", ogr.wkbPolygon25D, "POLYGON ((0 0 -10,0 1 -10,1 1 -10,1 0 -10,0 0 -10))", "MULTIPOLYGON (((0 0 -10,0 1 -10,1 1 -10,1 0 -10,0 0 -10)))" ],
                 [ "multipolygon25D", ogr.wkbMultiPolygon25D, "MULTIPOLYGON (((0 0 -10,0 1 -10,1 1 -10,1 0 -10,0 0 -10)))" ],
                 [ "multipatch", ogr.wkbMultiPolygon25D, "GEOMETRYCOLLECTION Z (TIN Z (((0.0 0.0 0,0.0 1.0 0,1.0 0.0 0,0.0 0.0 0)),((0.0 1.0 0,1.0 0.0 0,1.0 1.0 0,0.0 1.0 0))),TIN Z (((10.0 0.0 0,10.0 1.0 0,11.0 0.0 0,10.0 0.0 0)),((10.0 0.0 0,11.0 0.0 0,10.0 -1.0 0,10.0 0.0 0))),TIN Z (((5.0 0.0 0,5.0 1.0 0,6.0 0.0 0,5.0 0.0 0))),MULTIPOLYGON Z (((100.0 0.0 0,100.0 1.0 0,101.0 1.0 0,101.0 0.0 0,100.0 0.0 0),(100.25 0.25 0,100.75 0.25 0,100.75 0.75 0,100.75 0.25 0,100.25 0.25 0))))" ],
                 [ "tin", ogr.wkbTINZ, "TIN Z (((0.0 0.0 0,0.0 1.0 0,1.0 0.0 0,0.0 0.0 0)),((0.0 1.0 0,1.0 0.0 0,1.0 1.0 0,0.0 1.0 0)))" ],
                 [ "null_polygon", ogr.wkbPolygon, None],
                 [ "empty_polygon", ogr.wkbPolygon, "POLYGON EMPTY", None],
               ]

    options = ['COLUMN_TYPES=smallint=esriFieldTypeSmallInteger,float=esriFieldTypeSingle,guid=esriFieldTypeGUID,xml=esriFieldTypeXML']

    for data in datalist:
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
        lyr.CreateField(ogr.FieldDefn("binary2", ogr.OFTBinary))
        fld_defn = ogr.FieldDefn("smallint2", ogr.OFTInteger)
        fld_defn.SetSubType(ogr.OFSTInt16)
        lyr.CreateField(fld_defn)
        fld_defn = ogr.FieldDefn("float2", ogr.OFTReal)
        fld_defn.SetSubType(ogr.OFSTFloat32)
        lyr.CreateField(fld_defn)

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
            feat.SetField("xml", "<foo></foo>")
            feat.SetFieldBinaryFromHexString("binary", "00FF7F")
            feat.SetFieldBinaryFromHexString("binary2", "123456")
            feat.SetField("smallint2", -32768)
            feat.SetField("float2", 1.5)
            lyr.CreateFeature(feat)

    for data in datalist:
        lyr = ds.GetLayerByName(data[0])
        if data[1] != ogr.wkbNone:
            if lyr.GetSpatialRef().IsSame(srs) != 1:
                gdaltest.post_reason('fail')
                print(lyr.GetSpatialRef())
                return 'fail'
        feat = lyr.GetNextFeature()
        if data[1] != ogr.wkbNone:
            try:
                expected_wkt = data[3]
            except:
                expected_wkt = data[2]
            if expected_wkt is None:
                if feat.GetGeometryRef() is not None:
                    gdaltest.post_reason('fail')
                    print(data)
                    feat.DumpReadable()
                    return 'fail'
            elif ogrtest.check_feature_geometry(feat, expected_wkt) != 0:
                gdaltest.post_reason('fail')
                print(data)
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
           feat.GetField('binary2') != "123456" or \
           feat.GetField('smallint2') != -32768:
            gdaltest.post_reason('fail')
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
# Test DeleteField()

def ogr_fgdb_DeleteField():
    if ogrtest.fgdb_drv is None:
        return 'skip'

    ds = ogr.Open("tmp/test.gdb", update = 1)
    lyr = ds.GetLayerByIndex(0)

    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('smallint')).GetSubType() != ogr.OFSTInt16:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('smallint2')).GetSubType() != ogr.OFSTInt16:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('float')).GetSubType() != ogr.OFSTFloat32:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('float2')).GetSubType() != ogr.OFSTFloat32:
        gdaltest.post_reason('fail')
        return 'fail'

    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('smallint')).GetWidth() != 0:
        gdaltest.post_reason('failure')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('str')).GetWidth() != 0:
        gdaltest.post_reason('failure')
        return 'fail'
    if lyr.DeleteField(lyr.GetLayerDefn().GetFieldIndex('str')) != 0:
        gdaltest.post_reason('failure')
        return 'fail'

    # Needed since FileGDB v1.4, otherwise crash/error ...
    if True:
        ds = ogr.Open("tmp/test.gdb", update = 1)
        lyr = ds.GetLayerByIndex(0)

    fld_defn = ogr.FieldDefn("str2", ogr.OFTString)
    fld_defn.SetWidth(80)
    lyr.CreateField(fld_defn)
    feat = lyr.GetNextFeature()
    feat.SetField("str2", "foo2_\xc3\xa9")
    lyr.SetFeature(feat)

    # Test updating non-existing feature
    feat.SetFID(-10)
    if lyr.SetFeature( feat ) != ogr.OGRERR_NON_EXISTING_FEATURE:
        gdaltest.post_reason( 'Expected failure of SetFeature().' )
        return 'fail'

    # Test deleting non-existing feature
    if lyr.DeleteFeature( -10 ) != ogr.OGRERR_NON_EXISTING_FEATURE:
        gdaltest.post_reason( 'Expected failure of DeleteFeature().' )
        return 'fail'

    feat = None
    ds = None

    ds = ogr.Open("tmp/test.gdb")
    lyr = ds.GetLayerByIndex(0)
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('str2')).GetWidth() != 80:
        gdaltest.post_reason('failure')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldIndex('str') != -1:
        gdaltest.post_reason('failure')
        return 'fail'
    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString("str2") != "foo2_\xc3\xa9":
        gdaltest.post_reason('failure')
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Run test_ogrsf

def ogr_fgdb_2():
    if ogrtest.fgdb_drv is None:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro tmp/test.gdb --config OGR_SKIP OpenFileGDB')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Run ogr2ogr

def ogr_fgdb_3():
    if ogrtest.fgdb_drv is None:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_ogr2ogr_path() is None:
        return 'skip'

    try:
        shutil.rmtree("tmp/poly.gdb")
    except:
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f filegdb tmp/poly.gdb data/poly.shp -nlt MULTIPOLYGON -a_srs None')

    ds = ogr.Open('tmp/poly.gdb')
    if ds is None or ds.GetLayerCount() == 0:
        gdaltest.post_reason('ogr2ogr failed')
        return 'fail'
    ds = None

    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' tmp/poly.gdb')
    #print ret

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test SQL support

def ogr_fgdb_sql():
    if ogrtest.fgdb_drv is None:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_ogr2ogr_path() is None:
        return 'skip'

    ds = ogr.Open('tmp/poly.gdb')

    ds.ExecuteSQL("CREATE INDEX idx_poly_eas_id ON poly(EAS_ID)")

    sql_lyr = ds.ExecuteSQL("SELECT * FROM POLY WHERE EAS_ID = 170", dialect = 'FileGDB')
    feat = sql_lyr.GetNextFeature()
    if feat is None:
        return 'fail'
    feat = sql_lyr.GetNextFeature()
    if feat is not None:
        return 'fail'
    feat = None
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    return 'success'

###############################################################################
# Test delete layer

def ogr_fgdb_4():
    if ogrtest.fgdb_drv is None:
        return 'skip'

    for j in range(2):

        # Create a layer
        ds = ogr.Open("tmp/test.gdb", update = 1)
        srs = osr.SpatialReference()
        srs.SetFromUserInput("WGS84")
        lyr = ds.CreateLayer("layer_to_remove", geom_type = ogr.wkbPoint, srs = srs)
        lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(2 49)'))
        feat.SetField("str", "foo")
        feat = None
        lyr = None

        if j == 1:
            ds = None
            ds = ogr.Open("tmp/test.gdb", update = 1)

        # Delete it
        for i in range(ds.GetLayerCount()):
            if ds.GetLayer(i).GetName() == 'layer_to_remove':
                ds.DeleteLayer(i)
                break

        # Check it no longer exists
        lyr = ds.GetLayerByName('layer_to_remove')
        ds = None

        if lyr is not None:
            gdaltest.post_reason('failed at iteration %d' % j)
            return 'fail'

    return 'success'

###############################################################################
# Test DeleteDataSource()

def ogr_fgdb_5():
    if ogrtest.fgdb_drv is None:
        return 'skip'

    if ogrtest.fgdb_drv.DeleteDataSource("tmp/test.gdb") != 0:
        gdaltest.post_reason('DeleteDataSource() failed')
        return 'fail'

    try:
        os.stat("tmp/test.gdb")
        gdaltest.post_reason("tmp/test.gdb still existing")
        return 'fail'
    except:
        pass

    return 'success'

###############################################################################
# Test adding a layer to an existing feature dataset

def ogr_fgdb_6():
    if ogrtest.fgdb_drv is None:
        return 'skip'

    srs = osr.SpatialReference()
    srs.SetFromUserInput("WGS84")

    ds = ogrtest.fgdb_drv.CreateDataSource('tmp/test.gdb')
    ds.CreateLayer('layer1', srs = srs, geom_type = ogr.wkbPoint, options = ['FEATURE_DATASET=featuredataset'])
    ds.CreateLayer('layer2', srs = srs, geom_type = ogr.wkbPoint, options = ['FEATURE_DATASET=featuredataset'])
    ds = None

    ds = ogr.Open('tmp/test.gdb')
    if ds.GetLayerCount() != 2:
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test bulk loading (#4420)

def ogr_fgdb_7():
    if ogrtest.fgdb_drv is None:
        return 'skip'

    try:
        shutil.rmtree("tmp/test.gdb")
    except:
        pass

    srs = osr.SpatialReference()
    srs.SetFromUserInput("WGS84")

    ds = ogrtest.fgdb_drv.CreateDataSource('tmp/test.gdb')
    lyr = ds.CreateLayer('test', srs = srs, geom_type = ogr.wkbPoint)
    lyr.CreateField(ogr.FieldDefn('id', ogr.OFTInteger))
    gdal.SetConfigOption('FGDB_BULK_LOAD', 'YES')
    for i in range(1000):
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetField(0, i)
        geom = ogr.CreateGeometryFromWkt('POINT(0 1)')
        feat.SetGeometry(geom)
        lyr.CreateFeature(feat)
        feat = None

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat.GetField(0) != 0:
        return 'fail'
    ds = None

    gdal.SetConfigOption('FGDB_BULK_LOAD', None)

    return 'success'

###############################################################################
# Test field name laundering (#4458)

def ogr_fgdb_8():
    if ogrtest.fgdb_drv is None:
        return 'skip'

    try:
        shutil.rmtree("tmp/test.gdb")
    except:
        pass

    srs = osr.SpatialReference()
    srs.SetFromUserInput("WGS84")

    ds = ogrtest.fgdb_drv.CreateDataSource('tmp/test.gdb')
    lyr = ds.CreateLayer('test', srs = srs, geom_type = ogr.wkbPoint)
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr.CreateField(ogr.FieldDefn('FROM', ogr.OFTInteger)) # reserved keyword
    lyr.CreateField(ogr.FieldDefn('1NUMBER', ogr.OFTInteger)) # starting with a number
    lyr.CreateField(ogr.FieldDefn('WITH SPACE AND !$*!- special characters', ogr.OFTInteger)) # unallowed characters
    lyr.CreateField(ogr.FieldDefn('A123456789012345678901234567890123456789012345678901234567890123', ogr.OFTInteger)) # 64 characters : ok
    lyr.CreateField(ogr.FieldDefn('A1234567890123456789012345678901234567890123456789012345678901234', ogr.OFTInteger)) # 65 characters : nok
    lyr.CreateField(ogr.FieldDefn('A12345678901234567890123456789012345678901234567890123456789012345', ogr.OFTInteger)) # 66 characters : nok
    gdal.PopErrorHandler()

    lyr_defn = lyr.GetLayerDefn()
    expected_names = [ 'FROM_', '_1NUMBER', 'WITH_SPACE_AND_______special_characters',
                       'A123456789012345678901234567890123456789012345678901234567890123',
                       'A1234567890123456789012345678901234567890123456789012345678901_1',
                       'A1234567890123456789012345678901234567890123456789012345678901_2']
    for i in range(5):
        if lyr_defn.GetFieldIndex(expected_names[i]) != i:
            gdaltest.post_reason('did not find %s' % expected_names[i])
            return 'fail'

    return 'success'

###############################################################################
# Test layer name laundering (#4466)

def ogr_fgdb_9():
    if ogrtest.fgdb_drv is None:
        return 'skip'

    try:
        shutil.rmtree("tmp/test.gdb")
    except:
        pass

    srs = osr.SpatialReference()
    srs.SetFromUserInput("WGS84")

    _160char = ''.join(['A123456789' for i in range(16)])

    in_names = [ 'FROM', # reserved keyword
                 '1NUMBER', # starting with a number
                 'WITH SPACE AND !$*!- special characters', # banned characters
                 'sde_foo', # reserved prefixes
                 _160char, # OK
                 _160char + 'A', # too long
                 _160char + 'B', # still too long
               ]

    ds = ogrtest.fgdb_drv.CreateDataSource('tmp/test.gdb')
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    for i in range(len(in_names)):
        lyr = ds.CreateLayer(in_names[i], srs = srs, geom_type = ogr.wkbPoint)
    gdal.PopErrorHandler()

    lyr.GetLayerDefn()
    expected_names = [ 'FROM_',
                       '_1NUMBER',
                       'WITH_SPACE_AND_______special_characters',
                       '_sde_foo',
                       _160char,
                       _160char[0:158] + '_1',
                       _160char[0:158] + '_2' ]
    for i in range(len(expected_names)):
        if ds.GetLayerByIndex(i).GetName() != expected_names[i]:
            gdaltest.post_reason('did not find %s' % expected_names[i])
            return 'fail'

    return 'success'

###############################################################################
# Test SRS support

def ogr_fgdb_10():
    if ogrtest.fgdb_drv is None:
        return 'skip'

    try:
        shutil.rmtree("tmp/test.gdb")
    except:
        pass

    srs_exact_4326 = osr.SpatialReference()
    srs_exact_4326.ImportFromEPSG(4326)

    srs_approx_4326 = srs_exact_4326.Clone()
    srs_approx_4326.MorphToESRI()
    srs_approx_4326.MorphFromESRI()

    srs_exact_2193 = osr.SpatialReference()
    srs_exact_2193.ImportFromEPSG(2193)

    srs_approx_2193 = srs_exact_2193.Clone()
    srs_approx_2193.MorphToESRI()
    srs_approx_2193.MorphFromESRI()

    srs_not_in_db = osr.SpatialReference("""PROJCS["foo",
    GEOGCS["foo",
        DATUM["foo",
            SPHEROID["foo",6000000,300]],
        PRIMEM["Greenwich",0],
        UNIT["Degree",0.017453292519943295]],
    PROJECTION["Transverse_Mercator"],
    PARAMETER["latitude_of_origin",0],
    PARAMETER["central_meridian",0],
    PARAMETER["scale_factor",1],
    PARAMETER["false_easting",500000],
    PARAMETER["false_northing",0],
    UNIT["Meter",1]]""")

    srs_exact_4230 = osr.SpatialReference()
    srs_exact_4230.ImportFromEPSG(4230)
    srs_approx_4230 = srs_exact_4230.Clone()
    srs_approx_4230.MorphToESRI()
    srs_approx_4230.MorphFromESRI()

    srs_approx_intl = osr.SpatialReference()
    srs_approx_intl.ImportFromProj4('+proj=longlat +ellps=intl +no_defs')

    srs_exact_4233 = osr.SpatialReference()
    srs_exact_4233.ImportFromEPSG(4233)

    ds = ogrtest.fgdb_drv.CreateDataSource('tmp/test.gdb')
    lyr = ds.CreateLayer("srs_exact_4326", srs = srs_exact_4326, geom_type = ogr.wkbPoint)
    lyr = ds.CreateLayer("srs_approx_4326", srs = srs_approx_4326, geom_type = ogr.wkbPoint)
    lyr = ds.CreateLayer("srs_exact_2193", srs = srs_exact_2193, geom_type = ogr.wkbPoint)
    lyr = ds.CreateLayer("srs_approx_2193", srs = srs_approx_2193, geom_type = ogr.wkbPoint)

    lyr = ds.CreateLayer("srs_approx_4230", srs = srs_approx_4230, geom_type = ogr.wkbPoint)

    # will fail
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr = ds.CreateLayer("srs_approx_intl", srs = srs_approx_intl, geom_type = ogr.wkbPoint)
    gdal.PopErrorHandler()

    # will fail: 4233 doesn't exist in DB
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr = ds.CreateLayer("srs_exact_4233", srs = srs_exact_4233, geom_type = ogr.wkbPoint)
    gdal.PopErrorHandler()

    # will fail
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr = ds.CreateLayer("srs_not_in_db", srs = srs_not_in_db, geom_type = ogr.wkbPoint)
    gdal.PopErrorHandler()

    ds = None

    ds = ogr.Open('tmp/test.gdb')
    lyr = ds.GetLayerByName("srs_exact_4326")
    if lyr.GetSpatialRef().ExportToWkt().find('4326') == -1:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr = ds.GetLayerByName("srs_approx_4326")
    if lyr.GetSpatialRef().ExportToWkt().find('4326') == -1:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr = ds.GetLayerByName("srs_exact_2193")
    if lyr.GetSpatialRef().ExportToWkt().find('2193') == -1:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr = ds.GetLayerByName("srs_approx_2193")
    if lyr.GetSpatialRef().ExportToWkt().find('2193') == -1:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr = ds.GetLayerByName("srs_approx_4230")
    if lyr.GetSpatialRef().ExportToWkt().find('4230') == -1:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test all data types

def ogr_fgdb_11():
    if ogrtest.fgdb_drv is None:
        return 'skip'

    try:
        shutil.rmtree("tmp/test.gdb")
    except:
        pass

    f = open('data/test_filegdb_field_types.xml', 'rt')
    xml_def = f.read()
    f.close()

    ds = ogrtest.fgdb_drv.CreateDataSource('tmp/test.gdb')
    lyr = ds.CreateLayer("test", geom_type = ogr.wkbNone, options = ['XML_DEFINITION=%s' % xml_def])
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("esriFieldTypeSmallInteger", 12)
    feat.SetField("esriFieldTypeInteger", 3456)
    feat.SetField("esriFieldTypeSingle", 78.9)
    feat.SetField("esriFieldTypeDouble", 1.23)
    feat.SetField("esriFieldTypeDate", "2012/12/31 12:34:56")
    feat.SetField("esriFieldTypeString", "astr")
    feat.SetField("esriFieldTypeGlobalID", "{12345678-9ABC-DEF0-1234-567890ABCDEF}") # This is ignored and value is generated by FileGDB SDK itself
    feat.SetField("esriFieldTypeGUID", "{12345678-9abc-DEF0-1234-567890ABCDEF}")
    lyr.CreateFeature(feat)
    feat = None

    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)
    feat = None

    # Create a esriFieldTypeGlobalID field
    lyr = ds.CreateLayer('test2', geom_type = ogr.wkbNone, options = ['COLUMN_TYPES=global_id=esriFieldTypeGlobalID'])
    lyr.CreateField(ogr.FieldDefn('global_id', ogr.OFTString))
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)
    feat = None

    ds = None

    ds = ogr.Open('tmp/test.gdb')
    lyr = ds.GetLayerByName('test')
    feat = lyr.GetNextFeature()
    if feat.GetField('esriFieldTypeSmallInteger') != 12 or \
       feat.GetField('esriFieldTypeInteger') != 3456 or \
       abs(feat.GetField('esriFieldTypeSingle') - 78.9) > 1e-2 or \
       feat.GetField('esriFieldTypeDouble') != 1.23 or \
       feat.GetField('esriFieldTypeDate') != '2012/12/31 12:34:56' or \
       feat.GetField('esriFieldTypeString') != 'astr' or \
       feat.GetField('esriFieldTypeGUID') != '{12345678-9ABC-DEF0-1234-567890ABCDEF}' or \
       (not feat.IsFieldSet('esriFieldTypeGlobalID')):
        feat.DumpReadable()
        return 'fail'

    feat = lyr.GetNextFeature()
    if not feat.IsFieldSet('esriFieldTypeGlobalID'):
        feat.DumpReadable()
        return 'fail'

    lyr = ds.GetLayerByName('test2')
    feat = lyr.GetNextFeature()
    if not feat.IsFieldSet('global_id'):
        feat.DumpReadable()
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test failed Open()

def ogr_fgdb_12():
    if ogrtest.fgdb_drv is None:
        return 'skip'

    ds = ogr.Open('tmp/non_existing.gdb')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    try:
        os.unlink('tmp/dummy.gdb')
    except:
        pass
    try:
        shutil.rmtree('tmp/dummy.gdb')
    except:
        pass

    f = open('tmp/dummy.gdb', 'wb')
    f.close()

    ds = ogr.Open('tmp/dummy.gdb')
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    os.unlink('tmp/dummy.gdb')

    os.mkdir('tmp/dummy.gdb')

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogr.Open('tmp/dummy.gdb')
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    shutil.rmtree('tmp/dummy.gdb')

    return 'success'

###############################################################################
# Test failed CreateDataSource() and DeleteDataSource()

def ogr_fgdb_13():
    if ogrtest.fgdb_drv is None:
        return 'skip'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogrtest.fgdb_drv.CreateDataSource('tmp/foo')
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    f = open('tmp/dummy.gdb', 'wb')
    f.close()

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogrtest.fgdb_drv.CreateDataSource('tmp/dummy.gdb')
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    os.unlink('tmp/dummy.gdb')

    try:
        shutil.rmtree("/nonexistingdir")
    except:
        pass

    name = '/nonexistingdrive:/nonexistingdir/dummy.gdb'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogrtest.fgdb_drv.CreateDataSource(name)
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = ogrtest.fgdb_drv.DeleteDataSource(name)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test interleaved opening and closing of databases (#4270)

def ogr_fgdb_14():
    if ogrtest.fgdb_drv is None:
        return 'skip'

    for i in range(3):
        ds1 = ogr.Open("tmp/test.gdb")
        if ds1 is None:
            gdaltest.post_reason('fail')
            return 'fail'
        ds2 = ogr.Open("tmp/test.gdb")
        if ds2 is None:
            gdaltest.post_reason('fail')
            return 'fail'
        ds2 = None
        ds1 = None

    return 'success'

###############################################################################
# Test opening a FGDB with both SRID and LatestSRID set (#5638)

def ogr_fgdb_15():
    if ogrtest.fgdb_drv is None:
        return 'skip'

    try:
        shutil.rmtree('tmp/test3005.gdb')
    except:
        pass
    gdaltest.unzip( 'tmp', 'data/test3005.gdb.zip')
    ds = ogr.Open('tmp/test3005.gdb')
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
# Test fix for #5674

def ogr_fgdb_16():
    if ogrtest.fgdb_drv is None or ogrtest.openfilegdb_drv is None:
        return 'skip'

    try:
        gdaltest.unzip( 'tmp/cache', 'data/ESSENCE_NAIPF_ORI_PROV_sub93.gdb.zip')
    except:
        pass
    try:
        os.stat('tmp/cache/ESSENCE_NAIPF_ORI_PROV_sub93.gdb')
    except:
        return 'skip'

    ogrtest.fgdb_drv.Deregister()

    # Force FileGDB first
    ogrtest.fgdb_drv.Register()
    ogrtest.openfilegdb_drv.Register()

    ds = ogr.Open('tmp/cache/ESSENCE_NAIPF_ORI_PROV_sub93.gdb')
    if ds is None:
        ret = 'fail'
    else:
        ret = 'success'

    # Deregister OpenFileGDB again
    ogrtest.openfilegdb_drv.Deregister()

    shutil.rmtree('tmp/cache/ESSENCE_NAIPF_ORI_PROV_sub93.gdb')

    return ret

###############################################################################
# Test not nullable fields

def ogr_fgdb_17():

    if ogrtest.fgdb_drv is None:
        return 'skip'


    try:
        shutil.rmtree("tmp/test.gdb")
    except:
        pass

    ds = ogrtest.fgdb_drv.CreateDataSource('tmp/test.gdb')
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    lyr = ds.CreateLayer('test', geom_type = ogr.wkbPoint, srs = sr, options = ['GEOMETRY_NULLABLE=NO'])
    if lyr.GetLayerDefn().GetGeomFieldDefn(0).IsNullable() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    field_defn = ogr.FieldDefn('field_not_nullable', ogr.OFTString)
    field_defn.SetNullable(0)
    lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn('field_nullable', ogr.OFTString)
    lyr.CreateField(field_defn)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('field_not_nullable', 'not_null')
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT(0 0)'))
    ret = lyr.CreateFeature(f)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    f = None

    # Error case: missing geometry
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('field_not_nullable', 'not_null')
    gdal.PushErrorHandler()
    ret = lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    f = None

    # Error case: missing non-nullable field
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT(0 0)'))
    gdal.PushErrorHandler()
    ret = lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    f = None

    ds = None

    ds = ogr.Open('tmp/test.gdb', update=1)
    lyr = ds.GetLayerByName('test')
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_not_nullable')).IsNullable() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_nullable')).IsNullable() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetGeomFieldDefn(0).IsNullable() != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test default values

def ogr_fgdb_18():

    if ogrtest.fgdb_drv is None:
        return 'skip'

    try:
        shutil.rmtree("tmp/test.gdb")
    except:
        pass

    ds = ogrtest.fgdb_drv.CreateDataSource('tmp/test.gdb')
    lyr = ds.CreateLayer('test', geom_type = ogr.wkbNone)

    field_defn = ogr.FieldDefn( 'field_string', ogr.OFTString )
    field_defn.SetDefault("'a''b'")
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn( 'field_int', ogr.OFTInteger )
    field_defn.SetDefault('123')
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn( 'field_real', ogr.OFTReal )
    field_defn.SetDefault('1.23')
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn( 'field_nodefault', ogr.OFTInteger )
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn( 'field_datetime', ogr.OFTDateTime )
    field_defn.SetDefault("CURRENT_TIMESTAMP")
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn( 'field_datetime2', ogr.OFTDateTime )
    field_defn.SetDefault("'2015/06/30 12:34:56'")
    lyr.CreateField(field_defn)

    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    f = None

    ds = None

    if ogrtest.openfilegdb_drv is not None:
        ogrtest.openfilegdb_drv.Register()
    ret = ogr_fgdb_18_test_results()
    if ogrtest.openfilegdb_drv is not None:
        ogrtest.openfilegdb_drv.Deregister()

    return ret


def ogr_fgdb_18_test_results():

    ds = ogr.Open('tmp/test.gdb', update=1)
    lyr = ds.GetLayerByName('test')
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_string')).GetDefault() != "'a''b'":
        gdaltest.post_reason('fail')
        return 'fail'
    if ogrtest.openfilegdb_drv is not None:
        if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_int')).GetDefault() != '123':
            gdaltest.post_reason('fail')
            return 'fail'
        if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_real')).GetDefault() != '1.23':
            print(lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_real')).GetDefault())
            gdaltest.post_reason('fail')
            return 'fail'
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_nodefault')).GetDefault() is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    #if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_datetime')).GetDefault() != 'CURRENT_TIMESTAMP':
    #    gdaltest.post_reason('fail')
    #    return 'fail'
    #if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_datetime2')).GetDefault() != "'2015/06/30 12:34:56'":
    #    gdaltest.post_reason('fail')
    #    print(lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_datetime2')).GetDefault())
    #    return 'fail'
    f = lyr.GetNextFeature()
    if f.GetField('field_string') != 'a\'b' or f.GetField('field_int') != 123 or \
       f.GetField('field_real') != 1.23 or \
       not f.IsFieldNull('field_nodefault') or not f.IsFieldSet('field_datetime')  or \
       f.GetField('field_datetime2') != '2015/06/30 12:34:56':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test transaction support

def ogr_fgdb_19_open_update(filename):

    # We need the OpenFileGDB driver for Linux improved StartTransaction()
    bPerLayerCopyingForTransaction = False
    if ogrtest.openfilegdb_drv is not None:
        ogrtest.openfilegdb_drv.Register()
        if os.name != 'nt':
            val = gdal.GetConfigOption('FGDB_PER_LAYER_COPYING_TRANSACTION', 'TRUE')
            if val == 'TRUE' or val == 'YES' or val == 'ON':
                bPerLayerCopyingForTransaction = True

    ds = ogr.Open(filename, update = 1)

    if ogrtest.openfilegdb_drv is not None:
        ogrtest.openfilegdb_drv.Deregister()
        ogrtest.fgdb_drv.Deregister()
        # Force OpenFileGDB first
        ogrtest.openfilegdb_drv.Register()
        ogrtest.fgdb_drv.Register()

    return (bPerLayerCopyingForTransaction, ds)

def ogr_fgdb_19():

    if ogrtest.fgdb_drv is None:
        return 'skip'

    try:
        shutil.rmtree("tmp/test.gdb.ogrtmp")
    except:
        pass
    try:
        shutil.rmtree("tmp/test.gdb.ogredited")
    except:
        pass

    # Error case: try in read-only
    ds = ogr.Open('tmp/test.gdb')
    gdal.PushErrorHandler()
    ret = ds.StartTransaction(force = True)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    (bPerLayerCopyingForTransaction, ds) = ogr_fgdb_19_open_update('tmp/test.gdb')

    if ds.TestCapability(ogr.ODsCEmulatedTransactions) != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    # Error case: try in non-forced mode
    gdal.PushErrorHandler()
    ret = ds.StartTransaction(force = False)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Error case: try StartTransaction() with a ExecuteSQL layer still active
    sql_lyr = ds.ExecuteSQL('SELECT * FROM test')
    gdal.PushErrorHandler()
    ret = ds.StartTransaction(force = True)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)

    # Error case: call CommitTransaction() while there is no transaction
    gdal.PushErrorHandler()
    ret = ds.CommitTransaction()
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Error case: call RollbackTransaction() while there is no transaction
    gdal.PushErrorHandler()
    ret = ds.RollbackTransaction()
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Error case: try StartTransaction() with another active connection
    ds2 = ogr.Open('tmp/test.gdb', update=1)
    gdal.PushErrorHandler()
    ret = ds2.StartTransaction(force = True)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds2 = None

    # Successful StartTransaction() finally!
    lyr = ds.GetLayer(0)
    lyr = ds.GetLayer(0) # again
    old_count = lyr.GetFeatureCount()
    lyr_defn = lyr.GetLayerDefn()
    layer_created_before_transaction = ds.CreateLayer('layer_created_before_transaction', geom_type = ogr.wkbNone)
    layer_created_before_transaction_defn = layer_created_before_transaction.GetLayerDefn()

    if ds.StartTransaction(force = True) != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    try:
        os.stat('tmp/test.gdb.ogredited')
    except:
        gdaltest.post_reason('fail')
        return 'fail'
    try:
        os.stat('tmp/test.gdb.ogrtmp')
        gdaltest.post_reason('fail')
        return 'fail'
    except:
        pass

    ret = lyr.CreateField(ogr.FieldDefn('foobar', ogr.OFTString))
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    ret = lyr.DeleteField(lyr.GetLayerDefn().GetFieldIndex('foobar'))
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.PushErrorHandler()
    ret = lyr.CreateGeomField(ogr.GeomFieldDefn('foobar', ogr.wkbPoint))
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.PushErrorHandler()
    ret = lyr.ReorderFields([i for i in range(lyr.GetLayerDefn().GetFieldCount())])
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.PushErrorHandler()
    ret = lyr.AlterFieldDefn(0, ogr.FieldDefn('foo', ogr.OFTString), 0)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    f = ogr.Feature(lyr_defn)
    f.SetField('field_string', 'foo')
    lyr.CreateFeature(f)
    lyr.SetFeature(f)
    fid = f.GetFID()
    if fid <= 0:
        gdaltest.post_reason('fail')
        print(fid)
        return 'fail'
    lyr.ResetReading()
    for i in range(fid):
        f = lyr.GetNextFeature()
    if f.GetFID() != fid or f.GetField('field_string') != 'foo':
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetFeature(fid)
    if f.GetFID() != fid or f.GetField('field_string') != 'foo':
        gdaltest.post_reason('fail')
        return 'fail'

    f = ogr.Feature(layer_created_before_transaction_defn)
    layer_created_before_transaction.CreateFeature(f)

    # Error case: call StartTransaction() while there is an active transaction
    gdal.PushErrorHandler()
    ret = ds.StartTransaction(force = True)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Error case: try CommitTransaction() with a ExecuteSQL layer still active
    sql_lyr = ds.ExecuteSQL('SELECT * FROM test')
    gdal.PushErrorHandler()
    ret = ds.CommitTransaction()
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)

    # Error case: try RollbackTransaction() with a ExecuteSQL layer still active
    sql_lyr = ds.ExecuteSQL('SELECT * FROM test')
    gdal.PushErrorHandler()
    ret = ds.RollbackTransaction()
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)

    # Test that CommitTransaction() works
    if ds.CommitTransaction() != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    try:
        os.stat('tmp/test.gdb.ogredited')
        gdaltest.post_reason('fail')
        return 'fail'
    except:
        pass

    try:
        os.stat('tmp/test.gdb.ogrtmp')
        gdaltest.post_reason('fail')
        return 'fail'
    except:
        pass

    lst = gdal.ReadDir('tmp/test.gdb')
    for filename in lst:
        if filename.find('.tmp') >= 0:
            gdaltest.post_reason('fail')
            print(lst)
            return 'fail'

    lyr_tmp = ds.GetLayer(0)
    lyr_tmp = ds.GetLayer(0)
    new_count = lyr_tmp.GetFeatureCount()
    if new_count != old_count + 1:
        gdaltest.post_reason('fail')
        return 'fail'
    old_count = new_count

    if layer_created_before_transaction.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    for i in range(ds.GetLayerCount()):
        if ds.GetLayer(i).GetName() == layer_created_before_transaction.GetName():
            ds.DeleteLayer(i)
            break
    layer_created_before_transaction = None

    # Test suppression of layer within transaction
    lyr_count = ds.GetLayerCount()
    ds.CreateLayer('layer_tmp', geom_type = ogr.wkbNone)
    ret = ds.StartTransaction(force = True)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.DeleteLayer(ds.GetLayerCount()-1)
    if ds.CommitTransaction() != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    new_lyr_count = ds.GetLayerCount()
    if new_lyr_count != lyr_count:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test that RollbackTransaction() works
    ret = ds.StartTransaction(force = True)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    f = ogr.Feature(lyr_defn)
    lyr.CreateFeature(f)

    layer_created_during_transaction = ds.CreateLayer('layer_created_during_transaction', geom_type = ogr.wkbNone)
    layer_created_during_transaction.CreateField(ogr.FieldDefn('foo', ogr.OFTString))

    if ds.RollbackTransaction() != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    try:
        os.stat('tmp/test.gdb.ogredited')
        gdaltest.post_reason('fail')
        return 'fail'
    except:
        pass

    try:
        os.stat('tmp/test.gdb.ogrtmp')
        gdaltest.post_reason('fail')
        return 'fail'
    except:
        pass

    if lyr.GetFeatureCount() != old_count:
        gdaltest.post_reason('fail')
        print(lyr.GetFeatureCount())
        print(old_count)
        return 'fail'

    # Cannot retrieve the layer any more from fresh
    if ds.GetLayerByName('layer_created_during_transaction') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    # Pointer is in ghost state
    if layer_created_during_transaction.GetLayerDefn().GetFieldCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # Simulate an error case where StartTransaction() cannot copy backup files
    lyr_count = ds.GetLayerCount()
    gdal.SetConfigOption('FGDB_SIMUL_FAIL', 'CASE1')
    gdal.PushErrorHandler()
    ret = ds.StartTransaction(force = True)
    gdal.PopErrorHandler()
    gdal.SetConfigOption('FGDB_SIMUL_FAIL', None)
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.GetLayerCount() != lyr_count:
        gdaltest.post_reason('fail')
        print(lyr_count)
        print(ds.GetLayerCount())
        return 'fail'


    # Simulate an error case where StartTransaction() cannot reopen database
    gdal.SetConfigOption('FGDB_SIMUL_FAIL', 'CASE2')
    gdal.PushErrorHandler()
    ret = ds.StartTransaction(force = True)
    gdal.PopErrorHandler()
    gdal.SetConfigOption('FGDB_SIMUL_FAIL', None)
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.GetLayerCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    shutil.rmtree('tmp/test.gdb.ogredited')

    # Test method on ghost datasource and layer
    ds.GetName()
    ds.GetLayerCount()
    ds.GetLayer(0)
    ds.GetLayerByName("test")
    ds.DeleteLayer(0)
    ds.TestCapability('foo')
    ds.CreateLayer('bar', geom_type = ogr.wkbNone)
    ds.CopyLayer(lyr, 'baz')
    ds.GetStyleTable()
    #ds.SetStyleTableDirectly(None)
    ds.SetStyleTable(None)
    sql_lyr = ds.ExecuteSQL('SELECT * FROM test')
    ds.ReleaseResultSet(sql_lyr)
    ds.FlushCache()
    ds.GetMetadata()
    ds.GetMetadataItem('foo')
    ds.SetMetadata(None)
    ds.SetMetadataItem('foo', None)

    lyr.GetSpatialFilter()
    lyr.SetSpatialFilter(None)
    lyr.SetSpatialFilterRect(0,0,0,0)
    lyr.SetSpatialFilter(0,None)
    lyr.SetSpatialFilterRect(0,0,0,0,0)
    lyr.SetAttributeFilter(None)
    lyr.ResetReading()
    lyr.GetNextFeature()
    lyr.SetNextByIndex(0)
    lyr.GetFeature(0)
    lyr.SetFeature(ogr.Feature(lyr.GetLayerDefn()))
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    lyr.DeleteFeature(0)
    lyr.GetName()
    lyr.GetGeomType()
    lyr.GetLayerDefn()
    lyr.GetSpatialRef()
    lyr.GetFeatureCount()
    lyr.GetExtent()
    lyr.GetExtent(0)
    lyr.TestCapability('foo')
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    lyr.DeleteField(0)
    lyr.ReorderFields([i for i in range(lyr.GetLayerDefn().GetFieldCount())])
    lyr.AlterFieldDefn(0, ogr.FieldDefn('foo', ogr.OFTString), 0)
    lyr.SyncToDisk()
    lyr.GetStyleTable()
    #lyr.SetStyleTableDirectly(None)
    lyr.SetStyleTable(None)
    lyr.StartTransaction()
    lyr.CommitTransaction()
    lyr.RollbackTransaction()
    lyr.SetIgnoredFields([])
    lyr.GetMetadata()
    lyr.GetMetadataItem('foo')
    lyr.SetMetadata(None)
    lyr.SetMetadataItem('foo', None)

    ds = None

    if bPerLayerCopyingForTransaction:

        # Test an error case where we simulate a failure of destroying a
        # layer destroyed during transaction
        (bPerLayerCopyingForTransaction, ds) = ogr_fgdb_19_open_update('tmp/test.gdb')

        layer_tmp = ds.CreateLayer('layer_tmp', geom_type = ogr.wkbNone)
        layer_tmp.CreateField(ogr.FieldDefn('foo', ogr.OFTString))

        if ds.StartTransaction(force = True) != 0:
            gdaltest.post_reason('fail')
            return 'fail'

        ds.DeleteLayer(ds.GetLayerCount()-1)

        gdal.SetConfigOption('FGDB_SIMUL_FAIL', 'CASE1')
        gdal.PushErrorHandler()
        ret = ds.CommitTransaction()
        gdal.PopErrorHandler()
        gdal.SetConfigOption('FGDB_SIMUL_FAIL', None)
        if ret == 0:
            gdaltest.post_reason('fail')
            return 'fail'

        ds = None

        shutil.rmtree('tmp/test.gdb.ogredited')

        lst = gdal.ReadDir('tmp/test.gdb')
        for filename in lst:
            if filename.find('.tmp') >= 0:
                gdaltest.post_reason('fail')
                print(lst)
                return 'fail'

        # Test an error case where we simulate a failure in renaming
        # a file in original directory
        (bPerLayerCopyingForTransaction, ds) = ogr_fgdb_19_open_update('tmp/test.gdb')

        for i in range(ds.GetLayerCount()):
            if ds.GetLayer(i).GetName() == 'layer_tmp':
                ds.DeleteLayer(i)
                break

        if ds.StartTransaction(force = True) != 0:
            gdaltest.post_reason('fail')
            return 'fail'

        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        lyr.SetFeature(f)
        f = None

        gdal.SetConfigOption('FGDB_SIMUL_FAIL', 'CASE2')
        gdal.PushErrorHandler()
        ret = ds.CommitTransaction()
        gdal.PopErrorHandler()
        gdal.SetConfigOption('FGDB_SIMUL_FAIL', None)
        if ret == 0:
            gdaltest.post_reason('fail')
            return 'fail'

        ds = None

        shutil.rmtree('tmp/test.gdb.ogredited')

        lst = gdal.ReadDir('tmp/test.gdb')
        for filename in lst:
            if filename.find('.tmp') >= 0:
                gdaltest.post_reason('fail')
                print(lst)
                return 'fail'

        # Test an error case where we simulate a failure in moving
        # a file into original directory
        (bPerLayerCopyingForTransaction, ds) = ogr_fgdb_19_open_update('tmp/test.gdb')

        if ds.StartTransaction(force = True) != 0:
            gdaltest.post_reason('fail')
            return 'fail'

        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        lyr.SetFeature(f)
        f = None

        gdal.SetConfigOption('FGDB_SIMUL_FAIL', 'CASE3')
        gdal.PushErrorHandler()
        ret = ds.CommitTransaction()
        gdal.PopErrorHandler()
        gdal.SetConfigOption('FGDB_SIMUL_FAIL', None)
        if ret == 0:
            gdaltest.post_reason('fail')
            return 'fail'

        ds = None

        shutil.rmtree('tmp/test.gdb.ogredited')

        # Remove left over .tmp files
        lst = gdal.ReadDir('tmp/test.gdb')
        for filename in lst:
            if filename.find('.tmp') >= 0:
                os.remove('tmp/test.gdb/' + filename)

        # Test not critical error in removing a temporary file
        for case in ('CASE4', 'CASE5'):
            (bPerLayerCopyingForTransaction, ds) = ogr_fgdb_19_open_update('tmp/test.gdb')

            if ds.StartTransaction(force = True) != 0:
                gdaltest.post_reason('fail')
                return 'fail'

            lyr = ds.GetLayer(0)
            f = lyr.GetNextFeature()
            lyr.SetFeature(f)
            f = None

            gdal.SetConfigOption('FGDB_SIMUL_FAIL', case)
            gdal.PushErrorHandler()
            ret = ds.CommitTransaction()
            gdal.PopErrorHandler()
            gdal.SetConfigOption('FGDB_SIMUL_FAIL', None)
            if ret != 0:
                gdaltest.post_reason('fail')
                print(case)
                return 'fail'

            ds = None

            if case == 'CASE4':
                try:
                    os.stat('tmp/test.gdb.ogredited')
                    gdaltest.post_reason('fail')
                    print(case)
                    return 'fail'
                except:
                    pass
            else:
                shutil.rmtree('tmp/test.gdb.ogredited')

            # Remove left over .tmp files
            lst = gdal.ReadDir('tmp/test.gdb')
            for filename in lst:
                if filename.find('.tmp') >= 0:
                    os.remove('tmp/test.gdb/' + filename)

    else:
        # Test an error case where we simulate a failure of rename from .gdb to .gdb.ogrtmp during commit
        (bPerLayerCopyingForTransaction, ds) = ogr_fgdb_19_open_update('tmp/test.gdb')
        lyr = ds.GetLayer(0)
        lyr_defn = lyr.GetLayerDefn()

        if ds.StartTransaction(force = True) != 0:
            gdaltest.post_reason('fail')
            return 'fail'

        gdal.SetConfigOption('FGDB_SIMUL_FAIL', 'CASE1')
        gdal.PushErrorHandler()
        ret = ds.CommitTransaction()
        gdal.PopErrorHandler()
        gdal.SetConfigOption('FGDB_SIMUL_FAIL', None)
        if ret == 0:
            gdaltest.post_reason('fail')
            return 'fail'

        ds = None

        # Test an error case where we simulate a failure of rename from .gdb.ogredited to .gdb during commit
        (bPerLayerCopyingForTransaction, ds) = ogr_fgdb_19_open_update('tmp/test.gdb')
        lyr = ds.GetLayer(0)
        lyr_defn = lyr.GetLayerDefn()

        if ds.StartTransaction(force = True) != 0:
            gdaltest.post_reason('fail')
            return 'fail'

        gdal.SetConfigOption('FGDB_SIMUL_FAIL', 'CASE2')
        gdal.PushErrorHandler()
        ret = ds.CommitTransaction()
        gdal.PopErrorHandler()
        gdal.SetConfigOption('FGDB_SIMUL_FAIL', None)
        if ret == 0:
            gdaltest.post_reason('fail')
            return 'fail'

        ds = None
        os.rename('tmp/test.gdb.ogrtmp', 'tmp/test.gdb')

        # Test an error case where we simulate a failure of removing from .gdb.ogrtmp during commit
        (bPerLayerCopyingForTransaction, ds) = ogr_fgdb_19_open_update('tmp/test.gdb')
        lyr = ds.GetLayer(0)
        lyr_defn = lyr.GetLayerDefn()

        if ds.StartTransaction(force = True) != 0:
            gdaltest.post_reason('fail')
            return 'fail'

        gdal.SetConfigOption('FGDB_SIMUL_FAIL', 'CASE3')
        gdal.PushErrorHandler()
        ret = ds.CommitTransaction()
        gdal.PopErrorHandler()
        gdal.SetConfigOption('FGDB_SIMUL_FAIL', None)
        if ret != 0:
            gdaltest.post_reason('fail')
            return 'fail'

        ds = None
        shutil.rmtree('tmp/test.gdb.ogrtmp')

    # Test an error case where we simulate a failure of reopening the committed DB
    (bPerLayerCopyingForTransaction, ds) = ogr_fgdb_19_open_update('tmp/test.gdb')
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()

    if ds.StartTransaction(force = True) != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.SetConfigOption('FGDB_SIMUL_FAIL', 'CASE_REOPEN')
    gdal.PushErrorHandler()
    ret = ds.CommitTransaction()
    gdal.PopErrorHandler()
    gdal.SetConfigOption('FGDB_SIMUL_FAIL', None)
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.GetLayerCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = None

    # Test an error case where we simulate a failure of removing from .gdb.ogredited during rollback
    (bPerLayerCopyingForTransaction, ds) = ogr_fgdb_19_open_update('tmp/test.gdb')
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()

    if ds.StartTransaction(force = True) != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.SetConfigOption('FGDB_SIMUL_FAIL', 'CASE1')
    gdal.PushErrorHandler()
    ret = ds.RollbackTransaction()
    gdal.PopErrorHandler()
    gdal.SetConfigOption('FGDB_SIMUL_FAIL', None)
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = None
    shutil.rmtree('tmp/test.gdb.ogredited')

    # Test an error case where we simulate a failure of reopening the rollbacked DB
    (bPerLayerCopyingForTransaction, ds) = ogr_fgdb_19_open_update('tmp/test.gdb')
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()

    if ds.StartTransaction(force = True) != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.SetConfigOption('FGDB_SIMUL_FAIL', 'CASE2')
    gdal.PushErrorHandler()
    ret = ds.RollbackTransaction()
    gdal.PopErrorHandler()
    gdal.SetConfigOption('FGDB_SIMUL_FAIL', None)
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    if ds.GetLayerCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = None

    if ogrtest.openfilegdb_drv is not None:
        ogrtest.openfilegdb_drv.Deregister()

    return 'success'

# Same, but retry without per-layer copying optimization (in the case
# this was what was tested in previous step)
def ogr_fgdb_19bis():

    if ogrtest.fgdb_drv is None:
        return 'skip'

    (bPerLayerCopyingForTransaction, ds) = ogr_fgdb_19_open_update('tmp/test.gdb')
    del ds
    if not bPerLayerCopyingForTransaction:
        return 'skip'

    gdal.SetConfigOption('FGDB_PER_LAYER_COPYING_TRANSACTION', 'FALSE')
    ret = ogr_fgdb_19()
    gdal.SetConfigOption('FGDB_PER_LAYER_COPYING_TRANSACTION', None)
    return ret

###############################################################################
# Test CreateFeature() with user defined FID

def ogr_fgdb_20():

    if ogrtest.fgdb_drv is None:
        return 'skip'

    if ogrtest.openfilegdb_drv is None:
        return 'skip'

    if not os.path.exists('tmp/test.gdb'):
        ds = ogrtest.fgdb_drv.CreateDataSource("tmp/test.gdb")
        ds = None

    # We need the OpenFileGDB driver for CreateFeature() with user defined FID
    ogrtest.openfilegdb_drv.Register()
    ds = ogr.Open('tmp/test.gdb', update = 1)
    ogrtest.openfilegdb_drv.Deregister()
    ogrtest.fgdb_drv.Deregister()
    # Force OpenFileGDB first
    ogrtest.openfilegdb_drv.Register()
    ogrtest.fgdb_drv.Register()

    lyr = ds.CreateLayer('ogr_fgdb_20', geom_type = ogr.wkbNone)
    lyr.CreateField(ogr.FieldDefn('id', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('str', ogr.OFTString))

    ds.ExecuteSQL('CREATE INDEX ogr_fgdb_20_id ON ogr_fgdb_20(id)')

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('id', 1)
    ret = lyr.CreateFeature(f)
    if ret != 0 or f.GetFID() != 1 or lyr.GetMetadataItem('1', 'MAP_OGR_FID_TO_FGDB_FID') is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Existing FID
    gdal.PushErrorHandler()
    ret = lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    for invalid_fid in [-2,0,9876543210]:
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetFID(invalid_fid)
        gdal.PushErrorHandler()
        ret = lyr.CreateFeature(f)
        gdal.PopErrorHandler()
        if ret == 0:
            gdaltest.post_reason('fail')
            print(invalid_fid)
            return 'fail'

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(2)
    f.SetField('id', 2)
    ret = lyr.CreateFeature(f)
    if ret != 0 or f.GetFID() != 2  or lyr.GetMetadataItem('2', 'MAP_OGR_FID_TO_FGDB_FID') is not None:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    # OGR FID = 4, FileGDB FID = 3
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(4)
    f.SetField('id', 4)

    # Cannot call CreateFeature() with a set FID when a dataset is opened more than once
    ds2 = ogr.Open('tmp/test.gdb', update = 1)
    gdal.PushErrorHandler()
    ret = lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds2 = None

    ret = lyr.CreateFeature(f)
    if ret != 0 or f.GetFID() != 4 or lyr.GetMetadataItem('4', 'MAP_OGR_FID_TO_FGDB_FID') != '3':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        print(lyr.GetMetadataItem('4', 'MAP_OGR_FID_TO_FGDB_FID'))
        return 'fail'

    #  Cannot open geodatabase at the moment since it is in 'FID hack mode'
    gdal.PushErrorHandler()
    ds2 = ogr.Open('tmp/test.gdb', update = 1)
    gdal.PopErrorHandler()
    if ds2 is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds2 = None

    # Existing FID, but only in OGR space
    gdal.PushErrorHandler()
    ret = lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    # This FID exists as a FGDB ID, but should not be user visible.
    f.SetFID(3)
    ret = lyr.SetFeature(f)
    if ret != ogr.OGRERR_NON_EXISTING_FEATURE:
        gdaltest.post_reason('fail')
        return 'fail'
    ret = lyr.DeleteFeature(3)
    if ret != ogr.OGRERR_NON_EXISTING_FEATURE:
        gdaltest.post_reason('fail')
        return 'fail'
    ret = lyr.GetFeature(3)
    if ret is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Trying to set OGR FID = 3 --> FileGDB FID = 4
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(3)
    f.SetField('id', 3)

    ret = lyr.CreateFeature(f)
    if ret != 0 or f.GetFID() != 3 or lyr.GetMetadataItem('3', 'MAP_OGR_FID_TO_FGDB_FID') != '4':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    lyr.ResetReading()
    expected = [ (1, None), (2, None), (4, 3), (3, 4) ]
    for i in range(2):
        for (fid, fgdb_fid) in expected:
            if i == 0:
                f = lyr.GetNextFeature()
            else:
                f = lyr.GetFeature(fid)
            if f is None:
                gdaltest.post_reason('fail')
                return 'fail'
            if f.GetFID() != fid or f.GetField('id') != fid:
                gdaltest.post_reason('fail')
                f.DumpReadable()
                print(fid)
                return 'fail'
            got_fgdb_fid = lyr.GetMetadataItem(str(f.GetFID()), 'MAP_OGR_FID_TO_FGDB_FID')
            if got_fgdb_fid is None:
                if fgdb_fid is not None:
                    gdaltest.post_reason('fail')
                    return 'fail'
            elif int(got_fgdb_fid) != fgdb_fid:
                gdaltest.post_reason('fail')
                print(got_fgdb_fid)
                print(fgdb_fid)
                return 'fail'

    for fid in [ -9876543210, 0, 100]:
        f = lyr.GetFeature(fid)
        if f is not None:
            gdaltest.post_reason('fail')
            f.DumpReadable()
            return 'fail'

    for invalid_fid in [-2,0,9876543210]:
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetFID(invalid_fid)
        ret = lyr.SetFeature(f)
        if ret != ogr.OGRERR_NON_EXISTING_FEATURE:
            gdaltest.post_reason('fail')
            return 'fail'
        ret = lyr.DeleteFeature(invalid_fid)
        if ret != ogr.OGRERR_NON_EXISTING_FEATURE:
            gdaltest.post_reason('fail')
            return 'fail'

    f = lyr.GetFeature(3)
    f.SetField('str', '3')
    ret = lyr.SetFeature(f)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    f = lyr.GetFeature(3)
    if f.GetField('str') != '3':
        gdaltest.post_reason('fail')
        return 'fail'

    ret = lyr.DeleteFeature(1)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    ret = lyr.DeleteFeature(3)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'


    for (fid, fgdb_fid) in [ (3, 5), (2049,6), (10,7), (7,8), (9, None), (8, 10), (12, 11) ]:
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetFID(fid)
        f.SetField('id', fid)
        ret = lyr.CreateFeature(f)
        if ret != 0 or f.GetFID() != fid or str(lyr.GetMetadataItem(str(fid), 'MAP_OGR_FID_TO_FGDB_FID')) != str(fgdb_fid):
            gdaltest.post_reason('fail')
            f.DumpReadable()
            print(fid)
            print(lyr.GetMetadataItem(str(fid), 'MAP_OGR_FID_TO_FGDB_FID'))
            return 'fail'

    # Normally 12 should be attributed, but it has already been reserved
    f = ogr.Feature(lyr.GetLayerDefn())
    ret = lyr.CreateFeature(f)
    if ret != 0 or f.GetFID() != 13:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f.SetField('id', f.GetFID())
    lyr.SetFeature(f)

    lyr.ResetReading()
    expected = [ (2, None), (4, 3), (3, 5), (2049,6), (10,7), (7,8), (9, None), (8, 10) ]
    for (fid, fgdb_fid) in expected:
        f = lyr.GetNextFeature()
        if f is None:
            gdaltest.post_reason('fail')
            return 'fail'
        if f.GetFID() != fid or f.GetField('id') != fid or str(lyr.GetMetadataItem(str(fid), 'MAP_OGR_FID_TO_FGDB_FID')) != str(fgdb_fid):
            gdaltest.post_reason('fail')
            f.DumpReadable()
            print(fid)
            print(lyr.GetMetadataItem(str(fid), 'MAP_OGR_FID_TO_FGDB_FID'))
            return 'fail'

    lyr.SetAttributeFilter('id = 3')
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f.GetFID() != 3:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    # This will cause a resync of indexes
    lyr.SetAttributeFilter('OBJECTID = 3')
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f.GetFID() != 3:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    # No sparse pages
    lyr = ds.CreateLayer('ogr_fgdb_20_simple', geom_type = ogr.wkbNone)
    lyr.CreateField(ogr.FieldDefn('id', ogr.OFTInteger))

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(2)
    f.SetField('id', 2)
    lyr.CreateFeature(f)

    # This will cause a resync of indexes
    sql_lyr = ds.ExecuteSQL('SELECT * FROM ogr_fgdb_20_simple')
    f = sql_lyr.GetNextFeature()
    if f.GetFID() != 2:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    # Do not allow user set FID while a select layer is in progress
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(3)
    f.SetField('id', 3)
    gdal.PushErrorHandler()
    ret = lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    ds.ReleaseResultSet(sql_lyr)

    # Do it in transaction, but this is completely orthogonal
    ds.StartTransaction(force = True)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(3)
    f.SetField('id', 3)
    lyr.CreateFeature(f)
    f = None

    ds.CommitTransaction()

    # Multi-page indexes
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32630)
    gdal.SetConfigOption('FGDB_RESYNC_THRESHOLD', '600')
    lyr = ds.CreateLayer('ogr_fgdb_20_indexes', geom_type = ogr.wkbPoint, srs = srs)
    gdal.SetConfigOption('FGDB_RESYNC_THRESHOLD', None)
    lyr.CreateField(ogr.FieldDefn('id', ogr.OFTInteger))
    ds.ExecuteSQL('CREATE INDEX ogr_fgdb_20_indexes_id ON ogr_fgdb_20_indexes(id)')
    gdal.SetConfigOption('FGDB_BULK_LOAD', 'YES')
    for i in range(1000):
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetFID(i+2)
        f.SetField('id', i+2)
        f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (%d 0)' % i))
        lyr.CreateFeature(f)
    gdal.SetConfigOption('FGDB_BULK_LOAD', None)
    ds = None

    # Check consistency after re-opening
    gdal.ErrorReset()
    for update in [0,1]:
        ds = ogr.Open('tmp/test.gdb', update = update)
        lyr = ds.GetLayerByName('ogr_fgdb_20')
        if lyr.GetFeatureCount() != 10:
            gdaltest.post_reason('fail')
            print(lyr.GetFeatureCount())
            return 'fail'
        lyr.ResetReading()
        expected = [ 2, 3, 4, 7, 8, 9, 10, 12, 13, 2049 ]
        for fid in expected:
            f = lyr.GetNextFeature()
            if gdal.GetLastErrorType() != 0:
                gdaltest.post_reason('fail')
                return 'fail'
            if f is None:
                gdaltest.post_reason('fail')
                print(fid)
                return 'fail'
            if f.GetFID() != fid or f.GetField('id') != fid:
                gdaltest.post_reason('fail')
                f.DumpReadable()
                print(fid)
                return 'fail'

        for fid in expected:
            lyr.SetAttributeFilter('id = %d' % fid)
            lyr.ResetReading()
            f = lyr.GetNextFeature()
            if f.GetFID() != fid or f.GetField('id') != fid:
                gdaltest.post_reason('fail')
                f.DumpReadable()
                print(fid)
                return 'fail'

        lyr = ds.GetLayerByName('ogr_fgdb_20_simple')
        f = lyr.GetNextFeature()
        if f.GetFID() != 2:
            gdaltest.post_reason('fail')
            return 'fail'
        f = lyr.GetNextFeature()
        if f.GetFID() != 3:
            gdaltest.post_reason('fail')
            return 'fail'

        # Check attribute index
        lyr = ds.GetLayerByName('ogr_fgdb_20_indexes')
        for i in range(1000):
            fid = i + 2
            lyr.SetAttributeFilter('id = %d' % fid)
            lyr.ResetReading()
            f = lyr.GetNextFeature()
            if f.GetFID() != fid:
                gdaltest.post_reason('fail')
                print(f.GetFID())
                print(fid)
                return 'fail'

        # Check spatial index
        lyr.SetAttributeFilter(None)
        if update == 1:
            for i in range(1000):
                fid = i + 2
                lyr.SetSpatialFilterRect(i - 0.01, -0.01, i + 0.01, 0.01)
                lyr.ResetReading()
                f = lyr.GetNextFeature()
                if f.GetFID() != fid:
                    gdaltest.post_reason('fail')
                    print(f.GetFID())
                    print(fid)
                    return 'fail'

    # Insert new features
    ds = ogr.Open('tmp/test.gdb', update = 1)
    lyr = ds.GetLayerByName('ogr_fgdb_20')
    for (fid, fgdb_fid) in [ (10000000, 2050), (10000001,2051), (8191,2052), (16384,2053) ]:
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetFID(fid)
        f.SetField('id', fid)
        ret = lyr.CreateFeature(f)
        if ret != 0 or f.GetFID() != fid or str(lyr.GetMetadataItem(str(fid), 'MAP_OGR_FID_TO_FGDB_FID')) != str(fgdb_fid):
            gdaltest.post_reason('fail')
            f.DumpReadable()
            print(lyr.GetMetadataItem(str(fid), 'MAP_OGR_FID_TO_FGDB_FID'))
            return 'fail'

    ds = None

    # Insert a new intermediate FIDs
    for (fid, fgdb_fid) in [ (1000000, 10000002), (1000001,10000002) ]:

        ds = ogr.Open('tmp/test.gdb', update = 1)
        lyr = ds.GetLayerByName('ogr_fgdb_20')
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetFID(fid)
        f.SetField('id', fid)
        ret = lyr.CreateFeature(f)
        if ret != 0 or f.GetFID() != fid or lyr.GetMetadataItem(str(fid), 'MAP_OGR_FID_TO_FGDB_FID') != str(fgdb_fid):
            gdaltest.post_reason('fail')
            f.DumpReadable()
            print(lyr.GetMetadataItem(str(fid), 'MAP_OGR_FID_TO_FGDB_FID'))
            return 'fail'
        ds = None

    # Check consistency after re-opening
    gdal.ErrorReset()
    for update in [0,1]:
        ds = ogr.Open('tmp/test.gdb', update = update)
        lyr = ds.GetLayerByName('ogr_fgdb_20')
        if lyr.GetFeatureCount() != 16:
            gdaltest.post_reason('fail')
            print(lyr.GetFeatureCount())
            return 'fail'
        lyr.ResetReading()
        expected = [ 2, 3, 4, 7, 8, 9, 10, 12, 13, 2049, 8191, 16384, 1000000, 1000001, 10000000, 10000001 ]
        for fid in expected:
            f = lyr.GetNextFeature()
            if gdal.GetLastErrorType() != 0:
                gdaltest.post_reason('fail')
                return 'fail'
            if f is None:
                gdaltest.post_reason('fail')
                print(fid)
                return 'fail'
            if f.GetFID() != fid or f.GetField('id') != fid:
                gdaltest.post_reason('fail')
                f.DumpReadable()
                print(fid)
                return 'fail'


    # Simulate different errors when database reopening is done
    # to sync ids
    for case in ('CASE1', 'CASE2', 'CASE3'):
        try:
            shutil.rmtree("tmp/test2.gdb")
        except:
            pass

        ds = ogrtest.fgdb_drv.CreateDataSource("tmp/test2.gdb")

        lyr = ds.CreateLayer('foo', geom_type = ogr.wkbNone)
        lyr.CreateField(ogr.FieldDefn('id', ogr.OFTInteger))
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetFID(2)
        f.SetField('id', 2)
        lyr.CreateFeature(f)

        gdal.PushErrorHandler()
        gdal.SetConfigOption('FGDB_SIMUL_FAIL_REOPEN', case)
        sql_lyr = ds.ExecuteSQL('SELECT * FROM foo')
        gdal.SetConfigOption('FGDB_SIMUL_FAIL_REOPEN', None)
        gdal.PopErrorHandler()
        if case == 'CASE3':
            if sql_lyr is None:
                gdaltest.post_reason('fail')
                print(case)
                return 'fail'
            ds.ReleaseResultSet(sql_lyr)
        else:
            if sql_lyr is not None:
                gdaltest.post_reason('fail')
                print(case)
                return 'fail'

        # Everything will fail, but hopefully without crashing
        lyr.ResetReading()
        if lyr.GetNextFeature() is not None:
            gdaltest.post_reason('fail')
            return 'fail'
        if lyr.GetFeature(1) is not None:
            gdaltest.post_reason('fail')
            return 'fail'
        if lyr.DeleteFeature(1) == 0:
            gdaltest.post_reason('fail')
            return 'fail'
        if lyr.CreateFeature(f) == 0:
            gdaltest.post_reason('fail')
            return 'fail'
        if lyr.SetFeature(f) == 0:
            gdaltest.post_reason('fail')
            return 'fail'
        if case != 'CASE3':
            if ds.CreateLayer('bar', geom_type = ogr.wkbNone) is not None:
                gdaltest.post_reason('fail')
                return 'fail'
            if ds.DeleteLayer(0) == 0:
                gdaltest.post_reason('fail')
                return 'fail'
        sql_lyr = ds.ExecuteSQL('SELECT * FROM foo')
        if case != 'CASE3' and sql_lyr is not None:
            gdaltest.post_reason('fail')
            print(case)
            return 'fail'
        ds.ReleaseResultSet(sql_lyr)

        ds = None

    #sys.exit(0)

    return 'success'

###############################################################################
# Test M support

def ogr_fgdb_21():

    if ogrtest.fgdb_drv is None:
        return 'skip'

    if not ogr_fgdb_is_sdk_1_4_or_later():
        print('SDK 1.4 required')
        return 'skip'

    try:
        shutil.rmtree("tmp/test.gdb")
    except:
        pass

    ds = ogrtest.fgdb_drv.CreateDataSource('tmp/test.gdb')


    datalist = [ [ "pointm", ogr.wkbPointM, "POINT M (1 2 3)" ],
                 [ "pointzm", ogr.wkbPointM, "POINT ZM (1 2 3 4)" ],
                 [ "multipointm", ogr.wkbMultiPointM, "MULTIPOINT M ((1 2 3),(4 5 6))" ],
                 [ "multipointzm", ogr.wkbMultiPointZM, "MULTIPOINT ZM ((1 2 3 4),(5 6 7 8))" ],
                 [ "linestringm", ogr.wkbLineStringM, "LINESTRING M (1 2 3,4 5 6)", "MULTILINESTRING M ((1 2 3,4 5 6))" ],
                 [ "linestringzm", ogr.wkbLineStringZM, "LINESTRING ZM (1 2 3 4,5 6 7 8)", "MULTILINESTRING ZM ((1 2 3 4,5 6 7 8))" ],
                 [ "multilinestringm", ogr.wkbMultiLineStringM, "MULTILINESTRING M ((1 2 3,4 5 6))" ],
                 [ "multilinestringzm", ogr.wkbMultiLineStringZM, "MULTILINESTRING ZM ((1 2 3 4,5 6 7 8))" ],
                 [ "polygonm", ogr.wkbPolygonM, "POLYGON M ((0 0 1,0 1 2,1 1 3,1 0 4,0 0 1))", "MULTIPOLYGON M (((0 0 1,0 1 2,1 1 3,1 0 4,0 0 1)))" ],
                 [ "polygonzm", ogr.wkbPolygonZM, "POLYGON ZM ((0 0 1 -1,0 1 2 -2,1 1 3 -3,1 0 4 -4,0 0 1 -1))", "MULTIPOLYGON ZM (((0 0 1 -1,0 1 2 -2,1 1 3 -3,1 0 4 -4,0 0 1 -1)))" ],
                 [ "multipolygonm", ogr.wkbMultiPolygonM, "MULTIPOLYGON M (((0 0 1,0 1 2,1 1 3,1 0 4,0 0 1)))" ],
                 [ "multipolygonzm", ogr.wkbMultiPolygonZM, "MULTIPOLYGON ZM (((0 0 1 -1,0 1 2 -2,1 1 3 -3,1 0 4 -4,0 0 1 -1)))" ],
                 [ "empty_polygonm", ogr.wkbPolygonM, 'POLYGON M EMPTY', None],
               ]

    srs = osr.SpatialReference()
    srs.SetFromUserInput("WGS84")

    for data in datalist:
        lyr = ds.CreateLayer(data[0], geom_type=data[1], srs=srs, options = [] )

        feat = ogr.Feature(lyr.GetLayerDefn())
        #print(data[2])
        feat.SetGeometry(ogr.CreateGeometryFromWkt(data[2]))
        lyr.CreateFeature(feat)

    ds = None
    ds = ogr.Open('tmp/test.gdb')

    for data in datalist:
        lyr = ds.GetLayerByName(data[0])
        expected_geom_type = data[1]
        if expected_geom_type == ogr.wkbLineStringM:
            expected_geom_type = ogr.wkbMultiLineStringM
        elif expected_geom_type == ogr.wkbLineStringZM:
            expected_geom_type = ogr.wkbMultiLineStringZM
        elif expected_geom_type == ogr.wkbPolygonM:
            expected_geom_type = ogr.wkbMultiPolygonM
        elif expected_geom_type == ogr.wkbPolygonZM:
            expected_geom_type = ogr.wkbMultiPolygonZM

        if lyr.GetGeomType() != expected_geom_type:
            gdaltest.post_reason('fail')
            print(data)
            print(lyr.GetGeomType())
            return 'fail'
        feat = lyr.GetNextFeature()
        try:
            expected_wkt = data[3]
        except:
            expected_wkt = data[2]
        if expected_wkt is None:
            if feat.GetGeometryRef() is not None:
                gdaltest.post_reason('fail')
                print(data)
                feat.DumpReadable()
                return 'fail'
        elif ogrtest.check_feature_geometry(feat, expected_wkt) != 0:
            gdaltest.post_reason('fail')
            print(data)
            feat.DumpReadable()
            return 'fail'

    return 'success'

###############################################################################
# Read curves

def ogr_fgdb_22():

    if ogrtest.fgdb_drv is None:
        return 'skip'

    ds = ogr.Open('data/curves.gdb')
    lyr = ds.GetLayerByName('line')
    ds_ref = ogr.Open('data/curves_line.csv')
    lyr_ref = ds_ref.GetLayer(0)
    for f in lyr:
        f_ref = lyr_ref.GetNextFeature()
        if ogrtest.check_feature_geometry(f, f_ref.GetGeometryRef()) != 0:
            gdaltest.post_reason('fail')
            print(f.GetGeometryRef().ExportToWkt())
            print(f_ref.GetGeometryRef().ExportToWkt())
            return 'fail'

    lyr = ds.GetLayerByName('polygon')
    ds_ref = ogr.Open('data/curves_polygon.csv')
    lyr_ref = ds_ref.GetLayer(0)
    for f in lyr:
        f_ref = lyr_ref.GetNextFeature()
        if ogrtest.check_feature_geometry(f, f_ref.GetGeometryRef()) != 0:
            gdaltest.post_reason('fail')
            print(f.GetGeometryRef().ExportToWkt())
            print(f_ref.GetGeometryRef().ExportToWkt())
            return 'fail'

    ds = ogr.Open('data/curve_circle_by_center.gdb')
    lyr = ds.GetLayer(0)
    ds_ref = ogr.Open('data/curve_circle_by_center.csv')
    lyr_ref = ds_ref.GetLayer(0)
    for f in lyr:
        f_ref = lyr_ref.GetNextFeature()
        if ogrtest.check_feature_geometry(f, f_ref.GetGeometryRef()) != 0:
            gdaltest.post_reason('fail')
            print(f.GetGeometryRef().ExportToWkt())
            print(f_ref.GetGeometryRef().ExportToWkt())
            return 'fail'

    return 'success'

###############################################################################
# Test opening '.'

def ogr_fgdb_23():

    if ogrtest.fgdb_drv is None:
        return 'skip'

    os.chdir('data/curves.gdb')
    ds = ogr.Open('.')
    os.chdir('../..')
    if ds is None:
        return 'fail'

    return 'success'

###############################################################################
# Read polygons with M component where the M of the closing point is not the
# one of the starting point (#7017)

def ogr_fgdb_24():

    ds = ogr.Open('data/filegdb_polygonzm_m_not_closing_with_curves.gdb')
    lyr = ds.GetLayer(0)
    ds_ref = ogr.Open('data/filegdb_polygonzm_m_not_closing_with_curves.gdb.csv')
    lyr_ref = ds_ref.GetLayer(0)
    for f in lyr:
        f_ref = lyr_ref.GetNextFeature()
        if ogrtest.check_feature_geometry(f, f_ref.GetGeometryRef()) != 0:
            gdaltest.post_reason('fail')
            print(f.GetGeometryRef().ExportToIsoWkt())
            print(f_ref.GetGeometryRef().ExportToIsoWkt())
            return 'fail'

    ds = ogr.Open('data/filegdb_polygonzm_nan_m_with_curves.gdb')
    lyr = ds.GetLayer(0)
    ds_ref = ogr.Open('data/filegdb_polygonzm_nan_m_with_curves.gdb.csv')
    lyr_ref = ds_ref.GetLayer(0)
    for f in lyr:
        f_ref = lyr_ref.GetNextFeature()
        if ogrtest.check_feature_geometry(f, f_ref.GetGeometryRef()) != 0:
            gdaltest.post_reason('fail')
            print(f.GetGeometryRef().ExportToIsoWkt())
            print(f_ref.GetGeometryRef().ExportToIsoWkt())
            return 'fail'

    return 'success'

###############################################################################
# Cleanup

def ogr_fgdb_cleanup():
    if ogrtest.fgdb_drv is None:
        return 'skip'

    try:
        shutil.rmtree("tmp/test.gdb")
    except:
        pass

    try:
        shutil.rmtree("tmp/test2.gdb")
    except:
        pass
    try:
        shutil.rmtree("tmp/poly.gdb")
    except:
        pass
    try:
        shutil.rmtree('tmp/test3005.gdb')
    except:
        pass

    if ogrtest.openfilegdb_drv is not None:
        ogrtest.fgdb_drv.Deregister()
        # Force OpenFileGDB first
        ogrtest.openfilegdb_drv.Register()
        ogrtest.fgdb_drv.Register()

    return 'success'

gdaltest_list = [
    ogr_fgdb_init,
    ogr_fgdb_1,
    ogr_fgdb_DeleteField,
    ogr_fgdb_2,
    ogr_fgdb_3,
    ogr_fgdb_sql,
    ogr_fgdb_4,
    ogr_fgdb_5,
    ogr_fgdb_6,
    ogr_fgdb_7,
    ogr_fgdb_8,
    ogr_fgdb_9,
    ogr_fgdb_10,
    ogr_fgdb_11,
    ogr_fgdb_12,
    ogr_fgdb_13,
    ogr_fgdb_14,
    ogr_fgdb_15,
    ogr_fgdb_16,
    ogr_fgdb_17,
    ogr_fgdb_18,
    ogr_fgdb_19,
    ogr_fgdb_19bis,
    ogr_fgdb_20,
    ogr_fgdb_21,
    ogr_fgdb_22,
    ogr_fgdb_23,
    ogr_fgdb_24,
    ogr_fgdb_cleanup,
    ]

disabled_gdaltest_list = [
    ogr_fgdb_init,
    ogr_fgdb_20,
    ogr_fgdb_cleanup
]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_fgdb' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()




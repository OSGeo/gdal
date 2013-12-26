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
# Copyright (c) 2011, Even Rouault <even dot rouault at mines dash paris dot org>
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
                 [ "multilinestring", ogr.wkbMultiLineString, "MULTILINESTRING ((1 2,3 4))" ],
                 [ "polygon", ogr.wkbPolygon, "POLYGON ((0 0,0 1,1 1,1 0,0 0))", "MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0)))" ],
                 [ "multipolygon", ogr.wkbMultiPolygon, "MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0)))" ],
                 [ "point25D", ogr.wkbPoint25D, "POINT (1 2 3)" ],
                 [ "multipoint25D", ogr.wkbMultiPoint25D, "MULTIPOINT (1 2 -10,3 4 -20)" ],
                 [ "linestring25D", ogr.wkbLineString25D, "LINESTRING (1 2 -10,3 4 -20)", "MULTILINESTRING ((1 2 -10,3 4 -20))" ],
                 [ "multilinestring25D", ogr.wkbMultiLineString25D, "MULTILINESTRING ((1 2 -10,3 4 -20))" ],
                 [ "polygon25D", ogr.wkbPolygon25D, "POLYGON ((0 0 -10,0 1 -10,1 1 -10,1 0 -10,0 0 -10))", "MULTIPOLYGON (((0 0 -10,0 1 -10,1 1 -10,1 0 -10,0 0 -10)))" ],
                 [ "multipolygon25D", ogr.wkbMultiPolygon25D, "MULTIPOLYGON (((0 0 -10,0 1 -10,1 1 -10,1 0 -10,0 0 -10)))" ],
                 [ "null_polygon", ogr.wkbPolygon, None],
                 [ "empty_polygon", ogr.wkbPolygon, "POLYGON EMPTY", None],
               ]

    options = ['COLUMN_TYPES=smallint=esriFieldTypeSmallInteger,float=esriFieldTypeSingle,guid=esriFieldTypeGUID,xml=esriFieldTypeXML']

    for data in datalist:
        #import pdb; pdb.set_trace()
        if data[1] == ogr.wkbNone:
            lyr = ds.CreateLayer(data[0], geom_type=data[1], options = options )
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
            lyr.CreateFeature(feat)

    for data in datalist:
        lyr = ds.GetLayerByName(data[0])
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
           feat.GetField('xml') != "<foo></foo>":
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
    if lyr.DeleteField(lyr.GetLayerDefn().GetFieldIndex('str')) != 0:
        gdaltest.post_reason('failure')
        return 'fail'
    lyr.CreateField(ogr.FieldDefn("str2", ogr.OFTString))
    feat = lyr.GetNextFeature()
    feat.SetField("str2", "foo2_\xc3\xa9")
    lyr.SetFeature(feat)
    feat = None
    ds = None

    ds = ogr.Open("tmp/test.gdb")
    lyr = ds.GetLayerByIndex(0)
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

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro tmp/test.gdb')

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
    lyr = ds.CreateLayer('layer1', srs = srs, geom_type = ogr.wkbPoint, options = ['FEATURE_DATASET=featuredataset'])
    lyr = ds.CreateLayer('layer2', srs = srs, geom_type = ogr.wkbPoint, options = ['FEATURE_DATASET=featuredataset'])
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
                 'WITH SPACE AND !$*!- special characters', # unallowed characters
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

    lyr_defn = lyr.GetLayerDefn()
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
    ds = None
    
    ds = ogr.Open('tmp/test.gdb')
    lyr = ds.GetLayer(0)
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
# Cleanup

def ogr_fgdb_cleanup():
    if ogrtest.fgdb_drv is None:
        return 'skip'

    try:
        shutil.rmtree("tmp/test.gdb")
    except:
        pass

    try:
        shutil.rmtree("tmp/poly.gdb")
    except:
        pass

    if ogrtest.openfilegdb_drv is not None:
        ogrtest.fgdb_drv.Deregister()
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
    ogr_fgdb_cleanup,
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_fgdb' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()




#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  FGDB driver testing.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2011-2014, Even Rouault <even dot rouault at spatialys.com>
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
import shutil
import sys


import gdaltest
import ogrtest
from osgeo import gdal
from osgeo import ogr
from osgeo import osr
import pytest

pytestmark = pytest.mark.require_driver('FileGDB')


###############################################################################

@pytest.fixture(autouse=True, scope='module')
def startup_and_cleanup():
    try:
        shutil.rmtree("tmp/test.gdb")
    except OSError:
        pass

    yield

    try:
        shutil.rmtree("tmp/test.gdb")
    except OSError:
        pass

    try:
        shutil.rmtree("tmp/test2.gdb")
    except OSError:
        pass
    try:
        shutil.rmtree("tmp/poly.gdb")
    except OSError:
        pass
    try:
        shutil.rmtree('tmp/test3005.gdb')
    except OSError:
        pass
    try:
        shutil.rmtree('tmp/roads_clip Drawing.gdb')
    except OSError:
        pass

###############################################################################

@pytest.fixture(autouse=True, scope='module')
def openfilegdb_drv():
    drv = ogr.GetDriverByName('OpenFileGDB')
    yield drv

###############################################################################

@pytest.fixture(autouse=True, scope='module')
def fgdb_drv(openfilegdb_drv):
    drv = ogr.GetDriverByName('FileGDB')
    if drv is None:
        pytest.skip('FileGDB driver not available', allow_module_level=True)

    if openfilegdb_drv is not None:
        openfilegdb_drv.Deregister()

    yield drv

    if openfilegdb_drv is not None:
        drv.Deregister()
        # Force OpenFileGDB first
        openfilegdb_drv.Register()
        drv.Register()

###############################################################################

@pytest.fixture()
def fgdb_sdk_1_4_or_later(fgdb_drv):
    fgdb_is_sdk_1_4 = False

    try:
        shutil.rmtree("tmp/ogr_fgdb_is_sdk_1_4_or_later.gdb")
    except OSError:
        pass

    ds = fgdb_drv.CreateDataSource("tmp/ogr_fgdb_is_sdk_1_4_or_later.gdb")
    srs = osr.SpatialReference()
    srs.ImportFromProj4('+proj=tmerc +datum=WGS84 +no_defs')
    with gdaltest.error_handler():
        lyr = ds.CreateLayer('test', srs=srs, geom_type=ogr.wkbPoint)
    if lyr is not None:
        fgdb_is_sdk_1_4 = True
    ds = None
    shutil.rmtree("tmp/ogr_fgdb_is_sdk_1_4_or_later.gdb")
    if not fgdb_is_sdk_1_4:
        pytest.skip('SDK 1.4 required')

###############################################################################

@pytest.fixture()
def ogrsf_path():
    import test_cli_utilities
    path = test_cli_utilities.get_test_ogrsf_path()
    if path is None:
        pytest.skip('ogrsf test utility not found')

    return path

###############################################################################
# Write and read back various geometry types

def test_ogr_fgdb_1(fgdb_drv):

    srs = osr.SpatialReference()
    srs.SetFromUserInput("WGS84")

    ds = fgdb_drv.CreateDataSource("tmp/test.gdb")

    datalist = [["none", ogr.wkbNone, None],
                ["point", ogr.wkbPoint, "POINT (1 2)"],
                ["multipoint", ogr.wkbMultiPoint, "MULTIPOINT (1 2,3 4)"],
                ["linestring", ogr.wkbLineString, "LINESTRING (1 2,3 4)", "MULTILINESTRING ((1 2,3 4))"],
                ["multilinestring", ogr.wkbMultiLineString, "MULTILINESTRING ((1 2,3 4),(5 6,7 8))"],
                ["polygon", ogr.wkbPolygon, "POLYGON ((0 0,0 1,1 1,1 0,0 0))", "MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0)))"],
                ["multipolygon", ogr.wkbMultiPolygon, "MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0),(0.25 0.25,0.75 0.25,0.75 0.75,0.25 0.75,0.25 0.25)),((2 0,2 1,3 1,3 0,2 0)))"],
                ["point25D", ogr.wkbPoint25D, "POINT (1 2 3)"],
                ["multipoint25D", ogr.wkbMultiPoint25D, "MULTIPOINT (1 2 -10,3 4 -20)"],
                ["linestring25D", ogr.wkbLineString25D, "LINESTRING (1 2 -10,3 4 -20)", "MULTILINESTRING ((1 2 -10,3 4 -20))"],
                ["multilinestring25D", ogr.wkbMultiLineString25D, "MULTILINESTRING ((1 2 -10,3 4 -20))"],
                ["polygon25D", ogr.wkbPolygon25D, "POLYGON ((0 0 -10,0 1 -10,1 1 -10,1 0 -10,0 0 -10))", "MULTIPOLYGON (((0 0 -10,0 1 -10,1 1 -10,1 0 -10,0 0 -10)))"],
                ["multipolygon25D", ogr.wkbMultiPolygon25D, "MULTIPOLYGON (((0 0 -10,0 1 -10,1 1 -10,1 0 -10,0 0 -10)))"],
                ["multipatch", ogr.wkbMultiPolygon25D, "GEOMETRYCOLLECTION Z (TIN Z (((0.0 0.0 0,0.0 1.0 0,1.0 0.0 0,0.0 0.0 0)),((0.0 1.0 0,1.0 0.0 0,1.0 1.0 0,0.0 1.0 0))),TIN Z (((10.0 0.0 0,10.0 1.0 0,11.0 0.0 0,10.0 0.0 0)),((10.0 0.0 0,11.0 0.0 0,10.0 -1.0 0,10.0 0.0 0))),TIN Z (((5.0 0.0 0,5.0 1.0 0,6.0 0.0 0,5.0 0.0 0))),MULTIPOLYGON Z (((100.0 0.0 0,100.0 1.0 0,101.0 1.0 0,101.0 0.0 0,100.0 0.0 0),(100.25 0.25 0,100.75 0.25 0,100.75 0.75 0,100.75 0.25 0,100.25 0.25 0))))"],
                ["tin", ogr.wkbTINZ, "TIN Z (((0.0 0.0 0,0.0 1.0 0,1.0 0.0 0,0.0 0.0 0)),((0.0 1.0 0,1.0 0.0 0,1.0 1.0 0,0.0 1.0 0)))"],
                ["null_polygon", ogr.wkbPolygon, None],
                ["empty_polygon", ogr.wkbPolygon, "POLYGON EMPTY", None],
               ]

    options = ['COLUMN_TYPES=smallint=esriFieldTypeSmallInteger,float=esriFieldTypeSingle,guid=esriFieldTypeGUID,xml=esriFieldTypeXML']

    for data in datalist:
        if data[1] == ogr.wkbNone:
            lyr = ds.CreateLayer(data[0], geom_type=data[1], options=options)
        elif data[0] == 'multipatch':
            lyr = ds.CreateLayer(data[0], geom_type=data[1], srs=srs, options=['CREATE_MULTIPATCH=YES', options[0]])
        else:
            lyr = ds.CreateLayer(data[0], geom_type=data[1], srs=srs, options=options)
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
            if data[1] != ogr.wkbNone and data[2] is not None:
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
            assert lyr.GetSpatialRef().IsSame(srs, options = ['IGNORE_DATA_AXIS_TO_SRS_AXIS_MAPPING=YES']) == 1
        feat = lyr.GetNextFeature()
        if data[1] != ogr.wkbNone:
            try:
                expected_wkt = data[3]
            except IndexError:
                expected_wkt = data[2]
            if expected_wkt is None:
                if feat.GetGeometryRef() is not None:
                    feat.DumpReadable()
                    pytest.fail(data)
            elif ogrtest.check_feature_geometry(feat, expected_wkt) != 0:
                feat.DumpReadable()
                pytest.fail(data)

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
            feat.DumpReadable()
            pytest.fail()

        sql_lyr = ds.ExecuteSQL("GetLayerDefinition %s" % lyr.GetName())
        assert sql_lyr is not None
        feat = sql_lyr.GetNextFeature()
        assert feat is not None
        feat = sql_lyr.GetNextFeature()
        assert feat is None
        lyr.ResetReading()
        lyr.TestCapability("foo")
        ds.ReleaseResultSet(sql_lyr)

        sql_lyr = ds.ExecuteSQL("GetLayerMetadata %s" % lyr.GetName())
        assert sql_lyr is not None
        feat = sql_lyr.GetNextFeature()
        assert feat is not None
        ds.ReleaseResultSet(sql_lyr)

    sql_lyr = ds.ExecuteSQL("GetLayerDefinition foo")
    assert sql_lyr is None

    sql_lyr = ds.ExecuteSQL("GetLayerMetadata foo")
    assert sql_lyr is None

    ds = None

###############################################################################
# Test DeleteField()


def test_ogr_fgdb_DeleteField():

    ds = ogr.Open("tmp/test.gdb", update=1)
    lyr = ds.GetLayerByIndex(0)

    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('smallint')).GetSubType() == ogr.OFSTInt16
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('smallint2')).GetSubType() == ogr.OFSTInt16
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('float')).GetSubType() == ogr.OFSTFloat32
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('float2')).GetSubType() == ogr.OFSTFloat32

    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('smallint')).GetWidth() == 0
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('str')).GetWidth() == 0
    assert lyr.DeleteField(lyr.GetLayerDefn().GetFieldIndex('str')) == 0

    # Needed since FileGDB v1.4, otherwise crash/error ...
    if True:  # pylint: disable=using-constant-test
        ds = ogr.Open("tmp/test.gdb", update=1)
        lyr = ds.GetLayerByIndex(0)

    fld_defn = ogr.FieldDefn("str2", ogr.OFTString)
    fld_defn.SetWidth(80)
    lyr.CreateField(fld_defn)
    feat = lyr.GetNextFeature()
    feat.SetField("str2", "foo2_\xc3\xa9")
    lyr.SetFeature(feat)

    # Test updating non-existing feature
    feat.SetFID(-10)
    assert lyr.SetFeature(feat) == ogr.OGRERR_NON_EXISTING_FEATURE, \
        'Expected failure of SetFeature().'

    # Test deleting non-existing feature
    assert lyr.DeleteFeature(-10) == ogr.OGRERR_NON_EXISTING_FEATURE, \
        'Expected failure of DeleteFeature().'

    sql_lyr = ds.ExecuteSQL("REPACK")
    assert sql_lyr
    f = sql_lyr.GetNextFeature()
    assert f[0] == 'true'
    ds.ReleaseResultSet(sql_lyr)

    feat = None
    ds = None

    ds = ogr.Open("tmp/test.gdb")
    lyr = ds.GetLayerByIndex(0)
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('str2')).GetWidth() == 80
    assert lyr.GetLayerDefn().GetFieldIndex('str') == -1
    feat = lyr.GetNextFeature()
    assert feat.GetFieldAsString("str2") == "foo2_\xc3\xa9"
    ds = None

###############################################################################
# Run test_ogrsf


def test_ogr_fgdb_2(ogrsf_path):
    ret = gdaltest.runexternal(ogrsf_path + ' -ro tmp/test.gdb --config OGR_SKIP OpenFileGDB')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

###############################################################################
# Run ogr2ogr


def test_ogr_fgdb_3():

    import test_cli_utilities
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    try:
        shutil.rmtree("tmp/poly.gdb")
    except OSError:
        pass

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f filegdb tmp/poly.gdb data/poly.shp -nlt MULTIPOLYGON -a_srs None')

    ds = ogr.Open('tmp/poly.gdb')
    assert not (ds is None or ds.GetLayerCount() == 0), 'ogr2ogr failed'
    ds = None

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' tmp/poly.gdb')
    # print ret

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

###############################################################################
# Test SQL support


def test_ogr_fgdb_sql():

    import test_cli_utilities
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    ds = ogr.Open('tmp/poly.gdb')

    ds.ExecuteSQL("CREATE INDEX idx_poly_eas_id ON poly(EAS_ID)")

    sql_lyr = ds.ExecuteSQL("SELECT * FROM POLY WHERE EAS_ID = 170", dialect='FileGDB')
    feat = sql_lyr.GetNextFeature()
    assert feat is not None
    feat = sql_lyr.GetNextFeature()
    assert feat is None
    feat = None
    ds.ReleaseResultSet(sql_lyr)
    ds = None

###############################################################################
# Test delete layer


def test_ogr_fgdb_4():

    for j in range(2):

        # Create a layer
        ds = ogr.Open("tmp/test.gdb", update=1)
        srs = osr.SpatialReference()
        srs.SetFromUserInput("WGS84")
        lyr = ds.CreateLayer("layer_to_remove", geom_type=ogr.wkbPoint, srs=srs)
        lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(2 49)'))
        feat.SetField("str", "foo")
        feat = None
        lyr = None

        if j == 1:
            ds = None
            ds = ogr.Open("tmp/test.gdb", update=1)

        # Delete it
        for i in range(ds.GetLayerCount()):
            if ds.GetLayer(i).GetName() == 'layer_to_remove':
                ds.DeleteLayer(i)
                break

        # Check it no longer exists
        lyr = ds.GetLayerByName('layer_to_remove')
        ds = None

        assert lyr is None, ('failed at iteration %d' % j)


###############################################################################
# Test DeleteDataSource()


def test_ogr_fgdb_5(fgdb_drv):

    assert fgdb_drv.DeleteDataSource("tmp/test.gdb") == 0, \
        'DeleteDataSource() failed'

    assert not os.path.exists("tmp/test.gdb")


###############################################################################
# Test adding a layer to an existing feature dataset


def test_ogr_fgdb_6(fgdb_drv):

    srs = osr.SpatialReference()
    srs.SetFromUserInput("WGS84")

    ds = fgdb_drv.CreateDataSource('tmp/test.gdb')
    ds.CreateLayer('layer1', srs=srs, geom_type=ogr.wkbPoint, options=['FEATURE_DATASET=featuredataset'])
    ds.CreateLayer('layer2', srs=srs, geom_type=ogr.wkbPoint, options=['FEATURE_DATASET=featuredataset'])
    ds = None

    ds = ogr.Open('tmp/test.gdb')
    assert ds.GetLayerCount() == 2
    ds = None

###############################################################################
# Test bulk loading (#4420)


def test_ogr_fgdb_7(fgdb_drv):

    try:
        shutil.rmtree("tmp/test.gdb")
    except OSError:
        pass

    srs = osr.SpatialReference()
    srs.SetFromUserInput("WGS84")

    ds = fgdb_drv.CreateDataSource('tmp/test.gdb')
    lyr = ds.CreateLayer('test', srs=srs, geom_type=ogr.wkbPoint)
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
    assert feat.GetField(0) == 0
    ds = None

    gdal.SetConfigOption('FGDB_BULK_LOAD', None)

###############################################################################
# Test field name laundering (#4458)


def test_ogr_fgdb_8(fgdb_drv):

    try:
        shutil.rmtree("tmp/test.gdb")
    except OSError:
        pass

    srs = osr.SpatialReference()
    srs.SetFromUserInput("WGS84")

    ds = fgdb_drv.CreateDataSource('tmp/test.gdb')
    lyr = ds.CreateLayer('test', srs=srs, geom_type=ogr.wkbPoint)
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr.CreateField(ogr.FieldDefn('FROM', ogr.OFTInteger))  # reserved keyword
    lyr.CreateField(ogr.FieldDefn('1NUMBER', ogr.OFTInteger))  # starting with a number
    lyr.CreateField(ogr.FieldDefn('WITH SPACE AND !$*!- special characters', ogr.OFTInteger))  # unallowed characters
    lyr.CreateField(ogr.FieldDefn('é' * 64, ogr.OFTInteger))  # OK
    lyr.CreateField(ogr.FieldDefn('A123456789012345678901234567890123456789012345678901234567890123', ogr.OFTInteger))  # 64 characters : ok
    lyr.CreateField(ogr.FieldDefn('A1234567890123456789012345678901234567890123456789012345678901234', ogr.OFTInteger))  # 65 characters : nok
    lyr.CreateField(ogr.FieldDefn('A12345678901234567890123456789012345678901234567890123456789012345', ogr.OFTInteger))  # 66 characters : nok
    gdal.PopErrorHandler()

    lyr_defn = lyr.GetLayerDefn()
    expected_names = ['FROM_', '_1NUMBER', 'WITH_SPACE_AND_______special_characters',
                      'é' * 64,
                      'A123456789012345678901234567890123456789012345678901234567890123',
                      'A1234567890123456789012345678901234567890123456789012345678901_1',
                      'A1234567890123456789012345678901234567890123456789012345678901_2']
    for i in range(5):
        assert lyr_defn.GetFieldIndex(expected_names[i]) == i, \
            ('did not find %s' % expected_names[i])


###############################################################################
# Test layer name laundering (#4466)


def test_ogr_fgdb_9(fgdb_drv):

    try:
        shutil.rmtree("tmp/test.gdb")
    except OSError:
        pass

    srs = osr.SpatialReference()
    srs.SetFromUserInput("WGS84")

    _160char = 'A123456789' * 16

    in_names = ['FROM',  # reserved keyword
                '1NUMBER',  # starting with a number
                'WITH SPACE AND !$*!- special characters',  # banned characters
                'sde_foo',  # reserved prefixes
                _160char,  # OK
                _160char + 'A',  # too long
                _160char + 'B',  # still too long
               ]

    ds = fgdb_drv.CreateDataSource('tmp/test.gdb')
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    for in_name in in_names:
        lyr = ds.CreateLayer(in_name, srs=srs, geom_type=ogr.wkbPoint)
    gdal.PopErrorHandler()

    lyr.GetLayerDefn()
    expected_names = ['FROM_',
                      '_1NUMBER',
                      'WITH_SPACE_AND_______special_characters',
                      '_sde_foo',
                      _160char,
                      _160char[0:158] + '_1',
                      _160char[0:158] + '_2']
    for i, exp_name in enumerate(expected_names):
        assert ds.GetLayerByIndex(i).GetName() == exp_name, ('did not find %s' % exp_name)


###############################################################################
# Test SRS support


def test_ogr_fgdb_10(fgdb_drv):

    try:
        shutil.rmtree("tmp/test.gdb")
    except OSError:
        pass

    srs_exact_4326 = osr.SpatialReference()
    srs_exact_4326.ImportFromEPSG(4326)

    srs_approx_4326 = srs_exact_4326.Clone()
    srs_approx_4326.MorphToESRI()
    srs_approx_4326.ImportFromWkt(srs_approx_4326.ExportToWkt())

    srs_exact_2193 = osr.SpatialReference()
    srs_exact_2193.ImportFromEPSG(2193)

    srs_approx_2193 = srs_exact_2193.Clone()
    srs_approx_2193.MorphToESRI()
    srs_approx_2193.ImportFromWkt(srs_approx_2193.ExportToWkt())

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
    srs_approx_4230.ImportFromWkt(srs_approx_4230.ExportToWkt())

    srs_approx_intl = osr.SpatialReference()
    srs_approx_intl.ImportFromProj4('+proj=longlat +ellps=intl +no_defs')

    srs_exact_4233 = osr.SpatialReference()
    srs_exact_4233.ImportFromEPSG(4233)

    ds = fgdb_drv.CreateDataSource('tmp/test.gdb')
    lyr = ds.CreateLayer("srs_exact_4326", srs=srs_exact_4326, geom_type=ogr.wkbPoint)
    lyr = ds.CreateLayer("srs_approx_4326", srs=srs_approx_4326, geom_type=ogr.wkbPoint)
    lyr = ds.CreateLayer("srs_exact_2193", srs=srs_exact_2193, geom_type=ogr.wkbPoint)
    lyr = ds.CreateLayer("srs_approx_2193", srs=srs_approx_2193, geom_type=ogr.wkbPoint)

    lyr = ds.CreateLayer("srs_approx_4230", srs=srs_approx_4230, geom_type=ogr.wkbPoint)

    # will fail
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr = ds.CreateLayer("srs_approx_intl", srs=srs_approx_intl, geom_type=ogr.wkbPoint)
    gdal.PopErrorHandler()

    # will fail: 4233 doesn't exist in DB
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr = ds.CreateLayer("srs_exact_4233", srs=srs_exact_4233, geom_type=ogr.wkbPoint)
    gdal.PopErrorHandler()

    # will fail
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr = ds.CreateLayer("srs_not_in_db", srs=srs_not_in_db, geom_type=ogr.wkbPoint)
    gdal.PopErrorHandler()

    ds = None

    ds = ogr.Open('tmp/test.gdb')
    lyr = ds.GetLayerByName("srs_exact_4326")
    assert lyr.GetSpatialRef().ExportToWkt().find('4326') != -1
    lyr = ds.GetLayerByName("srs_approx_4326")
    assert lyr.GetSpatialRef().ExportToWkt().find('4326') != -1
    lyr = ds.GetLayerByName("srs_exact_2193")
    assert lyr.GetSpatialRef().ExportToWkt().find('2193') != -1
    lyr = ds.GetLayerByName("srs_approx_2193")
    assert lyr.GetSpatialRef().ExportToWkt().find('2193') != -1
    lyr = ds.GetLayerByName("srs_approx_4230")
    assert lyr.GetSpatialRef().ExportToWkt().find('4230') != -1
    ds = None

###############################################################################
# Test all data types


def test_ogr_fgdb_11(fgdb_drv):

    try:
        shutil.rmtree("tmp/test.gdb")
    except OSError:
        pass

    f = open('data/filegdb/test_filegdb_field_types.xml', 'rt')
    xml_def = f.read()
    f.close()

    ds = fgdb_drv.CreateDataSource('tmp/test.gdb')
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbNone, options=['XML_DEFINITION=%s' % xml_def])
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField("esriFieldTypeSmallInteger", 12)
    feat.SetField("esriFieldTypeInteger", 3456)
    feat.SetField("esriFieldTypeSingle", 78.9)
    feat.SetField("esriFieldTypeDouble", 1.23)
    feat.SetField("esriFieldTypeDate", "2012/12/31 12:34:56")
    feat.SetField("esriFieldTypeString", "astr")
    feat.SetField("esriFieldTypeGlobalID", "{12345678-9ABC-DEF0-1234-567890ABCDEF}")  # This is ignored and value is generated by FileGDB SDK itself
    feat.SetField("esriFieldTypeGUID", "{12345678-9abc-DEF0-1234-567890ABCDEF}")
    lyr.CreateFeature(feat)
    feat = None

    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)
    feat = None

    # Create a esriFieldTypeGlobalID field
    lyr = ds.CreateLayer('test2', geom_type=ogr.wkbNone, options=['COLUMN_TYPES=global_id=esriFieldTypeGlobalID'])
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
       feat.GetField('esriFieldTypeSingle') != pytest.approx(78.9, abs=1e-2) or \
       feat.GetField('esriFieldTypeDouble') != 1.23 or \
       feat.GetField('esriFieldTypeDate') != '2012/12/31 12:34:56' or \
       feat.GetField('esriFieldTypeString') != 'astr' or \
       feat.GetField('esriFieldTypeGUID') != '{12345678-9ABC-DEF0-1234-567890ABCDEF}' or \
       (not feat.IsFieldSet('esriFieldTypeGlobalID')):
        feat.DumpReadable()
        pytest.fail()

    feat = lyr.GetNextFeature()
    if not feat.IsFieldSet('esriFieldTypeGlobalID'):
        feat.DumpReadable()
        pytest.fail()

    lyr = ds.GetLayerByName('test2')
    feat = lyr.GetNextFeature()
    if not feat.IsFieldSet('global_id'):
        feat.DumpReadable()
        pytest.fail()

    ds = None

###############################################################################
# Test failed Open()


def test_ogr_fgdb_12():

    ds = ogr.Open('tmp/non_existing.gdb')
    assert ds is None

    gdal.Unlink('tmp/dummy.gdb')
    try:
        shutil.rmtree('tmp/dummy.gdb')
    except OSError:
        pass

    f = open('tmp/dummy.gdb', 'wb')
    f.close()

    ds = ogr.Open('tmp/dummy.gdb')
    assert ds is None

    os.unlink('tmp/dummy.gdb')

    os.mkdir('tmp/dummy.gdb')

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogr.Open('tmp/dummy.gdb')
    gdal.PopErrorHandler()
    assert ds is None

    shutil.rmtree('tmp/dummy.gdb')

###############################################################################
# Test failed CreateDataSource() and DeleteDataSource()


def test_ogr_fgdb_13(fgdb_drv):

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = fgdb_drv.CreateDataSource('tmp/foo')
    gdal.PopErrorHandler()
    assert ds is None

    f = open('tmp/dummy.gdb', 'wb')
    f.close()

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = fgdb_drv.CreateDataSource('tmp/dummy.gdb')
    gdal.PopErrorHandler()
    assert ds is None

    os.unlink('tmp/dummy.gdb')

    try:
        shutil.rmtree("/nonexistingdir")
    except OSError:
        pass

    if sys.platform == 'win32':
        name = '/nonexistingdrive:/nonexistingdir/dummy.gdb'
    else:
        name = '/proc/dummy.gdb'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = fgdb_drv.CreateDataSource(name)
    gdal.PopErrorHandler()
    assert ds is None

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = fgdb_drv.DeleteDataSource(name)
    gdal.PopErrorHandler()
    assert ret != 0

###############################################################################
# Test interleaved opening and closing of databases (#4270)


def test_ogr_fgdb_14():

    for _ in range(3):
        ds1 = ogr.Open("tmp/test.gdb")
        assert ds1 is not None
        ds2 = ogr.Open("tmp/test.gdb")
        assert ds2 is not None
        ds2 = None
        ds1 = None


###############################################################################
# Test opening a FGDB with both SRID and LatestSRID set (#5638)


def test_ogr_fgdb_15():

    try:
        shutil.rmtree('tmp/test3005.gdb')
    except OSError:
        pass
    gdaltest.unzip('tmp', 'data/filegdb/test3005.gdb.zip')
    ds = ogr.Open('tmp/test3005.gdb')
    lyr = ds.GetLayer(0)
    got_wkt = lyr.GetSpatialRef().ExportToWkt()
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(3005)
    expected_wkt = sr.ExportToWkt()
    assert got_wkt == expected_wkt
    ds = None

###############################################################################
# Test fix for #5674


def test_ogr_fgdb_16(openfilegdb_drv, fgdb_drv):
    if fgdb_drv is None or openfilegdb_drv is None:
        pytest.skip()

    try:
        gdaltest.unzip('tmp/cache', 'data/filegdb/ESSENCE_NAIPF_ORI_PROV_sub93.gdb.zip')
    except OSError:
        pass
    try:
        os.stat('tmp/cache/ESSENCE_NAIPF_ORI_PROV_sub93.gdb')
    except OSError:
        pytest.skip()

    fgdb_drv.Deregister()

    # Force FileGDB first
    fgdb_drv.Register()
    openfilegdb_drv.Register()

    ds = ogr.Open('tmp/cache/ESSENCE_NAIPF_ORI_PROV_sub93.gdb')
    if ds is None:
        ret = 'fail'
    else:
        ret = 'success'

    # Deregister OpenFileGDB again
    openfilegdb_drv.Deregister()

    shutil.rmtree('tmp/cache/ESSENCE_NAIPF_ORI_PROV_sub93.gdb')

    return ret

###############################################################################
# Test not nullable fields


def test_ogr_fgdb_17(fgdb_drv):

    try:
        shutil.rmtree("tmp/test.gdb")
    except OSError:
        pass

    ds = fgdb_drv.CreateDataSource('tmp/test.gdb')
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4326)
    lyr = ds.CreateLayer('test', geom_type=ogr.wkbPoint, srs=sr, options=['GEOMETRY_NULLABLE=NO'])
    assert lyr.GetLayerDefn().GetGeomFieldDefn(0).IsNullable() == 0
    field_defn = ogr.FieldDefn('field_not_nullable', ogr.OFTString)
    field_defn.SetNullable(0)
    lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn('field_nullable', ogr.OFTString)
    lyr.CreateField(field_defn)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('field_not_nullable', 'not_null')
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT(0 0)'))
    ret = lyr.CreateFeature(f)
    assert ret == 0
    f = None

    # Error case: missing geometry
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('field_not_nullable', 'not_null')
    gdal.PushErrorHandler()
    ret = lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    assert ret != 0
    f = None

    # Error case: missing non-nullable field
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT(0 0)'))
    gdal.PushErrorHandler()
    ret = lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    assert ret != 0
    f = None

    ds = None

    ds = ogr.Open('tmp/test.gdb', update=1)
    lyr = ds.GetLayerByName('test')
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_not_nullable')).IsNullable() == 0
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_nullable')).IsNullable() == 1
    assert lyr.GetLayerDefn().GetGeomFieldDefn(0).IsNullable() == 0

    ds = None

###############################################################################
# Test default values


def test_ogr_fgdb_18(openfilegdb_drv, fgdb_drv):

    try:
        shutil.rmtree("tmp/test.gdb")
    except OSError:
        pass

    ds = fgdb_drv.CreateDataSource('tmp/test.gdb')
    lyr = ds.CreateLayer('test', geom_type=ogr.wkbNone)

    field_defn = ogr.FieldDefn('field_string', ogr.OFTString)
    field_defn.SetDefault("'a''b'")
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn('field_int', ogr.OFTInteger)
    field_defn.SetDefault('123')
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn('field_real', ogr.OFTReal)
    field_defn.SetDefault('1.23')
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn('field_nodefault', ogr.OFTInteger)
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn('field_datetime', ogr.OFTDateTime)
    field_defn.SetDefault("CURRENT_TIMESTAMP")
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn('field_datetime2', ogr.OFTDateTime)
    field_defn.SetDefault("'2015/06/30 12:34:56'")
    lyr.CreateField(field_defn)

    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    f = None

    ds = None

    if openfilegdb_drv is not None:
        openfilegdb_drv.Register()
    ret = ogr_fgdb_18_test_results(openfilegdb_drv)
    if openfilegdb_drv is not None:
        openfilegdb_drv.Deregister()

    return ret


def ogr_fgdb_18_test_results(openfilegdb_drv):

    ds = ogr.Open('tmp/test.gdb', update=1)
    lyr = ds.GetLayerByName('test')
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_string')).GetDefault() == "'a''b'"
    if openfilegdb_drv is not None:
        assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_int')).GetDefault() == '123'
        assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_real')).GetDefault() == '1.23'
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_nodefault')).GetDefault() is None
    # if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_datetime')).GetDefault() != 'CURRENT_TIMESTAMP':
    #    gdaltest.post_reason('fail')
    #    return 'fail'
    # if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_datetime2')).GetDefault() != "'2015/06/30 12:34:56'":
    #    gdaltest.post_reason('fail')
    #    print(lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_datetime2')).GetDefault())
    #    return 'fail'
    f = lyr.GetNextFeature()
    if f.GetField('field_string') != 'a\'b' or f.GetField('field_int') != 123 or \
       f.GetField('field_real') != 1.23 or \
       not f.IsFieldNull('field_nodefault') or not f.IsFieldSet('field_datetime') or \
       f.GetField('field_datetime2') != '2015/06/30 12:34:56':
        f.DumpReadable()
        pytest.fail()
    ds = None

###############################################################################
# Test transaction support


def ogr_fgdb_19_open_update(openfilegdb_drv, fgdb_drv, filename):

    # We need the OpenFileGDB driver for Linux improved StartTransaction()
    bPerLayerCopyingForTransaction = False
    if openfilegdb_drv is not None:
        openfilegdb_drv.Register()
        if os.name != 'nt':
            val = gdal.GetConfigOption('FGDB_PER_LAYER_COPYING_TRANSACTION', 'TRUE')
            if val == 'TRUE' or val == 'YES' or val == 'ON':
                bPerLayerCopyingForTransaction = True

    ds = ogr.Open(filename, update=1)

    if openfilegdb_drv is not None:
        openfilegdb_drv.Deregister()
        fgdb_drv.Deregister()
        # Force OpenFileGDB first
        openfilegdb_drv.Register()
        fgdb_drv.Register()

    return (bPerLayerCopyingForTransaction, ds)


def test_ogr_fgdb_19(openfilegdb_drv, fgdb_drv):

    # FIXME likely due to too old FileGDB SDK on those targets
    # fails with ERROR 1: Failed to open Geodatabase (The system cannot find the file specified.)
    # File "ogr_fgdb.py", line 1664, in ogr_fgdb_19
    # if ds.StartTransaction(force=True) != 0:
    if gdaltest.is_travis_branch('ubuntu_2004') or gdaltest.is_travis_branch('ubuntu_1804') or gdaltest.is_travis_branch('ubuntu_1604') or gdaltest.is_travis_branch('trusty_clang') or gdaltest.is_travis_branch('python3') or gdaltest.is_travis_branch('trunk_with_coverage'):
        pytest.skip()

    try:
        shutil.rmtree("tmp/test.gdb.ogrtmp")
    except OSError:
        pass
    try:
        shutil.rmtree("tmp/test.gdb.ogredited")
    except OSError:
        pass

    # Error case: try in read-only
    ds = ogr.Open('tmp/test.gdb')
    gdal.PushErrorHandler()
    ret = ds.StartTransaction(force=True)
    gdal.PopErrorHandler()
    assert ret != 0
    ds = None

    (bPerLayerCopyingForTransaction, ds) = ogr_fgdb_19_open_update(openfilegdb_drv, fgdb_drv, 'tmp/test.gdb')

    assert ds.TestCapability(ogr.ODsCEmulatedTransactions) == 1

    # Error case: try in non-forced mode
    gdal.PushErrorHandler()
    ret = ds.StartTransaction(force=False)
    gdal.PopErrorHandler()
    assert ret != 0

    # Error case: try StartTransaction() with a ExecuteSQL layer still active
    sql_lyr = ds.ExecuteSQL('SELECT * FROM test')
    gdal.PushErrorHandler()
    ret = ds.StartTransaction(force=True)
    gdal.PopErrorHandler()
    assert ret != 0
    ds.ReleaseResultSet(sql_lyr)

    # Error case: call CommitTransaction() while there is no transaction
    gdal.PushErrorHandler()
    ret = ds.CommitTransaction()
    gdal.PopErrorHandler()
    assert ret != 0

    # Error case: call RollbackTransaction() while there is no transaction
    gdal.PushErrorHandler()
    ret = ds.RollbackTransaction()
    gdal.PopErrorHandler()
    assert ret != 0

    # Error case: try StartTransaction() with another active connection
    ds2 = ogr.Open('tmp/test.gdb', update=1)
    gdal.PushErrorHandler()
    ret = ds2.StartTransaction(force=True)
    gdal.PopErrorHandler()
    assert ret != 0
    ds2 = None

    # Successful StartTransaction() finally!
    lyr = ds.GetLayer(0)
    lyr = ds.GetLayer(0)  # again
    old_count = lyr.GetFeatureCount()
    lyr_defn = lyr.GetLayerDefn()
    layer_created_before_transaction = ds.CreateLayer('layer_created_before_transaction', geom_type=ogr.wkbNone)
    layer_created_before_transaction_defn = layer_created_before_transaction.GetLayerDefn()

    assert ds.StartTransaction(force=True) == 0

    assert os.path.exists('tmp/test.gdb.ogredited')
    assert not os.path.exists('tmp/test.gdb.ogrtmp')


    ret = lyr.CreateField(ogr.FieldDefn('foobar', ogr.OFTString))
    assert ret == 0

    ret = lyr.DeleteField(lyr.GetLayerDefn().GetFieldIndex('foobar'))
    assert ret == 0

    gdal.PushErrorHandler()
    ret = lyr.CreateGeomField(ogr.GeomFieldDefn('foobar', ogr.wkbPoint))
    gdal.PopErrorHandler()
    assert ret != 0

    gdal.PushErrorHandler()
    ret = lyr.ReorderFields([i for i in range(lyr.GetLayerDefn().GetFieldCount())])
    gdal.PopErrorHandler()
    assert ret != 0

    gdal.PushErrorHandler()
    ret = lyr.AlterFieldDefn(0, ogr.FieldDefn('foo', ogr.OFTString), 0)
    gdal.PopErrorHandler()
    assert ret != 0

    f = ogr.Feature(lyr_defn)
    f.SetField('field_string', 'foo')
    lyr.CreateFeature(f)
    lyr.SetFeature(f)
    fid = f.GetFID()
    assert fid > 0
    lyr.ResetReading()
    for i in range(fid):
        f = lyr.GetNextFeature()
    assert f.GetFID() == fid and f.GetField('field_string') == 'foo'
    f = lyr.GetFeature(fid)
    assert f.GetFID() == fid and f.GetField('field_string') == 'foo'

    f = ogr.Feature(layer_created_before_transaction_defn)
    layer_created_before_transaction.CreateFeature(f)

    # Error case: call StartTransaction() while there is an active transaction
    gdal.PushErrorHandler()
    ret = ds.StartTransaction(force=True)
    gdal.PopErrorHandler()
    assert ret != 0

    # Error case: try CommitTransaction() with a ExecuteSQL layer still active
    sql_lyr = ds.ExecuteSQL('SELECT * FROM test')
    gdal.PushErrorHandler()
    ret = ds.CommitTransaction()
    gdal.PopErrorHandler()
    assert ret != 0
    ds.ReleaseResultSet(sql_lyr)

    # Error case: try RollbackTransaction() with a ExecuteSQL layer still active
    sql_lyr = ds.ExecuteSQL('SELECT * FROM test')
    gdal.PushErrorHandler()
    ret = ds.RollbackTransaction()
    gdal.PopErrorHandler()
    assert ret != 0
    ds.ReleaseResultSet(sql_lyr)

    # Test that CommitTransaction() works
    assert ds.CommitTransaction() == 0

    assert not os.path.exists('tmp/test.gdb.ogredited')
    assert not os.path.exists('tmp/test.gdb.ogrtmp')

    lst = gdal.ReadDir('tmp/test.gdb')
    for filename in lst:
        assert '.tmp' not in filename, lst

    lyr_tmp = ds.GetLayer(0)
    lyr_tmp = ds.GetLayer(0)
    new_count = lyr_tmp.GetFeatureCount()
    assert new_count == old_count + 1
    old_count = new_count

    assert layer_created_before_transaction.GetFeatureCount() == 1

    for i in range(ds.GetLayerCount()):
        if ds.GetLayer(i).GetName() == layer_created_before_transaction.GetName():
            ds.DeleteLayer(i)
            break
    layer_created_before_transaction = None

    # Test suppression of layer within transaction
    lyr_count = ds.GetLayerCount()
    ds.CreateLayer('layer_tmp', geom_type=ogr.wkbNone)
    ret = ds.StartTransaction(force=True)
    assert ret == 0
    ds.DeleteLayer(ds.GetLayerCount() - 1)
    assert ds.CommitTransaction() == 0

    new_lyr_count = ds.GetLayerCount()
    assert new_lyr_count == lyr_count

    # Test that RollbackTransaction() works
    ret = ds.StartTransaction(force=True)
    assert ret == 0

    f = ogr.Feature(lyr_defn)
    lyr.CreateFeature(f)

    layer_created_during_transaction = ds.CreateLayer('layer_created_during_transaction', geom_type=ogr.wkbNone)
    layer_created_during_transaction.CreateField(ogr.FieldDefn('foo', ogr.OFTString))

    assert ds.RollbackTransaction() == 0

    assert not os.path.exists('tmp/test.gdb.ogredited')
    assert not os.path.exists('tmp/test.gdb.ogrtmp')

    assert lyr.GetFeatureCount() == old_count

    # Cannot retrieve the layer any more from fresh
    assert ds.GetLayerByName('layer_created_during_transaction') is None
    # Pointer is in ghost state
    assert layer_created_during_transaction.GetLayerDefn().GetFieldCount() == 0

    # Simulate an error case where StartTransaction() cannot copy backup files
    lyr_count = ds.GetLayerCount()
    gdal.SetConfigOption('FGDB_SIMUL_FAIL', 'CASE1')
    gdal.PushErrorHandler()
    ret = ds.StartTransaction(force=True)
    gdal.PopErrorHandler()
    gdal.SetConfigOption('FGDB_SIMUL_FAIL', None)
    assert ret != 0

    assert ds.GetLayerCount() == lyr_count

    # Simulate an error case where StartTransaction() cannot reopen database
    gdal.SetConfigOption('FGDB_SIMUL_FAIL', 'CASE2')
    gdal.PushErrorHandler()
    ret = ds.StartTransaction(force=True)
    gdal.PopErrorHandler()
    gdal.SetConfigOption('FGDB_SIMUL_FAIL', None)
    assert ret != 0

    assert ds.GetLayerCount() == 0
    shutil.rmtree('tmp/test.gdb.ogredited')

    # Test method on ghost datasource and layer
    ds.GetName()
    ds.GetLayerCount()
    ds.GetLayer(0)
    ds.GetLayerByName("test")
    ds.DeleteLayer(0)
    ds.TestCapability('foo')
    ds.CreateLayer('bar', geom_type=ogr.wkbNone)
    ds.CopyLayer(lyr, 'baz')
    ds.GetStyleTable()
    # ds.SetStyleTableDirectly(None)
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
    lyr.SetSpatialFilterRect(0, 0, 0, 0)
    lyr.SetSpatialFilter(0, None)
    lyr.SetSpatialFilterRect(0, 0, 0, 0, 0)
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
    # lyr.SetStyleTableDirectly(None)
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
        (bPerLayerCopyingForTransaction, ds) = ogr_fgdb_19_open_update(openfilegdb_drv, fgdb_drv, 'tmp/test.gdb')

        layer_tmp = ds.CreateLayer('layer_tmp', geom_type=ogr.wkbNone)
        layer_tmp.CreateField(ogr.FieldDefn('foo', ogr.OFTString))

        assert ds.StartTransaction(force=True) == 0

        ds.DeleteLayer(ds.GetLayerCount() - 1)

        gdal.SetConfigOption('FGDB_SIMUL_FAIL', 'CASE1')
        gdal.PushErrorHandler()
        ret = ds.CommitTransaction()
        gdal.PopErrorHandler()
        gdal.SetConfigOption('FGDB_SIMUL_FAIL', None)
        assert ret != 0

        ds = None

        shutil.rmtree('tmp/test.gdb.ogredited')

        lst = gdal.ReadDir('tmp/test.gdb')
        for filename in lst:
            assert '.tmp' not in filename, lst

        # Test an error case where we simulate a failure in renaming
        # a file in original directory
        (bPerLayerCopyingForTransaction, ds) = ogr_fgdb_19_open_update(openfilegdb_drv, fgdb_drv, 'tmp/test.gdb')

        for i in range(ds.GetLayerCount()):
            if ds.GetLayer(i).GetName() == 'layer_tmp':
                ds.DeleteLayer(i)
                break

        assert ds.StartTransaction(force=True) == 0

        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        lyr.SetFeature(f)
        f = None

        gdal.SetConfigOption('FGDB_SIMUL_FAIL', 'CASE2')
        gdal.PushErrorHandler()
        ret = ds.CommitTransaction()
        gdal.PopErrorHandler()
        gdal.SetConfigOption('FGDB_SIMUL_FAIL', None)
        assert ret != 0

        ds = None

        shutil.rmtree('tmp/test.gdb.ogredited')

        lst = gdal.ReadDir('tmp/test.gdb')
        for filename in lst:
            assert '.tmp' not in filename, lst

        # Test an error case where we simulate a failure in moving
        # a file into original directory
        (bPerLayerCopyingForTransaction, ds) = ogr_fgdb_19_open_update(openfilegdb_drv, fgdb_drv, 'tmp/test.gdb')

        assert ds.StartTransaction(force=True) == 0

        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        lyr.SetFeature(f)
        f = None

        gdal.SetConfigOption('FGDB_SIMUL_FAIL', 'CASE3')
        gdal.PushErrorHandler()
        ret = ds.CommitTransaction()
        gdal.PopErrorHandler()
        gdal.SetConfigOption('FGDB_SIMUL_FAIL', None)
        assert ret != 0

        ds = None

        shutil.rmtree('tmp/test.gdb.ogredited')

        # Remove left over .tmp files
        lst = gdal.ReadDir('tmp/test.gdb')
        for filename in lst:
            if '.tmp' in filename:
                os.remove('tmp/test.gdb/' + filename)

        # Test not critical error in removing a temporary file
        for case in ('CASE4', 'CASE5'):
            (bPerLayerCopyingForTransaction, ds) = ogr_fgdb_19_open_update(openfilegdb_drv, fgdb_drv, 'tmp/test.gdb')

            assert ds.StartTransaction(force=True) == 0

            lyr = ds.GetLayer(0)
            f = lyr.GetNextFeature()
            lyr.SetFeature(f)
            f = None

            gdal.SetConfigOption('FGDB_SIMUL_FAIL', case)
            gdal.PushErrorHandler()
            ret = ds.CommitTransaction()
            gdal.PopErrorHandler()
            gdal.SetConfigOption('FGDB_SIMUL_FAIL', None)
            assert ret == 0, case

            ds = None

            if case == 'CASE4':
                assert not os.path.exists('tmp/test.gdb.ogredited'), case
            else:
                shutil.rmtree('tmp/test.gdb.ogredited')

            # Remove left over .tmp files
            lst = gdal.ReadDir('tmp/test.gdb')
            for filename in lst:
                if '.tmp' in filename:
                    os.remove('tmp/test.gdb/' + filename)

    else:
        # Test an error case where we simulate a failure of rename from .gdb to .gdb.ogrtmp during commit
        (bPerLayerCopyingForTransaction, ds) = ogr_fgdb_19_open_update(openfilegdb_drv, fgdb_drv, 'tmp/test.gdb')
        lyr = ds.GetLayer(0)
        lyr_defn = lyr.GetLayerDefn()

        assert ds.StartTransaction(force=True) == 0

        gdal.SetConfigOption('FGDB_SIMUL_FAIL', 'CASE1')
        gdal.PushErrorHandler()
        ret = ds.CommitTransaction()
        gdal.PopErrorHandler()
        gdal.SetConfigOption('FGDB_SIMUL_FAIL', None)
        assert ret != 0

        ds = None

        # Test an error case where we simulate a failure of rename from .gdb.ogredited to .gdb during commit
        (bPerLayerCopyingForTransaction, ds) = ogr_fgdb_19_open_update(openfilegdb_drv, fgdb_drv, 'tmp/test.gdb')
        lyr = ds.GetLayer(0)
        lyr_defn = lyr.GetLayerDefn()

        assert ds.StartTransaction(force=True) == 0

        gdal.SetConfigOption('FGDB_SIMUL_FAIL', 'CASE2')
        gdal.PushErrorHandler()
        ret = ds.CommitTransaction()
        gdal.PopErrorHandler()
        gdal.SetConfigOption('FGDB_SIMUL_FAIL', None)
        assert ret != 0

        ds = None
        os.rename('tmp/test.gdb.ogrtmp', 'tmp/test.gdb')

        # Test an error case where we simulate a failure of removing from .gdb.ogrtmp during commit
        (bPerLayerCopyingForTransaction, ds) = ogr_fgdb_19_open_update(openfilegdb_drv, fgdb_drv, 'tmp/test.gdb')
        lyr = ds.GetLayer(0)
        lyr_defn = lyr.GetLayerDefn()

        assert ds.StartTransaction(force=True) == 0

        gdal.SetConfigOption('FGDB_SIMUL_FAIL', 'CASE3')
        gdal.PushErrorHandler()
        ret = ds.CommitTransaction()
        gdal.PopErrorHandler()
        gdal.SetConfigOption('FGDB_SIMUL_FAIL', None)
        assert ret == 0

        ds = None
        shutil.rmtree('tmp/test.gdb.ogrtmp')

    # Test an error case where we simulate a failure of reopening the committed DB
    (bPerLayerCopyingForTransaction, ds) = ogr_fgdb_19_open_update(openfilegdb_drv, fgdb_drv, 'tmp/test.gdb')
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()

    assert ds.StartTransaction(force=True) == 0

    gdal.SetConfigOption('FGDB_SIMUL_FAIL', 'CASE_REOPEN')
    gdal.PushErrorHandler()
    ret = ds.CommitTransaction()
    gdal.PopErrorHandler()
    gdal.SetConfigOption('FGDB_SIMUL_FAIL', None)
    assert ret != 0

    assert ds.GetLayerCount() == 0

    ds = None

    # Test an error case where we simulate a failure of removing from .gdb.ogredited during rollback
    (bPerLayerCopyingForTransaction, ds) = ogr_fgdb_19_open_update(openfilegdb_drv, fgdb_drv, 'tmp/test.gdb')
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()

    assert ds.StartTransaction(force=True) == 0

    gdal.SetConfigOption('FGDB_SIMUL_FAIL', 'CASE1')
    gdal.PushErrorHandler()
    ret = ds.RollbackTransaction()
    gdal.PopErrorHandler()
    gdal.SetConfigOption('FGDB_SIMUL_FAIL', None)
    assert ret != 0

    ds = None
    shutil.rmtree('tmp/test.gdb.ogredited')

    # Test an error case where we simulate a failure of reopening the rollbacked DB
    (bPerLayerCopyingForTransaction, ds) = ogr_fgdb_19_open_update(openfilegdb_drv, fgdb_drv, 'tmp/test.gdb')
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()

    assert ds.StartTransaction(force=True) == 0

    gdal.SetConfigOption('FGDB_SIMUL_FAIL', 'CASE2')
    gdal.PushErrorHandler()
    ret = ds.RollbackTransaction()
    gdal.PopErrorHandler()
    gdal.SetConfigOption('FGDB_SIMUL_FAIL', None)
    assert ret != 0

    assert ds.GetLayerCount() == 0

    ds = None

    if openfilegdb_drv is not None:
        openfilegdb_drv.Deregister()


# Same, but retry without per-layer copying optimization (in the case
# this was what was tested in previous step)


def test_ogr_fgdb_19bis(openfilegdb_drv, fgdb_drv):

    if gdaltest.is_travis_branch('ubuntu_2004') or gdaltest.is_travis_branch('ubuntu_1804') or gdaltest.is_travis_branch('ubuntu_1604') or gdaltest.is_travis_branch('trusty_clang') or gdaltest.is_travis_branch('python3') or gdaltest.is_travis_branch('trunk_with_coverage'):
        pytest.skip()

    (bPerLayerCopyingForTransaction, ds) = ogr_fgdb_19_open_update(openfilegdb_drv, fgdb_drv, 'tmp/test.gdb')
    del ds
    if not bPerLayerCopyingForTransaction:
        pytest.skip()

    gdal.SetConfigOption('FGDB_PER_LAYER_COPYING_TRANSACTION', 'FALSE')
    ret = test_ogr_fgdb_19(openfilegdb_drv, fgdb_drv)
    gdal.SetConfigOption('FGDB_PER_LAYER_COPYING_TRANSACTION', None)
    return ret

###############################################################################
# Test CreateFeature() with user defined FID


def test_ogr_fgdb_20(openfilegdb_drv, fgdb_drv):

    if openfilegdb_drv is None:
        pytest.skip('No OpenFileGDB driver available')

    if gdaltest.is_travis_branch('ubuntu_2004') or gdaltest.is_travis_branch('ubuntu_1804') or gdaltest.is_travis_branch('ubuntu_1604') or gdaltest.is_travis_branch('trusty_clang') or gdaltest.is_travis_branch('python3') or gdaltest.is_travis_branch('trunk_with_coverage'):
        pytest.skip()

    if not os.path.exists('tmp/test.gdb'):
        ds = fgdb_drv.CreateDataSource("tmp/test.gdb")
        ds = None

    # We need the OpenFileGDB driver for CreateFeature() with user defined FID
    openfilegdb_drv.Register()
    ds = ogr.Open('tmp/test.gdb', update=1)
    openfilegdb_drv.Deregister()
    fgdb_drv.Deregister()
    # Force OpenFileGDB first
    openfilegdb_drv.Register()
    fgdb_drv.Register()


    lyr = ds.CreateLayer('test_2147483647', geom_type=ogr.wkbNone)
    lyr.CreateField(ogr.FieldDefn('int', ogr.OFTInteger))
    f = ogr.Feature(lyr.GetLayerDefn())
    fid = 2147483647
    f.SetFID(fid)
    f.SetField(0, fid)
    lyr.CreateFeature(f)
    ds = None

    ds = openfilegdb_drv.Open('tmp/test.gdb')
    lyr = ds.GetLayerByName('test_2147483647')
    f = lyr.GetNextFeature()
    assert f
    assert f.GetFID() == 2147483647
    ds = None

    ds = ogr.Open('tmp/test.gdb', update=1)
    lyr = ds.GetLayerByName('test_2147483647')
    # GetNextFeature() is excruciatingly slow on such huge FID with the SDK driver
    f = lyr.GetFeature(2147483647)
    assert f

    lyr = ds.CreateLayer('ogr_fgdb_20', geom_type=ogr.wkbNone)
    lyr.CreateField(ogr.FieldDefn('id', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('str', ogr.OFTString))

    ds.ExecuteSQL('CREATE INDEX ogr_fgdb_20_id ON ogr_fgdb_20(id)')

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('id', 1)
    ret = lyr.CreateFeature(f)
    assert ret == 0 and f.GetFID() == 1 and lyr.GetMetadataItem('1', 'MAP_OGR_FID_TO_FGDB_FID') is None

    # Existing FID
    gdal.PushErrorHandler()
    ret = lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    assert ret != 0

    for invalid_fid in [-2, 0, 9876543210]:
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetFID(invalid_fid)
        gdal.PushErrorHandler()
        ret = lyr.CreateFeature(f)
        gdal.PopErrorHandler()
        assert ret != 0, invalid_fid

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(2)
    f.SetField('id', 2)
    ret = lyr.CreateFeature(f)
    if ret != 0 or f.GetFID() != 2 or lyr.GetMetadataItem('2', 'MAP_OGR_FID_TO_FGDB_FID') is not None:
        f.DumpReadable()
        pytest.fail()

    # OGR FID = 4, FileGDB FID = 3
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(4)
    f.SetField('id', 4)

    # Cannot call CreateFeature() with a set FID when a dataset is opened more than once
    ds2 = ogr.Open('tmp/test.gdb', update=1)
    gdal.PushErrorHandler()
    ret = lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    assert ret != 0
    ds2 = None

    ret = lyr.CreateFeature(f)
    if ret != 0 or f.GetFID() != 4 or lyr.GetMetadataItem('4', 'MAP_OGR_FID_TO_FGDB_FID') != '3':
        f.DumpReadable()
        pytest.fail(lyr.GetMetadataItem('4', 'MAP_OGR_FID_TO_FGDB_FID'))

    #  Cannot open geodatabase at the moment since it is in 'FID hack mode'
    gdal.PushErrorHandler()
    ds2 = ogr.Open('tmp/test.gdb', update=1)
    gdal.PopErrorHandler()
    assert ds2 is None
    ds2 = None

    # Existing FID, but only in OGR space
    gdal.PushErrorHandler()
    ret = lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    assert ret != 0

    # This FID exists as a FGDB ID, but should not be user visible.
    f.SetFID(3)
    ret = lyr.SetFeature(f)
    assert ret == ogr.OGRERR_NON_EXISTING_FEATURE
    ret = lyr.DeleteFeature(3)
    assert ret == ogr.OGRERR_NON_EXISTING_FEATURE
    ret = lyr.GetFeature(3)
    assert ret is None

    # Trying to set OGR FID = 3 --> FileGDB FID = 4
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(3)
    f.SetField('id', 3)

    ret = lyr.CreateFeature(f)
    if ret != 0 or f.GetFID() != 3 or lyr.GetMetadataItem('3', 'MAP_OGR_FID_TO_FGDB_FID') != '4':
        f.DumpReadable()
        pytest.fail()

    lyr.ResetReading()
    expected = [(1, None), (2, None), (4, 3), (3, 4)]
    for i in range(2):
        for (fid, fgdb_fid) in expected:
            if i == 0:
                f = lyr.GetNextFeature()
            else:
                f = lyr.GetFeature(fid)
            assert f is not None
            if f.GetFID() != fid or f.GetField('id') != fid:
                f.DumpReadable()
                pytest.fail(fid)
            got_fgdb_fid = lyr.GetMetadataItem(str(f.GetFID()), 'MAP_OGR_FID_TO_FGDB_FID')
            if got_fgdb_fid is None:
                assert fgdb_fid is None
            elif int(got_fgdb_fid) != fgdb_fid:
                print(fgdb_fid)
                pytest.fail(got_fgdb_fid)

    for fid in [-9876543210, 0, 100]:
        f = lyr.GetFeature(fid)
        if f is not None:
            f.DumpReadable()
            pytest.fail()

    for invalid_fid in [-2, 0, 9876543210]:
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetFID(invalid_fid)
        ret = lyr.SetFeature(f)
        assert ret == ogr.OGRERR_NON_EXISTING_FEATURE
        ret = lyr.DeleteFeature(invalid_fid)
        assert ret == ogr.OGRERR_NON_EXISTING_FEATURE

    f = lyr.GetFeature(3)
    f.SetField('str', '3')
    ret = lyr.SetFeature(f)
    assert ret == 0

    f = lyr.GetFeature(3)
    assert f.GetField('str') == '3'

    ret = lyr.DeleteFeature(1)
    assert ret == 0

    ret = lyr.DeleteFeature(3)
    assert ret == 0

    for (fid, fgdb_fid) in [(3, 5), (2049, 6), (10, 7), (7, 8), (9, None), (8, 10), (12, 11)]:
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetFID(fid)
        f.SetField('id', fid)
        ret = lyr.CreateFeature(f)
        if ret != 0 or f.GetFID() != fid or str(lyr.GetMetadataItem(str(fid), 'MAP_OGR_FID_TO_FGDB_FID')) != str(fgdb_fid):
            f.DumpReadable()
            print(lyr.GetMetadataItem(str(fid), 'MAP_OGR_FID_TO_FGDB_FID'))
            pytest.fail(fid)

    # Normally 12 should be attributed, but it has already been reserved
    f = ogr.Feature(lyr.GetLayerDefn())
    ret = lyr.CreateFeature(f)
    if ret != 0 or f.GetFID() != 13:
        f.DumpReadable()
        pytest.fail()
    f.SetField('id', f.GetFID())
    lyr.SetFeature(f)

    lyr.ResetReading()
    expected = [(2, None), (4, 3), (3, 5), (2049, 6), (10, 7), (7, 8), (9, None), (8, 10)]
    for (fid, fgdb_fid) in expected:
        f = lyr.GetNextFeature()
        assert f is not None
        if f.GetFID() != fid or f.GetField('id') != fid or str(lyr.GetMetadataItem(str(fid), 'MAP_OGR_FID_TO_FGDB_FID')) != str(fgdb_fid):
            f.DumpReadable()
            print(lyr.GetMetadataItem(str(fid), 'MAP_OGR_FID_TO_FGDB_FID'))
            pytest.fail(fid)

    lyr.SetAttributeFilter('id = 3')
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f.GetFID() != 3:
        f.DumpReadable()
        pytest.fail()

    # This will cause a resync of indexes
    lyr.SetAttributeFilter('OBJECTID = 3')
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f.GetFID() != 3:
        f.DumpReadable()
        pytest.fail()

    # No sparse pages
    lyr = ds.CreateLayer('ogr_fgdb_20_simple', geom_type=ogr.wkbNone)
    lyr.CreateField(ogr.FieldDefn('id', ogr.OFTInteger))

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(2)
    f.SetField('id', 2)
    lyr.CreateFeature(f)

    # This will cause a resync of indexes
    sql_lyr = ds.ExecuteSQL('SELECT * FROM ogr_fgdb_20_simple')
    f = sql_lyr.GetNextFeature()
    if f.GetFID() != 2:
        f.DumpReadable()
        pytest.fail()

    # Do not allow user set FID while a select layer is in progress
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(3)
    f.SetField('id', 3)
    gdal.PushErrorHandler()
    ret = lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    assert ret != 0

    ds.ReleaseResultSet(sql_lyr)

    # Do it in transaction, but this is completely orthogonal
    ds.StartTransaction(force=True)

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
    lyr = ds.CreateLayer('ogr_fgdb_20_indexes', geom_type=ogr.wkbPoint, srs=srs)
    gdal.SetConfigOption('FGDB_RESYNC_THRESHOLD', None)
    lyr.CreateField(ogr.FieldDefn('id', ogr.OFTInteger))
    ds.ExecuteSQL('CREATE INDEX ogr_fgdb_20_indexes_id ON ogr_fgdb_20_indexes(id)')
    gdal.SetConfigOption('FGDB_BULK_LOAD', 'YES')
    for i in range(1000):
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetFID(i + 2)
        f.SetField('id', i + 2)
        f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (%d 0)' % i))
        lyr.CreateFeature(f)
    gdal.SetConfigOption('FGDB_BULK_LOAD', None)
    ds = None

    # Check consistency after re-opening
    gdal.ErrorReset()
    for update in [0, 1]:
        ds = ogr.Open('tmp/test.gdb', update=update)
        lyr = ds.GetLayerByName('ogr_fgdb_20')
        assert lyr.GetFeatureCount() == 10
        lyr.ResetReading()
        expected = [2, 3, 4, 7, 8, 9, 10, 12, 13, 2049]
        for fid in expected:
            f = lyr.GetNextFeature()
            assert gdal.GetLastErrorType() == 0
            assert f is not None, fid
            if f.GetFID() != fid or f.GetField('id') != fid:
                f.DumpReadable()
                pytest.fail(fid)

        for fid in expected:
            lyr.SetAttributeFilter('id = %d' % fid)
            lyr.ResetReading()
            f = lyr.GetNextFeature()
            if f.GetFID() != fid or f.GetField('id') != fid:
                f.DumpReadable()
                pytest.fail(fid)

        lyr = ds.GetLayerByName('ogr_fgdb_20_simple')
        f = lyr.GetNextFeature()
        assert f.GetFID() == 2
        f = lyr.GetNextFeature()
        assert f.GetFID() == 3

        # Check attribute index
        lyr = ds.GetLayerByName('ogr_fgdb_20_indexes')
        for i in range(1000):
            fid = i + 2
            lyr.SetAttributeFilter('id = %d' % fid)
            lyr.ResetReading()
            f = lyr.GetNextFeature()
            assert f.GetFID() == fid

        # Check spatial index
        lyr.SetAttributeFilter(None)
        if update == 1:
            for i in range(1000):
                fid = i + 2
                lyr.SetSpatialFilterRect(i - 0.01, -0.01, i + 0.01, 0.01)
                lyr.ResetReading()
                f = lyr.GetNextFeature()
                assert f.GetFID() == fid

    # Insert new features
    ds = ogr.Open('tmp/test.gdb', update=1)
    lyr = ds.GetLayerByName('ogr_fgdb_20')
    for (fid, fgdb_fid) in [(10000000, 2050), (10000001, 2051), (8191, 2052), (16384, 2053)]:
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetFID(fid)
        f.SetField('id', fid)
        ret = lyr.CreateFeature(f)
        if ret != 0 or f.GetFID() != fid or str(lyr.GetMetadataItem(str(fid), 'MAP_OGR_FID_TO_FGDB_FID')) != str(fgdb_fid):
            f.DumpReadable()
            pytest.fail(lyr.GetMetadataItem(str(fid), 'MAP_OGR_FID_TO_FGDB_FID'))

    ds = None

    # Insert a new intermediate FIDs
    for (fid, fgdb_fid) in [(1000000, 10000002), (1000001, 10000002)]:

        ds = ogr.Open('tmp/test.gdb', update=1)
        lyr = ds.GetLayerByName('ogr_fgdb_20')
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetFID(fid)
        f.SetField('id', fid)
        ret = lyr.CreateFeature(f)
        if ret != 0 or f.GetFID() != fid or lyr.GetMetadataItem(str(fid), 'MAP_OGR_FID_TO_FGDB_FID') != str(fgdb_fid):
            f.DumpReadable()
            pytest.fail(lyr.GetMetadataItem(str(fid), 'MAP_OGR_FID_TO_FGDB_FID'))
        ds = None

    # Check consistency after re-opening
    gdal.ErrorReset()
    for update in [0, 1]:
        ds = ogr.Open('tmp/test.gdb', update=update)
        lyr = ds.GetLayerByName('ogr_fgdb_20')
        assert lyr.GetFeatureCount() == 16
        lyr.ResetReading()
        expected = [2, 3, 4, 7, 8, 9, 10, 12, 13, 2049, 8191, 16384, 1000000, 1000001, 10000000, 10000001]
        for fid in expected:
            f = lyr.GetNextFeature()
            assert gdal.GetLastErrorType() == 0
            assert f is not None, fid
            if f.GetFID() != fid or f.GetField('id') != fid:
                f.DumpReadable()
                pytest.fail(fid)

    # Simulate different errors when database reopening is done
    # to sync ids
    for case in ('CASE1', 'CASE2', 'CASE3'):
        try:
            shutil.rmtree("tmp/test2.gdb")
        except OSError:
            pass

        ds = fgdb_drv.CreateDataSource("tmp/test2.gdb")

        lyr = ds.CreateLayer('foo', geom_type=ogr.wkbNone)
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
            assert sql_lyr is not None, case
            ds.ReleaseResultSet(sql_lyr)
        else:
            assert sql_lyr is None, case

        # Everything will fail, but hopefully without crashing
        lyr.ResetReading()
        assert lyr.GetNextFeature() is None
        assert lyr.GetFeature(1) is None
        assert lyr.DeleteFeature(1) != 0
        assert lyr.CreateFeature(f) != 0
        assert lyr.SetFeature(f) != 0
        if case != 'CASE3':
            assert ds.CreateLayer('bar', geom_type=ogr.wkbNone) is None
            assert ds.DeleteLayer(0) != 0
        sql_lyr = ds.ExecuteSQL('SELECT * FROM foo')
        assert case == 'CASE3' or sql_lyr is None
        ds.ReleaseResultSet(sql_lyr)

        ds = None

    openfilegdb_drv.Deregister()
    # sys.exit(0)


###############################################################################
# Test M support


def test_ogr_fgdb_21(fgdb_drv, fgdb_sdk_1_4_or_later):
    # Fails on MULTIPOINT ZM
    if gdaltest.is_travis_branch('ubuntu_2004') or gdaltest.is_travis_branch('ubuntu_1804') or gdaltest.is_travis_branch('ubuntu_1604') or gdaltest.is_travis_branch('python3'):
        pytest.skip()

    try:
        shutil.rmtree("tmp/test.gdb")
    except OSError:
        pass

    ds = fgdb_drv.CreateDataSource('tmp/test.gdb')

    datalist = [["pointm", ogr.wkbPointM, "POINT M (1 2 3)"],
                ["pointzm", ogr.wkbPointM, "POINT ZM (1 2 3 4)"],
                ["multipointm", ogr.wkbMultiPointM, "MULTIPOINT M ((1 2 3),(4 5 6))"],
                ["multipointzm", ogr.wkbMultiPointZM, "MULTIPOINT ZM ((1 2 3 4),(5 6 7 8))"],
                ["linestringm", ogr.wkbLineStringM, "LINESTRING M (1 2 3,4 5 6)", "MULTILINESTRING M ((1 2 3,4 5 6))"],
                ["linestringzm", ogr.wkbLineStringZM, "LINESTRING ZM (1 2 3 4,5 6 7 8)", "MULTILINESTRING ZM ((1 2 3 4,5 6 7 8))"],
                ["multilinestringm", ogr.wkbMultiLineStringM, "MULTILINESTRING M ((1 2 3,4 5 6))"],
                ["multilinestringzm", ogr.wkbMultiLineStringZM, "MULTILINESTRING ZM ((1 2 3 4,5 6 7 8))"],
                ["polygonm", ogr.wkbPolygonM, "POLYGON M ((0 0 1,0 1 2,1 1 3,1 0 4,0 0 1))", "MULTIPOLYGON M (((0 0 1,0 1 2,1 1 3,1 0 4,0 0 1)))"],
                ["polygonzm", ogr.wkbPolygonZM, "POLYGON ZM ((0 0 1 -1,0 1 2 -2,1 1 3 -3,1 0 4 -4,0 0 1 -1))", "MULTIPOLYGON ZM (((0 0 1 -1,0 1 2 -2,1 1 3 -3,1 0 4 -4,0 0 1 -1)))"],
                ["multipolygonm", ogr.wkbMultiPolygonM, "MULTIPOLYGON M (((0 0 1,0 1 2,1 1 3,1 0 4,0 0 1)))"],
                ["multipolygonzm", ogr.wkbMultiPolygonZM, "MULTIPOLYGON ZM (((0 0 1 -1,0 1 2 -2,1 1 3 -3,1 0 4 -4,0 0 1 -1)))"],
                ["empty_polygonm", ogr.wkbPolygonM, 'POLYGON M EMPTY', None],
               ]

    srs = osr.SpatialReference()
    srs.SetFromUserInput("WGS84")

    for data in datalist:
        lyr = ds.CreateLayer(data[0], geom_type=data[1], srs=srs, options=[])

        feat = ogr.Feature(lyr.GetLayerDefn())
        # print(data[2])
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

        assert lyr.GetGeomType() == expected_geom_type, data
        feat = lyr.GetNextFeature()
        try:
            expected_wkt = data[3]
        except IndexError:
            expected_wkt = data[2]
        if expected_wkt is None:
            if feat.GetGeometryRef() is not None:
                feat.DumpReadable()
                pytest.fail(data)
        elif ogrtest.check_feature_geometry(feat, expected_wkt) != 0:
            feat.DumpReadable()
            pytest.fail(data)


###############################################################################
# Read curves


def test_ogr_fgdb_22():

    ds = ogr.Open('data/filegdb/curves.gdb')
    lyr = ds.GetLayerByName('line')
    ds_ref = ogr.Open('data/filegdb/curves_line.csv')
    lyr_ref = ds_ref.GetLayer(0)
    for f in lyr:
        f_ref = lyr_ref.GetNextFeature()
        if ogrtest.check_feature_geometry(f, f_ref.GetGeometryRef()) != 0:
            print(f.GetGeometryRef().ExportToWkt())
            pytest.fail(f_ref.GetGeometryRef().ExportToWkt())

    lyr = ds.GetLayerByName('polygon')
    ds_ref = ogr.Open('data/filegdb/curves_polygon.csv')
    lyr_ref = ds_ref.GetLayer(0)
    for f in lyr:
        f_ref = lyr_ref.GetNextFeature()
        if ogrtest.check_feature_geometry(f, f_ref.GetGeometryRef()) != 0:
            print(f.GetGeometryRef().ExportToWkt())
            pytest.fail(f_ref.GetGeometryRef().ExportToWkt())

    ds = ogr.Open('data/filegdb/curve_circle_by_center.gdb')
    lyr = ds.GetLayer(0)
    ds_ref = ogr.Open('data/filegdb/curve_circle_by_center.csv')
    lyr_ref = ds_ref.GetLayer(0)
    for f in lyr:
        f_ref = lyr_ref.GetNextFeature()
        if ogrtest.check_feature_geometry(f, f_ref.GetGeometryRef()) != 0:
            print(f.GetGeometryRef().ExportToWkt())
            pytest.fail(f_ref.GetGeometryRef().ExportToWkt())


###############################################################################
# Test opening '.'


def test_ogr_fgdb_23():

    os.chdir('data/filegdb/curves.gdb')
    ds = ogr.Open('.')
    os.chdir('../../..')
    assert ds is not None

###############################################################################
# Read polygons with M component where the M of the closing point is not the
# one of the starting point (#7017)


def test_ogr_fgdb_24():

    ds = ogr.Open('data/filegdb/filegdb_polygonzm_m_not_closing_with_curves.gdb')
    lyr = ds.GetLayer(0)
    ds_ref = ogr.Open('data/filegdb/filegdb_polygonzm_m_not_closing_with_curves.gdb.csv')
    lyr_ref = ds_ref.GetLayer(0)
    for f in lyr:
        f_ref = lyr_ref.GetNextFeature()
        if ogrtest.check_feature_geometry(f, f_ref.GetGeometryRef()) != 0:
            print(f.GetGeometryRef().ExportToIsoWkt())
            pytest.fail(f_ref.GetGeometryRef().ExportToIsoWkt())

    ds = ogr.Open('data/filegdb/filegdb_polygonzm_nan_m_with_curves.gdb')
    lyr = ds.GetLayer(0)
    ds_ref = ogr.Open('data/filegdb/filegdb_polygonzm_nan_m_with_curves.gdb.csv')
    lyr_ref = ds_ref.GetLayer(0)
    for f in lyr:
        f_ref = lyr_ref.GetNextFeature()
        if ogrtest.check_feature_geometry(f, f_ref.GetGeometryRef()) != 0:
            print(f.GetGeometryRef().ExportToIsoWkt())
            pytest.fail(f_ref.GetGeometryRef().ExportToIsoWkt())


###############################################################################
# Test selecting FID column with OGRSQL


def test_ogr_fgdb_25():

    ds = ogr.Open('data/filegdb/curves.gdb')
    sql_lyr = ds.ExecuteSQL('SELECT OBJECTID FROM polygon WHERE OBJECTID = 2')
    assert sql_lyr is not None
    f = sql_lyr.GetNextFeature()
    if f.GetFID() != 2:
        f.DumpReadable()
        pytest.fail()
    f = sql_lyr.GetNextFeature()
    assert f is None
    ds.ReleaseResultSet(sql_lyr)

    lyr = ds.GetLayerByName('polygon')
    lyr.SetAttributeFilter('OBJECTID = 2')
    f = lyr.GetNextFeature()
    if f.GetFID() != 2:
        f.DumpReadable()
        pytest.fail()


###############################################################################
# Test bugfix for https://github.com/OSGeo/gdal/issues/1369
# where a polygon with inner rings has its exterior ring with wrong orientation

def test_ogr_fgdb_weird_winding_order(fgdb_sdk_1_4_or_later):
    if not ogrtest.have_geos():
        pytest.skip()

    try:
        shutil.rmtree('tmp/roads_clip Drawing.gdb')
    except OSError:
        pass
    gdaltest.unzip('tmp', 'data/filegdb/weird_winding_order_fgdb.zip')

    ds = ogr.Open('tmp/roads_clip Drawing.gdb')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    g = f.GetGeometryRef()
    assert g.GetGeometryCount() == 1
    assert g.GetGeometryRef(0).GetGeometryCount() == 17

###############################################################################
# Test bugfix for https://github.com/OSGeo/gdal/issues/1369
# where a polygon with inner rings has its exterior ring with wrong orientation

def test_ogr_fgdb_utc_datetime():

    ds = ogr.Open('data/filegdb/testdatetimeutc.gdb')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    # Check that the timezone +00 is present
    assert f.GetFieldAsString('EditDate') == '2020/06/22 07:49:36+00'

###############################################################################
# Test field alias


def test_ogr_fgdb_alias(fgdb_drv):

    try:
        shutil.rmtree("tmp/alias.gdb")
    except OSError:
        pass

    srs = osr.SpatialReference()
    srs.SetFromUserInput("WGS84")

    ds = fgdb_drv.CreateDataSource('tmp/alias.gdb')
    lyr = ds.CreateLayer('test', srs=srs, geom_type=ogr.wkbPoint)
    fld_defn = ogr.FieldDefn('short_name', ogr.OFTInteger)
    fld_defn.SetAlternativeName('longer name')
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn('regular_name', ogr.OFTInteger)
    lyr.CreateField(fld_defn)
    ds = None

    ds = ogr.Open('tmp/alias.gdb')
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()
    assert lyr_defn.GetFieldDefn(0).GetAlternativeName() == 'longer name'
    assert lyr_defn.GetFieldDefn(1).GetAlternativeName() == ''

    try:
        shutil.rmtree("tmp/alias.gdb")
    except OSError:
        pass

###############################################################################
# Test field alias with ampersand character. Requires OpenFileGDB to be read back


def test_ogr_fgdb_alias_with_ampersand(fgdb_drv, openfilegdb_drv):

    try:
        shutil.rmtree("tmp/alias.gdb")
    except OSError:
        pass

    srs = osr.SpatialReference()
    srs.SetFromUserInput("WGS84")

    ds = fgdb_drv.CreateDataSource('tmp/alias.gdb')
    lyr = ds.CreateLayer('test', srs=srs, geom_type=ogr.wkbPoint)
    fld_defn = ogr.FieldDefn('short_name', ogr.OFTInteger)
    fld_defn.SetAlternativeName('longer & name')
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn('regular_name', ogr.OFTInteger)
    lyr.CreateField(fld_defn)
    ds = None

    openfilegdb_drv.Register()
    ds = fgdb_drv.Open('tmp/alias.gdb')
    openfilegdb_drv.Deregister()
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()
    assert lyr_defn.GetFieldDefn(0).GetAlternativeName() == 'longer & name'
    assert lyr_defn.GetFieldDefn(1).GetAlternativeName() == ''

    try:
        shutil.rmtree("tmp/alias.gdb")
    except OSError:
        pass


###############################################################################
# Test reading field domains

def _check_domains(ds):

    assert set(ds.GetFieldDomainNames()) == {'MedianType', 'RoadSurfaceType', 'SpeedLimit'}

    with gdaltest.error_handler():
        assert ds.GetFieldDomain('i_dont_exist') is None
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()

    fld_defn = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex('MaxSpeed'))
    assert fld_defn.GetDomainName() == 'SpeedLimit'

    domain = ds.GetFieldDomain('SpeedLimit')
    assert domain is not None
    assert domain.GetName() == 'SpeedLimit'
    assert domain.GetDescription() == 'The maximun speed of the road'
    assert domain.GetDomainType() == ogr.OFDT_RANGE
    assert domain.GetFieldType() == fld_defn.GetType()
    assert domain.GetFieldSubType() == fld_defn.GetSubType()
    assert domain.GetMinAsDouble() == 40.0
    assert domain.GetMaxAsDouble() == 100.0

    fld_defn = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex('MedianType'))
    assert fld_defn.GetDomainName() == 'MedianType'

    domain = ds.GetFieldDomain('MedianType')
    assert domain is not None
    assert domain.GetName() == 'MedianType'
    assert domain.GetDescription() == 'Road median types.'
    assert domain.GetDomainType() == ogr.OFDT_CODED
    assert domain.GetFieldType() == fld_defn.GetType()
    assert domain.GetFieldSubType() == fld_defn.GetSubType()
    assert domain.GetEnumeration() == {'0': 'None', '1': 'Cement'}


def test_ogr_fgdb_read_domains():

    ds = gdal.OpenEx('data/filegdb/Domains.gdb', gdal.OF_VECTOR)
    _check_domains(ds)


###############################################################################
# Test writing field domains


def test_ogr_fgdb_write_domains(fgdb_drv):

    out_dir = "tmp/test_ogr_fgdb_write_domains.gdb"
    try:
        shutil.rmtree(out_dir)
    except OSError:
        pass

    ds = gdal.VectorTranslate(out_dir, 'data/filegdb/Domains.gdb',
                              options = '-f FileGDB')
    _check_domains(ds)

    assert ds.TestCapability(ogr.ODsCAddFieldDomain) == 1
    assert ds.TestCapability(ogr.ODsCDeleteFieldDomain) == 1
    assert ds.TestCapability(ogr.ODsCUpdateFieldDomain) == 1

    with gdaltest.error_handler():
        assert not ds.DeleteFieldDomain('not_existing')

    domain = ogr.CreateCodedFieldDomain('unused_domain', 'desc', ogr.OFTInteger, ogr.OFSTNone, {1: "one", "2": None})
    assert ds.AddFieldDomain(domain)
    assert ds.DeleteFieldDomain('unused_domain')
    domain = ds.GetFieldDomain('unused_domain')
    assert domain is None

    domain = ogr.CreateRangeFieldDomain('SpeedLimit', 'desc', ogr.OFTInteger, ogr.OFSTNone, 1, True, 2, True)
    assert ds.UpdateFieldDomain(domain)

    ds = None

    ds = gdal.OpenEx(out_dir, allowed_drivers = ['FileGDB'])
    domain = ds.GetFieldDomain('SpeedLimit')
    assert domain.GetDescription() == 'desc'
    ds = None

    try:
        shutil.rmtree(out_dir)
    except OSError:
        pass


###############################################################################
# Test reading layer hierarchy


def test_ogr_fgdb_read_layer_hierarchy():

    if False:
        # Test dataset produced with:
        from osgeo import ogr, osr
        srs = osr.SpatialReference()
        srs.SetFromUserInput("WGS84")
        ds = ogr.GetDriverByName('FileGDB').CreateDataSource('featuredataset.gdb')
        ds.CreateLayer('fd1_lyr1', srs=srs, geom_type=ogr.wkbPoint, options=['FEATURE_DATASET=fd1'])
        ds.CreateLayer('fd1_lyr2', srs=srs, geom_type=ogr.wkbPoint, options=['FEATURE_DATASET=fd1'])
        srs2 = osr.SpatialReference()
        srs2.ImportFromEPSG(32631)
        ds.CreateLayer('standalone', srs=srs2, geom_type=ogr.wkbPoint)
        srs3 = osr.SpatialReference()
        srs3.ImportFromEPSG(32632)
        ds.CreateLayer('fd2_lyr', srs=srs3, geom_type=ogr.wkbPoint, options=['FEATURE_DATASET=fd2'])

    ds = gdal.OpenEx('data/filegdb/featuredataset.gdb')
    rg = ds.GetRootGroup()

    assert rg.GetGroupNames() == ['fd1', 'fd2']
    assert rg.OpenGroup('not_existing') is None

    fd1 = rg.OpenGroup('fd1')
    assert fd1 is not None
    assert fd1.GetVectorLayerNames() == ['fd1_lyr1', 'fd1_lyr2']
    assert fd1.OpenVectorLayer('not_existing') is None
    assert fd1.GetGroupNames() is None

    fd1_lyr1 = fd1.OpenVectorLayer('fd1_lyr1')
    assert fd1_lyr1 is not None
    assert fd1_lyr1.GetName() == 'fd1_lyr1'

    fd1_lyr2 = fd1.OpenVectorLayer('fd1_lyr2')
    assert fd1_lyr2 is not None
    assert fd1_lyr2.GetName() == 'fd1_lyr2'

    fd2 = rg.OpenGroup('fd2')
    assert fd2 is not None
    assert fd2.GetVectorLayerNames() == ['fd2_lyr']
    fd2_lyr = fd2.OpenVectorLayer('fd2_lyr')
    assert fd2_lyr is not None

    assert rg.GetVectorLayerNames() == ['standalone']
    standalone = rg.OpenVectorLayer('standalone')
    assert standalone is not None


###############################################################################
# Test renaming a layer


@pytest.mark.parametrize("options", [ [], ['FEATURE_DATASET=fd1'] ])
def test_ogr_fgdb_rename_layer(fgdb_drv, options):

    try:
        shutil.rmtree("tmp/rename.gdb")
    except OSError:
        pass

    srs4326 = osr.SpatialReference()
    srs4326.ImportFromEPSG(4326)

    ds = fgdb_drv.CreateDataSource('tmp/rename.gdb')
    ds.CreateLayer('other_layer', geom_type=ogr.wkbNone)
    lyr = ds.CreateLayer('foo', geom_type=ogr.wkbPoint, srs=srs4326, options=options)
    assert lyr.TestCapability(ogr.OLCRename) == 1
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (1 2)'))
    lyr.CreateFeature(f)

    assert lyr.Rename('bar') == ogr.OGRERR_NONE
    assert lyr.GetDescription() == 'bar'
    assert lyr.GetLayerDefn().GetName() == 'bar'

    with gdaltest.error_handler():
        assert lyr.Rename('bar') != ogr.OGRERR_NONE

    with gdaltest.error_handler():
        assert lyr.Rename('other_layer') != ogr.OGRERR_NONE

    # Second renaming
    assert lyr.Rename('baz') == ogr.OGRERR_NONE
    assert lyr.GetDescription() == 'baz'
    assert lyr.GetLayerDefn().GetName() == 'baz'

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef() is not None

    ds = None

    ds = ogr.Open('tmp/rename.gdb')
    lyr = ds.GetLayerByName('baz')
    assert lyr is not None, [ ds.GetLayer(i).GetName() for i in range(ds.GetLayerCount()) ]

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef() is not None

    ds = None

    try:
        shutil.rmtree("tmp/rename.gdb")
    except OSError:
        pass


###############################################################################
# Test that non-spatial tables which are not present in GDB_Items are listed
# see https://github.com/OSGeo/gdal/issues/4463


def test_ogr_filegdb_non_spatial_table_outside_gdb_items(openfilegdb_drv, fgdb_drv):
    openfilegdb_drv.Deregister()
    fgdb_drv.Deregister()

    # Force FileGDB first
    fgdb_drv.Register()
    openfilegdb_drv.Register()

    ds = ogr.Open('data/filegdb/table_outside_gdbitems.gdb')
    assert ds is not None
    assert ds.GetDriver().GetName() == 'FileGDB'

    assert ds.GetLayerCount() == 3, 'did not get expected layer count'
    layer_names = set(ds.GetLayer(i).GetName() for i in range(ds.GetLayerCount()))
    assert layer_names == {'aquaduct', 'flat_table1', 'flat_table2'}


###############################################################################
# Test reading .gdb where the CRS in the XML definition of the feature
# table is not consistent with the one of the feature dataset


def test_ogr_filegdb_inconsistent_crs_feature_dataset_and_feature_table():
    ds = ogr.Open('data/filegdb/inconsistent_crs_feature_dataset_and_feature_table.gdb')
    assert ds is not None
    lyr = ds.GetLayer(0)
    srs = lyr.GetSpatialRef()
    assert srs is not None
    assert srs.GetAuthorityCode(None) == '4326'


###############################################################################
# Test reading .gdb with LengthFieldName / AreaFieldName


def test_ogr_filegdb_shape_length_shape_area_as_default_in_field_defn(fgdb_drv):
    ds = ogr.Open('data/filegdb/filegdb_polygonzm_m_not_closing_with_curves.gdb')
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()
    assert lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex('Shape_Area')).GetDefault() == 'FILEGEODATABASE_SHAPE_AREA'
    assert lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex('Shape_Length')).GetDefault() == 'FILEGEODATABASE_SHAPE_LENGTH'


###############################################################################
# Test explicit CREATE_SHAPE_AREA_AND_LENGTH_FIELDS=YES option


def test_ogr_filegdb_CREATE_SHAPE_AREA_AND_LENGTH_FIELDS_explicit(fgdb_drv):

    dirname = 'tmp/test_ogr_filegdb_CREATE_SHAPE_AREA_AND_LENGTH_FIELDS_explicit.gdb'
    ds = fgdb_drv.CreateDataSource(dirname)

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)

    lyr = ds.CreateLayer('line', srs=srs, geom_type=ogr.wkbLineString, options=['CREATE_SHAPE_AREA_AND_LENGTH_FIELDS=YES'])
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('LINESTRING(0 0,2 0)'))
    lyr.CreateFeature(f)

    lyr = ds.CreateLayer('area', srs=srs, geom_type=ogr.wkbPolygon, options=['CREATE_SHAPE_AREA_AND_LENGTH_FIELDS=YES'])
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POLYGON((0 0,0 1,1 1,1 0,0 0))'))
    lyr.CreateFeature(f)

    ds = None

    ds = ogr.Open(dirname)

    lyr = ds.GetLayerByName('line')
    f = lyr.GetNextFeature()
    lyr_defn = lyr.GetLayerDefn()
    assert lyr_defn.GetFieldIndex('Shape_Length') >= 0
    assert lyr_defn.GetFieldIndex('Shape_Area') < 0
    assert lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex('Shape_Length')).GetDefault() == 'FILEGEODATABASE_SHAPE_LENGTH'
    assert f['Shape_Length'] == 2

    lyr = ds.GetLayerByName('area')
    f = lyr.GetNextFeature()
    lyr_defn = lyr.GetLayerDefn()
    assert lyr_defn.GetFieldIndex('Shape_Length') >= 0
    assert lyr_defn.GetFieldIndex('Shape_Area') >= 0
    assert lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex('Shape_Area')).GetDefault() == 'FILEGEODATABASE_SHAPE_AREA'
    assert lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex('Shape_Length')).GetDefault() == 'FILEGEODATABASE_SHAPE_LENGTH'
    assert f['Shape_Length'] == 4
    assert f['Shape_Area'] == 1

    ds = None

    try:
        shutil.rmtree(dirname)
    except OSError:
        pass


###############################################################################
# Test explicit CREATE_SHAPE_AREA_AND_LENGTH_FIELDS=YES option


def test_ogr_filegdb_CREATE_SHAPE_AREA_AND_LENGTH_FIELDS_implicit(fgdb_drv):

    dirname = 'tmp/test_ogr_filegdb_CREATE_SHAPE_AREA_AND_LENGTH_FIELDS_implicit.gdb'
    gdal.VectorTranslate(dirname, 'data/filegdb/filegdb_polygonzm_m_not_closing_with_curves.gdb', options = '-f FileGDB -unsetfid -fid 1')

    ds = ogr.Open(dirname)
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()
    assert lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex('Shape_Area')).GetDefault() == 'FILEGEODATABASE_SHAPE_AREA'
    assert lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex('Shape_Length')).GetDefault() == 'FILEGEODATABASE_SHAPE_LENGTH'

    ds = None

    try:
        shutil.rmtree(dirname)
    except OSError:
        pass

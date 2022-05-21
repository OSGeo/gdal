#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  OpenFileGDB driver testing.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2014, Even Rouault <even dot rouault at spatialys.com>
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

import gdaltest
import ogrtest
from osgeo import gdal
from osgeo import ogr
from osgeo import osr
import pytest

ogrtest.openfilegdb_datalist = [["none", ogr.wkbNone, None],
                                ["point", ogr.wkbPoint, "POINT (1 2)"],
                                ["multipoint", ogr.wkbMultiPoint, "MULTIPOINT (1 2,3 4)"],
                                ["linestring", ogr.wkbLineString, "LINESTRING (1 2,3 4)", "MULTILINESTRING ((1 2,3 4))"],
                                ["multilinestring", ogr.wkbMultiLineString, "MULTILINESTRING ((1 2,3 4))"],
                                ["multilinestring_multipart", ogr.wkbMultiLineString, "MULTILINESTRING ((1 2,3 4),(5 6,7 8))"],
                                ["polygon", ogr.wkbPolygon, "POLYGON ((0 0,0 1,1 1,1 0,0 0))", "MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0)))"],
                                ["multipolygon", ogr.wkbMultiPolygon, "MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0),(0.25 0.25,0.75 0.25,0.75 0.75,0.25 0.75,0.25 0.25)),((2 0,2 1,3 1,3 0,2 0)))"],
                                ["point25D", ogr.wkbPoint25D, "POINT (1 2 3)"],
                                ["multipoint25D", ogr.wkbMultiPoint25D, "MULTIPOINT (1 2 -10,3 4 -20)"],
                                ["linestring25D", ogr.wkbLineString25D, "LINESTRING (1 2 -10,3 4 -20)", "MULTILINESTRING ((1 2 -10,3 4 -20))"],
                                ["multilinestring25D", ogr.wkbMultiLineString25D, "MULTILINESTRING ((1 2 -10,3 4 -20))"],
                                ["multilinestring25D_multipart", ogr.wkbMultiLineString25D, "MULTILINESTRING ((1 2 -10,3 4 -20),(5 6 -30,7 8 -40))"],
                                ["polygon25D", ogr.wkbPolygon25D, "POLYGON ((0 0 -10,0 1 -10,1 1 -10,1 0 -10,0 0 -10))", "MULTIPOLYGON (((0 0 -10,0 1 -10,1 1 -10,1 0 -10,0 0 -10)))"],
                                ["multipolygon25D", ogr.wkbMultiPolygon25D, "MULTIPOLYGON (((0 0 -10,0 1 -10,1 1 -10,1 0 -10,0 0 -10)))"],
                                ["multipatch", ogr.wkbGeometryCollection25D, "GEOMETRYCOLLECTION Z (TIN Z (((0.0 0.0 0,0.0 1.0 0,1.0 0.0 0,0.0 0.0 0)),((0.0 1.0 0,1.0 0.0 0,1.0 1.0 0,0.0 1.0 0))),TIN Z (((10.0 0.0 0,10.0 1.0 0,11.0 0.0 0,10.0 0.0 0)),((10.0 0.0 0,11.0 0.0 0,10.0 -1.0 0,10.0 0.0 0))),TIN Z (((5.0 0.0 0,5.0 1.0 0,6.0 0.0 0,5.0 0.0 0))),MULTIPOLYGON Z (((100.0 0.0 0,100.0 1.0 0,101.0 1.0 0,101.0 0.0 0,100.0 0.0 0),(100.25 0.25 0,100.75 0.25 0,100.75 0.75 0,100.75 0.25 0,100.25 0.25 0))))"],
                                ["null_polygon", ogr.wkbPolygon, None],
                                ["empty_polygon", ogr.wkbPolygon, "POLYGON EMPTY", None],
                                ["empty_multipoint", ogr.wkbMultiPoint, "MULTIPOINT EMPTY", None],
                               ]


ogrtest.openfilegdb_datalist_m = [["pointm", ogr.wkbPointM, "POINT M (1 2 3)"],
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


@pytest.fixture(scope="module", autouse=True)
def setup_driver():
    # remove FileGDB driver before running tests
    filegdb_driver = ogr.GetDriverByName('FileGDB')
    if filegdb_driver is not None:
        filegdb_driver.Deregister()

    yield

    if filegdb_driver is not None:
        print('Reregistering FileGDB driver')
        filegdb_driver.Register()


@pytest.fixture()
def ogrsf_path():
    import test_cli_utilities
    path = test_cli_utilities.get_test_ogrsf_path()
    if path is None:
        pytest.skip('ogrsf test utility not found')

    return path


@pytest.fixture(params=[{'src': 'data/filegdb/testopenfilegdb.gdb.zip', 'version_10': True},
                        {'src': 'data/filegdb/testopenfilegdb92.gdb.zip', 'version_10': False},
                        {'src': 'data/filegdb/testopenfilegdb93.gdb.zip', 'version_10': False}])
def gdb_source(request):
    return request.param


###############################################################################
# Make test data


def ogr_openfilegdb_make_test_data():

    try:
        shutil.rmtree("data/filegdb/testopenfilegdb.gdb")
    except OSError:
        pass
    ds = ogrtest.fgdb_drv.CreateDataSource('data/filegdb/testopenfilegdb.gdb')

    srs = osr.SpatialReference()
    srs.SetFromUserInput("WGS84")

    options = ['COLUMN_TYPES=smallint=esriFieldTypeSmallInteger,float=esriFieldTypeSingle,guid=esriFieldTypeGUID,xml=esriFieldTypeXML']

    for data in ogrtest.openfilegdb_datalist:
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
        lyr.CreateField(ogr.FieldDefn("nullint", ogr.OFTInteger))
        lyr.CreateField(ogr.FieldDefn("binary2", ogr.OFTBinary))

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
            feat.SetFieldBinaryFromHexString("binary", "00FF7F")
            feat.SetField("xml", "<foo></foo>")
            feat.SetFieldBinaryFromHexString("binary2", "123456")
            lyr.CreateFeature(feat)

        if data[0] == 'none':
            # Create empty feature
            feat = ogr.Feature(lyr.GetLayerDefn())
            lyr.CreateFeature(feat)

    if False:  # pylint: disable=using-constant-test
        lyr = ds.CreateLayer('sparse_layer', geom_type=ogr.wkbPoint)
        for i in range(4096):
            feat = ogr.Feature(lyr.GetLayerDefn())
            lyr.CreateFeature(feat)
            lyr.DeleteFeature(feat.GetFID())
        feat = ogr.Feature(lyr.GetLayerDefn())
        lyr.CreateFeature(feat)

    if True:  # pylint: disable=using-constant-test
        lyr = ds.CreateLayer('big_layer', geom_type=ogr.wkbNone)
        lyr.CreateField(ogr.FieldDefn("real", ogr.OFTReal))
        gdal.SetConfigOption('FGDB_BULK_LOAD', 'YES')
        # for i in range(340*341+1):
        for i in range(340 + 1):
            feat = ogr.Feature(lyr.GetLayerDefn())
            feat.SetField(0, i % 4)
            lyr.CreateFeature(feat)
        gdal.SetConfigOption('FGDB_BULK_LOAD', None)

    if True:  # pylint: disable=using-constant-test
        lyr = ds.CreateLayer('hole', geom_type=ogr.wkbPoint, srs=None)
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
            feat.SetField('str', 'fid%d' % (4 + i))
            feat.SetField('int0', 4 + i)
            feat.SetField('str2', '                                            ')
            lyr.CreateFeature(feat)
        feat = None

        for i in range(8):
            lyr.CreateField(ogr.FieldDefn('int%d' % (i + 1), ogr.OFTInteger))

        lyr.DeleteFeature(1)

        feat = ogr.Feature(lyr.GetLayerDefn())
        lyr.CreateFeature(feat)
        feat = None

        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetField('str', 'fid13')
        lyr.CreateFeature(feat)
        feat = None

    if True:  # pylint: disable=using-constant-test
        lyr = ds.CreateLayer('no_field', geom_type=ogr.wkbNone, srs=None)
        for i in range(5):
            feat = ogr.Feature(lyr.GetLayerDefn())
            lyr.CreateFeature(feat)
            feat = None

    if True:  # pylint: disable=using-constant-test
        lyr = ds.CreateLayer('several_polygons', geom_type=ogr.wkbPolygon, srs=None)
        for i in range(3):
            for j in range(3):
                feat = ogr.Feature(lyr.GetLayerDefn())
                x1 = 2 * i
                x2 = 2 * i + 1
                y1 = 2 * j
                y2 = 2 * j + 1
                geom = ogr.CreateGeometryFromWkt('POLYGON((%d %d,%d %d,%d %d,%d %d,%d %d))' % (x1, y1, x1, y2, x2, y2, x2, y1, x1, y1))
                feat.SetGeometry(geom)
                lyr.CreateFeature(feat)
                feat = None

    if True:  # pylint: disable=using-constant-test
        lyr = ds.CreateLayer('testnotnullable', geom_type=ogr.wkbPoint, srs=None, options=['GEOMETRY_NULLABLE=NO'])
        field_defn = ogr.FieldDefn('field_not_nullable', ogr.OFTString)
        field_defn.SetNullable(0)
        lyr.CreateField(field_defn)
        field_defn = ogr.FieldDefn('field_nullable', ogr.OFTString)
        lyr.CreateField(field_defn)
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField('field_not_nullable', 'not_null')
        f.SetGeomFieldDirectly('geomfield_not_nullable', ogr.CreateGeometryFromWkt('POINT(0 0)'))
        lyr.CreateFeature(f)
        f = None

    for data in ogrtest.openfilegdb_datalist_m:
        lyr = ds.CreateLayer(data[0], geom_type=data[1], srs=srs, options=[])

        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetGeometry(ogr.CreateGeometryFromWkt(data[2]))
        lyr.CreateFeature(feat)

    for fld_name in ['id', 'str', 'smallint', 'int', 'float', 'real', 'adate', 'guid', 'nullint']:
        ds.ExecuteSQL('CREATE INDEX idx_%s ON point(%s)' % (fld_name, fld_name))
    ds.ExecuteSQL('CREATE INDEX idx_id ON none(id)')
    ds.ExecuteSQL('CREATE INDEX idx_real ON big_layer(real)')
    ds = None

    gdal.Unlink('data/filegdb/testopenfilegdb.gdb.zip')
    os.chdir('data/filegdb')
    os.system('zip -r -9 testopenfilegdb.gdb.zip testopenfilegdb.gdb')
    os.chdir('../..')
    shutil.rmtree('data/filegdb/testopenfilegdb.gdb')

###############################################################################
# Basic tests


def test_ogr_openfilegdb_1(gdb_source):
    filename = gdb_source['src']
    version10 = gdb_source['version_10']

    srs = osr.SpatialReference()
    srs.SetFromUserInput("WGS84")

    ds = ogr.Open(filename)

    for data in ogrtest.openfilegdb_datalist:
        lyr_name = data[0]
        if lyr_name == 'multilinestring_multipart' and not version10:
            continue
        if lyr_name == 'multilinestring25D_multipart' and not version10:
            continue
        lyr = ds.GetLayerByName(lyr_name)
        expected_geom_type = data[1]
        if expected_geom_type == ogr.wkbLineString:
            expected_geom_type = ogr.wkbMultiLineString
        elif expected_geom_type == ogr.wkbLineString25D:
            expected_geom_type = ogr.wkbMultiLineString25D
        elif expected_geom_type == ogr.wkbPolygon:
            expected_geom_type = ogr.wkbMultiPolygon
        elif expected_geom_type == ogr.wkbPolygon25D:
            expected_geom_type = ogr.wkbMultiPolygon25D
        assert lyr.GetGeomType() == expected_geom_type, lyr.GetName()
        assert expected_geom_type is ogr.wkbNone or lyr.GetLayerDefn().GetGeomFieldDefn(0).IsNullable() == 1
        assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('str')).GetWidth() == 0
        assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('smallint')).GetSubType() == ogr.OFSTInt16
        assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('float')).GetSubType() == ogr.OFSTFloat32
        if data[1] != ogr.wkbNone:
            assert lyr.GetSpatialRef().IsSame(srs, options = ['IGNORE_DATA_AXIS_TO_SRS_AXIS_MAPPING=YES']) == 1
        feat = lyr.GetNextFeature()
        if data[1] != ogr.wkbNone:
            try:
                expected_wkt = data[3]
            except IndexError:
                expected_wkt = data[2]
            geom = feat.GetGeometryRef()
            if geom:
                geom = geom.ExportToWkt()
            if geom != expected_wkt and ogrtest.check_feature_geometry(feat, expected_wkt) == 1:
                feat.DumpReadable()
                pytest.fail(expected_wkt)

        if feat.GetField('id') != 1 or \
           feat.GetField('smallint') != -13 or \
           feat.GetField('int') != 123 or \
           feat.GetField('float') != 1.5 or \
           feat.GetField('real') != 4.56 or \
           feat.GetField('adate') != "2013/12/26 12:34:56" or \
           feat.GetField('guid') != "{12345678-9ABC-DEF0-1234-567890ABCDEF}" or \
           (version10 and feat.GetField('xml') != "<foo></foo>") or \
           feat.GetField('binary') != "00FF7F" or \
           feat.GetField('binary2') != "123456":
            feat.DumpReadable()
            pytest.fail()

        if version10:
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

    if version10:
        sql_lyr = ds.ExecuteSQL("GetLayerDefinition foo")
        assert sql_lyr is None

        sql_lyr = ds.ExecuteSQL("GetLayerMetadata foo")
        assert sql_lyr is None

    if version10:
        for data in ogrtest.openfilegdb_datalist_m:
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

    ds = None


###############################################################################
# Run test_ogrsf


@pytest.fixture()
def ogrsf_run(ogrsf_path, gdb_source):
    ret = gdaltest.runexternal(ogrsf_path + ' -ro ' + gdb_source['src'])

    success = 'INFO' in ret and 'ERROR' not in ret
    assert success


def test_ogr_openfilegdb_2(ogrsf_run, gdb_source):
    pass


###############################################################################
# Open a .gdbtable directly


def test_ogr_openfilegdb_3():

    ds = ogr.Open('/vsizip/data/filegdb/testopenfilegdb.gdb.zip/testopenfilegdb.gdb/a00000009.gdbtable')
    assert ds.GetLayerCount() == 1
    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'none'

    # Try opening a system table
    lyr = ds.GetLayerByName('GDB_SystemCatalog')
    assert lyr.GetName() == 'GDB_SystemCatalog'
    feat = lyr.GetNextFeature()
    assert feat.GetField('Name') == 'GDB_SystemCatalog'
    lyr = ds.GetLayerByName('GDB_SystemCatalog')
    assert lyr.GetName() == 'GDB_SystemCatalog'

    ds = None

###############################################################################
# Test use of attribute indexes


def test_ogr_openfilegdb_4():

    ds = ogr.Open('/vsizip/data/filegdb/testopenfilegdb.gdb.zip/testopenfilegdb.gdb')

    lyr = ds.GetLayerByName('point')
    tests = [('id = 1', [1]),
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
             ('id >= 1', [1, 2, 3, 4, 5]),
             ('id > 0', [1, 2, 3, 4, 5]),
             ('0 < id', [1, 2, 3, 4, 5]),
             ('id <= 5', [1, 2, 3, 4, 5]),
             ('id < 6', [1, 2, 3, 4, 5]),
             ('id <> 0', [1, 2, 3, 4, 5]),
             ('id IS NOT NULL', [1, 2, 3, 4, 5]),
             ('id IS NULL', []),
             ('nullint IS NOT NULL', []),
             ('nullint IS NULL', [1, 2, 3, 4, 5]),
             ("str = 'foo_e'", []),
             ("str = 'foo_é'", [1, 2, 3, 4, 5]),
             ("str <= 'foo_é'", [1, 2, 3, 4, 5]),
             ("str >= 'foo_é'", [1, 2, 3, 4, 5]),
             ("str <> 'foo_é'", []),
             ("str < 'foo_é'", []),
             ("str > 'foo_é'", []),
             ('smallint = -13', [1, 2, 3, 4, 5]),
             ('smallint <= -13', [1, 2, 3, 4, 5]),
             ('smallint >= -13', [1, 2, 3, 4, 5]),
             ('smallint < -13', []),
             ('smallint > -13', []),
             ('int = 123', [1, 2, 3, 4, 5]),
             ('int <= 123', [1, 2, 3, 4, 5]),
             ('int >= 123', [1, 2, 3, 4, 5]),
             ('int < 123', []),
             ('int > 123', []),
             ('float = 1.5', [1, 2, 3, 4, 5]),
             ('float <= 1.5', [1, 2, 3, 4, 5]),
             ('float >= 1.5', [1, 2, 3, 4, 5]),
             ('float < 1.5', []),
             ('float > 1.5', []),
             ('real = 4.56', [1, 2, 3, 4, 5]),
             ('real <= 4.56', [1, 2, 3, 4, 5]),
             ('real >= 4.56', [1, 2, 3, 4, 5]),
             ('real < 4.56', []),
             ('real > 4.56', []),
             ("adate = '2013/12/26 12:34:56'", [1, 2, 3, 4, 5]),
             ("adate <= '2013/12/26 12:34:56'", [1, 2, 3, 4, 5]),
             ("adate >= '2013/12/26 12:34:56'", [1, 2, 3, 4, 5]),
             ("adate < '2013/12/26 12:34:56'", []),
             ("adate > '2013/12/26 12:34:56'", []),
             ("guid = '{12345678-9ABC-DEF0-1234-567890ABCDEF}'", [1, 2, 3, 4, 5]),
             ("guid <= '{12345678-9ABC-DEF0-1234-567890ABCDEF}'", [1, 2, 3, 4, 5]),
             ("guid >= '{12345678-9ABC-DEF0-1234-567890ABCDEF}'", [1, 2, 3, 4, 5]),
             ("guid < '{12345678-9ABC-DEF0-1234-567890ABCDEF}'", []),
             ("guid > '{12345678-9ABC-DEF0-1234-567890ABCDEF}'", []),
             ("guid = '{'", []),
             ("guid > '{'", [1, 2, 3, 4, 5]),
             ("NOT(id = 1)", [2, 3, 4, 5]),
             ("id = 1 OR id = -1", [1]),
             ("id = -1 OR id = 1", [1]),
             ("id = 1 OR id = 1", [1]),
             ("id = 1 OR id = 2", [1, 2]),  # exclusive branches
             ("id < 3 OR id > 3", [1, 2, 4, 5]),  # exclusive branches
             ("id > 3 OR id < 3", [1, 2, 4, 5]),  # exclusive branches
             ("id <= 3 OR id >= 4", [1, 2, 3, 4, 5]),  # exclusive branches
             ("id >= 4 OR id <= 3", [1, 2, 3, 4, 5]),  # exclusive branches
             ("id < 3 OR id >= 3", [1, 2, 3, 4, 5]),
             ("id <= 3 OR id >= 3", [1, 2, 3, 4, 5]),
             ("id <= 5 OR id >= 1", [1, 2, 3, 4, 5]),
             ("id <= 1.5 OR id >= 2", [1, 2, 3, 4, 5]),
             ('id IS NULL OR id IS NOT NULL', [1, 2, 3, 4, 5]),
             ('float < 1.5 OR float > 1.5', []),
             ('float <= 1.5 OR float >= 1.5', [1, 2, 3, 4, 5]),
             ('float < 1.5 OR float > 2', []),
             ('float < 1 OR float > 2.5', []),
             ("str < 'foo_é' OR str > 'z'", []),
             ("adate < '2013/12/26 12:34:56' OR adate > '2014/01/01'", []),
             ("id = 1 AND id = -1", []),
             ("id = -1 AND id = 1", []),
             ("id = 1 AND id = 1", [1]),
             ("id = 1 AND id = 2", []),
             ("id <= 5 AND id >= 1", [1, 2, 3, 4, 5]),
             ("id <= 3 AND id >= 3", [3]),
             ("id = 1 AND float = 1.5", [1]),
             ("id BETWEEN 1 AND 5", [1, 2, 3, 4, 5]),
             ("id IN (1)", [1]),
             ("id IN (5,4,3,2,1)", [1, 2, 3, 4, 5]),
             ('fid = 1', [1], 0),  # no index used
             ('fid BETWEEN 1 AND 1', [1], 0),  # no index used
             ('fid IN (1)', [1], 0),  # no index used
             ('fid IS NULL', [], 0),  # no index used
             ('fid IS NOT NULL', [1, 2, 3, 4, 5], 0),  # no index used
             ("xml <> ''", [1, 2, 3, 4, 5], 0),  # no index used
             ("id = 1 AND xml <> ''", [1], 1),  # index partially used
             ("xml <> '' AND id = 1", [1], 1),  # index partially used
             ("NOT(id = 1 AND xml <> '')", [2, 3, 4, 5], 0),  # no index used
             ("id = 1 OR xml <> ''", [1, 2, 3, 4, 5], 0),  # no index used
             ('id = id', [1, 2, 3, 4, 5], 0),  # no index used
             ('id = 1 + 0', [1], 0),  # no index used (currently...)
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
        assert attr_index_use == expected_attr_index_use, \
            (where_clause, fids, expected_attr_index_use)
        assert lyr.GetFeatureCount() == len(fids), (where_clause, fids)
        for fid in fids:
            feat = lyr.GetNextFeature()
            assert feat.GetFID() == fid, (where_clause, fids)
        feat = lyr.GetNextFeature()
        assert feat is None, (where_clause, fids)

    lyr = ds.GetLayerByName('none')
    tests = [('id = 1', [1]),
             ('id IS NULL', [6]),
             ('id IS NOT NULL', [1, 2, 3, 4, 5]),
             ('id IS NULL OR id IS NOT NULL', [1, 2, 3, 4, 5, 6]),
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
        assert attr_index_use == expected_attr_index_use, \
            (where_clause, fids, expected_attr_index_use)
        assert lyr.GetFeatureCount() == len(fids), (where_clause, fids)
        for fid in fids:
            feat = lyr.GetNextFeature()
            assert feat.GetFID() == fid, (where_clause, fids)
        feat = lyr.GetNextFeature()
        assert feat is None, (where_clause, fids)

    lyr = ds.GetLayerByName('big_layer')
    tests = [('real = 0', 86, 1),
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
        assert lyr.GetFeatureCount() == count, (where_clause, count)
        for i in range(count):
            feat = lyr.GetNextFeature()
            assert (not (feat is None or \
               (start is not None and
                    feat.GetFID() != i * 4 + start))), (where_clause, count)
        feat = lyr.GetNextFeature()
        assert feat is None, (where_clause, count)

    ds = None

###############################################################################
# Test opening an unzipped dataset


def test_ogr_openfilegdb_5():

    try:
        shutil.rmtree('tmp/testopenfilegdb.gdb')
    except OSError:
        pass
    try:
        gdaltest.unzip('tmp/', 'data/filegdb/testopenfilegdb.gdb.zip')
    except OSError:
        pytest.skip()
    try:
        os.stat('tmp/testopenfilegdb.gdb')
    except OSError:
        pytest.skip()

    ds = ogr.Open('tmp/testopenfilegdb.gdb')
    assert ds is not None

###############################################################################
# Test special SQL processing for min/max/count/sum/avg values


def test_ogr_openfilegdb_6():

    ds = ogr.Open('data/filegdb/testopenfilegdb.gdb.zip')

    # With indices
    sql_lyr = ds.ExecuteSQL("select min(id), max(id), count(id), sum(id), avg(id), min(str), min(smallint), "
                            "avg(smallint), min(float), avg(float), min(real), avg(real), min(adate), avg(adate), min(guid), min(nullint), avg(nullint) from point")
    assert sql_lyr is not None
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
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)

    # No index
    sql_lyr = ds.ExecuteSQL("select min(id),  avg(id) from multipoint")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField('MIN_id') != 1 or \
       feat.GetField('AVG_id') != 3.0:
        feat.DumpReadable()
        ds.ReleaseResultSet(sql_lyr)
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)

###############################################################################
# Test special SQL processing for ORDER BY


def test_ogr_openfilegdb_7():

    ds = ogr.Open('data/filegdb/testopenfilegdb.gdb.zip')

    tests = [  # Optimized:
        ("select * from point order by id", 5, 1, 1),
        ("select id, str from point order by id desc", 5, 5, 1),
        ("select * from point where id = 1 order by id", 1, 1, 1),
        ("select * from big_layer order by real", 86 + 3 * 85, 1, 1),
        ("select * from big_layer order by real desc", 86 + 3 * 85, 4 * 85, 1),
        # Invalid :
        ("select foo from", None, None, None),
        ("select foo from bar", None, None, None),
        ("select * from point order by foo", None, None, None),
        # Non-optimized :
        ("select * from point order by xml", None, None, 0),
        ("select fid from point order by id", None, None, 0),
        ("select cast(id as float) from point order by id", None, None, 0),
        ("select distinct id from point order by id", None, None, 0),
        ("select 1 from point order by id", None, None, 0),
        ("select count(*) from point order by id", None, None, 0),
        ("select * from point order by nullint", None, None, 0),
        ("select * from point where id = 1 or id = 2 order by id", None, None, 0),
        ("select * from point where id = 1 order by id, float", None, None, 0),
        ("select * from point where float > 0 order by id", None, None, 0),
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
                pytest.fail(sql, feat_count, first_fid)
            continue
        assert sql_lyr is not None, (sql, feat_count, first_fid)
        if expected_optimized:
            if sql_lyr.GetFeatureCount() != feat_count:
                ds.ReleaseResultSet(sql_lyr)
                pytest.fail(sql, feat_count, first_fid)
            feat = sql_lyr.GetNextFeature()
            if feat.GetFID() != first_fid:
                ds.ReleaseResultSet(sql_lyr)
                feat.DumpReadable()
                pytest.fail(sql, feat_count, first_fid)
        ds.ReleaseResultSet(sql_lyr)

        sql_lyr = ds.ExecuteSQL('GetLastSQLUsedOptimizedImplementation')
        optimized = int(sql_lyr.GetNextFeature().GetField(0))
        ds.ReleaseResultSet(sql_lyr)
        assert optimized == expected_optimized, (sql, feat_count, first_fid)

        if optimized and 'big_layer' not in sql:
            import test_cli_utilities
            if test_cli_utilities.get_test_ogrsf_path() is not None:
                ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/filegdb/testopenfilegdb.gdb.zip -sql "%s"' % sql)
                assert ret.find('INFO') != -1 and ret.find('ERROR') == -1, \
                    (sql, feat_count, first_fid)


###############################################################################
# Test reading a .gdbtable without .gdbtablx


def test_ogr_openfilegdb_8():

    ds = ogr.Open('data/filegdb/testopenfilegdb.gdb.zip')
    dict_feat_count = {}
    for i in range(ds.GetLayerCount()):
        lyr = ds.GetLayer(i)
        dict_feat_count[lyr.GetName()] = lyr.GetFeatureCount()
    ds = None

    dict_feat_count2 = {}
    gdal.SetConfigOption('OPENFILEGDB_IGNORE_GDBTABLX', 'YES')
    ds = ogr.Open('data/filegdb/testopenfilegdb.gdb.zip')
    for i in range(ds.GetLayerCount()):
        lyr = ds.GetLayer(i)
        dict_feat_count2[lyr.GetName()] = lyr.GetFeatureCount()
    gdal.SetConfigOption('OPENFILEGDB_IGNORE_GDBTABLX', None)

    assert dict_feat_count == dict_feat_count2

    lyr = ds.GetLayerByName('hole')
    # Not exactly in the order that one might expect, but logical when
    # looking at the structure of the .gdbtable
    expected_str = ['fid13', 'fid2', 'fid3', 'fid4', 'fid5', 'fid6', 'fid7', 'fid8', 'fid9', 'fid10', 'fid11', None]
    i = 0
    feat = lyr.GetNextFeature()
    while feat is not None:
        if feat.GetField('str') != expected_str[i]:
            feat.DumpReadable()
            pytest.fail()
        i = i + 1
        feat = lyr.GetNextFeature()


###############################################################################
# Test reading a .gdbtable outside a .gdb


def test_ogr_openfilegdb_9():

    try:
        os.stat('tmp/testopenfilegdb.gdb')
    except OSError:
        pytest.skip()

    shutil.copy('tmp/testopenfilegdb.gdb/a00000009.gdbtable', 'tmp/a00000009.gdbtable')
    shutil.copy('tmp/testopenfilegdb.gdb/a00000009.gdbtablx', 'tmp/a00000009.gdbtablx')
    ds = ogr.Open('tmp/a00000009.gdbtable')
    assert ds is not None
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    assert feat is not None

###############################################################################
# Test various error conditions


def fuzz(filename, offset):
    with open(filename, "rb+") as f:
        f.seek(offset, 0)
        v = ord(f.read(1))
        f.seek(offset, 0)
        f.write(chr(255 - v).encode('ISO-8859-1'))
    return (filename, offset, v)


def unfuzz(backup):
    (filename, offset, v) = backup
    with open(filename, "rb+") as f:
        f.seek(offset, 0)
        f.write(chr(v).encode('ISO-8859-1'))


def test_ogr_openfilegdb_10():

    try:
        os.stat('tmp/testopenfilegdb.gdb')
    except OSError:
        pytest.skip()

    shutil.copytree('tmp/testopenfilegdb.gdb', 'tmp/testopenfilegdb_fuzzed.gdb')

    if False:  # pylint: disable=using-constant-test
        for filename in ['tmp/testopenfilegdb_fuzzed.gdb/a00000001.gdbtable',
                         'tmp/testopenfilegdb_fuzzed.gdb/a00000001.gdbtablx']:
            errors = set()
            offsets = []
            last_error_msg = ''
            last_offset = -1
            for offset in range(os.stat(filename).st_size):
                # print(offset)
                backup = fuzz(filename, offset)
                gdal.ErrorReset()
                # print(offset)
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
                        if error_msg != '' and error_msg not in errors:
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
                # print(offset)
                backup = fuzz(filename, offset)
                gdal.ErrorReset()
                # print(offset)
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
                        if error_msg != '' and error_msg not in errors:
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

        for (filename, offsets) in [('tmp/testopenfilegdb_fuzzed.gdb/a00000001.gdbtable', [4, 5, 6, 7, 32, 33, 41, 42, 52, 59, 60, 63, 64, 72, 73, 77, 78, 79, 80, 81, 101, 102, 104, 105, 111, 180]),
                                    ('tmp/testopenfilegdb_fuzzed.gdb/a00000001.gdbtablx', [4, 7, 11, 12, 16, 31, 5136, 5140, 5142, 5144])]:
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

        for (filename, offsets) in [('tmp/testopenfilegdb_fuzzed.gdb/a00000004.gdbindexes', [0, 4, 5, 44, 45, 66, 67, 100, 101, 116, 117, 148, 149, 162, 163, 206, 207, 220, 221, 224, 280, 281]),
                                    ('tmp/testopenfilegdb_fuzzed.gdb/a00000004.CatItemsByPhysicalName.atx', [4, 12, 8196, 8300, 8460, 8620, 8780, 8940, 9100, 12290, 12294, 12298])]:
            for offset in offsets:
                # print(offset)
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


def test_ogr_openfilegdb_in_memory_spatial_filter():

  with gdaltest.config_option('OPENFILEGDB_USE_SPATIAL_INDEX', 'NO'):

    # Test building spatial index with GetFeatureCount()
    ds = ogr.Open('data/filegdb/testopenfilegdb.gdb.zip')
    lyr = ds.GetLayerByName('several_polygons')
    assert get_spi_state(ds, lyr) == SPI_IN_BUILDING
    lyr.ResetReading()
    assert get_spi_state(ds, lyr) == SPI_IN_BUILDING
    assert lyr.TestCapability(ogr.OLCFastFeatureCount) == 1
    lyr.SetSpatialFilterRect(0.25, 0.25, 0.5, 0.5)
    assert lyr.GetFeatureCount() == 1
    assert get_spi_state(ds, lyr) == SPI_COMPLETED
    # Should return cached value
    assert lyr.GetFeatureCount() == 1
    # Should use index
    c = 0
    feat = lyr.GetNextFeature()
    while feat is not None:
        c = c + 1
        feat = lyr.GetNextFeature()
    assert c == 1
    feat = None
    lyr = None
    ds = None

    # Test iterating without spatial index already built
    ds = ogr.Open('data/filegdb/testopenfilegdb.gdb.zip')
    lyr = ds.GetLayerByName('several_polygons')
    lyr.SetSpatialFilterRect(0.25, 0.25, 0.5, 0.5)
    c = 0
    feat = lyr.GetNextFeature()
    assert get_spi_state(ds, lyr) == SPI_IN_BUILDING
    while feat is not None:
        c = c + 1
        feat = lyr.GetNextFeature()
    assert c == 1
    assert get_spi_state(ds, lyr) == SPI_COMPLETED
    feat = None
    lyr = None
    ds = None

    # Test GetFeatureCount() without spatial index already built, with no matching feature
    # when GEOS is available
    if ogrtest.have_geos():
        expected_count = 0
    else:
        expected_count = 5

    ds = ogr.Open('data/filegdb/testopenfilegdb.gdb.zip')
    lyr = ds.GetLayerByName('multipolygon')
    lyr.SetSpatialFilterRect(1.4, 0.4, 1.6, 0.6)
    assert lyr.GetFeatureCount() == expected_count
    lyr = None
    ds = None

    # Test iterating without spatial index already built, with no matching feature
    # when GEOS is available
    ds = ogr.Open('data/filegdb/testopenfilegdb.gdb.zip')
    lyr = ds.GetLayerByName('multipolygon')
    lyr.SetSpatialFilterRect(1.4, 0.4, 1.6, 0.6)
    c = 0
    feat = lyr.GetNextFeature()
    while feat is not None:
        c = c + 1
        feat = lyr.GetNextFeature()
    assert c == expected_count
    assert lyr.GetFeatureCount() == expected_count
    feat = None
    lyr = None
    ds = None

    # GetFeature() should not impact spatial index building
    ds = ogr.Open('data/filegdb/testopenfilegdb.gdb.zip')
    lyr = ds.GetLayerByName('several_polygons')
    lyr.SetSpatialFilterRect(0.25, 0.25, 0.5, 0.5)
    feat = lyr.GetFeature(1)
    feat = lyr.GetFeature(1)
    assert get_spi_state(ds, lyr) == SPI_IN_BUILDING
    feat = lyr.GetNextFeature()
    while feat is not None:
        feat = lyr.GetNextFeature()
    assert get_spi_state(ds, lyr) == SPI_COMPLETED
    lyr.ResetReading()
    c = 0
    feat = lyr.GetNextFeature()
    while feat is not None:
        c = c + 1
        feat = lyr.GetNextFeature()
    assert c == 1
    assert get_spi_state(ds, lyr) == SPI_COMPLETED

    # This will create an array of filtered features
    lyr.SetSpatialFilterRect(0.25, 0.25, 0.5, 0.5)
    assert lyr.TestCapability(ogr.OLCFastSetNextByIndex) == 1
    # Test SetNextByIndex() with filtered features
    assert lyr.SetNextByIndex(-1) != 0
    assert lyr.SetNextByIndex(1) != 0
    assert lyr.SetNextByIndex(0) == 0
    feat = lyr.GetNextFeature()
    assert feat.GetFID() == 1
    assert get_spi_state(ds, lyr) == SPI_COMPLETED

    feat = None
    lyr = None
    ds = None

    # SetNextByIndex() impacts spatial index building
    ds = ogr.Open('data/filegdb/testopenfilegdb.gdb.zip')
    lyr = ds.GetLayerByName('multipolygon')
    lyr.SetNextByIndex(3)
    assert get_spi_state(ds, lyr) == SPI_INVALID
    feat = None
    lyr = None
    ds = None

    # and ResetReading() as well
    ds = ogr.Open('data/filegdb/testopenfilegdb.gdb.zip')
    lyr = ds.GetLayerByName('multipolygon')
    feat = lyr.GetNextFeature()
    lyr.ResetReading()
    assert get_spi_state(ds, lyr) == SPI_INVALID
    feat = None
    lyr = None
    ds = None

    # and SetAttributeFilter() with an index too
    ds = ogr.Open('data/filegdb/testopenfilegdb.gdb.zip')
    lyr = ds.GetLayerByName('point')
    lyr.SetAttributeFilter('id = 1')
    assert get_spi_state(ds, lyr) == SPI_INVALID
    feat = None
    lyr = None
    ds = None


def test_ogr_openfilegdb_spx_spatial_filter():

    # Test GetFeatureCount() and then iterating
    ds = ogr.Open('data/filegdb/testopenfilegdb.gdb.zip')
    lyr = ds.GetLayerByName('several_polygons')
    assert lyr.TestCapability(ogr.OLCFastFeatureCount) == 1
    assert lyr.TestCapability(ogr.OLCFastSpatialFilter) == 1
    lyr.SetSpatialFilterRect(0.25, 0.25, 0.5, 0.5)
    assert lyr.GetFeatureCount() == 1
    c = 0
    for f in lyr:
        c += 1
    assert c == 1

    # Set another spatial filter
    lyr.SetSpatialFilterRect(0, 2, 1, 5)
    assert lyr.GetFeatureCount() == 2

    # Unset spatial filter
    lyr.SetSpatialFilter(None)
    assert lyr.GetFeatureCount() == 9

    # Set again a spatial filter
    lyr.SetSpatialFilterRect(0.25, 0.25, 0.5, 0.5)
    assert lyr.GetFeatureCount() == 1

    lyr = None
    ds = None

    # Test iterating without spatial index already built
    ds = ogr.Open('data/filegdb/testopenfilegdb.gdb.zip')
    lyr = ds.GetLayerByName('several_polygons')
    lyr.SetSpatialFilterRect(0.25, 0.25, 0.5, 0.5)
    c = 0
    for f in lyr:
        c += 1
    assert c == 1
    lyr = None
    ds = None

    # Test GetFeatureCount(), with no matching feature when GEOS is available
    if ogrtest.have_geos():
        expected_count = 0
    else:
        expected_count = 5

    ds = ogr.Open('data/filegdb/testopenfilegdb.gdb.zip')
    lyr = ds.GetLayerByName('multipolygon')
    lyr.SetSpatialFilterRect(1.4, 0.4, 1.6, 0.6)
    assert lyr.GetFeatureCount() == expected_count
    lyr = None
    ds = None

    # Test iterating, with no matching feature when GEOS is available
    ds = ogr.Open('data/filegdb/testopenfilegdb.gdb.zip')
    lyr = ds.GetLayerByName('multipolygon')
    lyr.SetSpatialFilterRect(1.4, 0.4, 1.6, 0.6)
    c = 0
    for f in lyr:
        c += 1
    assert c == expected_count
    assert lyr.GetFeatureCount() == expected_count
    lyr = None
    ds = None

    # test with a SetAttributeFilter() with an index too
    ds = ogr.Open('data/filegdb/test_spatial_index.gdb.zip')
    lyr = ds.GetLayerByName('test')
    lyr.SetAttributeFilter('id = 1')
    lyr.SetSpatialFilterRect(400000, 0, 500100, 4500100)
    assert lyr.GetFeatureCount() == 1
    c = 0
    for f in lyr:
        c += 1
    assert c == 1

    # No intersection between filters
    lyr.SetSpatialFilterRect(500100, 4500000, 500200, 4500100)
    assert lyr.GetFeatureCount() == 0
    c = 0
    for f in lyr:
        c += 1
    assert c == 0

    lyr.SetAttributeFilter(None)
    assert lyr.GetFeatureCount() == 154


###############################################################################
# Test opening a FGDB with both SRID and LatestSRID set (#5638)


def test_ogr_openfilegdb_12():

    ds = ogr.Open('/vsizip/data/filegdb/test3005.gdb.zip')
    lyr = ds.GetLayer(0)
    got_wkt = lyr.GetSpatialRef().ExportToWkt()
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(3005)
    expected_wkt = sr.ExportToWkt()
    assert got_wkt == expected_wkt
    ds = None

###############################################################################
# Test opening a FGDB v9 with a non spatial table (#5673)


def test_ogr_openfilegdb_13():

    ds = ogr.Open('/vsizip/data/filegdb/ESSENCE_NAIPF_ORI_PROV_sub93.gdb.zip')
    lyr = ds.GetLayer(0)
    assert lyr.GetName() == 'DDE_ESSEN_NAIPF_ORI_VUE'
    assert lyr.GetSpatialRef() is None
    assert lyr.GetGeomType() == ogr.wkbNone
    f = lyr.GetNextFeature()
    if f.GetField('GEOCODE') != '-673985,22+745574,77':
        f.DumpReadable()
        pytest.fail()
    ds = None

###############################################################################
# Test not nullable fields


def test_ogr_openfilegdb_14():

    ds = ogr.Open('data/filegdb/testopenfilegdb.gdb.zip')
    lyr = ds.GetLayerByName('testnotnullable')
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_not_nullable')).IsNullable() == 0
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_nullable')).IsNullable() == 1
    assert lyr.GetLayerDefn().GetGeomFieldDefn(0).IsNullable() == 0
    ds = None

###############################################################################
# Test default values


def test_ogr_openfilegdb_15():

    ds = ogr.Open('data/filegdb/test_default_val.gdb.zip')
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('STR')).GetDefault() == "'default_val'"
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('STR')).GetWidth() == 50
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('INT32')).GetDefault() == "123456788"
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('INT16')).GetDefault() == "12345"
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('FLOAT32')).GetDefault().find('1.23') == 0
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('FLOAT64')).GetDefault().find('1.23456') == 0
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('DATETIME')).GetDefault() == "'2015/06/30 12:34:56'"


###############################################################################
# Read layers with sparse pages

def test_ogr_openfilegdb_16():

    ds = ogr.Open('data/filegdb/sparse.gdb.zip')
    lyr = ds.GetLayer(0)
    for fid in [2, 3, 4, 7, 8, 9, 10, 2049, 8191, 16384, 10000000, 10000001]:
        f = lyr.GetNextFeature()
        assert f.GetFID() == fid

    f = lyr.GetFeature(100000)
    assert f is None

    f = lyr.GetFeature(10000000 - 1)
    assert f is None
    f = lyr.GetNextFeature()
    assert f is None

    f = lyr.GetFeature(16384)
    assert f is not None

###############################################################################
# Read a MULTILINESTRING ZM with a dummy M array (#6528)


def test_ogr_openfilegdb_17():

    ds = ogr.Open('data/filegdb/multilinestringzm_with_dummy_m_array.gdb.zip')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetGeometryRef() is not None

###############################################################################
# Read curves


def test_ogr_openfilegdb_18():

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


def test_ogr_openfilegdb_19():

    os.chdir('data/filegdb/curves.gdb')
    ds = ogr.Open('.')
    os.chdir('../../..')
    assert ds is not None

###############################################################################
# Read polygons with M component where the M of the closing point is not the
# one of the starting point (#7017)


def test_ogr_openfilegdb_20():

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


def test_ogr_openfilegdb_21():

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

def test_ogr_openfilegdb_weird_winding_order():

    if not ogrtest.have_geos():
        pytest.skip()

    ds = ogr.Open('/vsizip/data/filegdb/weird_winding_order_fgdb.zip/roads_clip Drawing.gdb')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    g = f.GetGeometryRef()
    assert g.GetGeometryCount() == 1
    assert g.GetGeometryRef(0).GetGeometryCount() == 17

###############################################################################
# Test bugfix for https://github.com/OSGeo/gdal/issues/1369
# where a polygon with inner rings has its exterior ring with wrong orientation

def test_ogr_openfilegdb_utc_datetime():

    ds = ogr.Open('data/filegdb/testdatetimeutc.gdb')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    # Check that the timezone +00 is present
    assert f.GetFieldAsString('EditDate') == '2020/06/22 07:49:36+00'


###############################################################################
# Test that field alias are correctly read and mapped to OGR field alternativ
# names

def test_ogr_fgdb_alias():
    ds = ogr.Open('data/filegdb/field_alias.gdb')

    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('text')).GetAlternativeName() == 'My Text Field'
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('short_int')).GetAlternativeName() == 'My Short Int Field'
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('long_int')).GetAlternativeName() == 'My Long Int Field'
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('float')).GetAlternativeName() == 'My Float Field'
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('double')).GetAlternativeName() == 'My Double Field'
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('date')).GetAlternativeName() == 'My Date Field'
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('blob')).GetAlternativeName() == 'My Blob Field'
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('guid')).GetAlternativeName() == 'My GUID field'
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('raster')).GetAlternativeName() == 'My Raster Field'
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('SHAPE_Length')).GetAlternativeName() == ''


###############################################################################
# Test reading field domains


def test_ogr_openfilegdb_read_domains():

    ds = gdal.OpenEx('data/filegdb/Domains.gdb', gdal.OF_VECTOR)

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


###############################################################################
# Test reading layer hierarchy


def test_ogr_openfilegdb_read_layer_hierarchy():

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
# Test LIST_ALL_TABLES open option


def test_ogr_openfilegdb_list_all_tables_v10():
    ds = ogr.Open('data/filegdb/testopenfilegdb.gdb.zip')
    assert ds is not None

    assert ds.GetLayerCount() == 37, 'did not get expected layer count'
    layer_names = [ds.GetLayer(i).GetName() for i in range(ds.GetLayerCount())]
    # should not be exposed by default
    for name in ['GDB_DBTune', 'GDB_ItemRelationshipTypes', 'GDB_ItemRelationships', 'GDB_ItemTypes',
                 'GDB_Items', 'GDB_SpatialRefs', 'GDB_SystemCatalog']:
        assert name not in layer_names

    # Test LIST_ALL_TABLES=YES open option
    ds_all_table = gdal.OpenEx('data/filegdb/testopenfilegdb.gdb.zip', gdal.OF_VECTOR,
                                 open_options=['LIST_ALL_TABLES=YES'])

    assert ds_all_table.GetLayerCount() == 44, 'did not get expected layer count'
    layer_names = [ds_all_table.GetLayer(i).GetName() for i in range(ds_all_table.GetLayerCount())]

    for name in ['linestring', 'point', 'multipoint',
                 'GDB_DBTune', 'GDB_ItemRelationshipTypes', 'GDB_ItemRelationships', 'GDB_ItemTypes',
                 'GDB_Items', 'GDB_SpatialRefs', 'GDB_SystemCatalog']:
        assert name in layer_names

    private_layers = [ds_all_table.GetLayer(i).GetName() for i in range(ds_all_table.GetLayerCount()) if ds_all_table.IsLayerPrivate(i)]
    for name in ['GDB_DBTune', 'GDB_ItemRelationshipTypes', 'GDB_ItemRelationships', 'GDB_ItemTypes',
                 'GDB_Items', 'GDB_SpatialRefs', 'GDB_SystemCatalog']:
        assert name in private_layers
    for name in ['linestring', 'point', 'multipoint']:
        assert name not in private_layers


def test_ogr_openfilegdb_list_all_tables_v9():
    ds = ogr.Open('data/filegdb/testopenfilegdb93.gdb.zip')
    assert ds is not None

    assert ds.GetLayerCount() == 22, 'did not get expected layer count'
    layer_names = [ds.GetLayer(i).GetName() for i in range(ds.GetLayerCount())]
    # should not be exposed by default
    for name in ['GDB_DBTune', 'GDB_FeatureClasses', 'GDB_FeatureDataset', 'GDB_FieldInfo',
                 'GDB_ObjectClasses', 'GDB_Release', 'GDB_SpatialRefs', 'GDB_SystemCatalog', 'GDB_UserMetadata']:
        assert name not in layer_names

    # Test LIST_ALL_TABLES=YES open option
    ds_all_table = gdal.OpenEx('data/filegdb/testopenfilegdb93.gdb.zip', gdal.OF_VECTOR,
                                 open_options=['LIST_ALL_TABLES=YES'])

    assert ds_all_table.GetLayerCount() == 31, 'did not get expected layer count'
    layer_names = [ds_all_table.GetLayer(i).GetName() for i in range(ds_all_table.GetLayerCount())]
    print(layer_names)

    for name in ['linestring', 'point', 'multipoint',
                 'GDB_DBTune', 'GDB_FeatureClasses', 'GDB_FeatureDataset', 'GDB_FieldInfo',
                 'GDB_ObjectClasses', 'GDB_Release', 'GDB_SpatialRefs', 'GDB_SystemCatalog', 'GDB_UserMetadata']:
        assert name in layer_names

    private_layers = [ds_all_table.GetLayer(i).GetName() for i in range(ds_all_table.GetLayerCount()) if ds_all_table.IsLayerPrivate(i)]
    for name in ['GDB_DBTune', 'GDB_FeatureClasses', 'GDB_FeatureDataset', 'GDB_FieldInfo',
                 'GDB_ObjectClasses', 'GDB_Release', 'GDB_SpatialRefs', 'GDB_SystemCatalog', 'GDB_UserMetadata']:
        assert name in private_layers
    for name in ['linestring', 'point', 'multipoint']:
        assert name not in private_layers


###############################################################################
# Test that non-spatial tables which are not present in GDB_Items are listed
# see https://github.com/OSGeo/gdal/issues/4463


def test_ogr_openfilegdb_non_spatial_table_outside_gdb_items():
    ds = ogr.Open('data/filegdb/table_outside_gdbitems.gdb')
    assert ds is not None

    assert ds.GetLayerCount() == 3, 'did not get expected layer count'
    layer_names = set(ds.GetLayer(i).GetName() for i in range(ds.GetLayerCount()))
    assert layer_names == {'aquaduct', 'flat_table1', 'flat_table2'}

    # Test with the LIST_ALL_TABLES=YES open option
    ds_all_table = gdal.OpenEx('data/filegdb/table_outside_gdbitems.gdb', gdal.OF_VECTOR,
                                 open_options=['LIST_ALL_TABLES=YES'])

    assert ds_all_table.GetLayerCount() == 10, 'did not get expected layer count'
    layer_names = set(ds_all_table.GetLayer(i).GetName() for i in range(ds_all_table.GetLayerCount()))

    for name in ['aquaduct', 'flat_table1', 'flat_table2',
                 'GDB_DBTune', 'GDB_ItemRelationshipTypes', 'GDB_ItemRelationships', 'GDB_ItemTypes',
                 'GDB_Items', 'GDB_SpatialRefs', 'GDB_SystemCatalog']:
        assert name in layer_names

    private_layers = set(ds_all_table.GetLayer(i).GetName() for i in range(ds_all_table.GetLayerCount()) if ds_all_table.IsLayerPrivate(i))
    for name in ['GDB_DBTune', 'GDB_ItemRelationshipTypes', 'GDB_ItemRelationships', 'GDB_ItemTypes',
                 'GDB_Items', 'GDB_SpatialRefs', 'GDB_SystemCatalog']:
        assert name in private_layers
    for name in ['aquaduct', 'flat_table1', 'flat_table2']:
        assert name not in private_layers



###############################################################################
# Test reading .gdb with strings encoded as UTF16 instead of UTF8
# (e.g. using -lco CONFIGURATION_KEYWORD=TEXT_UTF16 of FileGDB driver)


def test_ogr_openfilegdb_strings_utf16():
    ds = ogr.Open('data/filegdb/test_utf16.gdb.zip')
    assert ds is not None
    lyr = ds.GetLayer(0)
    fld_defn = lyr.GetLayerDefn().GetFieldDefn(0)
    assert fld_defn.GetDefault() == "'éven'"
    f = lyr.GetNextFeature()
    assert f['str'] == 'évenéven'


###############################################################################
# Test reading .gdb where the CRS in the XML definition of the feature
# table is not consistent with the one of the feature dataset


def test_ogr_openfilegdb_inconsistent_crs_feature_dataset_and_feature_table():
    ds = ogr.Open('data/filegdb/inconsistent_crs_feature_dataset_and_feature_table.gdb')
    assert ds is not None
    lyr = ds.GetLayer(0)
    srs = lyr.GetSpatialRef()
    assert srs is not None
    assert srs.GetAuthorityCode(None) == '4326'


###############################################################################
# Cleanup


def test_ogr_openfilegdb_cleanup():

    try:
        shutil.rmtree('tmp/testopenfilegdb.gdb')
    except OSError:
        pass
    try:
        os.remove('tmp/a00000009.gdbtable')
        os.remove('tmp/a00000009.gdbtablx')
    except OSError:
        pass
    try:
        shutil.rmtree('tmp/testopenfilegdb_fuzzed.gdb')
    except OSError:
        pass


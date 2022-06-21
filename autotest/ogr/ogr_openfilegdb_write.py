#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  OpenFileGDB driver testing (write side)
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2022, Even Rouault <even dot rouault at spatialys.com>
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

from osgeo import gdal, ogr, osr
import struct
import sys

import gdaltest
import pytest


pytestmark = pytest.mark.require_driver('OpenFileGDB')


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

###############################################################################


def test_ogr_openfilegdb_invalid_filename():

    with gdaltest.error_handler():
        ds = ogr.GetDriverByName('OpenFileGDB').CreateDataSource('/vsimem/bad.extension')
        assert ds is None

    with gdaltest.error_handler():
        ds = ogr.GetDriverByName('OpenFileGDB').CreateDataSource('/parent/directory/does/not/exist.gdb')
        assert ds is None


###############################################################################


def test_ogr_openfilegdb_write_empty():

    dirname = '/vsimem/out.gdb'
    ds = ogr.GetDriverByName('OpenFileGDB').CreateDataSource(dirname)
    assert ds is not None
    ds = None

    ds = ogr.Open(dirname)
    assert ds is not None
    assert ds.GetLayerCount() == 0
    ds = None

    gdal.RmdirRecursive(dirname)


###############################################################################


@pytest.mark.parametrize('use_synctodisk', [False, True])
def test_ogr_openfilegdb_write_field_types(use_synctodisk):

    dirname = '/vsimem/out.gdb'
    try:
        ds = ogr.GetDriverByName('OpenFileGDB').CreateDataSource(dirname)
        assert ds.TestCapability(ogr.ODsCCreateLayer) == 1
        lyr = ds.CreateLayer('test', geom_type = ogr.wkbPoint,
                             options = ['COLUMN_TYPES=xml=esriFieldTypeXML,globalId=esriFieldTypeGlobalID,guid=esriFieldTypeGUID'])

        # Cannot create field with same name as an existing field (here the geometry one)
        with gdaltest.error_handler():
            fld_defn = ogr.FieldDefn('SHAPE', ogr.OFTString)
            assert lyr.CreateField(fld_defn) != ogr.OGRERR_NONE

        fld_defn = ogr.FieldDefn('str', ogr.OFTString)
        fld_defn.SetAlternativeName('alias')
        fld_defn.SetDefault("'default val'")
        fld_defn.SetWidth(100)
        assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

        fld_defn = ogr.FieldDefn('str_not_nullable', ogr.OFTString)
        fld_defn.SetNullable(False)
        with gdaltest.config_option('OPENFILEGDB_DEFAULT_STRING_WIDTH', '12345'):
            assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE
        assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldCount() - 1).GetWidth() == 12345

        fld_defn = ogr.FieldDefn('str_default_width', ogr.OFTString)
        assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE
        assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldCount() - 1).GetWidth() == 0

        fld_defn = ogr.FieldDefn('int32', ogr.OFTInteger)
        fld_defn.SetDefault('-12345')
        assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

        if use_synctodisk:
            assert lyr.SyncToDisk() == ogr.OGRERR_NONE
            # To check that we can rewrite-in-place a growing field description
            # section when it is at end of file

        fld_defn = ogr.FieldDefn('int16', ogr.OFTInteger)
        fld_defn.SetSubType(ogr.OFSTInt16)
        assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

        fld_defn = ogr.FieldDefn('float64', ogr.OFTReal)
        fld_defn.SetDefault('-1.25')
        assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

        fld_defn = ogr.FieldDefn('float32', ogr.OFTReal)
        fld_defn.SetSubType(ogr.OFSTFloat32)
        assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

        fld_defn = ogr.FieldDefn('int64', ogr.OFTInteger64)
        assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

        fld_defn = ogr.FieldDefn('dt', ogr.OFTDateTime)
        assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

        fld_defn = ogr.FieldDefn('binary', ogr.OFTBinary)
        assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

        fld_defn = ogr.FieldDefn('xml', ogr.OFTString)
        assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

        fld_defn = ogr.FieldDefn('globalId', ogr.OFTString)
        assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

        fld_defn = ogr.FieldDefn('guid', ogr.OFTString)
        assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField('str', 'my str')
        f.SetField('str_not_nullable', 'my str_not_nullable')
        f.SetField('int32', 123456789)
        f.SetField('int16', -32768)
        f.SetField('float64', 1.23456789)
        f.SetField('float32', 1.25)
        f.SetField('int64', 12345678912345)
        f.SetField('dt', '2022-11-04T12:34:56+02:00')
        f.SetFieldBinaryFromHexString("binary", "00FF7F")
        f.SetField('xml', "<some_elt/>")
        f.SetField('guid', "{12345678-9ABC-DEF0-1234-567890ABCDEF}")
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField('str_not_nullable', 'my str_not_nullable')
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

        f = ogr.Feature(lyr.GetLayerDefn())
        with gdaltest.error_handler():
            gdal.ErrorReset()
            assert lyr.CreateFeature(f) == ogr.OGRERR_FAILURE
            assert gdal.GetLastErrorMsg() == 'Attempting to write null/empty field in non-nullable field'

        ds = None

        ds = ogr.Open(dirname)
        lyr = ds.GetLayer(0)

        fld_defn = lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('str'))
        assert fld_defn.GetAlternativeName() == 'alias'
        assert fld_defn.GetDefault() == "'default val'"
        assert fld_defn.GetWidth() == 100

        fld_defn = lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('str_not_nullable'))
        assert fld_defn.IsNullable() == False
        assert fld_defn.GetWidth() == 12345

        fld_defn = lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('str_default_width'))
        assert fld_defn.GetWidth() == 0

        fld_defn = lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('int16'))
        assert fld_defn.GetSubType() == ogr.OFSTInt16

        fld_defn = lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('int32'))
        assert fld_defn.GetSubType() == ogr.OFSTNone
        assert fld_defn.GetDefault() == '-12345'

        fld_defn = lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('float32'))
        assert fld_defn.GetSubType() == ogr.OFSTFloat32

        fld_defn = lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('float64'))
        assert fld_defn.GetSubType() == ogr.OFSTNone
        assert fld_defn.GetDefault() == '-1.25'

        fld_defn = lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('int64'))
        assert fld_defn.GetType() == ogr.OFTReal

        fld_defn = lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('dt'))
        assert fld_defn.GetType() == ogr.OFTDateTime

        fld_defn = lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('binary'))
        assert fld_defn.GetType() == ogr.OFTBinary

        f = lyr.GetNextFeature()
        assert f['str'] == 'my str'
        assert f['str_not_nullable'] == 'my str_not_nullable'
        assert f['int32'] == 123456789
        assert f['int16'] == -32768
        assert f['float64'] == 1.23456789
        assert f['float32'] == 1.25
        assert f['int64'] == 12345678912345.0
        assert f['dt'] == '2022/11/04 10:34:56+00'
        assert f['binary'] == '00FF7F'
        assert f['xml'] == '<some_elt/>'
        assert f['globalId'].startswith('{') # check that a GlobaID has been generated
        assert f['guid'] == '{12345678-9ABC-DEF0-1234-567890ABCDEF}'

        f = lyr.GetNextFeature()
        assert f['str'] == 'default val'
        assert f['str_not_nullable'] == 'my str_not_nullable'
        assert not f.IsFieldSetAndNotNull('int16')


        ds = None

        with gdaltest.config_option('OPENFILEGDB_REPORT_GENUINE_FIELD_WIDTH', 'YES'):
            ds = ogr.Open(dirname)
            lyr = ds.GetLayer(0)
            fld_defn = lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('str_default_width'))
            assert fld_defn.GetWidth() == 65536
            ds = None

    finally:
        gdal.RmdirRecursive(dirname)


###############################################################################


testdata = [
    (ogr.wkbPoint,         ogr.wkbPoint,              None, None),
    (ogr.wkbPoint,         ogr.wkbPoint,              'POINT EMPTY', None),
    (ogr.wkbPoint,         ogr.wkbPoint,              'POINT (1 2)', None),
    (ogr.wkbPoint25D,      ogr.wkbPoint25D,           'POINT Z (1 2 3)', None),
    (ogr.wkbPointM,        ogr.wkbPointM,             'POINT M (1 2 3)', None),
    (ogr.wkbPointZM,       ogr.wkbPointZM,            'POINT ZM (1 2 3 4)', None),
    (ogr.wkbLineString,    ogr.wkbMultiLineString,    'LINESTRING EMPTY', None),
    (ogr.wkbLineString,    ogr.wkbMultiLineString,    'LINESTRING (1 2,3 4,-1 -2)', None),
    (ogr.wkbLineString25D, ogr.wkbMultiLineString25D, 'LINESTRING Z (1 2 10,3 4 20,-1 -2 15)', None),
    (ogr.wkbLineStringM,   ogr.wkbMultiLineStringM,   'LINESTRING M (1 2 10,3 4 20,-1 -2 15)', None),
    (ogr.wkbLineStringZM,  ogr.wkbMultiLineStringZM,  'LINESTRING ZM (1 2 10 100,3 4 20 200,-1 -2 15 150)', None),
    (ogr.wkbPolygon,       ogr.wkbMultiPolygon,       'POLYGON EMPTY', None),
    (ogr.wkbPolygon,       ogr.wkbMultiPolygon,       'POLYGON ((0 0,0 1,1 1,0 0))', None),
    (ogr.wkbPolygon,       ogr.wkbMultiPolygon,       'POLYGON ((0 0,1 1,0 1,0 0))', 'MULTIPOLYGON (((0 0,0 1,1 1,0 0)))'), # must fix winding order
    (ogr.wkbPolygon,       ogr.wkbMultiPolygon,       'POLYGON ((0 0,0 1,1 1,0 0),(0.2 0.2,0.8 0.8,0.2 0.8,0.2 0.2))', None),
    (ogr.wkbPolygon,       ogr.wkbMultiPolygon,       'POLYGON ((0 0,0 1,1 1,0 0),(0.2 0.2,0.2 0.8,0.8 0.8,0.2 0.2))', 'MULTIPOLYGON (((0 0,0 1,1 1,0 0),(0.2 0.2,0.8 0.8,0.2 0.8,0.2 0.2)))'), # must fix winding order of inner ring
    (ogr.wkbPolygon25D,    ogr.wkbMultiPolygon25D,    'POLYGON Z ((0 0 10,0 1 30,1 1 20,0 0 10))', None),
    (ogr.wkbPolygonM,      ogr.wkbMultiPolygonM,      'POLYGON M ((0 0 10,0 1 30,1 1 20,0 0 10))', None),
    (ogr.wkbPolygonZM,     ogr.wkbMultiPolygonZM,     'POLYGON ZM ((0 0 10 100,0 1 30 300,1 1 20 200,0 0 10 100))', None),
    (ogr.wkbMultiPoint,    ogr.wkbMultiPoint,         'MULTIPOINT (1 2)', None),
    (ogr.wkbMultiPoint,    ogr.wkbMultiPoint,         'MULTIPOINT (1 2,-3 -4,5 6)', None),
    (ogr.wkbMultiPoint25D, ogr.wkbMultiPoint25D,      'MULTIPOINT Z ((1 2 10),(-3 -4 30),(5 6 20))', None),
    (ogr.wkbMultiPointM,   ogr.wkbMultiPointM,        'MULTIPOINT M ((1 2 10),(-3 -4 30),(5 6 20))', None),
    (ogr.wkbMultiPointZM,  ogr.wkbMultiPointZM,       'MULTIPOINT ZM ((1 2 10 100),(-3 -4 30 300),(5 6 20 200))', None),
    (ogr.wkbMultiLineString,    ogr.wkbMultiLineString,    'MULTILINESTRING EMPTY', None),
    (ogr.wkbMultiLineString,    ogr.wkbMultiLineString,    'MULTILINESTRING ((1 2,3 4,-1 -2),(3 -4,5 6))', None),
    (ogr.wkbMultiLineString25D, ogr.wkbMultiLineString25D, 'MULTILINESTRING Z ((1 2 10,3 4 20,-1 -2 15),(3 -4 10,5 6 20))', None),
    (ogr.wkbMultiLineStringM,   ogr.wkbMultiLineStringM,   'MULTILINESTRING M ((1 2 10,3 4 20,-1 -2 15),(3 -4 10,5 6 20))', None),
    (ogr.wkbMultiLineStringZM,  ogr.wkbMultiLineStringZM,  'MULTILINESTRING ZM ((1 2 10 100,3 4 20 200,-1 -2 15 150),(3 -4 10 200,5 6 20 100))', None),
    (ogr.wkbMultiPolygon,       ogr.wkbMultiPolygon,       'MULTIPOLYGON EMPTY', None),
    (ogr.wkbMultiPolygon,       ogr.wkbMultiPolygon,       'MULTIPOLYGON (((0 0,0 1,1 1,0 0)))', None),
    (ogr.wkbMultiPolygon,       ogr.wkbMultiPolygon,       'MULTIPOLYGON (((0 0,0 1,1 1,0 0),(0.2 0.2,0.8 0.8,0.2 0.8,0.2 0.2)),((10 10,10 11,11 11,10 10)))', None),
    (ogr.wkbMultiPolygon25D,    ogr.wkbMultiPolygon25D,    'MULTIPOLYGON Z (((0 0 10,0 1 30,1 1 20,0 0 10)),((10 10 100,10 11 120,11 11 110,10 10 100)))', None),
    (ogr.wkbMultiPolygonM,      ogr.wkbMultiPolygonM,      'MULTIPOLYGON M (((0 0 10,0 1 30,1 1 20,0 0 10)),((10 10 100,10 11 120,11 11 110,10 10 100)))', None),
    (ogr.wkbMultiPolygonZM,     ogr.wkbMultiPolygonZM,     'MULTIPOLYGON ZM (((0 0 10 100,0 1 30 300,1 1 20 200,0 0 10 100)),((10 10 100 1000,10 11 120 1100,11 11 110 900,10 10 100 1000)))', None),
    (ogr.wkbCircularString, ogr.wkbMultiLineString,    'CIRCULARSTRING (0 0,1 1,2 0)', 'MULTICURVE (COMPOUNDCURVE (CIRCULARSTRING (0 0,1 1,2 0)))'),
    (ogr.wkbCircularStringZM, ogr.wkbMultiLineStringZM,    'CIRCULARSTRING ZM (0 0 10 0,1 1 10 0,2 0 10 0)', 'MULTICURVE ZM  (COMPOUNDCURVE ZM (CIRCULARSTRING ZM (0 0 10 0,1 1 10 0,2 0 10 0)))'),
    (ogr.wkbCompoundCurve, ogr.wkbMultiLineString,    'COMPOUNDCURVE ((0 0,1 1,2 0))', 'MULTILINESTRING ((0 0,1 1,2 0))'),
    (ogr.wkbCompoundCurve, ogr.wkbMultiLineString,    'COMPOUNDCURVE (CIRCULARSTRING (0 0,1 1,2 0),(2 0,3 0))', 'MULTICURVE (COMPOUNDCURVE (CIRCULARSTRING (0 0,1 1,2 0),(2 0,3 0)))'),
    (ogr.wkbCompoundCurveZM, ogr.wkbMultiLineStringZM,    'COMPOUNDCURVE ZM (CIRCULARSTRING ZM (0 0 10 0,1 1 10 0,2 0 10 0),(2 0 10 0,3 0 11 1))', 'MULTICURVE ZM (COMPOUNDCURVE ZM (CIRCULARSTRING ZM (0 0 10 0,1 1 10 0,2 0 10 0),(2 0 10 0,3 0 11 1)))'),
    (ogr.wkbMultiCurve,    ogr.wkbMultiLineString,    'MULTICURVE(CIRCULARSTRING (0 0,1 1,2 0),(0 0,1 1),COMPOUNDCURVE (CIRCULARSTRING(10 10,11 11,12 10)))', 'MULTICURVE (COMPOUNDCURVE (CIRCULARSTRING (0 0,1 1,2 0)),(0 0,1 1),COMPOUNDCURVE (CIRCULARSTRING(10 10,11 11,12 10)))'),
    (ogr.wkbCurvePolygon,  ogr.wkbMultiPolygon,       'CURVEPOLYGON ((0 0,1 1,0 1,0 0))', 'MULTIPOLYGON (((0 0,0 1,1 1,0 0)))'),
    (ogr.wkbCurvePolygon,  ogr.wkbMultiPolygon,       'CURVEPOLYGON ((0 0,0 1,1 1,0 0))', 'MULTIPOLYGON (((0 0,0 1,1 1,0 0)))'),  # must fix winding order
    (ogr.wkbCurvePolygon,  ogr.wkbMultiPolygon,       'CURVEPOLYGON ((0 0,0 1,1 1,0 0),(0.2 0.2,0.2 0.8,0.8 0.8,0.2 0.2))', 'MULTIPOLYGON (((0 0,0 1,1 1,0 0),(0.2 0.2,0.8 0.8,0.2 0.8,0.2 0.2)))'), # must fix winding order of inner ring
    (ogr.wkbCurvePolygon,  ogr.wkbMultiPolygon,       'CURVEPOLYGON (COMPOUNDCURVE ((0 0,0 1,1 1),CIRCULARSTRING (1 1,1.5 0.5,1 0),(1 0,0 0)))', 'MULTISURFACE (CURVEPOLYGON (COMPOUNDCURVE ((0 0,0 1,1 1),CIRCULARSTRING (1 1,1.5 0.5,1 0),(1 0,0 0))))'),
    (ogr.wkbCurvePolygon,  ogr.wkbMultiPolygon,       'CURVEPOLYGON (COMPOUNDCURVE((0 0,1 0),CIRCULARSTRING(1 0,1.5 0.5,1 1),(1 1,0 1,0 0)))', 'MULTISURFACE (CURVEPOLYGON (COMPOUNDCURVE ((0 0,0 1,1 1),CIRCULARSTRING (1 1,1.5 0.5,1 0),(1 0,0 0))))'),  # must fix winding order
    (ogr.wkbCurvePolygon,  ogr.wkbMultiPolygon,       'CURVEPOLYGON (CIRCULARSTRING(-10 0,0 10,10 0,0 -10,-10 0))', 'MULTISURFACE (CURVEPOLYGON (COMPOUNDCURVE (CIRCULARSTRING (-10 0,0 10,10 0),CIRCULARSTRING (10 0,0 -10,-10 0))))'),
    (ogr.wkbCurvePolygon,  ogr.wkbMultiPolygon,       'CURVEPOLYGON (CIRCULARSTRING(-10 0,0 -10,10 0,0 10,-10 0))', 'MULTISURFACE (CURVEPOLYGON (COMPOUNDCURVE (CIRCULARSTRING (-10 0,0 10,10 0),CIRCULARSTRING (10 0,0 -10,-10 0))))'),  # must fix winding order
    (ogr.wkbCurvePolygon,  ogr.wkbMultiPolygon,       'CURVEPOLYGON ((-100 -100,-100 100,100 100,100 -100,-100 -100),CIRCULARSTRING(-10 0,0 10,10 0,0 -10,-10 0))', 'MULTISURFACE (CURVEPOLYGON ((-100 -100,-100 100,100 100,100 -100,-100 -100),COMPOUNDCURVE (CIRCULARSTRING (-10 0,0 -10,10 0),CIRCULARSTRING (10 0,0 10,-10 0))))'),  # must fix winding order of inner ring
    (ogr.wkbCurvePolygon,  ogr.wkbMultiPolygon,       'CURVEPOLYGON (CIRCULARSTRING(-10 0,0 10,10 0,0 -10,-10 0),COMPOUNDCURVE ((0 0,0 1,1 1),CIRCULARSTRING (1 1,1.5 0.5,1 0),(1 0,0 0)))', 'MULTISURFACE (CURVEPOLYGON (COMPOUNDCURVE (CIRCULARSTRING (-10 0,0 10,10 0),CIRCULARSTRING (10 0,0 -10,-10 0)),COMPOUNDCURVE ((0 0,1 0),CIRCULARSTRING (1 0,1.5 0.5,1 1),(1 1,0 1,0 0))))'),  # must fix winding order of inner ring
    (ogr.wkbMultiSurface,       ogr.wkbMultiPolygon,       'MULTISURFACE (((0 0,0 1,1 1,0 0)))', None),
    (ogr.wkbMultiSurfaceZM,     ogr.wkbMultiPolygonZM,     'MULTISURFACE ZM (((100 0 10 100,100 1 10 101,101 1 10 102,100 0 10 100)),CURVEPOLYGON ZM (CIRCULARSTRING ZM(-10 0 10 0,0 10 10 0,10 0 10 0,0 -1010 0,-10 0 10 0)))', 'MULTISURFACE ZM (CURVEPOLYGON ZM ((100 0 10 100,100 1 10 101,101 1 10 102,100 0 10 100)),CURVEPOLYGON ZM (COMPOUNDCURVE ZM (CIRCULARSTRING ZM (-10 0 10 0,0 10 10 0,10 0 10 0),CIRCULARSTRING ZM (10 0 10 0,0 -1010 10 0,-10 0 10 0))))'),
    (ogr.wkbGeometryCollection25D, ogr.wkbUnknown | ogr.wkb25DBit,  'GEOMETRYCOLLECTION Z (TIN Z (((0 0 10,0 1 11,1 1 12,0 0 10)),((0 0 10,1 1 12,1 0 11,0 0 10))),TIN Z (((10 10 0,10 11 0,11 11 0,10 10 0))))', None),
]
@pytest.mark.parametrize('geom_type,read_geom_type,wkt,expected_wkt', testdata)
def test_ogr_openfilegdb_write_all_geoms(geom_type, read_geom_type, wkt, expected_wkt):

    dirname = '/vsimem/out.gdb'
    try:
        ds = ogr.GetDriverByName('OpenFileGDB').CreateDataSource(dirname)
        assert ds is not None
        options = ['XORIGIN=-1000', 'YORIGIN=-2000', 'XYSCALE=10000', 'XYTOLERANCE=0.001']
        lyr = ds.CreateLayer('test', geom_type=geom_type, options=options)
        assert lyr is not None
        f = ogr.Feature(lyr.GetLayerDefn())
        if wkt:
            ref_geom = ogr.CreateGeometryFromWkt(wkt)
            assert ref_geom is not None
        else:
            ref_geom = None
        f.SetGeometry(ref_geom)
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
        ds = None

        ds = ogr.Open(dirname)
        assert ds is not None
        lyr = ds.GetLayer(0)
        assert lyr.GetGeomType() == read_geom_type
        f = lyr.GetNextFeature()
        got_geom = f.GetGeometryRef()
        if ref_geom is None or ref_geom.IsEmpty():
            assert got_geom is None
        else:
            if expected_wkt:
                expected_geom = ogr.CreateGeometryFromWkt(expected_wkt)
            else:
                expected_geom = ogr.ForceTo(ref_geom, read_geom_type)
            assert got_geom.ExportToIsoWkt() == expected_geom.ExportToIsoWkt()

        # Test presence of a spatial index
        if ref_geom is not None and not ref_geom.IsEmpty() and \
           ogr.GT_Flatten(geom_type) != ogr.wkbPoint and \
           (ogr.GT_Flatten(geom_type) != ogr.wkbMultiPoint or ref_geom.GetPointCount() > 1) and \
           geom_type != ogr.wkbGeometryCollection25D:
            assert gdal.VSIStatL(dirname + '/a00000009.spx') is not None
            minx, maxx, miny, maxy = ref_geom.GetEnvelope()
            lyr.SetSpatialFilterRect(minx, miny, maxx, maxy)
            lyr.ResetReading()
            assert lyr.GetNextFeature() is not None

        ds = None
    finally:
        gdal.RmdirRecursive(dirname)


###############################################################################

@pytest.mark.parametrize('geom_type,wkt', [
    (ogr.wkbPoint, 'LINESTRING (0 0,1 1)'),
    (ogr.wkbLineString, 'POINT (0 0)'),
    (ogr.wkbPolygon, 'LINESTRING (0 0,1 1)'),
    (ogr.wkbTINZ, 'LINESTRING (0 0,1 1)'),
])
def test_ogr_openfilegdb_write_bad_geoms(geom_type, wkt):

    dirname = '/vsimem/out.gdb'
    try:
        ds = ogr.GetDriverByName('OpenFileGDB').CreateDataSource(dirname)
        assert ds is not None
        lyr = ds.CreateLayer('test', geom_type=geom_type)
        assert lyr is not None
        f = ogr.Feature(lyr.GetLayerDefn())
        ref_geom = ogr.CreateGeometryFromWkt(wkt)
        f.SetGeometry(ref_geom)
        with gdaltest.error_handler():
            assert lyr.CreateFeature(f) != ogr.OGRERR_NONE
        ds = None
    finally:
        gdal.RmdirRecursive(dirname)


###############################################################################


def test_ogr_openfilegdb_write_text_utf16():

    dirname = '/vsimem/out.gdb'
    try:
        ds = ogr.GetDriverByName('OpenFileGDB').CreateDataSource(dirname)
        lyr = ds.CreateLayer('test', geom_type=ogr.wkbNone,
                             options = ['CONFIGURATION_KEYWORD=TEXT_UTF16'])
        assert lyr is not None
        fld_defn = ogr.FieldDefn('str', ogr.OFTString)
        fld_defn.SetDefault("'éven'")
        assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField('str', 'évenéven')
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
        ds = None

        ds = ogr.Open(dirname)
        assert ds is not None
        lyr = ds.GetLayer(0)
        fld_defn = lyr.GetLayerDefn().GetFieldDefn(0)
        assert fld_defn.GetDefault() == "'éven'"
        f = lyr.GetNextFeature()
        assert f['str'] == 'évenéven'
        ds = None

    finally:
        gdal.RmdirRecursive(dirname)


###############################################################################


def gdbtablx_has_bitmap(gdbtablx_filename):
    def read_uint32(f):
        v = gdal.VSIFReadL(1, 4, f)
        return struct.unpack('<I', v)[0]

    f = gdal.VSIFOpenL(gdbtablx_filename, 'rb')
    assert f
    gdal.VSIFSeekL(f, 4, 0)
    n1024Blocks = read_uint32(f)
    read_uint32(f) # nfeaturesx
    size_tablx_offsets = read_uint32(f)
    if n1024Blocks != 0:
        gdal.VSIFSeekL(f, size_tablx_offsets * 1024 * n1024Blocks + 16, 0)
        nBitmapInt32Words = read_uint32(f)
        if nBitmapInt32Words != 0:
            gdal.VSIFCloseL(f)
            return True
    gdal.VSIFCloseL(f)
    return False


###############################################################################


@pytest.mark.parametrize('has_bitmap,ids',[
    (False, (1,)),       # First feature of first page
    (False, (1,(1,False))), # Inserting already inserted feature
    (False, (1024,)),    # Last feature of first page
    (False, (1, 1025)),
    (False, (1, 1025, 2049)),
    (False, (1, 1025, 2)),
    (False, (1, 1025, 2, 2049)),
    (True,  (1025,)),    # First feature of second page
    (True,  (1025,(1, False))), # Cannot add 1 as it is in the first page, which is unallocated
    (True,  (1025,1026)),
    (True,  (1026,2049,1025)),
    (True,  (1026,2049,1025,2050)),
    (True,  (1025,1+4*1024)),
    (True,  (1025,1+9*1024)), # 2-byte bitmap
    (True,  ((1 << 31) - 1,)), # Biggest possible FID
    (False, (((1 << 31), False),)), # Illegal FID
    (False, ((0, False),)), # Illegal FID
    (False, ((-2, False),)), # Illegal FID
])
@pytest.mark.parametrize('sync', [True, False])
def test_ogr_openfilegdb_write_create_feature_with_id_set(has_bitmap, ids, sync):

    dirname = '/vsimem/out.gdb'
    try:
        ds = ogr.GetDriverByName('OpenFileGDB').CreateDataSource(dirname)
        assert ds is not None
        lyr = ds.CreateLayer('test', geom_type = ogr.wkbNone)
        lyr.CreateField(ogr.FieldDefn('int', ogr.OFTInteger))
        for id in ids:
            if isinstance(id, tuple):
                id, ok = id
            else:
                ok = True
            f = ogr.Feature(lyr.GetLayerDefn())
            f.SetFID(id)
            if id < (1 << 31):
                f.SetField(0, id)
            if ok:
                assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
            else:
                with gdaltest.error_handler():
                    assert lyr.CreateFeature(f) != ogr.OGRERR_NONE
            if sync:
                lyr.SyncToDisk()
        ds = None

        if has_bitmap:
            assert gdbtablx_has_bitmap(dirname + '/a00000009.gdbtablx')
        else:
            assert not gdbtablx_has_bitmap(dirname + '/a00000009.gdbtablx')

        # Check that everything has been written correctly
        ds = ogr.Open(dirname)
        lyr = ds.GetLayer(0)
        ids_only = []
        for id in ids:
            if isinstance(id, tuple):
                id, ok = id
                if ok:
                    ids_only.append(id)
            else:
                ids_only.append(id)
        for id in sorted(ids_only):
            gdal.ErrorReset()
            f = lyr.GetNextFeature()
            assert gdal.GetLastErrorMsg() == ''
            assert f.GetFID() == id
            assert f[0] == id
        assert lyr.GetNextFeature() is None
        ds = None
    finally:
        gdal.RmdirRecursive(dirname)


###############################################################################


def test_ogr_openfilegdb_write_delete_feature():

    dirname = '/vsimem/out.gdb'
    try:
        ds = ogr.GetDriverByName('OpenFileGDB').CreateDataSource(dirname)
        lyr = ds.CreateLayer('test', geom_type = ogr.wkbPoint)
        assert lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn())) == ogr.OGRERR_NONE
        assert lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn())) == ogr.OGRERR_NONE
        assert lyr.DeleteFeature(1) == ogr.OGRERR_NONE
        assert lyr.DeleteFeature(0) == ogr.OGRERR_NON_EXISTING_FEATURE
        assert lyr.DeleteFeature(1) == ogr.OGRERR_NON_EXISTING_FEATURE
        assert lyr.DeleteFeature(3) == ogr.OGRERR_NON_EXISTING_FEATURE
        assert lyr.DeleteFeature(-1) == ogr.OGRERR_NON_EXISTING_FEATURE
        ds = None

        ds = ogr.Open(dirname)
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 1
        f = lyr.GetNextFeature()
        assert f.GetFID() == 2
        ds = None

    finally:
        gdal.RmdirRecursive(dirname)


###############################################################################


def test_ogr_openfilegdb_write_update_feature():

    dirname = '/vsimem/out.gdb'
    try:
        ds = ogr.GetDriverByName('OpenFileGDB').CreateDataSource(dirname)
        lyr = ds.CreateLayer('test', geom_type = ogr.wkbPoint)
        lyr.CreateField(ogr.FieldDefn('str', ogr.OFTString))

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField('str', 'one')
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetFID(3)
        f.SetField('str', 'three')
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetFID(4)
        f.SetField('str', 'four')
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetFID(0)
        assert lyr.SetFeature(f) == ogr.OGRERR_NON_EXISTING_FEATURE

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetFID(5)
        assert lyr.SetFeature(f) == ogr.OGRERR_NON_EXISTING_FEATURE

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetFID(2)
        assert lyr.SetFeature(f) == ogr.OGRERR_NON_EXISTING_FEATURE

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetFID(1)
        # rewrite same size
        f.SetField('str', 'ONE')
        assert lyr.SetFeature(f) == ogr.OGRERR_NONE

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetFID(4)
        # larger feature
        f.SetField('str', 'four4')
        assert lyr.SetFeature(f) == ogr.OGRERR_NONE

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetFID(3)
        # smaller feature
        f.SetField('str', '3')
        assert lyr.SetFeature(f) == ogr.OGRERR_NONE

        ds = None

        ds = ogr.Open(dirname)
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 3

        f = lyr.GetNextFeature()
        assert f['str'] == 'ONE'

        f = lyr.GetNextFeature()
        assert f['str'] == '3'

        f = lyr.GetNextFeature()
        assert f['str'] == 'four4'
        ds = None

    finally:
        gdal.RmdirRecursive(dirname)


###############################################################################


def test_ogr_openfilegdb_write_add_field_to_non_empty_table():

    dirname = '/vsimem/out.gdb'
    try:
        ds = ogr.GetDriverByName('OpenFileGDB').CreateDataSource(dirname)
        lyr = ds.CreateLayer('test', geom_type = ogr.wkbPoint)
        lyr.CreateField(ogr.FieldDefn('str', ogr.OFTString))

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField('str', 'one')
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
        f = None

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField('str', 'two')
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
        f = None

        fld_defn = ogr.FieldDefn('cannot_add_non_nullable_field_without_default_val', ogr.OFTString)
        fld_defn.SetNullable(False)
        with gdaltest.error_handler():
            assert lyr.CreateField(fld_defn) != ogr.OGRERR_NONE

        # No need to rewrite the file
        assert lyr.CreateField(ogr.FieldDefn('str2', ogr.OFTString)) == ogr.OGRERR_NONE
        assert lyr.CreateField(ogr.FieldDefn('str3', ogr.OFTString)) == ogr.OGRERR_NONE
        assert lyr.CreateField(ogr.FieldDefn('str4', ogr.OFTString)) == ogr.OGRERR_NONE
        assert lyr.CreateField(ogr.FieldDefn('str5', ogr.OFTString)) == ogr.OGRERR_NONE
        assert lyr.CreateField(ogr.FieldDefn('str6', ogr.OFTString)) == ogr.OGRERR_NONE
        assert lyr.CreateField(ogr.FieldDefn('str7', ogr.OFTString)) == ogr.OGRERR_NONE

        assert lyr.SyncToDisk() == ogr.OGRERR_NONE

        ds = None

        ds = ogr.Open(dirname)
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        assert f['str'] == 'one'
        assert f['str2'] is None
        assert f['str7'] is None
        f = lyr.GetNextFeature()
        assert f['str'] == 'two'
        assert f['str2'] is None
        assert f['str7'] is None
        ds = None

    finally:
        gdal.RmdirRecursive(dirname)


###############################################################################


def test_ogr_openfilegdb_write_add_field_to_non_empty_table_extra_nullable():

    dirname = '/vsimem/out.gdb'
    try:
        ds = ogr.GetDriverByName('OpenFileGDB').CreateDataSource(dirname)
        lyr = ds.CreateLayer('test', geom_type = ogr.wkbPoint)
        lyr.CreateField(ogr.FieldDefn('str', ogr.OFTString))

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField('str', 'one')
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
        f = None

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField('str', 'two')
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
        f = None

        assert lyr.CreateField(ogr.FieldDefn('str2', ogr.OFTString)) == ogr.OGRERR_NONE
        assert lyr.CreateField(ogr.FieldDefn('str3', ogr.OFTString)) == ogr.OGRERR_NONE
        assert lyr.CreateField(ogr.FieldDefn('str4', ogr.OFTString)) == ogr.OGRERR_NONE
        assert lyr.CreateField(ogr.FieldDefn('str5', ogr.OFTString)) == ogr.OGRERR_NONE
        assert lyr.CreateField(ogr.FieldDefn('str6', ogr.OFTString)) == ogr.OGRERR_NONE
        assert lyr.CreateField(ogr.FieldDefn('str7', ogr.OFTString)) == ogr.OGRERR_NONE

        # Will trigger a table rewrite
        assert lyr.CreateField(ogr.FieldDefn('str8', ogr.OFTString)) == ogr.OGRERR_NONE

        assert lyr.SyncToDisk() == ogr.OGRERR_NONE

        ds = None

        ds = ogr.Open(dirname)
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        assert f['str'] == 'one'
        assert f['str2'] is None
        assert f['str7'] is None
        assert f['str8'] is None
        f = lyr.GetNextFeature()
        assert f['str'] == 'two'
        assert f['str2'] is None
        assert f['str7'] is None
        assert f['str8'] is None
        ds = None

    finally:
        gdal.RmdirRecursive(dirname)


###############################################################################

modify_inplace_options = [
    {'OPENFILEGDB_MODIFY_IN_PLACE': 'FALSE'},
    {'OPENFILEGDB_MODIFY_IN_PLACE': 'TRUE'},
]
if sys.platform != 'win32':
    modify_inplace_options.append(
        {'OPENFILEGDB_MODIFY_IN_PLACE':'FALSE',
         'OPENFILEGDB_SIMUL_WIN32': 'TRUE'})

@pytest.mark.parametrize('options', modify_inplace_options)
@pytest.mark.parametrize('dirname', ['/vsimem/out.gdb', 'tmp/add_field_to_non_empty_table_extra_non_nullable.gdb'])
def test_ogr_openfilegdb_write_add_field_to_non_empty_table_extra_non_nullable(options, dirname):

    with gdaltest.config_options(options):
        try:
            ds = ogr.GetDriverByName('OpenFileGDB').CreateDataSource(dirname)
            lyr = ds.CreateLayer('test', geom_type = ogr.wkbPoint)
            lyr.CreateField(ogr.FieldDefn('str', ogr.OFTString))

            f = ogr.Feature(lyr.GetLayerDefn())
            f.SetField('str', 'one')
            assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
            f = None

            f = ogr.Feature(lyr.GetLayerDefn())
            f.SetField('str', 'two')
            assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
            f = None

            fld_defn = ogr.FieldDefn('str2', ogr.OFTString)
            fld_defn.SetNullable(False)
            fld_defn.SetDefault("'default val'")
            assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

            fld_defn = ogr.FieldDefn('int16', ogr.OFTInteger)
            fld_defn.SetSubType(ogr.OFSTInt16)
            fld_defn.SetNullable(False)
            fld_defn.SetDefault("-32768")
            assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

            fld_defn = ogr.FieldDefn('int32', ogr.OFTInteger)
            fld_defn.SetNullable(False)
            fld_defn.SetDefault('123456789')
            assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

            fld_defn = ogr.FieldDefn('float32', ogr.OFTReal)
            fld_defn.SetSubType(ogr.OFSTFloat32)
            fld_defn.SetNullable(False)
            fld_defn.SetDefault("1.25")
            assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

            fld_defn = ogr.FieldDefn('float64', ogr.OFTReal)
            fld_defn.SetNullable(False)
            fld_defn.SetDefault("1.23456789")
            assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

            fld_defn = ogr.FieldDefn('dt', ogr.OFTDateTime)
            fld_defn.SetNullable(False)
            fld_defn.SetDefault("'2022-11-04T12:34:56+02:00'")
            assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

            assert lyr.SyncToDisk() == ogr.OGRERR_NONE

            ds = None

            assert gdal.VSIStatL(dirname + '/a00000009.gdbtable.backup') is None
            assert gdal.VSIStatL(dirname + '/a00000009.gdbtablx.backup') is None
            assert gdal.VSIStatL(dirname + '/a00000009.gdbtable.compress') is None
            assert gdal.VSIStatL(dirname + '/a00000009.gdbtablx.compress') is None

            ds = ogr.Open(dirname)
            lyr = ds.GetLayer(0)
            f = lyr.GetNextFeature()
            assert f['str'] == 'one'
            assert f['str2'] == 'default val'
            assert f['int16'] == -32768
            assert f['int32'] == 123456789
            assert f['float32'] == 1.25
            assert f['float64'] == 1.23456789
            assert f['dt'] == '2022/11/04 10:34:56+00'
            f = lyr.GetNextFeature()
            assert f['str'] == 'two'
            assert f['str2'] == 'default val'
            ds = None

        finally:
            gdal.RmdirRecursive(dirname)


###############################################################################


@pytest.mark.parametrize('options', modify_inplace_options)
@pytest.mark.parametrize('dirname', ['/vsimem/out.gdb', 'tmp/add_field_to_non_empty_table_extra_non_nullable_simul_error.gdb'])
def test_ogr_openfilegdb_write_add_field_to_non_empty_table_extra_non_nullable_simul_error(options, dirname):

    with gdaltest.config_options(options):
        try:
            ds = ogr.GetDriverByName('OpenFileGDB').CreateDataSource(dirname)
            lyr = ds.CreateLayer('test', geom_type = ogr.wkbPoint)
            lyr.CreateField(ogr.FieldDefn('str', ogr.OFTString))

            f = ogr.Feature(lyr.GetLayerDefn())
            f.SetField('str', 'one')
            assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
            f = None

            f = ogr.Feature(lyr.GetLayerDefn())
            f.SetField('str', 'two')
            assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
            f = None

            fld_defn = ogr.FieldDefn('str2', ogr.OFTString)
            fld_defn.SetNullable(False)
            fld_defn.SetDefault("'default val'")
            with gdaltest.error_handler():
                with gdaltest.config_option('OPENFILEGDB_SIMUL_ERROR_IN_RewriteTableToAddLastAddedField', 'TRUE'):
                    assert lyr.CreateField(fld_defn) != ogr.OGRERR_NONE

            ds = None

            assert gdal.VSIStatL(dirname + '/a00000009.gdbtable.backup') is None
            assert gdal.VSIStatL(dirname + '/a00000009.gdbtablx.backup') is None
            assert gdal.VSIStatL(dirname + '/a00000009.gdbtable.compress') is None
            assert gdal.VSIStatL(dirname + '/a00000009.gdbtablx.compress') is None

            ds = ogr.Open(dirname)
            lyr = ds.GetLayer(0)
            assert lyr.GetLayerDefn().GetFieldCount() == 1
            f = lyr.GetNextFeature()
            assert f['str'] == 'one'
            f = lyr.GetNextFeature()
            assert f['str'] == 'two'
            ds = None

        finally:
            gdal.RmdirRecursive(dirname)


###############################################################################


def test_ogr_openfilegdb_write_add_field_after_reopening():

    dirname = '/vsimem/out.gdb'
    try:
        ds = ogr.GetDriverByName('OpenFileGDB').CreateDataSource(dirname)
        assert ds is not None
        ds = None

        ds = ogr.Open(dirname, update = 1)
        lyr = ds.CreateLayer('test', geom_type = ogr.wkbPoint)
        lyr.CreateField(ogr.FieldDefn('str', ogr.OFTString))

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField('str', 'one')
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
        f = None

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField('str', 'two')
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
        f = None

        ds = None

        ds = ogr.Open(dirname, update = 1)
        lyr = ds.GetLayer(0)
        assert lyr.CreateField(ogr.FieldDefn('str2', ogr.OFTString)) == ogr.OGRERR_NONE
        ds = None

        ds = ogr.Open(dirname)
        lyr = ds.GetLayer(0)
        assert lyr.GetLayerDefn().GetFieldCount() == 2
        f = lyr.GetNextFeature()
        assert f['str'] == 'one'
        assert f['str2'] is None
        f = lyr.GetNextFeature()
        assert f['str'] == 'two'

        sql_lyr = ds.ExecuteSQL('GetLayerDefinition test')
        assert sql_lyr
        f = sql_lyr.GetNextFeature()
        xml = f.GetField(0)
        f = None
        ds.ReleaseResultSet(sql_lyr)

        assert '<Name>str</Name>' in xml
        assert '<Name>str2</Name>' in xml

        ds = None

    finally:
        gdal.RmdirRecursive(dirname)


###############################################################################


@pytest.mark.parametrize('use_synctodisk', [False, True])
@pytest.mark.parametrize('field_to_delete', [0, 1])
def test_ogr_openfilegdb_write_delete_field(use_synctodisk, field_to_delete):

    dirname = '/vsimem/out.gdb'
    try:
        ds = ogr.GetDriverByName('OpenFileGDB').CreateDataSource(dirname)
        lyr = ds.CreateLayer('test', geom_type = ogr.wkbPoint)

        assert lyr.CreateField(ogr.FieldDefn('str1', ogr.OFTString)) == ogr.OGRERR_NONE

        fld_defn = ogr.FieldDefn('str2', ogr.OFTString)
        fld_defn.SetNullable(False)
        assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

        assert lyr.CreateField(ogr.FieldDefn('str3', ogr.OFTString)) == ogr.OGRERR_NONE

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField('str1', 'str1_1')
        f.SetField('str2', 'str2_1')
        f.SetField('str3', 'str3_1')
        f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1 2)'))
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
        f = None

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField('str1', 'str1_2')
        f.SetField('str2', 'str2_2')
        f.SetField('str3', 'str3_2')
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
        f = None

        if use_synctodisk:
            assert lyr.SyncToDisk() == ogr.OGRERR_NONE

        assert lyr.DeleteField(field_to_delete) == ogr.OGRERR_NONE
        assert lyr.GetLayerDefn().GetFieldCount() == 2

        if field_to_delete == 0:
            other_field = 'str2'
        else:
            other_field = 'str1'

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField(other_field, 'str2_3')
        f.SetField('str3', 'str3_3')
        f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(2 3)'))
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
        f = None

        def check_values(lyr):
            f = lyr.GetNextFeature()
            assert f[other_field].endswith('_1')
            assert f['str3'] == 'str3_1'
            assert f.GetGeometryRef() is not None
            f = None

            f = lyr.GetNextFeature()
            assert f[other_field].endswith('_2')
            assert f['str3'] == 'str3_2'
            assert f.GetGeometryRef() is None
            f = None

            f = lyr.GetNextFeature()
            assert f[other_field].endswith('_3')
            assert f['str3'] == 'str3_3'
            assert f.GetGeometryRef() is not None


        check_values(lyr)

        assert lyr.SyncToDisk() == ogr.OGRERR_NONE

        lyr.ResetReading()
        check_values(lyr)

        ds = None

        ds = ogr.Open(dirname)
        lyr = ds.GetLayer(0)

        check_values(lyr)

        sql_lyr = ds.ExecuteSQL('GetLayerDefinition test')
        assert sql_lyr
        f = sql_lyr.GetNextFeature()
        xml = f.GetField(0)
        f = None
        ds.ReleaseResultSet(sql_lyr)

        if field_to_delete == 0:
            assert '<Name>str1</Name>' not in xml
            assert '<Name>str2</Name>' in xml
        else:
            assert '<Name>str1</Name>' in xml
            assert '<Name>str2</Name>' not in xml
        assert '<Name>str3</Name>' in xml

        ds = None

    finally:
        gdal.RmdirRecursive(dirname)


###############################################################################


def test_ogr_openfilegdb_write_delete_field_before_geom():

    dirname = '/vsimem/out.gdb'
    try:
        ds = ogr.GetDriverByName('OpenFileGDB').CreateDataSource(dirname)

        with gdaltest.config_option('OPENFILEGDB_CREATE_FIELD_BEFORE_GEOMETRY', 'YES'):
            lyr = ds.CreateLayer('test', geom_type = ogr.wkbPoint)

        assert lyr.CreateField(ogr.FieldDefn('str', ogr.OFTString)) == ogr.OGRERR_NONE

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField('field_before_geom', 'to be deleted')
        f.SetField('str', 'foo')
        f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1 2)'))
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
        f = None

        assert lyr.DeleteField(lyr.GetLayerDefn().GetFieldIndex('field_before_geom')) == ogr.OGRERR_NONE

        lyr.ResetReading()
        f = lyr.GetNextFeature()
        assert f.GetField('str') == 'foo'
        assert f.GetGeometryRef() is not None

        ds = None

    finally:
        gdal.RmdirRecursive(dirname)


###############################################################################


def test_ogr_openfilegdb_write_feature_dataset_no_crs():

    dirname = '/vsimem/out.gdb'
    try:
        ds = ogr.GetDriverByName('OpenFileGDB').CreateDataSource(dirname)
        assert ds is not None
        ds = ogr.Open(dirname, update = 1)
        lyr = ds.CreateLayer('test', geom_type = ogr.wkbPoint,
                             options = ['FEATURE_DATASET=my_feature_dataset'])
        assert lyr is not None
        lyr = ds.CreateLayer('test2', geom_type = ogr.wkbPoint,
                             options = ['FEATURE_DATASET=my_feature_dataset'])
        assert lyr is not None
        ds = None

        ds = gdal.OpenEx(dirname)
        rg = ds.GetRootGroup()

        assert rg.GetGroupNames() == ['my_feature_dataset']

        fd = rg.OpenGroup('my_feature_dataset')
        assert fd is not None
        assert fd.GetVectorLayerNames() == ['test', 'test2']

        lyr = ds.GetLayerByName('GDB_Items')
        assert lyr.GetFeatureCount() == 5  # == root, workspace, feature dataset, 2 layers

        lyr = ds.GetLayerByName('GDB_ItemRelationships')
        assert lyr.GetFeatureCount() == 3  # == feature dataset, 2 layers

    finally:
        gdal.RmdirRecursive(dirname)


###############################################################################


def test_ogr_openfilegdb_write_feature_dataset_crs():

    dirname = '/vsimem/out.gdb'
    try:
        ds = ogr.GetDriverByName('OpenFileGDB').CreateDataSource(dirname)

        srs = osr.SpatialReference()
        srs.ImportFromEPSG(4326)

        lyr = ds.CreateLayer('test', geom_type = ogr.wkbPoint,
                             srs = srs,
                             options = ['FEATURE_DATASET=my_feature_dataset'])
        assert lyr is not None

        lyr = ds.CreateLayer('test2', geom_type = ogr.wkbPoint,
                             srs = srs,
                             options = ['FEATURE_DATASET=my_feature_dataset'])
        assert lyr is not None

        lyr = ds.CreateLayer('inherited_srs', geom_type = ogr.wkbPoint,
                             options = ['FEATURE_DATASET=my_feature_dataset'])
        assert lyr is not None

        other_srs = osr.SpatialReference()
        other_srs.ImportFromEPSG(4269)

        with gdaltest.error_handler():
            lyr = ds.CreateLayer('other_srs', geom_type = ogr.wkbPoint,
                                 srs = other_srs,
                                 options = ['FEATURE_DATASET=my_feature_dataset'])
            assert lyr is None

        ds = None

        ds = gdal.OpenEx(dirname)
        lyr = ds.GetLayerByName('inherited_srs')
        srs = lyr.GetSpatialRef()
        assert srs is not None
        assert srs.GetAuthorityCode(None) == '4326'

    finally:
        gdal.RmdirRecursive(dirname)



###############################################################################


@pytest.mark.parametrize('numPoints,maxFeaturesPerSpxPage', [
    (1, 2), # depth 1
    (2, 2),
    (3, 2), # depth 2
    (4, 2),
    (5, 2),
    (6, 2), # depth 3
    (7, 2),
    (8, 2),
    (9, 2),
    (10, 2),
    (11, 2),
    (12, 2),
    (13, 2),
    (14, 2),
    (15, 2), # depth 4
    (16, 2),
    (29, 2),
    (30, 2),
    (31, 2), # depth 5 -> unsupported

    # With default value for maxFeaturesPerSpxPage (340)
    (339, None), # depth 1
    (340, None), # depth 1
    (341, None), # depth 2
    #  (340*341, None), # depth 2   # a bit too slow for unit tests
])
def test_ogr_openfilegdb_write_spatial_index(numPoints,maxFeaturesPerSpxPage):

    dirname = '/vsimem/out.gdb'
    try:
        ds = ogr.GetDriverByName('OpenFileGDB').CreateDataSource(dirname)
        lyr = ds.CreateLayer('points', geom_type = ogr.wkbPoint)
        for j in range(numPoints):
            feat = ogr.Feature(lyr.GetLayerDefn())
            geom = ogr.CreateGeometryFromWkt('POINT(%d %d)' % (j, j))
            feat.SetGeometry(geom)
            lyr.CreateFeature(feat)
        with gdaltest.config_option('OPENFILEGDB_MAX_FEATURES_PER_SPX_PAGE', str(maxFeaturesPerSpxPage) if maxFeaturesPerSpxPage else None):
                if maxFeaturesPerSpxPage == 2 and numPoints > 30:
                    with gdaltest.error_handler():
                        gdal.ErrorReset()
                        lyr.SyncToDisk()
                        assert gdal.GetLastErrorMsg() != ''
                else:
                    gdal.ErrorReset()
                    lyr.SyncToDisk()
                    assert gdal.GetLastErrorMsg() == ''
        ds = None

        ds = ogr.Open(dirname)
        lyr = ds.GetLayer(0)
        if numPoints > 1000:
            j = 0
            lyr.SetSpatialFilterRect(j-0.1, j-0.1, j+0.1, j+0.1)
            lyr.ResetReading()
            f = lyr.GetNextFeature()
            assert f is not None

            j = numPoints - 1
            lyr.SetSpatialFilterRect(j-0.1, j-0.1, j+0.1, j+0.1)
            lyr.ResetReading()
            f = lyr.GetNextFeature()
            assert f is not None
        else:
            for j in range(numPoints):
                lyr.SetSpatialFilterRect(j-0.1, j-0.1, j+0.1, j+0.1)
                lyr.ResetReading()
                f = lyr.GetNextFeature()
                assert f is not None, j
        ds = None

    finally:
        gdal.RmdirRecursive(dirname)


###############################################################################

def test_ogr_openfilegdb_write_attribute_index():

    dirname = '/vsimem/out.gdb'
    try:
        ds = ogr.GetDriverByName('OpenFileGDB').CreateDataSource(dirname)
        lyr = ds.CreateLayer('test', geom_type = ogr.wkbPoint)
        fld_defn = ogr.FieldDefn('int16', ogr.OFTInteger)
        fld_defn.SetSubType(ogr.OFSTInt16)
        lyr.CreateField(fld_defn)
        fld_defn = ogr.FieldDefn('int32', ogr.OFTInteger)
        lyr.CreateField(fld_defn)
        fld_defn = ogr.FieldDefn('float32', ogr.OFTReal)
        fld_defn.SetSubType(ogr.OFSTFloat32)
        lyr.CreateField(fld_defn)
        fld_defn = ogr.FieldDefn('float64', ogr.OFTReal)
        lyr.CreateField(fld_defn)
        fld_defn = ogr.FieldDefn('str', ogr.OFTString)
        lyr.CreateField(fld_defn)
        fld_defn = ogr.FieldDefn('lower_str', ogr.OFTString)
        lyr.CreateField(fld_defn)
        fld_defn = ogr.FieldDefn('dt', ogr.OFTDateTime)
        lyr.CreateField(fld_defn)

        f = ogr.Feature(lyr.GetLayerDefn())
        f['int16'] = -1234
        f['int32'] = -12346789
        f['float32'] = 1.25
        f['float64'] = 1.256789
        f['str'] = 'my str'
        f['lower_str'] = 'MY STR'
        f['dt'] = '2022-06-03T16:06:00Z'
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

        f = ogr.Feature(lyr.GetLayerDefn())
        f['str'] = 'x' * 100
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

        f = ogr.Feature(lyr.GetLayerDefn())
        f['str'] = ('x' * 100) + 'y'
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
        f = None

        # Errors of index creation
        with gdaltest.error_handler():
            gdal.ErrorReset()
            ds.ExecuteSQL('CREATE INDEX this_name_is_wayyyyy_tooo_long ON test(int16)')
            assert gdal.GetLastErrorMsg() != ''

            gdal.ErrorReset()
            ds.ExecuteSQL('CREATE INDEX idx_int16 ON non_existing_layer(int16)')
            assert gdal.GetLastErrorMsg() != ''

            gdal.ErrorReset()
            ds.ExecuteSQL('CREATE INDEX invalid_field ON test(invalid_field)')
            assert gdal.GetLastErrorMsg() != ''

            # Reserved keyword
            gdal.ErrorReset()
            ds.ExecuteSQL('CREATE INDEX SELECT ON test(int16)')
            assert gdal.GetLastErrorMsg() != ''

            gdal.ErrorReset()
            ds.ExecuteSQL('CREATE INDEX _starting_by_ ON test(int16)')
            assert gdal.GetLastErrorMsg() != ''

            gdal.ErrorReset()
            ds.ExecuteSQL('CREATE INDEX a&b ON test(int16)')
            assert gdal.GetLastErrorMsg() != ''

        # Create indexes
        gdal.ErrorReset()
        for i in range(lyr.GetLayerDefn().GetFieldCount()):
            fld_name = lyr.GetLayerDefn().GetFieldDefn(i).GetName()
            if fld_name == 'lower_str':
                ds.ExecuteSQL('CREATE INDEX idx_%s ON test(LOWER(%s))' % (fld_name, fld_name))
            else:
                ds.ExecuteSQL('CREATE INDEX idx_%s ON test(%s)' % (fld_name, fld_name))
            assert gdal.GetLastErrorMsg() == ''
            assert gdal.VSIStatL(dirname + '/a00000009.idx_' + fld_name + '.atx') is not None

        fld_defn = ogr.FieldDefn('unindexed', ogr.OFTString)
        assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

        with gdaltest.error_handler():
            # Re-using an index name
            gdal.ErrorReset()
            ds.ExecuteSQL('CREATE INDEX idx_int16 ON test(unindexed)')
            assert gdal.GetLastErrorMsg() != ''

            # Trying to index twice a field
            gdal.ErrorReset()
            ds.ExecuteSQL('CREATE INDEX int16_again ON test(int16)')
            assert gdal.GetLastErrorMsg() != ''

            gdal.ErrorReset()
            ds.ExecuteSQL('CREATE INDEX lower_str_again ON test(lower_str)')
            assert gdal.GetLastErrorMsg() != ''

        ds = None

        def check_index_fully_used(ds, lyr):
            sql_lyr = ds.ExecuteSQL('GetLayerAttrIndexUse ' + lyr.GetName())
            attr_index_use = int(sql_lyr.GetNextFeature().GetField(0))
            ds.ReleaseResultSet(sql_lyr)
            assert attr_index_use == 2 # IteratorSufficientToEvaluateFilter

        ds = ogr.Open(dirname, update=1)
        lyr = ds.GetLayer(0)

        lyr.SetAttributeFilter('int16 = -1234')
        check_index_fully_used(ds, lyr)
        assert lyr.GetFeatureCount() == 1

        lyr.SetAttributeFilter('int16 = 1234')
        assert lyr.GetFeatureCount() == 0

        lyr.SetAttributeFilter('int32 = -12346789')
        check_index_fully_used(ds, lyr)
        assert lyr.GetFeatureCount() == 1

        lyr.SetAttributeFilter('int32 = 12346789')
        assert lyr.GetFeatureCount() == 0

        lyr.SetAttributeFilter('float32 = 1.25')
        check_index_fully_used(ds, lyr)
        assert lyr.GetFeatureCount() == 1

        lyr.SetAttributeFilter('float32 = -1.25')
        assert lyr.GetFeatureCount() == 0

        lyr.SetAttributeFilter('float64 = 1.256789')
        assert lyr.GetFeatureCount() == 1

        lyr.SetAttributeFilter('float64 = -1.256789')
        assert lyr.GetFeatureCount() == 0

        lyr.SetAttributeFilter("str = 'my str'")
        assert lyr.GetFeatureCount() == 1

        lyr.SetAttributeFilter("str = 'MY STR'")
        assert lyr.GetFeatureCount() == 0

        lyr.SetAttributeFilter("str = 'my st'")
        assert lyr.GetFeatureCount() == 0

        lyr.SetAttributeFilter("str = 'my str2'")
        assert lyr.GetFeatureCount() == 0

        # Test truncation to 80 characters
        #lyr.SetAttributeFilter("str = '%s'" % ('x' * 100))
        #assert lyr.GetFeatureCount() == 1

        #lyr.SetAttributeFilter("str = '%s'" % ('x' * 100 + 'y'))
        #assert lyr.GetFeatureCount() == 1

        #lyr.SetAttributeFilter("str = '%s'" % ('x' * 100 + 'z'))
        #assert lyr.GetFeatureCount() == 0

        # Actually should be "LOWER(lower_str) = 'my str'" ...
        # so this test may break if we implement this in a cleaner way
        lyr.SetAttributeFilter("lower_str = 'my str'")
        assert lyr.GetFeatureCount() == 1

        lyr.SetAttributeFilter("dt = '2022/06/03 16:06:00Z'")
        check_index_fully_used(ds, lyr)
        assert lyr.GetFeatureCount() == 1

        # Check that .gdbindexes is properly updated on field renaming
        fld_defn = ogr.FieldDefn('int32_renamed', ogr.OFTInteger)
        assert lyr.AlterFieldDefn(lyr.GetLayerDefn().GetFieldIndex('int32'), fld_defn, ogr.ALTER_ALL_FLAG) == ogr.OGRERR_NONE

        lyr.SetAttributeFilter('int32_renamed = -12346789')
        check_index_fully_used(ds, lyr)
        assert lyr.GetFeatureCount() == 1

        ds = None

        ds = ogr.Open(dirname, update=1)
        lyr = ds.GetLayer(0)

        lyr.SetAttributeFilter('int32_renamed = -12346789')
        check_index_fully_used(ds, lyr)
        assert lyr.GetFeatureCount() == 1

        # Check that the index is destroy on field deletion
        assert gdal.VSIStatL(dirname + '/a00000009.idx_int32.atx') is not None
        assert lyr.DeleteField(lyr.GetLayerDefn().GetFieldIndex('int32_renamed')) == ogr.OGRERR_NONE
        assert gdal.VSIStatL(dirname + '/a00000009.idx_int32.atx') is None

        ds = None

        ds = ogr.Open(dirname, update=1)
        lyr = ds.GetLayer(0)

        lyr.SetAttributeFilter('int16 = -1234')
        check_index_fully_used(ds, lyr)
        assert lyr.GetFeatureCount() == 1

        lyr.SetAttributeFilter('float32 = 1.25')
        check_index_fully_used(ds, lyr)
        assert lyr.GetFeatureCount() == 1

        ds = None

    finally:
        gdal.RmdirRecursive(dirname)


###############################################################################


def test_ogr_openfilegdb_write_delete_layer():

    dirname = '/vsimem/out.gdb'
    try:
        ds = ogr.GetDriverByName('OpenFileGDB').CreateDataSource(dirname)
        assert ds is not None
        ds = ogr.Open(dirname, update = 1)
        ds.CreateLayer('test', geom_type = ogr.wkbPoint)
        ds.CreateLayer('test2', geom_type = ogr.wkbPoint)
        ds = None

        ds = ogr.Open(dirname, update=1)
        assert ds.TestCapability(ogr.ODsCDeleteLayer) == 1

        lyr = ds.GetLayerByName('GDB_SystemCatalog')
        assert lyr.GetFeatureCount() == 10 # 8 system tables + 2 layers

        lyr = ds.GetLayerByName('GDB_Items')
        assert lyr.GetFeatureCount() == 4 # root, workspace + 2 layers

        lyr = ds.GetLayerByName('GDB_ItemRelationships')
        assert lyr.GetFeatureCount() == 2 # 2 layers

        ds.ExecuteSQL('DELLAYER:test')
        assert ds.GetLayerCount() == 1

        for filename in gdal.ReadDir(dirname):
            assert not filename.startswith('a00000009.gdbtable')

        assert ds.DeleteLayer(-1) != ogr.OGRERR_NONE
        assert ds.DeleteLayer(1) != ogr.OGRERR_NONE

        # The following should not work
        with gdaltest.error_handler():
            gdal.ErrorReset()
            ds.ExecuteSQL('DELLAYER:not_existing')
            assert gdal.GetLastErrorMsg() != ''
        with gdaltest.error_handler():
            gdal.ErrorReset()
            ds.ExecuteSQL('DELLAYER:GDB_SystemCatalog')
            assert gdal.GetLastErrorMsg() != ''

        ds = None

        ds = ogr.Open(dirname)
        assert ds.GetLayerCount() == 1
        assert ds.GetLayer(0).GetName() == 'test2'

        lyr = ds.GetLayerByName('GDB_SystemCatalog')
        assert lyr.GetFeatureCount() == 9

        lyr = ds.GetLayerByName('GDB_Items')
        assert lyr.GetFeatureCount() == 3

        lyr = ds.GetLayerByName('GDB_ItemRelationships')
        assert lyr.GetFeatureCount() == 1

    finally:
        gdal.RmdirRecursive(dirname)



###############################################################################


def _check_freelist_consistency(ds, lyr):

    sql_lyr = ds.ExecuteSQL('CHECK_FREELIST_CONSISTENCY:' + lyr.GetName())
    f = sql_lyr.GetNextFeature()
    res = f.GetField(0)
    ds.ReleaseResultSet(sql_lyr)
    assert res == '1'

###############################################################################


def test_ogr_openfilegdb_write_freelist():

    dirname = '/vsimem/out.gdb'
    table_filename = dirname + '/a00000009.gdbtable'
    freelist_filename = dirname + '/a00000009.freelist'
    try:
        ds = ogr.GetDriverByName('OpenFileGDB').CreateDataSource(dirname)
        assert ds is not None
        ds = ogr.Open(dirname, update = 1)
        lyr = ds.CreateLayer('test', geom_type = ogr.wkbNone)
        lyr.CreateField(ogr.FieldDefn('str', ogr.OFTString))

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField('str', 'X' * 5)
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

        lyr.SyncToDisk()
        filesize = gdal.VSIStatL(table_filename).size

        assert lyr.DeleteFeature(1) == 0

        assert gdal.VSIStatL(freelist_filename) is not None
        _check_freelist_consistency(ds, lyr)

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField('str', 'Y' * 5)
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

        assert filesize == gdal.VSIStatL(table_filename).size

        f = lyr.GetNextFeature()
        assert f['str'] == 'Y' * 5

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField('str', 'X' * 6)
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
        to_delete = [ f.GetFID() ]

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField('str', 'X' * 6)
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
        to_delete.append(f.GetFID())

        filesize = gdal.VSIStatL(table_filename).size

        for fid in to_delete:
            assert lyr.DeleteFeature(fid) == 0

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField('str', 'Y' * 6)
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField('str', 'Y' * 6)
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

        assert filesize == gdal.VSIStatL(table_filename).size

        assert gdal.VSIStatL(freelist_filename) is not None
        _check_freelist_consistency(ds, lyr)

        lyr.SyncToDisk()
        assert gdal.VSIStatL(freelist_filename) is None


    finally:
        gdal.RmdirRecursive(dirname)


###############################################################################


def test_ogr_openfilegdb_write_freelist_not_exactly_matching_sizes():

    dirname = '/vsimem/out.gdb'
    table_filename = dirname + '/a00000009.gdbtable'
    freelist_filename = dirname + '/a00000009.freelist'
    try:
        ds = ogr.GetDriverByName('OpenFileGDB').CreateDataSource(dirname)
        assert ds is not None
        ds = ogr.Open(dirname, update = 1)
        lyr = ds.CreateLayer('test', geom_type = ogr.wkbNone)
        lyr.CreateField(ogr.FieldDefn('str', ogr.OFTString))

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField('str', 'X' * 500)
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField('str', 'X' * 502)
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

        lyr.SyncToDisk()
        filesize = gdal.VSIStatL(table_filename).size

        assert lyr.DeleteFeature(1) == 0
        assert lyr.DeleteFeature(2) == 0

        assert gdal.VSIStatL(freelist_filename) is not None
        _check_freelist_consistency(ds, lyr)

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField('str', 'Y' * 490)
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField('str', 'Y' * 501)
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

        f = lyr.GetNextFeature()
        assert f['str'] == 'Y' * 490

        f = lyr.GetNextFeature()
        assert f['str'] == 'Y' * 501

        assert filesize == gdal.VSIStatL(table_filename).size
        _check_freelist_consistency(ds, lyr)


    finally:
        gdal.RmdirRecursive(dirname)



###############################################################################


def test_ogr_openfilegdb_write_freelist_scenario_two_sizes():

    dirname = '/vsimem/out.gdb'
    table_filename = dirname + '/a00000009.gdbtable'
    freelist_filename = dirname + '/a00000009.freelist'
    try:
        ds = ogr.GetDriverByName('OpenFileGDB').CreateDataSource(dirname)
        assert ds is not None
        ds = ogr.Open(dirname, update = 1)
        lyr = ds.CreateLayer('test', geom_type = ogr.wkbNone)
        lyr.CreateField(ogr.FieldDefn('str', ogr.OFTString))

        NFEATURES = 400

        # 500 and 600 are in the [440, 772[ range of the freelist Fibonacci suite
        SIZE1 = 600
        SIZE2 = 500
        assert SIZE2 < SIZE1

        for i in range(NFEATURES):
            f = ogr.Feature(lyr.GetLayerDefn())
            f['str'] = 'x' * SIZE1
            assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

        for i in range(NFEATURES):
            f = ogr.Feature(lyr.GetLayerDefn())
            f['str'] = 'x' * SIZE2
            assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

        for i in range(NFEATURES):
            f = ogr.Feature(lyr.GetLayerDefn())
            f['str'] = 'x' * SIZE1
            assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

        for i in range(NFEATURES):
            f = ogr.Feature(lyr.GetLayerDefn())
            f['str'] = 'x' * SIZE2
            assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

        lyr.SyncToDisk()
        filesize = gdal.VSIStatL(table_filename).size

        for i in range(NFEATURES*4):
            assert lyr.DeleteFeature(1+i) == ogr.OGRERR_NONE

        _check_freelist_consistency(ds, lyr)

        for i in range(NFEATURES):
            f = ogr.Feature(lyr.GetLayerDefn())
            f['str'] = 'x' * SIZE1
            assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

        for i in range(NFEATURES):
            f = ogr.Feature(lyr.GetLayerDefn())
            f['str'] = 'x' * SIZE2
            assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

        for i in range(NFEATURES):
            f = ogr.Feature(lyr.GetLayerDefn())
            f['str'] = 'x' * SIZE1
            assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

        for i in range(NFEATURES):
            f = ogr.Feature(lyr.GetLayerDefn())
            f['str'] = 'x' * SIZE2
            assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

        assert filesize == gdal.VSIStatL(table_filename).size

        assert gdal.VSIStatL(freelist_filename) is not None
        _check_freelist_consistency(ds, lyr)
        lyr.SyncToDisk()
        assert gdal.VSIStatL(freelist_filename) is None

    finally:
        gdal.RmdirRecursive(dirname)



###############################################################################


def test_ogr_openfilegdb_write_freelist_scenario_random():

    import functools
    import random
    r = random.Random(0)

    dirname = '/vsimem/out.gdb'
    table_filename = dirname + '/a00000009.gdbtable'
    freelist_filename = dirname + '/a00000009.freelist'
    try:
        ds = ogr.GetDriverByName('OpenFileGDB').CreateDataSource(dirname)
        assert ds is not None
        ds = ogr.Open(dirname, update = 1)
        lyr = ds.CreateLayer('test', geom_type = ogr.wkbNone)
        lyr.CreateField(ogr.FieldDefn('str', ogr.OFTString))

        NFEATURES = 1000

        sizes = []
        fids = []
        # Ranges that are used to allocate a slot in a series of page
        fibo_suite = functools.reduce(lambda x, _: x+[x[-1]+x[-2]], range(20-2), [8, 16])

        # Create features of random sizes
        for i in range(NFEATURES):
            series = r.randint(0, len(fibo_suite)-2)
            size = r.randint(fibo_suite[series], fibo_suite[series+1]-1)
            sizes.append(size)
            f = ogr.Feature(lyr.GetLayerDefn())
            f['str'] = 'x' * size
            assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
            fids.append(f.GetFID())

        # Delete them in random order
        for i in range(NFEATURES):
            idx = r.randint(0, len(fids)-1)
            fid = fids[idx]
            del fids[idx]

            assert lyr.DeleteFeature(fid) == ogr.OGRERR_NONE

        _check_freelist_consistency(ds, lyr)
        lyr.SyncToDisk()
        filesize = gdal.VSIStatL(table_filename).size

        # Re-create feature of the same previous sizes, in random order
        for i in range(NFEATURES):
            idx = r.randint(0, len(sizes)-1)
            size = sizes[idx]
            del sizes[idx]

            f = ogr.Feature(lyr.GetLayerDefn())
            f['str'] = 'x' * size
            assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

        assert filesize == gdal.VSIStatL(table_filename).size

        assert gdal.VSIStatL(freelist_filename) is not None
        _check_freelist_consistency(ds, lyr)
        lyr.SyncToDisk()
        assert gdal.VSIStatL(freelist_filename) is None

    finally:
        gdal.RmdirRecursive(dirname)



###############################################################################


def test_ogr_openfilegdb_write_repack():

    dirname = '/vsimem/out.gdb'
    table_filename = dirname + '/a00000009.gdbtable'
    freelist_filename = dirname + '/a00000009.freelist'
    try:
        ds = ogr.GetDriverByName('OpenFileGDB').CreateDataSource(dirname)
        lyr = ds.CreateLayer('test', geom_type = ogr.wkbNone)
        lyr.CreateField(ogr.FieldDefn('str', ogr.OFTString))

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField('str', '1' * 10)
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField('str', '2' * 10)
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField('str', '3' * 10)
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

        lyr.SyncToDisk()
        filesize = gdal.VSIStatL(table_filename).size

        with gdaltest.error_handler():
            assert ds.ExecuteSQL("REPACK unexisting_table") is None

        # Repack: nothing to do
        sql_lyr = ds.ExecuteSQL("REPACK")
        assert sql_lyr
        f = sql_lyr.GetNextFeature()
        assert f[0] == 'true'
        ds.ReleaseResultSet(sql_lyr)

        assert filesize == gdal.VSIStatL(table_filename).size

        # Suppress last feature
        assert lyr.DeleteFeature(3) == 0

        # Repack: truncate file
        sql_lyr = ds.ExecuteSQL("REPACK test")
        assert sql_lyr
        f = sql_lyr.GetNextFeature()
        assert f[0] == 'true'
        ds.ReleaseResultSet(sql_lyr)

        assert gdal.VSIStatL(table_filename).size < filesize
        filesize = gdal.VSIStatL(table_filename).size

        # Suppress first feature
        assert lyr.DeleteFeature(1) == 0

        assert gdal.VSIStatL(freelist_filename) is not None

        # Repack: rewrite whole file
        sql_lyr = ds.ExecuteSQL("REPACK")
        assert sql_lyr
        f = sql_lyr.GetNextFeature()
        assert f[0] == 'true'
        ds.ReleaseResultSet(sql_lyr)

        assert gdal.VSIStatL(table_filename).size < filesize
        filesize = gdal.VSIStatL(table_filename).size

        assert gdal.VSIStatL(freelist_filename) is None

        lyr.ResetReading()
        f = lyr.GetNextFeature()
        assert f.GetField(0) == '2' * 10

        # Repack: nothing to do
        sql_lyr = ds.ExecuteSQL("REPACK")
        assert sql_lyr
        f = sql_lyr.GetNextFeature()
        assert f[0] == 'true'
        ds.ReleaseResultSet(sql_lyr)

        assert gdal.VSIStatL(table_filename).size == filesize

    finally:
        gdal.RmdirRecursive(dirname)



###############################################################################


def test_ogr_openfilegdb_write_recompute_extent_on():

    dirname = '/vsimem/out.gdb'
    try:
        ds = ogr.GetDriverByName('OpenFileGDB').CreateDataSource(dirname)
        lyr = ds.CreateLayer('test', geom_type = ogr.wkbPoint)

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (1 2)'))
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (3 4)'))
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (5 6)'))
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

        f = ogr.Feature(lyr.GetLayerDefn())
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE

        assert lyr.GetExtent() == (1, 5, 2, 6)

        assert lyr.DeleteFeature(1) == ogr.OGRERR_NONE

        assert lyr.GetExtent() == (1, 5, 2, 6)

        gdal.ErrorReset()
        assert ds.ExecuteSQL("RECOMPUTE EXTENT ON test") is None
        assert gdal.GetLastErrorMsg() == ''

        with gdaltest.error_handler():
            gdal.ErrorReset()
            assert ds.ExecuteSQL("RECOMPUTE EXTENT ON non_existing_layer") is None
            assert gdal.GetLastErrorMsg() != ''

        assert lyr.GetExtent() == (3, 5, 4, 6)

        ds = None

        ds = ogr.Open(dirname, update = 1)
        lyr = ds.GetLayer(0)
        assert lyr.GetExtent() == (3, 5, 4, 6)

        assert lyr.DeleteFeature(2) == ogr.OGRERR_NONE
        assert lyr.DeleteFeature(3) == ogr.OGRERR_NONE

        assert ds.ExecuteSQL("RECOMPUTE EXTENT ON test") is None

        assert lyr.GetExtent(can_return_null=True) is None

        ds = None

    finally:
        gdal.RmdirRecursive(dirname)



###############################################################################


def test_ogr_openfilegdb_write_alter_field_defn():

    dirname = '/vsimem/out.gdb'
    try:
        ds = ogr.GetDriverByName('OpenFileGDB').CreateDataSource(dirname)
        lyr = ds.CreateLayer('test', geom_type = ogr.wkbPoint)
        assert lyr.TestCapability(ogr.OLCAlterFieldDefn) == 1

        fld_defn = ogr.FieldDefn('str', ogr.OFTString)
        assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE
        assert lyr.CreateField(ogr.FieldDefn('other_field', ogr.OFTString)) == ogr.OGRERR_NONE

        # No-op
        assert lyr.AlterFieldDefn(0, fld_defn, ogr.ALTER_ALL_FLAG) == ogr.OGRERR_NONE

        # Invalid index
        with gdaltest.error_handler():
            assert lyr.AlterFieldDefn(-1, fld_defn, ogr.ALTER_ALL_FLAG) != ogr.OGRERR_NONE
            assert lyr.AlterFieldDefn(lyr.GetLayerDefn().GetFieldCount(), fld_defn, ogr.ALTER_ALL_FLAG) != ogr.OGRERR_NONE

        ds = None

        ds = ogr.Open(dirname, update=1)
        lyr = ds.GetLayer(0)

        # Changing type not supported
        fld_defn = ogr.FieldDefn('str', ogr.OFTInteger)
        with gdaltest.error_handler():
            assert lyr.AlterFieldDefn(0, fld_defn, ogr.ALTER_ALL_FLAG) != ogr.OGRERR_NONE
            fld_defn = lyr.GetLayerDefn().GetFieldDefn(0)
            assert fld_defn.GetType() == ogr.OFTString

        # Changing subtype not supported
        fld_defn = ogr.FieldDefn('str', ogr.OFTString)
        fld_defn.SetSubType(ogr.OFSTUUID)
        with gdaltest.error_handler():
            assert lyr.AlterFieldDefn(0, fld_defn, ogr.ALTER_ALL_FLAG) != ogr.OGRERR_NONE
            fld_defn = lyr.GetLayerDefn().GetFieldDefn(0)
            assert fld_defn.GetType() == ogr.OFTString
            assert fld_defn.GetSubType() == ogr.OFSTNone

        # Changing nullable state not supported
        fld_defn = ogr.FieldDefn('str', ogr.OFTString)
        fld_defn.SetNullable(False)
        with gdaltest.error_handler():
            assert lyr.AlterFieldDefn(0, fld_defn, ogr.ALTER_ALL_FLAG) != ogr.OGRERR_NONE
            fld_defn = lyr.GetLayerDefn().GetFieldDefn(0)
            assert fld_defn.IsNullable()

        # Renaming to an other existing field not supported
        fld_defn = ogr.FieldDefn('other_field', ogr.OFTString)
        with gdaltest.error_handler():
            assert lyr.AlterFieldDefn(0, fld_defn, ogr.ALTER_ALL_FLAG) != ogr.OGRERR_NONE
            fld_defn = lyr.GetLayerDefn().GetFieldDefn(0)
            assert fld_defn.GetName() == 'str'

        fld_defn = ogr.FieldDefn('SHAPE', ogr.OFTString)
        with gdaltest.error_handler():
            assert lyr.AlterFieldDefn(0, fld_defn, ogr.ALTER_ALL_FLAG) != ogr.OGRERR_NONE
            fld_defn = lyr.GetLayerDefn().GetFieldDefn(0)
            assert fld_defn.GetName() == 'str'

        fld_defn = ogr.FieldDefn('str_renamed', ogr.OFTString)
        fld_defn.SetAlternativeName('alias')
        fld_defn.SetWidth(10)
        fld_defn.SetDefault("'aaa'")

        assert lyr.AlterFieldDefn(0, fld_defn, ogr.ALTER_ALL_FLAG) == ogr.OGRERR_NONE

        fld_defn = lyr.GetLayerDefn().GetFieldDefn(0)
        assert fld_defn.GetType() == ogr.OFTString
        assert fld_defn.GetName() == 'str_renamed'
        assert fld_defn.GetAlternativeName() == 'alias'
        assert fld_defn.GetWidth() == 10
        assert fld_defn.GetDefault() == "'aaa'"

        ds = None

        ds = ogr.Open(dirname, update=1)
        lyr = ds.GetLayer(0)

        fld_defn = lyr.GetLayerDefn().GetFieldDefn(0)
        assert fld_defn.GetType() == ogr.OFTString
        assert fld_defn.GetName() == 'str_renamed'
        assert fld_defn.GetAlternativeName() == 'alias'
        assert fld_defn.GetWidth() == 10
        assert fld_defn.GetDefault() == "'aaa'"

        sql_lyr = ds.ExecuteSQL('GetLayerDefinition test')
        assert sql_lyr
        f = sql_lyr.GetNextFeature()
        xml = f.GetField(0)
        f = None
        ds.ReleaseResultSet(sql_lyr)
        assert '<Name>str_renamed</Name>' in xml

        ds = None

    finally:
        gdal.RmdirRecursive(dirname)


###############################################################################
# Test writing field domains


def test_ogr_openfilegdb_write_domains_from_other_gdb():

    dirname = '/vsimem/out.gdb'
    try:
        ds = gdal.GetDriverByName('OpenFileGDB').Create(dirname, 0, 0, 0, gdal.GDT_Unknown)

        domain = ogr.CreateCodedFieldDomain('domain', 'desc', ogr.OFTInteger, ogr.OFSTNone, {1: "one", "2": None})
        assert ds.AddFieldDomain(domain)

        lyr = ds.CreateLayer('test', geom_type = ogr.wkbNone)

        fld_defn = ogr.FieldDefn('foo', ogr.OFTInteger)
        fld_defn.SetDomainName('domain')
        assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

        fld_defn = ogr.FieldDefn('foo2', ogr.OFTInteger)
        fld_defn.SetDomainName('domain')
        assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE
        ds = None

        ds = ogr.Open(dirname, update = 1)
        assert ds.GetLayerByName('GDB_ItemRelationships').GetFeatureCount() == 2
        ds = None

        ds = ogr.Open(dirname, update = 1)
        lyr = ds.GetLayer(0)
        assert lyr.DeleteField(0) == ogr.OGRERR_NONE
        ds = None

        ds = ogr.Open(dirname, update = 1)
        assert ds.GetLayerByName('GDB_ItemRelationships').GetFeatureCount() == 2
        ds = None

        ds = ogr.Open(dirname, update = 1)
        lyr = ds.GetLayer(0)
        assert lyr.DeleteField(0) == ogr.OGRERR_NONE
        ds = None

        ds = ogr.Open(dirname, update = 1)
        assert ds.GetLayerByName('GDB_ItemRelationships').GetFeatureCount() == 1
        ds = None

        ds = ogr.Open(dirname, update = 1)
        lyr = ds.GetLayer(0)
        fld_defn = ogr.FieldDefn('foo', ogr.OFTInteger)
        assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE
        ds = None

        ds = ogr.Open(dirname, update = 1)
        assert ds.GetLayerByName('GDB_ItemRelationships').GetFeatureCount() == 1
        ds = None

        ds = ogr.Open(dirname, update = 1)
        lyr = ds.GetLayer(0)
        fld_defn = ogr.FieldDefn('foo', ogr.OFTInteger)
        fld_defn.SetDomainName('domain')
        assert lyr.AlterFieldDefn(0, fld_defn, ogr.ALTER_ALL_FLAG) == ogr.OGRERR_NONE
        assert lyr.GetLayerDefn().GetFieldDefn(0).GetDomainName() == 'domain'
        ds = None

        ds = ogr.Open(dirname, update = 1)
        assert ds.GetLayerByName('GDB_ItemRelationships').GetFeatureCount() == 2
        ds = None

    finally:
        gdal.RmdirRecursive(dirname)


###############################################################################
# Test emulated transactions


def test_ogr_openfilegdb_write_emulated_transactions():

    dirname = 'tmp/test_ogr_openfilegdb_write_emulated_transactions.gdb'
    try:
        ds = ogr.GetDriverByName('OpenFileGDB').CreateDataSource(dirname)

        assert ds.StartTransaction(True) == ogr.OGRERR_NONE
        assert ds.CommitTransaction() == ogr.OGRERR_NONE

        assert ds.StartTransaction(True) == ogr.OGRERR_NONE
        assert ds.RollbackTransaction() == ogr.OGRERR_NONE

        assert ds.StartTransaction(True) == ogr.OGRERR_NONE
        with gdaltest.error_handler():
            assert ds.StartTransaction(True) != ogr.OGRERR_NONE
        assert ds.RollbackTransaction() == ogr.OGRERR_NONE

        with gdaltest.error_handler():
            assert ds.CommitTransaction() != ogr.OGRERR_NONE

        with gdaltest.error_handler():
            assert ds.RollbackTransaction() != ogr.OGRERR_NONE

        assert ds.StartTransaction(True) == ogr.OGRERR_NONE
        lyr = ds.CreateLayer('foo', geom_type = ogr.wkbNone)
        assert lyr is not None
        assert lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn())) == ogr.OGRERR_NONE
        assert lyr.GetFeatureCount() == 1
        assert ds.RollbackTransaction() == ogr.OGRERR_NONE

        # It is in a ghost state after rollback
        assert lyr.GetFeatureCount() == 0

        assert ds.StartTransaction(True) == ogr.OGRERR_NONE

        # Implicit rollback
        ds = None

        ds = ogr.Open(dirname, update=1)
        assert ds.GetLayerCount() == 0
        assert gdal.VSIStatL(dirname + '/a00000009.gdbtable') is None

        assert ds.StartTransaction(True) == ogr.OGRERR_NONE

        assert ds.CreateLayer('foo', geom_type = ogr.wkbNone) is not None
        assert gdal.VSIStatL(dirname + '/a00000009.gdbtable') is not None

        assert ds.DeleteLayer(0) == ogr.OGRERR_NONE
        assert gdal.VSIStatL(dirname + '/a00000009.gdbtable') is None

        assert ds.CreateLayer('foo2', geom_type = ogr.wkbNone) is not None
        assert gdal.VSIStatL(dirname + '/a0000000a.gdbtable') is not None

        assert ds.CommitTransaction() == ogr.OGRERR_NONE

        assert gdal.VSIStatL(dirname + '/a0000000a.gdbtable') is not None

        assert ds.StartTransaction(True) == ogr.OGRERR_NONE
        assert ds.DeleteLayer(0) == ogr.OGRERR_NONE
        assert gdal.VSIStatL(dirname + '/a0000000a.gdbtable') is None
        assert ds.RollbackTransaction() == ogr.OGRERR_NONE
        assert gdal.VSIStatL(dirname + '/a0000000a.gdbtable') is not None
        ds = None

        ds = ogr.Open(dirname, update=1)
        assert ds.GetLayerCount() == 1
        lyr = ds.GetLayerByName('foo2')

        assert ds.StartTransaction(True) == ogr.OGRERR_NONE
        assert lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn())) == ogr.OGRERR_NONE
        assert lyr.GetFeatureCount() == 1
        assert ds.CommitTransaction() == ogr.OGRERR_NONE
        assert lyr.GetFeatureCount() == 1

        ds = None

        ds = ogr.Open(dirname, update=1)
        lyr = ds.GetLayerByName('foo2')
        assert lyr.GetFeatureCount() == 1

        assert ds.StartTransaction(True) == ogr.OGRERR_NONE
        assert lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn())) == ogr.OGRERR_NONE
        assert lyr.GetFeatureCount() == 2
        assert ds.RollbackTransaction() == ogr.OGRERR_NONE
        assert lyr.GetFeatureCount() == 1

        # Test that StartTransaction() / RollbackTransaction() doesn't destroy
        # unmodified layers! (https://github.com/OSGeo/gdal/issues/5952)
        assert ds.StartTransaction(True) == ogr.OGRERR_NONE
        assert ds.RollbackTransaction() == ogr.OGRERR_NONE

        ds = None

        ds = ogr.Open(dirname, update=1)
        lyr = ds.GetLayerByName('foo2')
        assert lyr.GetFeatureCount() == 1
        ds = None

    finally:
        gdal.RmdirRecursive(dirname)


###############################################################################


def test_ogr_openfilegdb_write_emulated_transactions_delete_field_before_geom():

    dirname = '/vsimem/out.gdb'
    try:
        ds = ogr.GetDriverByName('OpenFileGDB').CreateDataSource(dirname)

        with gdaltest.config_option('OPENFILEGDB_CREATE_FIELD_BEFORE_GEOMETRY', 'YES'):
            lyr = ds.CreateLayer('test', geom_type = ogr.wkbPoint)

        assert lyr.CreateField(ogr.FieldDefn('str', ogr.OFTString)) == ogr.OGRERR_NONE

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField('field_before_geom', 'to be deleted')
        f.SetField('str', 'foo')
        f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1 2)'))
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
        f = None

        assert ds.StartTransaction(True) == ogr.OGRERR_NONE

        assert lyr.DeleteField(lyr.GetLayerDefn().GetFieldIndex('field_before_geom')) == ogr.OGRERR_NONE

        assert ds.RollbackTransaction() == ogr.OGRERR_NONE

        lyr.ResetReading()
        f = lyr.GetNextFeature()
        assert f.GetField('field_before_geom') == 'to be deleted'
        assert f.GetField('str') == 'foo'
        assert f.GetGeometryRef() is not None

        ds = None

    finally:
        gdal.RmdirRecursive(dirname)


###############################################################################
# Test renaming a layer


@pytest.mark.parametrize("options", [ [], ['FEATURE_DATASET=fd1'] ])
def test_ogr_openfilegdb_write_rename_layer( options):

    dirname = "tmp/rename.gdb"
    try:
        ds = ogr.GetDriverByName('OpenFileGDB').CreateDataSource(dirname)
        lyr = ds.CreateLayer('other_layer', geom_type=ogr.wkbNone)
        lyr.SyncToDisk()

        lyr = ds.CreateLayer('foo', geom_type=ogr.wkbPoint, options=options)
        assert lyr.TestCapability(ogr.OLCRename) == 1

        assert lyr.Rename('bar') == ogr.OGRERR_NONE
        assert lyr.GetDescription() == 'bar'
        assert lyr.GetLayerDefn().GetName() == 'bar'

        # Too long layer name
        with gdaltest.error_handler():
            assert lyr.Rename('x' * 200) != ogr.OGRERR_NONE

        with gdaltest.error_handler():
            assert lyr.Rename('bar') != ogr.OGRERR_NONE

        with gdaltest.error_handler():
            assert lyr.Rename('other_layer') != ogr.OGRERR_NONE

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (1 2)'))
        lyr.CreateFeature(f)

        ds = ogr.Open(dirname, update = 1)

        # Check system tables
        system_catolog_lyr = ds.GetLayerByName('GDB_SystemCatalog')
        f = system_catolog_lyr.GetFeature(10)
        assert f['Name'] == 'bar'

        items_lyr = ds.GetLayerByName('GDB_Items')
        if options == []:
            f = items_lyr.GetFeature(4)
            assert f['Path'] == '\\bar'
            assert '<CatalogPath>\\bar</CatalogPath>' in f['Definition']
        else:
            f = items_lyr.GetFeature(5)
            assert f['Path'] == '\\fd1\\bar'
            assert '<CatalogPath>\\fd1\\bar</CatalogPath>' in f['Definition']
        assert f['Name'] == 'bar'
        assert f['PhysicalName'] == 'BAR'
        assert '<Name>bar</Name>' in f['Definition']

        # Second renaming, after dataset reopening
        lyr = ds.GetLayerByName('bar')
        assert lyr.Rename('baz') == ogr.OGRERR_NONE
        assert lyr.GetDescription() == 'baz'
        assert lyr.GetLayerDefn().GetName() == 'baz'

        lyr.ResetReading()
        f = lyr.GetNextFeature()
        assert f.GetGeometryRef() is not None

        ds = None

        ds = ogr.Open(dirname)

        # Check system tables
        system_catolog_lyr = ds.GetLayerByName('GDB_SystemCatalog')
        f = system_catolog_lyr.GetFeature(10)
        assert f['Name'] == 'baz'

        items_lyr = ds.GetLayerByName('GDB_Items')
        if options == []:
            f = items_lyr.GetFeature(4)
            assert f['Path'] == '\\baz'
            assert '<CatalogPath>\\baz</CatalogPath>' in f['Definition']
        else:
            f = items_lyr.GetFeature(5)
            assert f['Path'] == '\\fd1\\baz'
            assert '<CatalogPath>\\fd1\\baz</CatalogPath>' in f['Definition']
        assert f['Name'] == 'baz'
        assert f['PhysicalName'] == 'BAZ'
        assert '<Name>baz</Name>' in f['Definition']

        lyr = ds.GetLayerByName('baz')
        assert lyr is not None, [ ds.GetLayer(i).GetName() for i in range(ds.GetLayerCount()) ]

        lyr.ResetReading()
        f = lyr.GetNextFeature()
        assert f.GetGeometryRef() is not None

        ds = None

    finally:
        gdal.RmdirRecursive(dirname)



###############################################################################
# Test field name laundering (#4458)


def test_ogr_openfilegdb_field_name_laundering():

    dirname = '/vsimem/out.gdb'
    try:
        ds = ogr.GetDriverByName('OpenFileGDB').CreateDataSource(dirname)
        lyr = ds.CreateLayer('test',  geom_type=ogr.wkbPoint)
        with gdaltest.error_handler():
            lyr.CreateField(ogr.FieldDefn('FROM', ogr.OFTInteger))  # reserved keyword
            lyr.CreateField(ogr.FieldDefn('1NUMBER', ogr.OFTInteger))  # starting with a number
            lyr.CreateField(ogr.FieldDefn('WITH SPACE AND !$*!- special characters', ogr.OFTInteger))  # unallowed characters
            lyr.CreateField(ogr.FieldDefn('é' * 64, ogr.OFTInteger))  # OK
            lyr.CreateField(ogr.FieldDefn('A123456789012345678901234567890123456789012345678901234567890123', ogr.OFTInteger))  # 64 characters : ok
            lyr.CreateField(ogr.FieldDefn('A1234567890123456789012345678901234567890123456789012345678901234', ogr.OFTInteger))  # 65 characters : nok
            lyr.CreateField(ogr.FieldDefn('A12345678901234567890123456789012345678901234567890123456789012345', ogr.OFTInteger))  # 66 characters : nok

        lyr_defn = lyr.GetLayerDefn()
        expected_names = ['FROM_', '_1NUMBER', 'WITH_SPACE_AND_______special_characters',
                          'é' * 64,
                          'A123456789012345678901234567890123456789012345678901234567890123',
                          'A1234567890123456789012345678901234567890123456789012345678901_1',
                          'A1234567890123456789012345678901234567890123456789012345678901_2']
        for i in range(5):
            assert lyr_defn.GetFieldIndex(expected_names[i]) == i, \
                ('did not find %s' % expected_names[i])

        ds = None

    finally:
        gdal.RmdirRecursive(dirname)

###############################################################################
# Test layer name laundering (#4466)


def test_ogr_openfilegdb_layer_name_laundering():

    dirname = '/vsimem/out.gdb'

    _160char = 'A123456789' * 16

    in_names = ['FROM',  # reserved keyword
                '1NUMBER',  # starting with a number
                'WITH SPACE AND !$*!- special characters',  # banned characters
                'sde_foo',  # reserved prefixes
                _160char,  # OK
                _160char + 'A',  # too long
                _160char + 'B',  # still too long
               ]

    try:
        ds = ogr.GetDriverByName('OpenFileGDB').CreateDataSource(dirname)
        with gdaltest.error_handler():
            for in_name in in_names:
                ds.CreateLayer(in_name, geom_type=ogr.wkbPoint)

        expected_names = ['FROM_',
                          '_1NUMBER',
                          'WITH_SPACE_AND_______special_characters',
                          '_sde_foo',
                          _160char,
                          _160char[0:158] + '_1',
                          _160char[0:158] + '_2']
        for i, exp_name in enumerate(expected_names):
            assert ds.GetLayerByIndex(i).GetName() == exp_name, ('did not find %s' % exp_name)

        ds = None

    finally:
        gdal.RmdirRecursive(dirname)

###############################################################################
# Test creating layer with documentation


def test_ogr_openfilegdb_layer_documentation():

    dirname = '/vsimem/out.gdb'

    try:
        ds = ogr.GetDriverByName('OpenFileGDB').CreateDataSource(dirname)
        ds.CreateLayer('test', geom_type=ogr.wkbPoint, options = ['DOCUMENTATION=<my_doc/>'])
        ds = None

        ds = ogr.Open(dirname)
        sql_lyr = ds.ExecuteSQL('GetLayerMetadata test')
        f = sql_lyr.GetNextFeature()
        assert f.GetField(0) == '<my_doc/>'
        ds.ReleaseResultSet(sql_lyr)
        ds = None

    finally:
        gdal.RmdirRecursive(dirname)


###############################################################################
# Test explicit CREATE_SHAPE_AREA_AND_LENGTH_FIELDS=YES option


def test_ogr_openfilegdb_CREATE_SHAPE_AREA_AND_LENGTH_FIELDS_explicit():

    dirname = '/vsimem/test_ogr_openfilegdb_CREATE_SHAPE_AREA_AND_LENGTH_FIELDS_explicit.gdb'

    try:
        ds = ogr.GetDriverByName('OpenFileGDB').CreateDataSource(dirname)

        srs = osr.SpatialReference()
        srs.ImportFromEPSG(4326)

        lyr = ds.CreateLayer('line', srs=srs, geom_type=ogr.wkbLineString, options=['CREATE_SHAPE_AREA_AND_LENGTH_FIELDS=YES'])
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('LINESTRING(0 0,2 0)'))
        lyr.CreateFeature(f)
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('COMPOUNDCURVE((0 0,2 0))'))
        lyr.CreateFeature(f)
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('MULTILINESTRING((0 0,2 0),(10 0,15 0))'))
        lyr.CreateFeature(f)
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('MULTICURVE((0 0,2 0),(10 0,15 0))'))
        lyr.CreateFeature(f)
        f = ogr.Feature(lyr.GetLayerDefn())
        lyr.CreateFeature(f)

        lyr = ds.CreateLayer('area', srs=srs, geom_type=ogr.wkbPolygon, options=['CREATE_SHAPE_AREA_AND_LENGTH_FIELDS=YES'])
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POLYGON((0 0,0 1,1 1,1 0,0 0),(0.2 0.2,0.2 0.8,0.8 0.8,0.8 0.2,0.2 0.2))'))
        lyr.CreateFeature(f)
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('CURVEPOLYGON((0 0,0 1,1 1,1 0,0 0),(0.2 0.2,0.2 0.8,0.8 0.8,0.8 0.2,0.2 0.2))'))
        lyr.CreateFeature(f)
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('MULTIPOLYGON(((0 0,0 1,1 1,1 0,0 0),(0.2 0.2,0.2 0.8,0.8 0.8,0.8 0.2,0.2 0.2)),((10 0,10 1,11 1,11 0,10 0)))'))
        lyr.CreateFeature(f)
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('MULTISURFACE(((0 0,0 1,1 1,1 0,0 0),(0.2 0.2,0.2 0.8,0.8 0.8,0.8 0.2,0.2 0.2)),((10 0,10 1,11 1,11 0,10 0)))'))
        lyr.CreateFeature(f)
        f = ogr.Feature(lyr.GetLayerDefn())
        lyr.CreateFeature(f)

        ds = None

        ds = ogr.Open(dirname, update = 1)

        lyr = ds.GetLayerByName('line')
        lyr_defn = lyr.GetLayerDefn()
        assert lyr_defn.GetFieldIndex('Shape_Length') >= 0
        assert lyr_defn.GetFieldIndex('Shape_Area') < 0
        assert lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex('Shape_Length')).GetDefault() == 'FILEGEODATABASE_SHAPE_LENGTH'
        f = lyr.GetNextFeature()
        assert f['Shape_Length'] == 2
        f = lyr.GetNextFeature()
        assert f['Shape_Length'] == 2
        f = lyr.GetNextFeature()
        assert f['Shape_Length'] == 2 + 5
        f = lyr.GetNextFeature()
        assert f['Shape_Length'] == 2 + 5
        f = lyr.GetNextFeature()
        assert f['Shape_Length'] is None

        lyr = ds.GetLayerByName('area')
        lyr_defn = lyr.GetLayerDefn()
        assert lyr_defn.GetFieldIndex('Shape_Length') >= 0
        assert lyr_defn.GetFieldIndex('Shape_Area') >= 0
        assert lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex('Shape_Area')).GetDefault() == 'FILEGEODATABASE_SHAPE_AREA'
        assert lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex('Shape_Length')).GetDefault() == 'FILEGEODATABASE_SHAPE_LENGTH'
        f = lyr.GetNextFeature()
        assert f['Shape_Length'] == pytest.approx(6.4)
        assert f['Shape_Area'] == pytest.approx(0.64)
        f = lyr.GetNextFeature()
        assert f['Shape_Length'] == pytest.approx(6.4)
        assert f['Shape_Area'] == pytest.approx(0.64)
        f = lyr.GetNextFeature()
        assert f['Shape_Length'] == pytest.approx(6.4 + 4)
        assert f['Shape_Area'] == pytest.approx(0.64 + 1)
        f = lyr.GetNextFeature()
        assert f['Shape_Length'] == pytest.approx(6.4 + 4)
        assert f['Shape_Area'] == pytest.approx(0.64 + 1)
        f = lyr.GetNextFeature()
        assert f['Shape_Length'] is None
        assert f['Shape_Area'] is None

        # Rename Shape_Length and Shape_Area fields (not sure the FileGDB SDK likes it)
        iShapeLength = lyr_defn.GetFieldIndex('Shape_Length')
        fld_defn = ogr.FieldDefn('Shape_Length_renamed', ogr.OFTReal)
        assert lyr.AlterFieldDefn(iShapeLength, fld_defn, ogr.ALTER_NAME_FLAG) == ogr.OGRERR_NONE

        iShapeArea = lyr_defn.GetFieldIndex('Shape_Area')
        fld_defn = ogr.FieldDefn('Shape_Area_renamed', ogr.OFTReal)
        assert lyr.AlterFieldDefn(iShapeArea, fld_defn, ogr.ALTER_NAME_FLAG) == ogr.OGRERR_NONE

        ds = ogr.Open(dirname, update = 1)

        sql_lyr = ds.ExecuteSQL('GetLayerDefinition area')
        assert sql_lyr
        f = sql_lyr.GetNextFeature()
        xml = f.GetField(0)
        f = None
        ds.ReleaseResultSet(sql_lyr)
        assert '<AreaFieldName>Shape_Area_renamed</AreaFieldName>' in xml
        assert '<LengthFieldName>Shape_Length_renamed</LengthFieldName>' in xml

        lyr = ds.GetLayerByName('area')
        lyr_defn = lyr.GetLayerDefn()

        # Delete Shape_Length and Shape_Area fields
        assert lyr.DeleteField(lyr_defn.GetFieldIndex('Shape_Length_renamed')) == ogr.OGRERR_NONE
        assert lyr.DeleteField(lyr_defn.GetFieldIndex('Shape_Area_renamed')) == ogr.OGRERR_NONE

        f = ogr.Feature(lyr_defn)
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POLYGON((0 0,0 1,1 1,1 0,0 0))'))
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
        ds = None

        ds = ogr.Open(dirname)

        sql_lyr = ds.ExecuteSQL('GetLayerDefinition area')
        assert sql_lyr
        f = sql_lyr.GetNextFeature()
        xml = f.GetField(0)
        f = None
        ds.ReleaseResultSet(sql_lyr)
        assert '<AreaFieldName />' in xml
        assert '<LengthFieldName />' in xml

        ds = None

    finally:
        gdal.RmdirRecursive(dirname)


###############################################################################
# Test explicit CREATE_SHAPE_AREA_AND_LENGTH_FIELDS=YES option


def test_ogr_openfilegdb_CREATE_SHAPE_AREA_AND_LENGTH_FIELDS_implicit():

    dirname = '/vsimem/test_ogr_openfilegdb_CREATE_SHAPE_AREA_AND_LENGTH_FIELDS_implicit.gdb'
    try:
        gdal.VectorTranslate(dirname, 'data/filegdb/filegdb_polygonzm_m_not_closing_with_curves.gdb', options = '-f OpenFileGDB -fid 1')

        ds = ogr.Open(dirname)
        lyr = ds.GetLayer(0)
        lyr_defn = lyr.GetLayerDefn()
        assert lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex('Shape_Area')).GetDefault() == 'FILEGEODATABASE_SHAPE_AREA'
        assert lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex('Shape_Length')).GetDefault() == 'FILEGEODATABASE_SHAPE_LENGTH'

        ds = None

    finally:
        gdal.RmdirRecursive(dirname)


###############################################################################
# Test AlterGeomFieldDefn()


def test_ogr_openfilegdb_write_alter_geom_field_defn():

    dirname = '/vsimem/test_ogr_openfilegdb_alter_geom_field_defn.gdb'
    try:
        ds = ogr.GetDriverByName('OpenFileGDB').CreateDataSource(dirname)

        srs = osr.SpatialReference()
        srs.ImportFromEPSG(4326)

        ds.CreateLayer('test', srs=srs, geom_type=ogr.wkbLineString)
        ds = None

        ds = ogr.Open(dirname, update=1)
        lyr = ds.GetLayer(0)

        assert lyr.TestCapability(ogr.OLCAlterGeomFieldDefn)

        # Change name
        fld_defn = ogr.GeomFieldDefn('shape_renamed', ogr.wkbLineString)
        assert lyr.AlterGeomFieldDefn(0, fld_defn, ogr.ALTER_GEOM_FIELD_DEFN_NAME_FLAG) == ogr.OGRERR_NONE
        assert lyr.GetGeometryColumn() == 'shape_renamed'
        ds = None

        ds = ogr.Open(dirname, update=1)
        lyr = ds.GetLayer(0)

        sql_lyr = ds.ExecuteSQL('GetLayerDefinition test')
        assert sql_lyr
        f = sql_lyr.GetNextFeature()
        xml = f.GetField(0)
        f = None
        ds.ReleaseResultSet(sql_lyr)
        assert '<Name>shape_renamed</Name>' in xml
        assert 'WKID' in xml

        assert lyr.GetGeometryColumn() == 'shape_renamed'
        assert lyr.GetSpatialRef().GetAuthorityCode(None) == '4326'

        # Set SRS to None
        fld_defn = ogr.GeomFieldDefn('shape_renamed', ogr.wkbLineString)
        fld_defn.SetSpatialRef(None)
        assert lyr.AlterGeomFieldDefn(0, fld_defn, ogr.ALTER_GEOM_FIELD_DEFN_SRS_FLAG) == ogr.OGRERR_NONE
        assert lyr.GetSpatialRef() is None
        ds = None

        ds = ogr.Open(dirname, update=1)
        lyr = ds.GetLayer(0)
        assert lyr.GetSpatialRef() is None

        sql_lyr = ds.ExecuteSQL('GetLayerDefinition test')
        assert sql_lyr
        f = sql_lyr.GetNextFeature()
        xml = f.GetField(0)
        f = None
        ds.ReleaseResultSet(sql_lyr)
        assert 'WKID' not in xml

        # Set SRS to EPSG:4326
        fld_defn = ogr.GeomFieldDefn('shape_renamed', ogr.wkbLineString)
        fld_defn.SetSpatialRef(srs)
        assert lyr.AlterGeomFieldDefn(0, fld_defn, ogr.ALTER_GEOM_FIELD_DEFN_SRS_FLAG) == ogr.OGRERR_NONE
        assert lyr.GetSpatialRef() is not None
        ds = None

        ds = ogr.Open(dirname, update=1)
        lyr = ds.GetLayer(0)
        assert lyr.GetSpatialRef() is not None

        sql_lyr = ds.ExecuteSQL('GetLayerDefinition test')
        assert sql_lyr
        f = sql_lyr.GetNextFeature()
        xml = f.GetField(0)
        f = None
        ds.ReleaseResultSet(sql_lyr)
        assert '<WKID>4326</WKID>' in xml

        srs4269 = osr.SpatialReference()
        srs4269.ImportFromEPSG(4269)

        # Set SRS to EPSG:4269
        fld_defn = ogr.GeomFieldDefn('shape_renamed', ogr.wkbLineString)
        fld_defn.SetSpatialRef(srs4269)
        assert lyr.AlterGeomFieldDefn(0, fld_defn, ogr.ALTER_GEOM_FIELD_DEFN_SRS_FLAG) == ogr.OGRERR_NONE
        assert lyr.GetSpatialRef() is not None
        assert lyr.GetSpatialRef().GetAuthorityCode(None) == '4269'
        ds = None

        ds = ogr.Open(dirname, update=1)
        lyr = ds.GetLayer(0)
        assert lyr.GetSpatialRef() is not None
        assert lyr.GetSpatialRef().GetAuthorityCode(None) == '4269'

        sql_lyr = ds.ExecuteSQL('GetLayerDefinition test')
        assert sql_lyr
        f = sql_lyr.GetNextFeature()
        xml = f.GetField(0)
        f = None
        ds.ReleaseResultSet(sql_lyr)
        assert '<WKID>4269</WKID>' in xml

        # Changing geometry type not supported
        fld_defn = ogr.GeomFieldDefn('shape_renamed', ogr.wkbPolygon)
        with gdaltest.error_handler():
            assert lyr.AlterGeomFieldDefn(0, fld_defn, ogr.ALTER_GEOM_FIELD_DEFN_TYPE_FLAG) != ogr.OGRERR_NONE

        # Changing nullable state not supported
        fld_defn = ogr.GeomFieldDefn('shape_renamed', ogr.wkbPolygon)
        fld_defn.SetNullable(False)
        with gdaltest.error_handler():
            assert lyr.AlterGeomFieldDefn(0, fld_defn, ogr.ALTER_GEOM_FIELD_DEFN_NULLABLE_FLAG) != ogr.OGRERR_NONE

        ds = None

    finally:
        gdal.RmdirRecursive(dirname)

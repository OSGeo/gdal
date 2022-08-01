#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  SQLite SQL dialect testing.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2012-2013, Even Rouault <even dot rouault at spatialys.com>
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

from http.server import BaseHTTPRequestHandler

from osgeo import ogr
from osgeo import osr
from osgeo import gdal
import gdaltest
import ogrtest
import webserver
import pytest


@pytest.fixture(autouse=True)
def clear_config_options():
    gdal.SetConfigOption('OGR_GEOCODE_CACHE_FILE', None)
    gdal.SetConfigOption('OGR_GEOCODE_APPLICATION', None)
    gdal.SetConfigOption('OGR_GEOCODE_EMAIL', None)
    gdal.SetConfigOption('OGR_GEOCODE_QUERY_TEMPLATE', None)
    gdal.SetConfigOption('OGR_GEOCODE_DELAY', None)
    gdal.SetConfigOption('OGR_GEOCODE_SERVICE', None)
    gdal.SetConfigOption('OGR_GEOCODE_USERNAME', None)
    gdal.SetConfigOption('OGR_GEOCODE_KEY', None)
    gdal.SetConfigOption('OGR_SQLITE_DIALECT_USE_SPATIALITE', None)


###############################################################################
# Detect OGR SQLite dialect availability


@pytest.fixture(autouse=True, scope='module')
def require_ogr_sql_sqlite():
    if ogr.GetDriverByName('SQLite') is None:
        pytest.skip()

    # If we have SQLite VFS support, then SQLite dialect should be available
    ds = ogr.GetDriverByName('SQLite').CreateDataSource('/vsimem/ogr_sql_sqlite_available.db')
    if ds is None:
        pytest.skip()
    ds = None
    gdal.Unlink('/vsimem/ogr_sql_sqlite_available.db')

    ds = ogr.GetDriverByName("Memory").CreateDataSource("my_ds")
    sql_lyr = ds.ExecuteSQL("SELECT * FROM sqlite_master", dialect='SQLite')
    ds.ReleaseResultSet(sql_lyr)
    assert sql_lyr is not None

###############################################################################
# Tests that don't involve geometry


def test_ogr_sql_sqlite_1():
    ds = ogr.GetDriverByName("Memory").CreateDataSource("my_ds")
    for geom in [ogr.wkbNone, ogr.wkbUnknown]:
        lyr = ds.CreateLayer("my_layer", geom_type=geom)
        field_defn = ogr.FieldDefn('intfield', ogr.OFTInteger)
        lyr.CreateField(field_defn)
        field_defn = ogr.FieldDefn('int64field', ogr.OFTInteger64)
        lyr.CreateField(field_defn)
        field_defn = ogr.FieldDefn('doublefield', ogr.OFTReal)
        lyr.CreateField(field_defn)
        field_defn = ogr.FieldDefn('strfield', ogr.OFTString)
        lyr.CreateField(field_defn)
        field_defn = ogr.FieldDefn('binaryfield', ogr.OFTBinary)
        lyr.CreateField(field_defn)
        field_defn = ogr.FieldDefn('nullablefield', ogr.OFTInteger)
        lyr.CreateField(field_defn)
        field_defn = ogr.FieldDefn('datetimefield', ogr.OFTDateTime)
        lyr.CreateField(field_defn)
        field_defn = ogr.FieldDefn('datefield', ogr.OFTDate)
        lyr.CreateField(field_defn)
        field_defn = ogr.FieldDefn('timefield', ogr.OFTTime)
        lyr.CreateField(field_defn)
        field_defn = ogr.FieldDefn('from', ogr.OFTString)
        lyr.CreateField(field_defn)
        field_defn = ogr.FieldDefn('boolfield', ogr.OFTInteger)
        field_defn.SetSubType(ogr.OFSTBoolean)
        lyr.CreateField(field_defn)
        field_defn = ogr.FieldDefn('int16field', ogr.OFTInteger)
        field_defn.SetSubType(ogr.OFSTInt16)
        lyr.CreateField(field_defn)
        field_defn = ogr.FieldDefn('float32field', ogr.OFTReal)
        field_defn.SetSubType(ogr.OFSTFloat32)
        lyr.CreateField(field_defn)
        field_defn = ogr.FieldDefn('intlistfield', ogr.OFTIntegerList)
        lyr.CreateField(field_defn)
        field_defn = ogr.FieldDefn('int64listfield', ogr.OFTInteger64List)
        lyr.CreateField(field_defn)
        field_defn = ogr.FieldDefn('doublelistfield', ogr.OFTRealList)
        lyr.CreateField(field_defn)
        field_defn = ogr.FieldDefn('strlistfield', ogr.OFTStringList)
        lyr.CreateField(field_defn)

        # Test INSERT
        sql_lyr = ds.ExecuteSQL("INSERT INTO my_layer (intfield, int64field, nullablefield, doublefield, strfield, binaryfield, datetimefield, datefield, timefield, \"from\", boolfield, int16field, float32field, intlistfield, int64listfield, doublelistfield, strlistfield) VALUES (1,1234567890123456,NULL,2.34,'foo',x'0001FF', '2012-08-23 21:24', '2012-08-23', '21:24', 'from_val', 1, -32768, 1.23, '(2:2,3)', '(1:1234567890123456)', '(1:1.23)', '(1:a)')", dialect='SQLite')
        ds.ReleaseResultSet(sql_lyr)

        lyr.ResetReading()
        feat = lyr.GetNextFeature()
        if feat.GetField('intfield') != 1 or \
           feat.GetField('int64field') != 1234567890123456 or \
           feat.GetField('nullablefield') is not None or \
           feat.GetField('doublefield') != 2.34 or \
           feat.GetField('strfield') != 'foo' or \
           feat.GetField('binaryfield') != '0001FF' or \
           feat.GetField('datetimefield') != '2012/08/23 21:24:00' or \
           feat.GetField('datefield') != '2012/08/23' or \
           feat.GetField('timefield') != '21:24:00' or \
           feat.GetField('from') != 'from_val':
            feat.DumpReadable()
            pytest.fail()
        feat = None

        # Test UPDATE
        sql_lyr = ds.ExecuteSQL("UPDATE my_layer SET intfield = 2, int64field = 234567890123, doublefield = 3.45, strfield = 'bar', timefield = '12:34' WHERE ROWID = 0", dialect='SQLite')
        ds.ReleaseResultSet(sql_lyr)

        lyr.ResetReading()
        feat = lyr.GetNextFeature()
        if feat.GetField('intfield') != 2 or \
           feat.GetField('int64field') != 234567890123 or \
           feat.GetField('doublefield') != 3.45 or \
           feat.GetField('strfield') != 'bar' or \
           feat.GetField('datetimefield') != '2012/08/23 21:24:00' or \
           feat.GetField('datefield') != '2012/08/23' or \
           feat.GetField('timefield') != '12:34:00':
            feat.DumpReadable()
            pytest.fail()

        feat.SetStyleString('cool_style')
        lyr.SetFeature(feat)

        feat = None

        # Test SELECT
        sql_lyr = ds.ExecuteSQL("SELECT * FROM my_layer", dialect='SQLite')
        assert sql_lyr.GetLayerDefn().GetFieldDefn(sql_lyr.GetLayerDefn().GetFieldIndex('boolfield')).GetSubType() == ogr.OFSTBoolean
        assert sql_lyr.GetLayerDefn().GetFieldDefn(sql_lyr.GetLayerDefn().GetFieldIndex('int16field')).GetSubType() == ogr.OFSTInt16
        assert sql_lyr.GetLayerDefn().GetFieldDefn(sql_lyr.GetLayerDefn().GetFieldIndex('float32field')).GetSubType() == ogr.OFSTFloat32
        assert sql_lyr.GetLayerDefn().GetFieldDefn(sql_lyr.GetLayerDefn().GetFieldIndex('intlistfield')).GetType() == ogr.OFTIntegerList
        assert sql_lyr.GetLayerDefn().GetFieldDefn(sql_lyr.GetLayerDefn().GetFieldIndex('doublelistfield')).GetType() == ogr.OFTRealList
        assert sql_lyr.GetLayerDefn().GetFieldDefn(sql_lyr.GetLayerDefn().GetFieldIndex('strlistfield')).GetType() == ogr.OFTStringList
        feat = sql_lyr.GetNextFeature()
        if feat.GetField('intfield') != 2 or \
           feat.GetField('int64field') != 234567890123 or \
           feat.GetField('nullablefield') is not None or \
           feat.GetField('doublefield') != 3.45 or \
           feat.GetField('strfield') != 'bar' or \
           feat.GetField('datetimefield') != '2012/08/23 21:24:00' or \
           feat.GetField('datefield') != '2012/08/23' or \
           feat.GetField('timefield') != '12:34:00' or \
           feat.GetField('boolfield') != 1 or \
           feat.GetField('int16field') != -32768 or \
           feat.GetField('float32field') != 1.23 or \
           feat.GetField('intlistfield') != [2, 3] or \
           feat.GetField('int64listfield') != [1234567890123456] or \
           feat.GetField('doublelistfield') != [1.23] or \
           feat.GetField('strlistfield') != ['a']:
            feat.DumpReadable()
            pytest.fail()
        feat = None
        ds.ReleaseResultSet(sql_lyr)

        # Test SELECT with OGR_STYLE
        sql_lyr = ds.ExecuteSQL("SELECT *, OGR_STYLE FROM my_layer", dialect='SQLite')
        feat = sql_lyr.GetNextFeature()
        if feat.GetField('intfield') != 2 or \
           feat.GetField('nullablefield') is not None or \
           feat.GetField('doublefield') != 3.45 or \
           feat.GetField('strfield') != 'bar' or \
           feat.GetStyleString() != 'cool_style':
            feat.DumpReadable()
            pytest.fail()
        feat = None
        ds.ReleaseResultSet(sql_lyr)

        # Test SELECT with filters

        # Success filters
        for cond in ['intfield = 2', 'intfield > 1', 'intfield >= 2', 'intfield < 3', 'intfield <= 2',
                     'int64field = 234567890123',
                     'doublefield = 3.45', 'doublefield > 3', 'doublefield >= 3.45', 'doublefield < 3.46', 'doublefield <= 3.45',
                     "strfield = 'bar'", "strfield > 'baq'", "strfield >= 'bar'", "strfield < 'bas'", "strfield <= 'bar'",
                     'nullablefield IS NULL',
                     "binaryfield = x'0001FF'",
                     "OGR_STYLE = 'cool_style'",
                     'intfield = 2 AND doublefield = 3.45',
                     'ROWID = 0',
                     'intfield IS 2',
                     'intfield IS NOT 10000',
                     'intfield IS NOT NULL',
                     "\"from\" = 'from_val'"]:
            sql_lyr = ds.ExecuteSQL("SELECT * FROM my_layer WHERE " + cond, dialect='SQLite')
            feat = sql_lyr.GetNextFeature()
            assert feat is not None, cond
            feat = None
            ds.ReleaseResultSet(sql_lyr)

        # Failed filters
        for cond in ['intfield = 0', 'intfield > 3', 'intfield >= 3', 'intfield < 0', 'intfield <= 0',
                     'doublefield = 0', 'doublefield > 3.46', 'doublefield >= 3.46', 'doublefield < 3.45', 'doublefield <= 0',
                     "strfield = 'XXX'", "strfield > 'bas'", "strfield >= 'bas'", "strfield < 'bar'", "strfield <= 'baq'",
                     'intfield = 2 AND doublefield = 0',
                     'ROWID = 10000',
                     'intfield IS 10000',
                     'intfield IS NOT 2',
                     "\"from\" = 'other_val'"]:
            sql_lyr = ds.ExecuteSQL("SELECT * FROM my_layer WHERE " + cond, dialect='SQLite')
            feat = sql_lyr.GetNextFeature()
            assert feat is None
            feat = None
            ds.ReleaseResultSet(sql_lyr)

        if geom != ogr.wkbNone:
            # Test a filter on geometry, to check that we won't try to optimize that
            sql_lyr = ds.ExecuteSQL("SELECT * FROM my_layer WHERE GEOMETRY = x'00'", dialect='SQLite')
            ds.ReleaseResultSet(sql_lyr)

        # Test INSERT with specified ROWID/FID
        sql_lyr = ds.ExecuteSQL("INSERT INTO my_layer (intfield, ROWID) VALUES (100, 1000)", dialect='SQLite')
        ds.ReleaseResultSet(sql_lyr)

        feat = lyr.GetFeature(1000)
        if feat.GetField('intfield') != 100:
            feat.DumpReadable()
            pytest.fail()
        feat = None

        # Test DELETE
        sql_lyr = ds.ExecuteSQL("DELETE FROM my_layer WHERE intfield = 2", dialect='SQLite')
        ds.ReleaseResultSet(sql_lyr)
        sql_lyr = ds.ExecuteSQL("DELETE FROM my_layer WHERE ROWID = 1000", dialect='SQLite')
        ds.ReleaseResultSet(sql_lyr)

        lyr.ResetReading()
        feat = lyr.GetNextFeature()
        if feat is not None:
            feat.DumpReadable()
            pytest.fail()
        feat = None

        ds.DeleteLayer(0)

    ds = None

###############################################################################
# Tests that involve geometry  (but without needing Spatialite)


def test_ogr_sql_sqlite_2():

    ds = ogr.GetDriverByName("Memory").CreateDataSource("my_ds")

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)

    lyr = ds.CreateLayer("my_layer", srs=srs)
    field_defn = ogr.FieldDefn('intfield', ogr.OFTInteger)
    lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn('doublefield', ogr.OFTReal)
    lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn('strfield', ogr.OFTString)
    lyr.CreateField(field_defn)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('intfield', 1)
    feat.SetField('doublefield', 2.34)
    feat.SetField('strfield', 'foo')
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (0 1)'))
    feat.SetStyleString('cool_style')
    lyr.CreateFeature(feat)
    feat = None

    # Test UPDATE
    sql_lyr = ds.ExecuteSQL("UPDATE my_layer SET intfield = 2, doublefield = 3.45, strfield = 'bar' WHERE ROWID = 0", dialect='SQLite')
    ds.ReleaseResultSet(sql_lyr)

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat.GetField('intfield') != 2 or \
       feat.GetField('doublefield') != 3.45 or \
       feat.GetField('strfield') != 'bar' or \
       feat.GetGeometryRef().ExportToWkt() != 'POINT (0 1)':
        feat.DumpReadable()
        pytest.fail()
    feat = None

    # Test SELECT
    sql_lyr = ds.ExecuteSQL("SELECT * FROM my_layer", dialect='SQLite')
    feat = sql_lyr.GetNextFeature()
    if feat.GetField('intfield') != 2 or \
       feat.GetField('doublefield') != 3.45 or \
       feat.GetField('strfield') != 'bar' or \
       feat.GetGeometryRef().ExportToWkt() != 'POINT (0 1)':
        feat.DumpReadable()
        pytest.fail()
    got_srs = feat.GetGeometryRef().GetSpatialReference()
    assert not (got_srs is None or srs.IsSame(got_srs, options = ['IGNORE_DATA_AXIS_TO_SRS_AXIS_MAPPING=YES']) == 0)
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    # Test SELECT with OGR_STYLE
    sql_lyr = ds.ExecuteSQL("SELECT *, OGR_STYLE FROM my_layer", dialect='SQLite')
    feat = sql_lyr.GetNextFeature()
    if feat.GetField('intfield') != 2 or \
       feat.GetField('doublefield') != 3.45 or \
       feat.GetField('strfield') != 'bar' or \
       feat.GetStyleString() != 'cool_style' or \
       feat.GetGeometryRef().ExportToWkt() != 'POINT (0 1)':
        feat.DumpReadable()
        pytest.fail()
    got_srs = feat.GetGeometryRef().GetSpatialReference()
    assert not (got_srs is None or srs.IsSame(got_srs, options = ['IGNORE_DATA_AXIS_TO_SRS_AXIS_MAPPING=YES']) == 0)
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    # Test with a custom SRS
    srs = osr.SpatialReference()
    srs.SetFromUserInput("""LOCAL_CS["foo"]""")
    lyr = ds.CreateLayer("my_layer2", srs=srs)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (0 1)'))
    lyr.CreateFeature(feat)
    feat = None

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (0 1)'))
    lyr.CreateFeature(feat)
    feat = None

    # Test SELECT
    sql_lyr = ds.ExecuteSQL("SELECT * FROM my_layer2", dialect='SQLite')

    layer_srs = sql_lyr.GetSpatialRef()
    assert not (layer_srs is None or srs.IsSame(layer_srs) == 0)

    for _ in range(2):
        feat = sql_lyr.GetNextFeature()
        if feat.GetGeometryRef().ExportToWkt() != 'POINT (0 1)':
            feat.DumpReadable()
            pytest.fail()
        got_srs = feat.GetGeometryRef().GetSpatialReference()
        assert not (got_srs is None or srs.IsSame(got_srs) == 0)
        feat = None

    ds.ReleaseResultSet(sql_lyr)

###############################################################################
# Test that involves a left join


def test_ogr_sql_sqlite_left_join():

    ds = ogr.Open('data')

    sql_lyr = ds.ExecuteSQL("SELECT p.*, idlink.* FROM poly p LEFT JOIN idlink USING (EAS_ID) ORDER BY EAS_ID", dialect='SQLite')
    count = sql_lyr.GetFeatureCount()
    sql_lyr.ResetReading()
    feat = sql_lyr.GetNextFeature()
    if feat.GetField('NAME') != '_158_':
        feat.DumpReadable()
        pytest.fail()
    geom = feat.GetGeometryRef()
    p = geom.GetGeometryRef(0).GetPoint_2D(0)
    if p != (480701.0625, 4764738.0):
        feat.DumpReadable()
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)

    assert count == 10

    ds = None

###############################################################################
# Test that involves a join on layers without fast feature count


def test_ogr_sql_sqlite_join_layers_without_fast_feature_count():

    gdal.FileFromMemBuffer('/vsimem/tblmain.csv', """id,attr1
1,one
2,two
3,three
""")

    gdal.FileFromMemBuffer('/vsimem/tblaux.csv', """id,attr2
1,ipsum
2,lorem
3,amet
""")

    ds = ogr.Open('/vsimem/tblmain.csv')
    sql_lyr = ds.ExecuteSQL("SELECT tblmain.id, tblmain.attr1, tblaux.attr2 FROM tblmain JOIN '/vsimem/tblaux.csv'.tblaux AS tblaux USING (id) ORDER BY id", dialect='SQLite')
    count = sql_lyr.GetFeatureCount()
    sql_lyr.ResetReading()
    f = sql_lyr.GetNextFeature()
    assert f['id'] == '1'
    assert f['attr1'] == 'one'
    assert f['attr2'] == 'ipsum'
    f = sql_lyr.GetNextFeature()
    assert f['id'] == '2'
    assert f['attr1'] == 'two'
    assert f['attr2'] == 'lorem'
    f = sql_lyr.GetNextFeature()
    assert f['id'] == '3'
    assert f['attr1'] == 'three'
    assert f['attr2'] == 'amet'
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    gdal.Unlink('/vsimem/tblmain.csv')
    gdal.Unlink('/vsimem/tblaux.csv')

    assert count == 3

###############################################################################
# Test that involves a self-join (to check that we can open twice the same table)


def test_ogr_sql_sqlite_4():

    ds = ogr.Open('data')

    sql_lyr = ds.ExecuteSQL("SELECT p.* FROM poly p JOIN poly USING (EAS_ID)", dialect='SQLite')
    count = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)

    assert count == 10

    ds = None

###############################################################################
# Test that involves spatialite


def test_ogr_sql_sqlite_5():

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogr.GetDriverByName('SQLite').CreateDataSource('/vsimem/foo.db', options=['SPATIALITE=YES'])
    ogrtest.has_spatialite = ds is not None
    if ogrtest.has_spatialite:
        sql_lyr = ds.ExecuteSQL("SELECT spatialite_version()")
        feat = sql_lyr.GetNextFeature()
        gdaltest.spatialite_version = feat.GetFieldAsString(0)
        ds.ReleaseResultSet(sql_lyr)
    ds = None
    gdal.Unlink('/vsimem/foo.db')
    gdal.PopErrorHandler()

    if ogrtest.has_spatialite is False:
        pytest.skip('Spatialite not available')

    ds = ogr.Open('data')

    sql_lyr = ds.ExecuteSQL("SELECT MAX(ST_Length(GEOMETRY)) FROM POLY", dialect='SQLite')
    count = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)

    ds = None

    assert count == 1

###############################################################################
# If Spatialite available, retry some tests without it, to check that
# we are fully compatible with regular SQLite


def test_ogr_sql_sqlite_6():

    if ogrtest.has_spatialite is False:
        pytest.skip()

    gdal.SetConfigOption('OGR_SQLITE_DIALECT_USE_SPATIALITE', 'NO')

    test_ogr_sql_sqlite_1()

    test_ogr_sql_sqlite_2()

    test_ogr_sql_sqlite_4()


###############################################################################
# Test if there's a text column called GEOMETRY already in the table


def test_ogr_sql_sqlite_7():

    ds = ogr.GetDriverByName("Memory").CreateDataSource("my_ds")

    lyr = ds.CreateLayer("my_layer")
    field_defn = ogr.FieldDefn('intfield', ogr.OFTInteger)
    lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn('geometry', ogr.OFTString)
    lyr.CreateField(field_defn)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('intfield', 1)
    feat.SetField('geometry', 'BLA')
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (0 1)'))
    lyr.CreateFeature(feat)
    feat = None

    # Test SELECT
    sql_lyr = ds.ExecuteSQL("SELECT * FROM my_layer", dialect='SQLite')

    assert sql_lyr.GetGeometryColumn() == 'GEOMETRY2'

    feat = sql_lyr.GetNextFeature()
    if feat.GetField('intfield') != 1 or \
       feat.GetField('geometry') != 'BLA' or \
       feat.GetGeometryRef().ExportToWkt() != 'POINT (0 1)':
        feat.DumpReadable()
        pytest.fail()
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    # Test SELECT
    sql_lyr = ds.ExecuteSQL("SELECT GEOMETRY2 FROM my_layer", dialect='SQLite')
    feat = sql_lyr.GetNextFeature()
    if feat.GetGeometryRef().ExportToWkt() != 'POINT (0 1)':
        feat.DumpReadable()
        pytest.fail()
    feat = None
    ds.ReleaseResultSet(sql_lyr)

###############################################################################
# Test join with an external datasource


def test_ogr_sql_sqlite_8():

    ds = ogr.Open('data')

    expect = [171, 172, 173, 179]

    sql_lyr = ds.ExecuteSQL(
        'SELECT p.*, il.name FROM poly p ' +
        'LEFT JOIN "data/idlink.dbf".idlink il USING (eas_id) ' +
        'WHERE eas_id > 170 ORDER BY eas_id', dialect='SQLite')

    tr = ogrtest.check_features_against_list(sql_lyr, 'eas_id', expect)

    ds.ReleaseResultSet(sql_lyr)

    assert tr

###############################################################################
# Check parsing of sub-selects


def test_ogr_sql_sqlite_9():

    ds = ogr.Open('data')

    sql_lyr = ds.ExecuteSQL("SELECT count(*) as cnt FROM (SELECT * FROM (SELECT * FROM\n'data'.poly my_alias))p,(SELECT * FROM 'data'.idlink) il WHERE p.EAS_ID = il.EAS_id", dialect='SQLite')

    feat = sql_lyr.GetNextFeature()
    cnt = feat.GetField('cnt')
    feat = None

    ds.ReleaseResultSet(sql_lyr)

    if cnt != 7:
        return' fail'


###############################################################################
# Test optimized count(*)


def test_ogr_sql_sqlite_10():

    ds = ogr.Open('data')

    sql_lyr = ds.ExecuteSQL("SELECT count(*) as cnt FROM poly", dialect='SQLite')

    feat = sql_lyr.GetNextFeature()
    cnt = feat.GetField('cnt')
    feat = None

    ds.ReleaseResultSet(sql_lyr)

    if cnt != 10:
        return' fail'


###############################################################################
# Test correct parsing of literals


def test_ogr_sql_sqlite_11():

    ds = ogr.GetDriverByName("Memory").CreateDataSource("my_ds")

    lyr = ds.CreateLayer("my_layer")
    field_defn = ogr.FieldDefn('intfield', ogr.OFTInteger)
    lyr.CreateField(field_defn)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('intfield', 1)
    lyr.CreateFeature(feat)
    feat = None

    sql_lyr = ds.ExecuteSQL("SELECT 'a' FROM \"my_layer\"", dialect='SQLite')
    cnt = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)

    ds = None

    if cnt != 1:
        return' fail'


###############################################################################
# Test various error conditions


def test_ogr_sql_sqlite_12():

    ds = ogr.GetDriverByName("Memory").CreateDataSource("my_ds")

    # Invalid SQL
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    sql_lyr = ds.ExecuteSQL("qdfdfdf", dialect='SQLite')
    gdal.PopErrorHandler()
    ds.ReleaseResultSet(sql_lyr)

    # Non existing external datasource
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    sql_lyr = ds.ExecuteSQL("SELECT * FROM 'foo'.'bar'", dialect='SQLite')
    gdal.PopErrorHandler()
    ds.ReleaseResultSet(sql_lyr)

    # Non existing layer in existing external datasource
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    sql_lyr = ds.ExecuteSQL("SELECT * FROM 'data'.'azertyuio'", dialect='SQLite')
    gdal.PopErrorHandler()
    ds.ReleaseResultSet(sql_lyr)

    ds = None

###############################################################################
# Test ogr_layer_Extent(), ogr_layer_SRID() and ogr_layer_GeometryType()


def test_ogr_sql_sqlite_13():

    ds = ogr.GetDriverByName("Memory").CreateDataSource("my_ds")

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)

    lyr = ds.CreateLayer("non_spatial", geom_type=ogr.wkbNone)

    lyr = ds.CreateLayer("my_layer", geom_type=ogr.wkbLineString, srs=srs)
    field_defn = ogr.FieldDefn('intfield', ogr.OFTInteger)
    lyr.CreateField(field_defn)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt('LINESTRING (0 1,2 3)'))
    lyr.CreateFeature(feat)
    feat = None

    # Test with invalid parameter
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    sql_lyr = ds.ExecuteSQL("SELECT ogr_layer_Extent(12)", dialect='SQLite')
    gdal.PopErrorHandler()
    feat = sql_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    ds.ReleaseResultSet(sql_lyr)

    assert geom is None

    # Test on non existing layer
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    sql_lyr = ds.ExecuteSQL("SELECT ogr_layer_Extent('foo')", dialect='SQLite')
    gdal.PopErrorHandler()
    feat = sql_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    ds.ReleaseResultSet(sql_lyr)

    assert geom is None

    # Test ogr_layer_Extent()
    sql_lyr = ds.ExecuteSQL("SELECT ogr_layer_Extent('my_layer')", dialect='SQLite')
    feat = sql_lyr.GetNextFeature()
    geom_wkt = feat.GetGeometryRef().ExportToWkt()
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    assert geom_wkt == 'POLYGON ((0 1,2 1,2 3,0 3,0 1))'

    # Test ogr_layer_FeatureCount()
    sql_lyr = ds.ExecuteSQL("SELECT ogr_layer_FeatureCount('my_layer') AS the_count", dialect='SQLite')
    feat = sql_lyr.GetNextFeature()
    count = feat.GetField('the_count')
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    assert count == 1

    # Test ogr_layer_Extent() on a non spatial layer
    sql_lyr = ds.ExecuteSQL("SELECT ogr_layer_Extent('non_spatial')", dialect='SQLite')
    feat = sql_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    ds.ReleaseResultSet(sql_lyr)

    assert geom is None

    # Test ogr_layer_SRID()
    sql_lyr = ds.ExecuteSQL("SELECT ogr_layer_SRID('my_layer') AS the_srid", dialect='SQLite')
    feat = sql_lyr.GetNextFeature()
    the_srid = feat.GetField('the_srid')
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    assert the_srid == 4326

    # Test ogr_layer_SRID() on a non spatial layer
    sql_lyr = ds.ExecuteSQL("SELECT ogr_layer_SRID('non_spatial') AS the_srid", dialect='SQLite')
    feat = sql_lyr.GetNextFeature()
    the_srid = feat.GetField('the_srid')
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    assert the_srid is None

    # Test ogr_layer_GeometryType()
    sql_lyr = ds.ExecuteSQL("SELECT ogr_layer_GeometryType('my_layer') AS the_geometrytype", dialect='SQLite')
    feat = sql_lyr.GetNextFeature()
    the_geometrytype = feat.GetField('the_geometrytype')
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    assert the_geometrytype == 'LINESTRING'

    # Test ogr_layer_GeometryType() on a non spatial layer
    sql_lyr = ds.ExecuteSQL("SELECT ogr_layer_GeometryType('non_spatial') AS the_geometrytype", dialect='SQLite')
    feat = sql_lyr.GetNextFeature()
    the_geometrytype = feat.GetField('the_geometrytype')
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    assert the_geometrytype is None

    # Test on a external virtual table
    ds_shape = ogr.GetDriverByName("ESRI Shapefile").CreateDataSource('/vsimem/ogr_sql_sqlite_13.shp')
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    lyr = ds_shape.CreateLayer('ogr_sql_sqlite_13', srs=srs)
    ds_shape = None

    sql_lyr = ds.ExecuteSQL("SELECT ogr_layer_SRID('/vsimem/ogr_sql_sqlite_13.shp'.ogr_sql_sqlite_13) AS the_srid_shp", dialect='SQLite')
    feat = sql_lyr.GetNextFeature()
    the_srid_shp = feat.GetField('the_srid_shp')
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    ogr.GetDriverByName("ESRI Shapefile").DeleteDataSource('/vsimem/ogr_sql_sqlite_13.shp')

    assert the_srid_shp == 32631

    ds = None

###############################################################################
#


def ogr_sql_sqlite_14_and_15(sql):

    ds = ogr.GetDriverByName("Memory").CreateDataSource("my_ds")

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)

    lyr = ds.CreateLayer("my_layer", geom_type=ogr.wkbLineString, srs=srs)
    field_defn = ogr.FieldDefn('intfield', ogr.OFTInteger)
    lyr.CreateField(field_defn)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 1)
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt('LINESTRING (0 0,1 1)'))
    lyr.CreateFeature(feat)
    feat = None

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 2)
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt('LINESTRING (10 0,11 1)'))
    lyr.CreateFeature(feat)
    feat = None

    lyr2 = ds.CreateLayer("my_layer2", geom_type=ogr.wkbLineString, srs=srs)
    field_defn = ogr.FieldDefn('intfield2', ogr.OFTInteger)
    lyr2.CreateField(field_defn)

    feat = ogr.Feature(lyr2.GetLayerDefn())
    feat.SetField(0, 11)
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt('LINESTRING (10 1,11 0)'))
    lyr2.CreateFeature(feat)
    feat = None

    feat = ogr.Feature(lyr2.GetLayerDefn())
    feat.SetField(0, 12)
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt('LINESTRING (0 1,1 0)'))
    lyr2.CreateFeature(feat)
    feat = None

    got_one = False
    got_two = False

    sql_lyr = ds.ExecuteSQL(sql, dialect='SQLite')
    for _ in range(2):
        feat = sql_lyr.GetNextFeature()
        i1 = feat.GetField('intfield')
        i2 = feat.GetField('intfield2')
        if (i1 == 1 and i2 == 12):
            got_one = True
        if (i1 == 2 and i2 == 11):
            got_two = True
        feat = None

    feat = sql_lyr.GetNextFeature()
    assert feat is None

    ds.ReleaseResultSet(sql_lyr)

    assert (got_one and got_two)

###############################################################################
# Test 'idx_layername_geometryname' spatial index recognition


def test_ogr_sql_sqlite_14():

    if ogrtest.has_spatialite is False:
        pytest.skip()

    sql = "SELECT intfield, intfield2 FROM my_layer, my_layer2 WHERE " + \
        "my_layer2.rowid IN (SELECT pkid FROM idx_my_layer2_geometry WHERE " + \
        "xmax > MbrMinX(my_layer.geometry) AND xmin < MbrMaxX(my_layer.geometry) AND " + \
        "ymax >= MbrMinY(my_layer.geometry) AND ymin <= MbrMaxY(my_layer.geometry) )"

    return ogr_sql_sqlite_14_and_15(sql)

###############################################################################
# Test 'SpatialIndex' spatial index recognition


def test_ogr_sql_sqlite_15():

    if ogrtest.has_spatialite is False:
        pytest.skip()

    if int(gdaltest.spatialite_version[0:gdaltest.spatialite_version.find('.')]) < 3:
        pytest.skip()

    sql = "SELECT intfield, intfield2 FROM my_layer, my_layer2 WHERE " + \
        "my_layer2.rowid IN (SELECT ROWID FROM SpatialIndex WHERE f_table_name = 'my_layer2' AND search_frame = my_layer.geometry)"

    return ogr_sql_sqlite_14_and_15(sql)


###############################################################################
do_log = False


class GeocodingHTTPHandler(BaseHTTPRequestHandler):

    def log_request(self, code='-', size='-'):
        pass

    def do_GET(self):

        try:
            if do_log:
                f = open('/tmp/log.txt', 'a')
                f.write('GET %s\n' % self.path)
                f.close()

            if self.path.find('/geocoding') != -1:
                if self.path == '/geocoding?q=Paris&addressdetails=1&limit=1&email=foo%40bar':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/xml')
                    self.end_headers()
                    self.wfile.write("""<?xml version="1.0" encoding="UTF-8"?>
<searchresults>
  <place lat="48.8566177374844" lon="2.34288146739775" display_name="Paris, Ile-de-France, France metropolitaine">
    <county>Paris</county>
    <state>Ile-de-France</state>
    <country>France metropolitaine</country>
    <country_code>fr</country_code>
  </place>
</searchresults>""".encode('ascii'))
                    return
                if self.path == '/geocoding?q=NonExistingPlace&addressdetails=1&limit=1&email=foo%40bar':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/xml')
                    self.end_headers()
                    self.wfile.write("""<?xml version="1.0" encoding="UTF-8"?><searchresults></searchresults>""".encode('ascii'))
                    return
                self.send_error(404, 'File Not Found: %s' % self.path)
                return

            elif self.path.find('/yahoogeocoding') != -1:
                if self.path == '/yahoogeocoding?q=Paris':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/xml')
                    self.end_headers()
                    self.wfile.write("""<?xml version="1.0" encoding="UTF-8" standalone="yes"?><ResultSet xmlns:ns1="http://www.yahooapis.com/v1/base.rng" version="2.0" xml:lang="en-US"><Error>0</Error><ErrorMessage>No error</ErrorMessage><Locale>en-US</Locale><Found>1</Found><Quality>40</Quality><Result><quality>40</quality><latitude>48.85693</latitude><longitude>2.3412</longitude><offsetlat>48.85693</offsetlat><offsetlon>2.3412</offsetlon><radius>9200</radius><name></name><line1></line1><line2>Paris</line2><line3></line3><line4>France</line4><house></house><street></street><xstreet></xstreet><unittype></unittype><unit></unit><postal></postal><neighborhood></neighborhood><city>Paris</city><county>Paris</county><state>Ile-de-France</state><country>France</country><countrycode>FR</countrycode><statecode></statecode><countycode>75</countycode><uzip>75001</uzip><hash></hash><woeid>615702</woeid><woetype>7</woetype></Result></ResultSet>
<!-- nws03.maps.bf1.yahoo.com uncompressed/chunked Sat Dec 29 04:59:06 PST 2012 -->
<!-- wws09.geotech.bf1.yahoo.com uncompressed/chunked Sat Dec 29 04:59:06 PST 2012 -->""".encode('ascii'))
                    return
                if self.path == '/yahoogeocoding?q=NonExistingPlace':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/xml')
                    self.end_headers()
                    self.wfile.write("""<?xml version="1.0" encoding="UTF-8" standalone="yes"?><ResultSet xmlns:ns1="http://www.yahooapis.com/v1/base.rng" version="2.0" xml:lang="en-US"><Error>7</Error><ErrorMessage>No result</ErrorMessage><Locale>en-US</Locale><Found>0</Found><Quality>0</Quality></ResultSet>
<!-- nws08.maps.bf1.yahoo.com uncompressed/chunked Sat Dec 29 05:00:45 PST 2012 -->
<!-- wws08.geotech.bf1.yahoo.com uncompressed/chunked Sat Dec 29 05:00:45 PST 2012 -->""".encode('ascii'))
                    return

                self.send_error(404, 'File Not Found: %s' % self.path)
                return

            elif self.path.find('/geonamesgeocoding') != -1:
                if self.path == '/geonamesgeocoding?q=Paris&username=demo':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/xml')
                    self.end_headers()
                    self.wfile.write("""<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<geonames style="MEDIUM">
<totalResultsCount>2356</totalResultsCount>
<geoname>
<toponymName>Paris</toponymName>
<name>Paris</name>
<lat>48.85341</lat>
<lng>2.3488</lng>
<geonameId>2988507</geonameId>
<countryCode>FR</countryCode>
<countryName>France</countryName>
<fcl>P</fcl>
<fcode>PPLC</fcode>
</geoname>
</geonames>""".encode('ascii'))
                    return
                if self.path == '/geonamesgeocoding?q=NonExistingPlace&username=demo':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/xml')
                    self.end_headers()
                    self.wfile.write("""<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<geonames style="MEDIUM">
<totalResultsCount>0</totalResultsCount>
</geonames>""".encode('ascii'))
                    return

                self.send_error(404, 'File Not Found: %s' % self.path)
                return

            elif self.path.find('/binggeocoding') != -1:
                if self.path == '/binggeocoding?q=Paris&key=fakekey':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/xml')
                    self.end_headers()
                    self.wfile.write("""<Response>
  <ResourceSets>
    <ResourceSet>
      <EstimatedTotal>1</EstimatedTotal>
      <Resources>
        <Location>
          <Name>Paris, Paris, France</Name>
          <Point>
            <Latitude>48</Latitude>
            <Longitude>2</Longitude>
          </Point>
          <BoundingBox>
            <SouthLatitude>48</SouthLatitude>
            <WestLongitude>2</WestLongitude>
            <NorthLatitude>48</NorthLatitude>
            <EastLongitude>2</EastLongitude>
          </BoundingBox>
          <Address>
            <AdminDistrict>IdF</AdminDistrict>
            <AdminDistrict2>Paris</AdminDistrict2>
            <CountryRegion>France</CountryRegion>
            <FormattedAddress>Paris, Paris, France</FormattedAddress>
            <Locality>Paris</Locality>
          </Address>
          <GeocodePoint>
            <Latitude>48</Latitude>
            <Longitude>2</Longitude>
            <CalculationMethod>Random</CalculationMethod>
            <UsageType>Display</UsageType>
          </GeocodePoint>
        </Location>
      </Resources>
    </ResourceSet>
  </ResourceSets>
</Response>""".encode('ascii'))
                    return
                if self.path == '/binggeocoding?q=NonExistingPlace&key=fakekey':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/xml')
                    self.end_headers()
                    self.wfile.write("""<Response>
  <ResourceSets>
    <ResourceSet>
      <EstimatedTotal>0</EstimatedTotal>
      <Resources/>
    </ResourceSet>
  </ResourceSets>
</Response>""".encode('ascii'))
                    return

                self.send_error(404, 'File Not Found: %s' % self.path)
                return

            # Below is for reverse geocoding
            elif self.path.find('/reversegeocoding') != -1:
                if self.path == '/reversegeocoding?lon=2.00000000&lat=49.00000000&email=foo%40bar' or \
                   self.path == '/reversegeocoding?lon=2.00000000&lat=49.00000000&zoom=12&email=foo%40bar':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/xml')
                    self.end_headers()
                    self.wfile.write("""<?xml version="1.0" encoding="UTF-8"?>
<reversegeocode>
  <result place_id="46754274" osm_type="way" osm_id="38621743" ref="Chemin du Cordon" lat="49.0002726061675" lon="1.99514157818059">Chemin du Cordon, Foret de l'Hautil, Triel-sur-Seine, Saint-Germain-en-Laye, Yvelines, Ile-de-France, 78510, France metropolitaine</result>
  <addressparts>
    <road>Chemin du Cordon</road>
    <forest>Foret de l'Hautil</forest>
    <city>Triel-sur-Seine</city>
    <county>Saint-Germain-en-Laye</county>
    <state>Ile-de-France</state>
    <postcode>78510</postcode>
    <country>France metropolitaine</country>
    <country_code>fr</country_code>
  </addressparts>
</reversegeocode>""".encode('ascii'))
                    return
                self.send_error(404, 'File Not Found: %s' % self.path)
                return

            elif self.path.find('/yahooreversegeocoding') != -1:
                if self.path == '/yahooreversegeocoding?q=49.00000000,2.00000000&gflags=R':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/xml')
                    self.end_headers()
                    self.wfile.write("""<?xml version="1.0" encoding="UTF-8" standalone="yes"?><ResultSet xmlns:ns1="http://www.yahooapis.com/v1/base.rng" version="2.0" xml:lang="en-US"><Error>0</Error><ErrorMessage>No error</ErrorMessage><Locale>en-US</Locale><Found>1</Found><Quality>99</Quality><Result><quality>72</quality><latitude>49.001</latitude><longitude>1.999864</longitude><offsetlat>49.001</offsetlat><offsetlon>1.999864</offsetlon><radius>400</radius><name>49.00000000,2.00000000</name><line1>Chemin de Menucourt</line1><line2>78510 Triel-sur-Seine</line2><line3></line3><line4>France</line4><house></house><street>Chemin de Menucourt</street><xstreet></xstreet><unittype></unittype><unit></unit><postal>78510</postal><neighborhood></neighborhood><city>Triel-sur-Seine</city><county>Yvelines</county><state>Ile-de-France</state><country>France</country><countrycode>FR</countrycode><statecode></statecode><countycode>78</countycode><uzip>78510</uzip><hash></hash><woeid>12727518</woeid><woetype>11</woetype></Result></ResultSet>
<!-- nws02.maps.bf1.yahoo.com uncompressed/chunked Sat Dec 29 05:03:31 PST 2012 -->
<!-- wws05.geotech.bf1.yahoo.com uncompressed/chunked Sat Dec 29 05:03:31 PST 2012 -->""".encode('ascii'))
                    return
                self.send_error(404, 'File Not Found: %s' % self.path)
                return

            elif self.path.find('/geonamesreversegeocoding') != -1:
                if self.path == '/geonamesreversegeocoding?lat=49.00000000&lng=2.00000000&username=demo':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/xml')
                    self.end_headers()
                    self.wfile.write("""<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<geonames>
<geoname>
<toponymName>Paris Basin</toponymName>
<name>Paris Basin</name>
<lat>49</lat>
<lng>2</lng>
<geonameId>2988503</geonameId>
<countryCode>FR</countryCode>
<countryName>France</countryName>
<fcl>T</fcl>
<fcode>DPR</fcode>
<distance>0</distance>
</geoname>
</geonames>""".encode('ascii'))
                    return
                self.send_error(404, 'File Not Found: %s' % self.path)
                return

            elif self.path.find('/bingreversegeocoding') != -1:
                if self.path == '/bingreversegeocoding?49.00000000,2.00000000&key=fakekey':
                    self.send_response(200)
                    self.send_header('Content-type', 'application/xml')
                    self.end_headers()
                    self.wfile.write("""<Response>
  <ResourceSets>
    <ResourceSet>
      <EstimatedTotal>1</EstimatedTotal>
      <Resources>
        <Location>
          <Name>Paris, Paris, France</Name>
          <Point>
            <Latitude>48</Latitude>
            <Longitude>2</Longitude>
          </Point>
          <BoundingBox>
            <SouthLatitude>48</SouthLatitude>
            <WestLongitude>2</WestLongitude>
            <NorthLatitude>48</NorthLatitude>
            <EastLongitude>2</EastLongitude>
          </BoundingBox>
          <Address>
            <AdminDistrict>IdF</AdminDistrict>
            <AdminDistrict2>Paris</AdminDistrict2>
            <CountryRegion>France</CountryRegion>
            <FormattedAddress>Paris, Paris, France</FormattedAddress>
            <Locality>Paris</Locality>
          </Address>
          <GeocodePoint>
            <Latitude>48</Latitude>
            <Longitude>2</Longitude>
            <CalculationMethod>Random</CalculationMethod>
            <UsageType>Display</UsageType>
          </GeocodePoint>
        </Location>
      </Resources>
    </ResourceSet>
  </ResourceSets>
</Response>""".encode('ascii'))
                    return
                self.send_error(404, 'File Not Found: %s' % self.path)
                return

            return
        except IOError:
            pass

        self.send_error(404, 'File Not Found: %s' % self.path)


###############################################################################
def test_ogr_sql_sqlite_start_webserver():

    ogrtest.webserver_process = None
    ogrtest.webserver_port = 0

    if gdal.GetDriverByName('HTTP') is None:
        pytest.skip()

    (ogrtest.webserver_process, ogrtest.webserver_port) = webserver.launch(handler=GeocodingHTTPHandler)
    if ogrtest.webserver_port == 0:
        pytest.skip()


###############################################################################
# Test ogr_geocode()


def test_ogr_sql_sqlite_16(service=None, template='http://127.0.0.1:%d/geocoding?q=%%s'):

    if ogrtest.webserver_port == 0:
        pytest.skip()

    gdal.SetConfigOption('OGR_GEOCODE_APPLICATION', 'GDAL/OGR autotest suite')
    gdal.SetConfigOption('OGR_GEOCODE_EMAIL', 'foo@bar')
    gdal.SetConfigOption('OGR_GEOCODE_QUERY_TEMPLATE', template % ogrtest.webserver_port)
    gdal.SetConfigOption('OGR_GEOCODE_DELAY', '0.1')
    gdal.SetConfigOption('OGR_GEOCODE_SERVICE', service)
    if service == 'GEONAMES':
        gdal.SetConfigOption('OGR_GEOCODE_USERNAME', 'demo')
    elif service == 'BING':
        gdal.SetConfigOption('OGR_GEOCODE_KEY', 'fakekey')

    for cache_filename in ['tmp/ogr_geocode_cache.sqlite', 'tmp/ogr_geocode_cache.csv']:

        gdal.Unlink(cache_filename)

        gdal.SetConfigOption('OGR_GEOCODE_CACHE_FILE', cache_filename)

        ds = ogr.GetDriverByName("Memory").CreateDataSource("my_ds")

        if service == 'BING':
            name_field = "Name"
        else:
            name_field = "display_name"

        for sql in ["SELECT ogr_geocode('Paris')",
                    "SELECT ogr_geocode('Paris', 'geometry')",
                    "SELECT ogr_geocode('Paris', '%s') AS %s" % (name_field, name_field),
                    "SELECT ogr_geocode('Paris', 'raw') AS raw"]:

            sql_lyr = ds.ExecuteSQL(sql, dialect='SQLite')
            feat = sql_lyr.GetNextFeature()
            if feat is None:
                print(sql)
                ds.ReleaseResultSet(sql_lyr)
                pytest.fail()

            if ((sql == "SELECT ogr_geocode('Paris')" or
                 sql == "SELECT ogr_geocode('Paris', 'geometry')") and feat.GetGeometryRef() is None) or \
                (sql == "SELECT ogr_geocode('Paris', '%s')" % name_field and not feat.IsFieldSet(name_field)) or \
                    (sql == "SELECT ogr_geocode('Paris', 'raw')" and not feat.IsFieldSet('raw')):
                feat.DumpReadable()
                print(sql)
                ds.ReleaseResultSet(sql_lyr)
                pytest.fail()

            ds.ReleaseResultSet(sql_lyr)

        for sql in ["SELECT ogr_geocode('NonExistingPlace')", "SELECT ogr_geocode('Error')"]:
            sql_lyr = ds.ExecuteSQL(sql, dialect='SQLite')
            feat = sql_lyr.GetNextFeature()
            if feat is None:
                ds.ReleaseResultSet(sql_lyr)
                pytest.fail()

            if feat.GetGeometryRef() is not None:
                feat.DumpReadable()
                ds.ReleaseResultSet(sql_lyr)
                pytest.fail()

            ds.ReleaseResultSet(sql_lyr)

        # Test various syntax errors
        sql_lyr = ds.ExecuteSQL("SELECT ogr_geocode()", dialect='SQLite')
        ds.ReleaseResultSet(sql_lyr)

        sql_lyr = ds.ExecuteSQL("SELECT ogr_geocode(5)", dialect='SQLite')
        ds.ReleaseResultSet(sql_lyr)

        sql_lyr = ds.ExecuteSQL("SELECT ogr_geocode('Paris', 5)", dialect='SQLite')
        ds.ReleaseResultSet(sql_lyr)

        sql_lyr = ds.ExecuteSQL("SELECT ogr_geocode('Paris', 'geometry', 5)", dialect='SQLite')
        ds.ReleaseResultSet(sql_lyr)

        ds = None

        # Check cache existence
        cache_ds = ogr.Open(cache_filename)
        assert cache_ds is not None
        if cache_ds.GetDriver().GetName().lower() != cache_filename[cache_filename.find('.') + 1:].lower():
            print(cache_ds.GetDriver().GetName())
            print(cache_filename)
            pytest.fail()
        cache_ds = None

        gdal.Unlink(cache_filename)

        ds = None


###############################################################################
# Test ogr_geocode_reverse()


def test_ogr_sql_sqlite_17(service=None, template='http://127.0.0.1:%d/reversegeocoding?lon={lon}&lat={lat}'):

    if ogrtest.webserver_port == 0:
        pytest.skip()

    gdal.SetConfigOption('OGR_GEOCODE_APPLICATION', 'GDAL/OGR autotest suite')
    gdal.SetConfigOption('OGR_GEOCODE_EMAIL', 'foo@bar')
    gdal.SetConfigOption('OGR_GEOCODE_REVERSE_QUERY_TEMPLATE', template % ogrtest.webserver_port)
    gdal.SetConfigOption('OGR_GEOCODE_DELAY', '0.1')
    gdal.SetConfigOption('OGR_GEOCODE_SERVICE', service)
    if service == 'GEONAMES':
        gdal.SetConfigOption('OGR_GEOCODE_USERNAME', 'demo')
    elif service == 'BING':
        gdal.SetConfigOption('OGR_GEOCODE_KEY', 'fakekey')

    for cache_filename in ['tmp/ogr_geocode_cache.sqlite', 'tmp/ogr_geocode_cache.csv']:

        gdal.Unlink(cache_filename)

        gdal.SetConfigOption('OGR_GEOCODE_CACHE_FILE', cache_filename)

        ds = ogr.GetDriverByName("Memory").CreateDataSource("my_ds")

        if service == 'GEONAMES':
            name_field = "name"
        elif service == 'BING':
            name_field = "Name"
        else:
            name_field = "display_name"

        sql_list = ["SELECT ogr_geocode_reverse(2,49,'%s') AS %s" % (name_field, name_field),
                    "SELECT ogr_geocode_reverse(2,49,'%s','zoom=12') AS %s" % (name_field, name_field),
                    "SELECT ogr_geocode_reverse(2.0,49.0,'%s') AS %s" % (name_field, name_field),
                    "SELECT ogr_geocode_reverse(2.0,49.0,'raw') AS raw"]
        if ogrtest.has_spatialite:
            sql_list.append("SELECT ogr_geocode_reverse(MakePoint(2,49),'%s') AS %s" % (name_field, name_field))
            sql_list.append("SELECT ogr_geocode_reverse(MakePoint(2,49),'%s','zoom=12') AS %s" % (name_field, name_field))

        for sql in sql_list:

            sql_lyr = ds.ExecuteSQL(sql, dialect='SQLite')
            feat = sql_lyr.GetNextFeature()
            if feat is None:
                print(sql)
                ds.ReleaseResultSet(sql_lyr)
                pytest.fail()

            if sql.find('raw') != -1:
                field_to_test = 'raw'
            else:
                field_to_test = name_field
            if not feat.IsFieldSet(field_to_test):
                feat.DumpReadable()
                print(sql)
                ds.ReleaseResultSet(sql_lyr)
                pytest.fail()

            ds.ReleaseResultSet(sql_lyr)

        # Test various syntax errors
        sql_lyr = ds.ExecuteSQL("SELECT ogr_geocode_reverse()", dialect='SQLite')
        ds.ReleaseResultSet(sql_lyr)

        sql_lyr = ds.ExecuteSQL("SELECT ogr_geocode_reverse(2)", dialect='SQLite')
        ds.ReleaseResultSet(sql_lyr)

        sql_lyr = ds.ExecuteSQL("SELECT ogr_geocode_reverse(2, 'foo')", dialect='SQLite')
        ds.ReleaseResultSet(sql_lyr)

        sql_lyr = ds.ExecuteSQL("SELECT ogr_geocode_reverse(2, 49)", dialect='SQLite')
        ds.ReleaseResultSet(sql_lyr)

        if ogrtest.has_spatialite:
            sql_lyr = ds.ExecuteSQL("SELECT ogr_geocode_reverse(MakePoint(2,49))", dialect='SQLite')
            ds.ReleaseResultSet(sql_lyr)

            sql_lyr = ds.ExecuteSQL("SELECT ogr_geocode_reverse(MakePoint(2,49), 5)", dialect='SQLite')
            ds.ReleaseResultSet(sql_lyr)

        ds = None

        # Check cache existence
        cache_ds = ogr.Open(cache_filename)
        assert cache_ds is not None
        cache_ds = None

        gdal.Unlink(cache_filename)

        ds = None

###############################################################################
# Test ogr_geocode() with Yahoo geocoding service


def test_ogr_sql_sqlite_18():

    return test_ogr_sql_sqlite_16('YAHOO', 'http://127.0.0.1:%d/yahoogeocoding?q=%%s')

###############################################################################
# Test ogr_geocode_reverse() with Yahoo geocoding service


def test_ogr_sql_sqlite_19():

    return test_ogr_sql_sqlite_17('YAHOO', 'http://127.0.0.1:%d/yahooreversegeocoding?q={lat},{lon}&gflags=R')

###############################################################################
# Test ogr_geocode() with GeoNames.org geocoding service


def test_ogr_sql_sqlite_20():

    return test_ogr_sql_sqlite_16('GEONAMES', 'http://127.0.0.1:%d/geonamesgeocoding?q=%%s')

###############################################################################
# Test ogr_geocode_reverse() with GeoNames.org geocoding service


def test_ogr_sql_sqlite_21():

    return test_ogr_sql_sqlite_17('GEONAMES', 'http://127.0.0.1:%d/geonamesreversegeocoding?lat={lat}&lng={lon}')

###############################################################################
# Test ogr_geocode() with Bing geocoding service


def test_ogr_sql_sqlite_22():

    return test_ogr_sql_sqlite_16('BING', 'http://127.0.0.1:%d/binggeocoding?q=%%s')

###############################################################################
# Test ogr_geocode_reverse() with Bing geocoding service


def test_ogr_sql_sqlite_23():

    return test_ogr_sql_sqlite_17('BING', 'http://127.0.0.1:%d/bingreversegeocoding?{lat},{lon}')

###############################################################################
# Test ogr_deflate() and ogr_inflate()


def test_ogr_sql_sqlite_24():

    ds = ogr.GetDriverByName("Memory").CreateDataSource("my_ds")

    # Very short string
    sql_lyr = ds.ExecuteSQL("SELECT CAST(ogr_inflate(ogr_deflate('ab')) AS VARCHAR)", dialect='SQLite')
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != 'ab':
        feat.DumpReadable()
        ds.ReleaseResultSet(sql_lyr)
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)

    # Big very compressible string
    bigstr = 'a' * 10000
    sql_lyr = ds.ExecuteSQL("SELECT CAST(ogr_inflate(ogr_deflate('%s')) AS VARCHAR)" % bigstr, dialect='SQLite')
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != bigstr:
        feat.DumpReadable()
        ds.ReleaseResultSet(sql_lyr)
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)

    # Blob
    sql_lyr = ds.ExecuteSQL("SELECT ogr_inflate(ogr_deflate(x'0203', 5))", dialect='SQLite')
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != '0203':
        feat.DumpReadable()
        ds.ReleaseResultSet(sql_lyr)
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)

    # Test inflating a random binary blob
    sql_lyr = ds.ExecuteSQL("SELECT ogr_inflate(x'0203')", dialect='SQLite')
    feat = sql_lyr.GetNextFeature()
    if not feat.IsFieldNull(0):
        feat.DumpReadable()
        ds.ReleaseResultSet(sql_lyr)
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)

    # Error case
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    sql_lyr = ds.ExecuteSQL("SELECT ogr_deflate()", dialect='SQLite')
    gdal.PopErrorHandler()
    if sql_lyr is not None:
        ds.ReleaseResultSet(sql_lyr)
        pytest.fail()

    # Error case
    sql_lyr = ds.ExecuteSQL("SELECT ogr_deflate('a', 'b')", dialect='SQLite')
    feat = sql_lyr.GetNextFeature()
    if not feat.IsFieldNull(0):
        feat.DumpReadable()
        ds.ReleaseResultSet(sql_lyr)
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)

    # Error case
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    sql_lyr = ds.ExecuteSQL("SELECT ogr_inflate()", dialect='SQLite')
    gdal.PopErrorHandler()
    if sql_lyr is not None:
        ds.ReleaseResultSet(sql_lyr)
        pytest.fail()

    # Error case
    sql_lyr = ds.ExecuteSQL("SELECT ogr_inflate('a')", dialect='SQLite')
    feat = sql_lyr.GetNextFeature()
    if not feat.IsFieldNull(0):
        feat.DumpReadable()
        ds.ReleaseResultSet(sql_lyr)
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)

###############################################################################


def test_ogr_sql_sqlite_stop_webserver():

    if ogrtest.webserver_port == 0:
        pytest.skip()

    webserver.server_stop(ogrtest.webserver_process, ogrtest.webserver_port)

###############################################################################
# If Spatialite is NOT available, test some of the minimal spatial functions
# implemented. Test it also if spatialite is available, so we have a cross
# validation...


def ogr_sql_sqlite_25_test_errors(ds, fct):
    for val in ['null', "'foo'", "x'00010203'"]:
        sql_lyr = ds.ExecuteSQL("SELECT %s(%s)" % (fct, val), dialect='SQLite')
        feat = sql_lyr.GetNextFeature()
        if not feat.IsFieldNull(0):
            feat.DumpReadable()
            ds.ReleaseResultSet(sql_lyr)
            print(val)
            return False
        ds.ReleaseResultSet(sql_lyr)
        return True


def test_ogr_sql_sqlite_25():

    # if ogrtest.has_spatialite is True:
    #    return 'skip'

    ds = ogr.GetDriverByName("Memory").CreateDataSource("my_ds")

    # Test ST_AsText, ST_GeomFromText, ST_AsBinary, ST_GeomFromWKB
    sql_lyr = ds.ExecuteSQL("SELECT ST_GeomFromWKB(ST_AsBinary(ST_GeomFromText(ST_AsText(ST_GeomFromText('POINT (0 1)')),4326)))", dialect='SQLite')
    feat = sql_lyr.GetNextFeature()
    if feat.GetGeometryRef().ExportToWkt() != 'POINT (0 1)':
        feat.DumpReadable()
        ds.ReleaseResultSet(sql_lyr)
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)

    for fct in ["ST_AsText", "ST_GeomFromText", "ST_AsBinary", "ST_GeomFromWKB"]:
        assert ogr_sql_sqlite_25_test_errors(ds, fct), ('fail with %s' % fct)

    # Test ST_SRID
    sql_lyr = ds.ExecuteSQL("SELECT ST_SRID(ST_GeomFromText('POINT(0 0)',4326))", dialect='SQLite')
    feat = sql_lyr.GetNextFeature()
    val_sql = feat.GetField(0)
    ds.ReleaseResultSet(sql_lyr)

    assert val_sql == 4326

    # Test ST_Area
    sql_lyr = ds.ExecuteSQL("SELECT ST_Area(ST_GeomFromText('%s')), ST_Area(null), ST_Area(x'00')" % 'POLYGON((0 0,0 1,1 1,1 0,0 0))', dialect='SQLite')
    feat = sql_lyr.GetNextFeature()
    val_sql = feat.GetField(0)
    val1_sql = feat.GetField(1)
    val2_sql = feat.GetField(2)
    ds.ReleaseResultSet(sql_lyr)

    geomA = ogr.CreateGeometryFromWkt('POLYGON((0 0,0 1,1 1,1 0,0 0))')
    val_ogr = geomA.GetArea()

    assert val_sql == pytest.approx(val_ogr, abs=1e-5)

    assert val1_sql is None

    assert val2_sql is None


def test_ogr_sql_sqlite_26():

    if not ogrtest.have_geos():
        pytest.skip()

    # if ogrtest.has_spatialite is True:
    #    return 'skip'

    ds = ogr.GetDriverByName("Memory").CreateDataSource("my_ds")

    geom1_wkt = 'POLYGON((0 0,0 1,1 1,1 0,0 0))'
    geom2_wkt = 'POLYGON((0.5 0.5,0.5 1.5,1.5 1.5,1.5 0.5,0.5 0.5))'
    geom3_wkt = 'POLYGON((0.25 0.25,0.25 0.75,0.75 0.75,0.75 0.25,0.25 0.25))'
    geom4_wkt = 'POLYGON((1 0,1 1,2 1,2 0,1 0))'

    # Test ST_Buffer
    op_str = 'Buffer'
    sql_lyr = ds.ExecuteSQL("SELECT %s(ST_GeomFromText('%s'),0.1)" % (op_str, geom1_wkt), dialect='SQLite')
    feat = sql_lyr.GetNextFeature()
    geom_sql = feat.GetGeometryRef()
    ds.ReleaseResultSet(sql_lyr)

    geom = ogr.CreateGeometryFromWkt(geom1_wkt)
    geom_geos = geom.Buffer(0.1)

    assert geom_sql.Equals(geom_geos) != 0, ('fail with %s' % op_str)

    for op_str in ["IsEmpty", "IsSimple", "IsValid"]:
        for wkt in ['POLYGON EMPTY', 'POINT(0 1)', 'POLYGON((0 0,1 1,0 1,1 0,0 0))']:
            sql_lyr = ds.ExecuteSQL("SELECT ST_%s(ST_GeomFromText('%s'))" % (op_str, wkt), dialect='SQLite')
            feat = sql_lyr.GetNextFeature()
            b_sql = feat.GetField(0)
            ds.ReleaseResultSet(sql_lyr)

            b_sql = bool(b_sql == 1)
            geom = ogr.CreateGeometryFromWkt(wkt)
            op = getattr(geom, op_str)
            b_geos = op()
            if b_sql != b_geos:
                if wkt == 'POLYGON EMPTY':
                    print('difference with op = %s and wkt = POLYGON EMPTY' % op_str)
                else:
                    print(wkt)
                    print(b_sql)
                    print(b_geos)
                    pytest.fail('fail with %s' % op_str)

    for op_str in ["Intersects", "Equals", "Disjoint",
                   "Touches", "Crosses", "Within",
                   "Contains", "Overlaps"]:
        for (geomA_wkt, geomB_wkt) in [(geom1_wkt, geom1_wkt),
                                       (geom1_wkt, geom2_wkt),
                                       (geom1_wkt, geom3_wkt),
                                       (geom1_wkt, geom4_wkt)]:
            sql_lyr = ds.ExecuteSQL("SELECT ST_%s(ST_GeomFromText('%s'), ST_GeomFromText('%s'))" % (op_str, geomA_wkt, geomB_wkt), dialect='SQLite')
            feat = sql_lyr.GetNextFeature()
            b_sql = feat.GetField(0)
            ds.ReleaseResultSet(sql_lyr)

            b_sql = bool(b_sql == 1)
            geomA = ogr.CreateGeometryFromWkt(geomA_wkt)
            geomB = ogr.CreateGeometryFromWkt(geomB_wkt)
            op = getattr(geomA, op_str)
            b_geos = op(geomB)
            assert b_sql == b_geos, ('fail with %s' % op_str)

    for op_str in ["Intersection", "Difference", "Union", "SymDifference"]:
        for (geomA_wkt, geomB_wkt) in [(geom1_wkt, geom1_wkt),
                                       (geom1_wkt, geom2_wkt),
                                       (geom1_wkt, geom3_wkt),
                                       (geom1_wkt, geom4_wkt)]:
            sql_lyr = ds.ExecuteSQL("SELECT ST_%s(ST_GeomFromText('%s'), ST_GeomFromText('%s'))" % (op_str, geomA_wkt, geomB_wkt), dialect='SQLite')
            feat = sql_lyr.GetNextFeature()
            geom_sql = feat.GetGeometryRef()
            if geom_sql is not None:
                geom_sql = geom_sql.Clone()
            ds.ReleaseResultSet(sql_lyr)

            geomA = ogr.CreateGeometryFromWkt(geomA_wkt)
            geomB = ogr.CreateGeometryFromWkt(geomB_wkt)
            op = getattr(geomA, op_str)
            geom_geos = op(geomB)

            if geom_sql is None:
                # GEOS can return empty geometry collection, while spatialite
                # does not
                if geom_geos is not None and geom_geos.IsEmpty() == 0:
                    print(geomA_wkt)
                    print(geomB_wkt)
                    print(geom_geos.ExportToWkt())
                    pytest.fail('fail with %s' % op_str)
            elif geom_sql.IsEmpty() and geom_geos.IsEmpty():
                # geom_sql might be a POLYGON made of an empty ring
                # while geom_geos is a POLYGON without a ring
                #import struct
                #print struct.unpack('B' * len(geom_sql.ExportToWkb()), geom_sql.ExportToWkb())
                #print struct.unpack('B' * len(geom_geos.ExportToWkb()), geom_geos.ExportToWkb())
                pass
            else:
                assert geom_sql.Equals(geom_geos) != 0, ('fail with %s: %s %s %s %s' % (op_str, geomA_wkt, geomB_wkt, geom_sql.ExportToWkt(), geom_geos.ExportToWkt()))

    # Error cases
    op_str = 'Intersects'
    for val in ['null', "'foo'", "x'00010203'"]:
        sql_lyr = ds.ExecuteSQL("SELECT ST_%s(ST_GeomFromText('%s'), %s), ST_%s(%s, ST_GeomFromText('%s'))" % (op_str, geom1_wkt, val, op_str, val, geom1_wkt), dialect='SQLite')
        feat = sql_lyr.GetNextFeature()
        b0_sql = feat.GetField(0)
        b1_sql = feat.GetField(1)
        ds.ReleaseResultSet(sql_lyr)
        assert b0_sql <= 0 and b1_sql <= 0, ('fail with %s' % op_str)

    op_str = 'Intersection'
    for val in ['null', "'foo'", "x'00010203'"]:
        sql_lyr = ds.ExecuteSQL("SELECT ST_%s(ST_GeomFromText('%s'), %s)" % (op_str, geom1_wkt, val), dialect='SQLite')
        feat = sql_lyr.GetNextFeature()
        geom_sql = feat.GetGeometryRef()
        ds.ReleaseResultSet(sql_lyr)
        assert geom_sql is None, ('fail with %s' % op_str)

        sql_lyr = ds.ExecuteSQL("SELECT ST_%s(%s, ST_GeomFromText('%s'))" % (op_str, val, geom1_wkt), dialect='SQLite')
        feat = sql_lyr.GetNextFeature()
        geom_sql = feat.GetGeometryRef()
        ds.ReleaseResultSet(sql_lyr)
        assert geom_sql is None, ('fail with %s' % op_str)



###############################################################################
# Test MIN(), MAX() on a date

def test_ogr_sql_sqlite_27():

    ds = ogr.GetDriverByName('Memory').CreateDataSource('')
    lyr = ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn('DATE', ogr.OFTDateTime))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, '2013/12/31 23:59:59')
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, '2013/01/01 00:00:00')
    lyr.CreateFeature(feat)
    lyr = ds.ExecuteSQL("SELECT MIN(DATE), MAX(DATE) from test", dialect='SQLite')
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTDateTime
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetType() == ogr.OFTDateTime
    tr = ogrtest.check_features_against_list(lyr, 'MIN(DATE)', ['2013/01/01 00:00:00'])
    lyr.ResetReading()
    tr2 = ogrtest.check_features_against_list(lyr, 'MAX(DATE)', ['2013/12/31 23:59:59'])

    ds.ReleaseResultSet(lyr)

    assert tr

    assert tr2

###############################################################################
# Test hstore_get_value()


def test_ogr_sql_sqlite_28():

    ds = ogr.GetDriverByName('Memory').CreateDataSource('')

    # Invalid parameters
    for sql in ["SELECT hstore_get_value('a')"]:
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        sql_lyr = ds.ExecuteSQL(sql, dialect='SQLite')
        gdal.PopErrorHandler()
        assert sql_lyr is None, sql

    # Invalid hstore syntax or empty result
    for sql in ["SELECT hstore_get_value('a', null)",
                "SELECT hstore_get_value(null, 'a')",
                "SELECT hstore_get_value(1,'a')",
                "SELECT hstore_get_value('a',1)",
                "SELECT hstore_get_value('a=>b','c')"]:
        sql_lyr = ds.ExecuteSQL(sql, dialect='SQLite')
        f = sql_lyr.GetNextFeature()
        if not f.IsFieldNull(0):
            f.DumpReadable()
            pytest.fail(sql)
        ds.ReleaseResultSet(sql_lyr)

    # Valid hstore syntax
    for (sql, expected) in [("SELECT hstore_get_value('a=>b', 'a')", 'b'), ]:
        sql_lyr = ds.ExecuteSQL(sql, dialect='SQLite')
        f = sql_lyr.GetNextFeature()
        if f.GetField(0) != expected:
            f.DumpReadable()
            pytest.fail(sql)
        ds.ReleaseResultSet(sql_lyr)


###############################################################################
# Test compat with curve geometries


def test_ogr_sql_sqlite_29():

    ds = ogr.GetDriverByName('Memory').CreateDataSource('')
    lyr = ds.CreateLayer('test', geom_type=ogr.wkbCircularString)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('CIRCULARSTRING(0 0,1 0,0 0)'))
    lyr.CreateFeature(f)
    f = None
    sql_lyr = ds.ExecuteSQL('select * from test', dialect='SQLite')
    geom_type = sql_lyr.GetGeomType()
    f = sql_lyr.GetNextFeature()
    got_wkt = f.GetGeometryRef().ExportToWkt()
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    assert geom_type == ogr.wkbCircularString

    assert got_wkt == 'CIRCULARSTRING (0 0,1 0,0 0)'

###############################################################################
# Test compat with M geometries


def test_ogr_sql_sqlite_30():

    ds = ogr.GetDriverByName('Memory').CreateDataSource('')
    lyr = ds.CreateLayer('testm', geom_type=ogr.wkbLineStringM)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING M (1 2 3)'))
    lyr.CreateFeature(f)
    f = None
    lyr = ds.CreateLayer('testzm', geom_type=ogr.wkbLineStringZM)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING ZM (1 2 3 4)'))
    lyr.CreateFeature(f)
    f = None
    sql_lyr = ds.ExecuteSQL('select * from testm', dialect='SQLite')
    geom_type = sql_lyr.GetGeomType()
    f = sql_lyr.GetNextFeature()
    got_wkt = f.GetGeometryRef().ExportToIsoWkt()
    ds.ReleaseResultSet(sql_lyr)

    assert geom_type == ogr.wkbLineStringM

    assert got_wkt == 'LINESTRING M (1 2 3)'

    sql_lyr = ds.ExecuteSQL('select * from testzm', dialect='SQLite')
    geom_type = sql_lyr.GetGeomType()
    f = sql_lyr.GetNextFeature()
    got_wkt = f.GetGeometryRef().ExportToIsoWkt()
    ds.ReleaseResultSet(sql_lyr)

    assert geom_type == ogr.wkbLineStringZM

    assert got_wkt == 'LINESTRING ZM (1 2 3 4)'

###############################################################################
# Test filtering complex field name


def test_ogr_sql_sqlite_31():

    ds = ogr.GetDriverByName('Memory').CreateDataSource('')
    lyr = ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn('50M3 @w35Om3 N@M3', ogr.OFTInteger))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField(0, 25)
    lyr.CreateFeature(f)
    f = None

    sql_lyr = ds.ExecuteSQL('select * from test where "50M3 @w35Om3 N@M3" = 25', dialect='SQLite')
    f = sql_lyr.GetNextFeature()
    value = f.GetField(0)
    ds.ReleaseResultSet(sql_lyr)

    assert value == 25



###############################################################################
# Test flattening of geometry collection inside geometry collection


def test_ogr_sql_sqlite_geomcollection_in_geomcollection():

    ds = ogr.GetDriverByName('Memory').CreateDataSource('')
    lyr = ds.CreateLayer('test', geom_type=ogr.wkbLineStringM)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION (MULTIPOINT(1 2,3 4),MULTILINESTRING((5 6,7 8),(9 10,11 12)))'))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('MULTILINESTRING ((5 6,7 8),(9 10,11 12))'))
    lyr.CreateFeature(f)
    f = None
    sql_lyr = ds.ExecuteSQL('select * from test', dialect='SQLite')
    f = sql_lyr.GetNextFeature()
    got_wkt_1 = f.GetGeometryRef().ExportToIsoWkt()
    f = sql_lyr.GetNextFeature()
    got_wkt_2 = f.GetGeometryRef().ExportToIsoWkt()
    ds.ReleaseResultSet(sql_lyr)

    assert got_wkt_1 == 'GEOMETRYCOLLECTION (POINT (1 2),POINT (3 4),LINESTRING (5 6,7 8),LINESTRING (9 10,11 12))'
    assert got_wkt_2 == 'MULTILINESTRING ((5 6,7 8),(9 10,11 12))'



###############################################################################
# Test ST_MakeValid()


def test_ogr_sql_sqlite_st_makevalid():

    # Check if MakeValid() is available
    g = ogr.CreateGeometryFromWkt('POLYGON ((0 0,10 10,0 10,10 0,0 0))')
    with gdaltest.error_handler():
        make_valid_available = g.MakeValid() is not None

    ds = ogr.GetDriverByName('Memory').CreateDataSource('')
    sql = "SELECT ST_MakeValid(ST_GeomFromText('POLYGON ((0 0,1 1,1 0,0 1,0 0))'))"
    with gdaltest.error_handler():
        sql_lyr = ds.ExecuteSQL(sql, dialect='SQLite')
    if sql_lyr is None:
        assert not make_valid_available
        pytest.skip()
    f = sql_lyr.GetNextFeature()
    g = f.GetGeometryRef()
    wkt = g.ExportToWkt() if g is not None else None
    ds.ReleaseResultSet(sql_lyr)

    if make_valid_available:
        assert ogrtest.check_feature_geometry(ogr.CreateGeometryFromWkt(wkt), 'MULTIPOLYGON (((0.5 0.5,0 0,0 1,0.5 0.5)),((0.5 0.5,1 1,1 0,0.5 0.5)))') == 0, wkt



###############################################################################
# Test field names with same case


def test_ogr_sql_sqlite_field_names_same_case():

    ds = ogr.GetDriverByName('Memory').CreateDataSource('')
    lyr = ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn('id'))
    lyr.CreateField(ogr.FieldDefn('ID'))
    lyr.CreateField(ogr.FieldDefn('ID2'))
    f = ogr.Feature(lyr.GetLayerDefn())
    f['id'] = 'foo'
    f['ID'] = 'bar'
    f['ID2'] = 'baz'
    lyr.CreateFeature(f)

    sql_lyr = ds.ExecuteSQL('SELECT * FROM test', dialect='SQLite')
    f = sql_lyr.GetNextFeature()
    ds.ReleaseResultSet(sql_lyr)
    assert f['id'] == 'foo'
    assert f['ID3'] == 'bar'
    assert f['ID2'] == 'baz'


###############################################################################
# Test attribute and geometry field name with same name


def test_ogr_sql_sqlite_attribute_and_geom_field_name_same():

    ds = ogr.GetDriverByName('Memory').CreateDataSource('')
    lyr = ds.CreateLayer('test', geom_type = ogr.wkbNone)
    lyr.CreateField(ogr.FieldDefn('foo'))
    lyr.CreateGeomField(ogr.GeomFieldDefn('foo'))
    f = ogr.Feature(lyr.GetLayerDefn())
    f['foo'] = 'bar'
    f.SetGeomFieldDirectly('foo', ogr.CreateGeometryFromWkt('POINT(0 0)'))
    lyr.CreateFeature(f)

    sql_lyr = ds.ExecuteSQL('SELECT * FROM test', dialect='SQLite')
    f = sql_lyr.GetNextFeature()
    ds.ReleaseResultSet(sql_lyr)
    assert f['foo'] == 'bar'
    assert f.GetGeomFieldRef(0).ExportToWkt() == 'POINT (0 0)'

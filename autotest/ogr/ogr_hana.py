#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test SAP HANA driver functionality.
# Author:   Maxim Rylov
#
###############################################################################
# Copyright (c) 2020, SAP SE
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
from os import environ

import gdaltest
import ogrtest
import pytest

from osgeo import ogr
from osgeo import osr
from osgeo import gdal

try:
    from hdbcli import dbapi
except ImportError:
    pytest.skip("hdbcli not available", allow_module_level=True)

pytestmark = pytest.mark.require_driver('HANA')


@pytest.fixture(scope="module", autouse=True)
def setup_driver():
    driver = ogr.GetDriverByName('HANA')
    if driver is None:
        pytest.skip("HANA driver not available", allow_module_level=True)

    conn = create_connection()

    uid = execute_sql_scalar(conn, "SELECT REPLACE(CURRENT_UTCDATE, '-', '') || '_' || BINTOHEX(SYSUUID) FROM DUMMY;")
    gdaltest.hana_schema_name = '{}_{}'.format('gdal_test', uid)

    execute_sql(conn, f'CREATE SCHEMA "{gdaltest.hana_schema_name}"')

    ds = open_datasource(1)
    create_tpoly_table(ds)

    yield

    execute_sql(conn, f'DROP SCHEMA "{gdaltest.hana_schema_name}" CASCADE')


@pytest.fixture()
def ogrsf_path():
    import test_cli_utilities
    path = test_cli_utilities.get_test_ogrsf_path()
    if path is None:
        pytest.skip('ogrsf test utility not found')
    return path


###############################################################################
# Test data source capabilities

def test_ogr_hana_1():
    def test_capabilities(update, capabilities):
        ds = open_datasource(update)
        assert ds is not None, 'Data source is none'

        for capability in capabilities:
            assert ds.TestCapability(capability[0]) == capability[1]

    test_capabilities(0, [[ogr.ODsCCreateLayer, False],
                          [ogr.ODsCDeleteLayer, False],
                          [ogr.ODsCCreateGeomFieldAfterCreateLayer, False],
                          [ogr.ODsCMeasuredGeometries, True],
                          [ogr.ODsCCurveGeometries, False],
                          [ogr.ODsCTransactions, True]])

    test_capabilities(1, [[ogr.ODsCCreateLayer, True],
                          [ogr.ODsCDeleteLayer, True],
                          [ogr.ODsCCreateGeomFieldAfterCreateLayer, True],
                          [ogr.ODsCMeasuredGeometries, True],
                          [ogr.ODsCCurveGeometries, False],
                          [ogr.ODsCTransactions, True]])


###############################################################################
# Verify data in TPOLY table

def test_ogr_hana_2():
    ds = open_datasource()
    layer = ds.GetLayerByName('TPOLY')

    shp_ds = ogr.Open('data/poly.shp')
    shp_layer = shp_ds.GetLayer(0)

    assert layer.GetFeatureCount() == shp_layer.GetFeatureCount(), \
        'feature count does not match'
    assert layer.GetSpatialRef().GetAuthorityCode(None) == shp_layer.GetSpatialRef().GetAuthorityCode(None), \
        'spatial ref does not match'

    layer.SetAttributeFilter(None)
    field_count = layer.GetLayerDefn().GetFieldCount()
    orig_feat = shp_layer.GetNextFeature()

    while orig_feat is not None:
        read_feat = layer.GetNextFeature()

        assert read_feat.GetFieldCount() == field_count, 'Field count does not match'

        assert (ogrtest.check_feature_geometry(read_feat, orig_feat.GetGeometryRef(),
                                               max_error=0.001) == 0)
        for fld in range(field_count - 1):
            assert orig_feat.GetField(fld) == read_feat.GetField(fld), \
                ('Attribute %d does not match' % fld)

        orig_feat = shp_layer.GetNextFeature()


###############################################################################
# Test attribute filter

def test_ogr_hana_3():
    ds = open_datasource()
    layer = ds.GetLayerByName('tpoly')

    layer.SetAttributeFilter('EAS_ID > 160 AND EAS_ID < 170')
    tr = ogrtest.check_features_against_list(layer, 'EAS_ID', [168, 169, 166, 165])

    assert layer.GetFeatureCount() == 4, \
        'GetFeatureCount() returned %d instead of 4' % layer.GetFeatureCount()

    assert tr


###############################################################################
# Test spatial filter

def test_ogr_hana_4():
    ds = open_datasource()
    layer = ds.GetLayerByName('TPOLY')

    geom = ogr.CreateGeometryFromWkt('LINESTRING(479505 4763195,480526 4762819)')
    layer.SetSpatialFilter(geom)

    assert layer.GetFeatureCount() == 1, \
        'GetFeatureCount() returned %d instead of 1' % layer.GetFeatureCount()

    assert ogrtest.check_features_against_list(layer, 'EAS_ID', [158])


###############################################################################
# Test reading a layer extent

def test_ogr_hana_5():
    ds = open_datasource()
    layer = ds.GetLayerByName('tpoly')
    assert layer is not None, 'did not get tpoly layer'

    check_bboxes(layer.GetExtent(), (478315.53125, 481645.3125, 4762880.5, 4765610.5), 0.0001)


###############################################################################
# Test reading a SQL result layer extent

def test_ogr_hana_6():
    ds = open_datasource()
    layer = ds.ExecuteSQL('SELECT * FROM TPOLY')
    check_bboxes(layer.GetExtent(), (478315.53125, 481645.3125, 4762880.5, 4765610.5), 0.0001)


###############################################################################
# Test returned spatial reference

def test_ogr_hana_7():
    ds = open_datasource()
    layer = ds.ExecuteSQL('SELECT * FROM TPOLY')
    assert layer.GetSpatialRef().GetAuthorityCode(None) == '27700', \
        'returned wrong spatial reference id'
    ds.ReleaseResultSet(layer)


###############################################################################
# Test returned geometry type

def test_ogr_hana_8():
    ds = open_datasource()
    layer = ds.ExecuteSQL('SELECT * FROM TPOLY')
    assert layer.GetLayerDefn().GetGeomFieldDefn(0).GetType() == ogr.wkbPolygon, \
        'Returned wrong geometry type'
    ds.ReleaseResultSet(layer)


###############################################################################
# Write new features with geometries and verify them

def test_ogr_hana_9():
    layer_name = get_test_name()
    ds = open_datasource(1)
    create_tpoly_table(ds, layer_name)

    layer = ds.GetLayerByName(layer_name)

    dst_feat = ogr.Feature(feature_def=layer.GetLayerDefn())
    wkt_list = ['10', '2', '1', '3d_1', '4', '5', '6']

    for item in wkt_list:
        wkt = open('data/wkb_wkt/' + item + '.wkt').read()
        geom = ogr.CreateGeometryFromWkt(wkt)

        ######################################################################
        # Write geometry as a new feature.

        dst_feat.SetGeometryDirectly(geom)
        dst_feat.SetField('PRFEDEA', item)
        dst_feat.SetFID(-1)
        layer.CreateFeature(dst_feat)

        ######################################################################
        # Read back the feature and get the geometry.

        layer.SetAttributeFilter("PRFEDEA = '%s'" % item)

        feat_read = layer.GetNextFeature()

        if ogrtest.check_feature_geometry(feat_read, geom) != 0:
            print(item)
            print(wkt)
            pytest.fail(geom)

    layer.ResetReading()


###############################################################################
# Test ExecuteSQL() without geometry

def test_ogr_hana_10():
    ds = open_datasource()
    layer = ds.ExecuteSQL('SELECT EAS_ID FROM tpoly WHERE EAS_ID IN (158, 170) ')
    assert layer.GetFeatureCount() == 2, \
        'GetFeatureCount() returned %d instead of 2' % layer.GetFeatureCount()
    assert ogrtest.check_features_against_list(layer, 'EAS_ID', [158, 170])


###############################################################################
# Test ExecuteSQL() results layers without geometry

def test_ogr_hana_11():
    ds = open_datasource()
    layer = ds.ExecuteSQL('SELECT DISTINCT EAS_ID FROM TPOLY ORDER BY EAS_ID DESC')
    assert layer.GetFeatureCount() == 10

    expected = [179, 173, 172, 171, 170, 169, 168, 166, 165, 158]
    tr = ogrtest.check_features_against_list(layer, 'EAS_ID', expected)
    ds.ReleaseResultSet(layer)

    assert tr


###############################################################################
# Test ExecuteSQL() with empty result set

def test_ogr_hana_12():
    ds = open_datasource()

    layer = ds.ExecuteSQL('SELECT * FROM TPOLY WHERE EAS_ID = 7892342')
    assert layer is not None, 'Expected a non None layer'

    feat = layer.GetNextFeature()
    assert feat is None, 'Expected no features'

    ds.ReleaseResultSet(layer)


###############################################################################
# Test ExecuteSQL() with quoted table name

def test_ogr_hana_13():
    ds = open_datasource()
    layer = ds.ExecuteSQL('SELECT EAS_ID FROM "TPOLY" WHERE EAS_ID IN (158, 170) ')
    assert ogrtest.check_features_against_list(layer, 'EAS_ID', [158, 170])


###############################################################################
# Test GetFeature() method with an invalid id

def test_ogr_hana_14():
    ds = open_datasource()
    layer = ds.GetLayerByName('tpoly')
    assert layer.GetFeature(0) is None


###############################################################################
# Test inserting features without geometry

def test_ogr_hana_15():
    layer_name = get_test_name()
    ds = open_datasource(1)
    create_tpoly_table(ds, layer_name)
    layer = ds.GetLayerByName(layer_name)
    feat_count = layer.GetFeatureCount()

    dst_feat = ogr.Feature(feature_def=layer.GetLayerDefn())
    dst_feat.SetField('PRFEDEA', '7777')
    dst_feat.SetField('EAS_ID', 2000)
    dst_feat.SetFID(-1)
    layer.CreateFeature(dst_feat)

    assert (feat_count + 1) == layer.GetFeatureCount(), \
        ('Feature count %d is not as expected %d' % (layer.GetFeatureCount(), feat_count + 1))


###############################################################################
# Test reading features without geometry

def test_ogr_hana_16():
    layer_name = get_test_name()
    ds = open_datasource(1)
    create_tpoly_table(ds, layer_name)
    layer = ds.GetLayerByName(layer_name)

    feat = ogr.Feature(feature_def=layer.GetLayerDefn())
    feat.SetField('PRFEDEA', '7777')
    feat.SetField('EAS_ID', 2000)
    feat.SetFID(-1)
    layer.CreateFeature(feat)

    layer.SetAttributeFilter("PRFEDEA = '7777'")
    feat = layer.GetNextFeature()
    assert feat.GetGeometryRef() is None, 'NULL geometry is expected'


###############################################################################
# Write a feature with too long a text value for a fixed length text field.
# The driver should now truncate this (but with a debug message).  Also,
# put some crazy stuff in the value to verify that quoting and escaping
# is working smoothly.
#
# No geometry in this test.

def test_ogr_hana_17():
    layer_name = get_test_name()
    ds = open_datasource(1)
    create_tpoly_table(ds, layer_name)
    layer = ds.GetLayerByName(layer_name)
    dst_feat = ogr.Feature(feature_def=layer.GetLayerDefn())

    dst_feat.SetField('PRFEDEA', 'CrazyKey')
    dst_feat.SetField('SHORTNAME', 'Crazy"\'Long')
    layer.CreateFeature(dst_feat)

    layer.SetAttributeFilter("PRFEDEA = 'CrazyKey'")
    feat_read = layer.GetNextFeature()

    assert feat_read is not None, 'creating crazy feature failed!'

    assert feat_read.GetField('shortname') == 'Crazy"\'L', \
        ('Value not properly escaped or truncated:' + feat_read.GetField('shortname'))


###############################################################################
# Verify inplace update of a feature with SetFeature()

def test_ogr_hana_18():
    layer_name = get_test_name()
    ds = open_datasource(1)
    create_tpoly_table(ds, layer_name)
    layer = ds.GetLayerByName(layer_name)

    feat_new = ogr.Feature(feature_def=layer.GetLayerDefn())
    feat_new.SetField('PRFEDEA', '9999')
    layer.CreateFeature(feat_new)
    feat_new.Destroy()

    layer.SetAttributeFilter("PRFEDEA = '9999'")
    feat = layer.GetNextFeature()
    layer.SetAttributeFilter(None)

    feat.SetField('SHORTNAME', 'Reset')
    point = ogr.Geometry(ogr.wkbPoint25D)
    point.SetPoint(0, 5, 6, 7)
    feat.SetGeometryDirectly(point)

    assert layer.SetFeature(feat) == 0, 'SetFeature() method failed.'

    fid = feat.GetFID()
    feat.Destroy()

    feat = layer.GetFeature(fid)
    assert feat is not None, ('GetFeature(%d) failed.' % fid)

    shortname = feat.GetField('SHORTNAME')
    assert shortname[:5] == 'Reset', ('SetFeature() did not update SHORTNAME, got %s.'
                                      % shortname)

    if ogrtest.check_feature_geometry(feat, 'POINT(5 6 7)') != 0:
        print(feat.GetGeometryRef())
        pytest.fail('Geometry update failed')

    feat.SetGeometryDirectly(None)
    assert layer.SetFeature(feat) == 0, 'SetFeature() method failed.'
    feat = layer.GetFeature(fid)
    assert feat.GetGeometryRef() is None, 'Geometry update failed. null geometry expected'

    feat.SetFieldNull('SHORTNAME')
    layer.SetFeature(feat)
    feat = layer.GetFeature(fid)
    assert feat.IsFieldNull('SHORTNAME'), 'SHORTNAME update failed. null value expected'

    # Test updating non-existing feature
    feat.SetFID(-10)
    assert layer.SetFeature(feat) == ogr.OGRERR_NON_EXISTING_FEATURE, \
        'Expected failure of SetFeature().'


###############################################################################
# Verify that DeleteFeature() works properly

def test_ogr_hana_19():
    layer_name = get_test_name()
    ds = open_datasource(1)
    create_tpoly_table(ds, layer_name)
    layer = ds.GetLayerByName(layer_name)
    layer.SetAttributeFilter("PRFEDEA = '35043411'")
    feat = layer.GetNextFeature()
    layer.SetAttributeFilter(None)

    fid = feat.GetFID()
    assert layer.DeleteFeature(fid) == 0, 'DeleteFeature() method failed.'

    layer.SetAttributeFilter("PRFEDEA = '35043411'")
    feat = layer.GetNextFeature()
    layer.SetAttributeFilter(None)

    assert feat is None, 'DeleteFeature() seems to have had no effect.'

    # Test deleting non-existing feature
    assert layer.DeleteFeature(-10) == ogr.OGRERR_NON_EXISTING_FEATURE, \
        'Expected failure of DeleteFeature().'


###############################################################################
# Test default values

def test_ogr_hana_20():
    ds = open_datasource(1)

    layer_name = get_test_name()
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    layer = ds.CreateLayer(layer_name, srs, options=[])

    field_values = [999, 1, 6823, 623445, 78912394123, 12.253, 534.23749234, 7234.89732, "'2018/04/25'", "'21:15:47'",
                    "'2018/04/25 21:15:47.987'", "'hello'", None, '74657374737472696e67', None, [], [], [], [],
                    'POINT (10 10)']
    field_values_expected = [999, 1, 6823, 623445, 78912394123, 12.253, 534.23749234, 7234.89732, '2018/04/25',
                             '21:15:47',
                             '2018/04/25 21:15:47.987', 'hello', None, b'74657374737472696e67', None, [], [], [], [],
                             None]

    field_defns = []

    field_defn = ogr.FieldDefn('FIELD_BOOLEAN', ogr.OFTInteger)
    field_defn.SetSubType(ogr.OFSTBoolean)
    field_defn.SetDefault(str(field_values[1]))
    field_defns.append(field_defn)

    field_defn = ogr.FieldDefn('FIELD_SHORT', ogr.OFTInteger)
    field_defn.SetSubType(ogr.OFSTInt16)
    field_defn.SetDefault(str(field_values[2]))
    field_defns.append(field_defn)

    field_defn = ogr.FieldDefn('FIELD_INT', ogr.OFTInteger)
    field_defn.SetDefault(str(field_values[3]))
    field_defns.append(field_defn)

    field_defn = ogr.FieldDefn('FIELD_INT64', ogr.OFTInteger64)
    field_defn.SetDefault(str(field_values[4]))
    field_defns.append(field_defn)

    field_defn = ogr.FieldDefn('FIELD_FLOAT', ogr.OFTReal)
    field_defn.SetSubType(ogr.OFSTFloat32)
    field_defn.SetDefault(str(field_values[5]))
    field_defns.append(field_defn)

    field_defn = ogr.FieldDefn('FIELD_DOUBLE', ogr.OFTReal)
    field_defn.SetDefault(str(field_values[6]))
    field_defns.append(field_defn)

    field_defn = ogr.FieldDefn('FIELD_DECIMAL', ogr.OFTReal)
    field_defn.SetDefault(str(field_values[7]))
    field_defn.SetWidth(16)
    field_defn.SetPrecision(5)
    field_defns.append(field_defn)

    field_defn = ogr.FieldDefn('FIELD_DATE', ogr.OFTDate)
    field_defn.SetDefault(str(field_values[8]))
    field_defns.append(field_defn)

    field_defn = ogr.FieldDefn('FIELD_TIME', ogr.OFTTime)
    field_defn.SetDefault(str(field_values[9]))
    field_defns.append(field_defn)

    field_defn = ogr.FieldDefn('FIELD_TIMESTAMP', ogr.OFTDateTime)
    field_defn.SetDefault(str(field_values[10]))
    field_defns.append(field_defn)

    field_defn = ogr.FieldDefn('FIELD_STRING', ogr.OFTString)
    field_defn.SetDefault(str(field_values[11]))
    field_defns.append(field_defn)

    field_defn = ogr.FieldDefn('FIELD_STRING_NULL', ogr.OFTString)
    field_defn.SetDefault(field_values[12])
    field_defns.append(field_defn)

    field_defn = ogr.FieldDefn('FIELD_BINARY', ogr.OFTBinary)
    field_defn.SetDefault(str(field_values[13]))
    field_defn.SetNullable(1)
    field_defn.SetWidth(500)
    field_defns.append(field_defn)

    field_defn = ogr.FieldDefn('FIELD_NO_DEFAULT', ogr.OFTInteger)
    field_defns.append(field_defn)

    field_defn = ogr.FieldDefn('FIELD_INT_LIST', ogr.OFTIntegerList)
    field_defns.append(field_defn)

    field_defn = ogr.FieldDefn('FIELD_INT64_LIST', ogr.OFTInteger64List)
    field_defns.append(field_defn)

    field_defn = ogr.FieldDefn('FIELD_REAL_LIST', ogr.OFTRealList)
    field_defns.append(field_defn)

    field_defn = ogr.FieldDefn('FIELD_STRING_LIST', ogr.OFTStringList)
    field_defns.append(field_defn)

    for field_defn in field_defns:
        assert layer.CreateField(field_defn) == ogr.OGRERR_NONE, \
            ('CreateField failed for %s' % field_defn.GetNameRef())

    new_feat = ogr.Feature(layer.GetLayerDefn())
    new_feat.SetFID(field_values[0])
    new_feat.SetFieldNull('FIELD_STRING_NULL')
    new_feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt(field_values[19]))
    assert layer.CreateFeature(new_feat) == ogr.OGRERR_NONE

    layer.ResetReading()

    layer_defn = layer.GetLayerDefn()

    ds = open_datasource(0)
    layer_new = ds.GetLayerByName(layer_name)
    layer_defn_new = layer_new.GetLayerDefn()
    feat = layer_new.GetNextFeature()

    for i in range(layer_defn.GetFieldCount()):
        field_defn = layer_defn.GetFieldDefn(i)
        field_name = field_defn.GetNameRef()
        field_defn_new = layer_defn_new.GetFieldDefn(layer_defn_new.GetFieldIndex(field_name))
        assert field_defn.GetDefault() == field_defn_new.GetDefault()
        if field_defn.GetType() in [ogr.OFTIntegerList, ogr.OFTInteger64List, ogr.OFTRealList, ogr.OFTStringList]:
            continue
        expected_value = field_values_expected[i + 1]
        actual = feat.GetFieldAsBinary(field_name) if field_defn.GetType() == ogr.OFTBinary else feat.GetField(
            field_name)
        assert check_values(actual, expected_value, 0.01), \
            pytest.fail('Values in field %s do not match (actual: %s, expected: %s)' %
                        (field_name, actual, expected_value))


###############################################################################
# Test creating a field with the fid name

def test_ogr_hana_21():
    ds = open_datasource(1)
    layer = ds.CreateLayer(get_test_name(), geom_type=ogr.wkbNone, options=['FID=fid', 'LAUNDER=NO'])

    with gdaltest.error_handler():
        assert layer.CreateField(ogr.FieldDefn('str', ogr.OFTString)) == 0
        assert layer.CreateField(ogr.FieldDefn('fid', ogr.OFTString)) != 0
        assert layer.CreateField(ogr.FieldDefn('fid', ogr.OFTInteger)) != 0

    layer.ResetReading()


###############################################################################
# Test very large query

def test_ogr_hana_22():
    ds = open_datasource()

    query = 'eas_id = 169'
    for eid in range(1000):
        query = query + (' or eas_id = %d' % (eid + 1000))

    layer = ds.GetLayerByName('TPOLY')
    layer.SetAttributeFilter(query)
    assert ogrtest.check_features_against_list(layer, 'eas_id', [169])


###############################################################################
# Test COLUMN_TYPES layer creation option

def test_ogr_hana_23():
    ds = open_datasource(1)

    layer_name = get_test_name()
    layer = ds.CreateLayer(layer_name,
                           options=['COLUMN_TYPES=SINT=SMALLINT,DEC1=DECIMAL(10,5),DEC2=DECIMAL(20,0)'])
    layer.CreateField(ogr.FieldDefn('SINT', ogr.OFTString))
    layer.CreateField(ogr.FieldDefn('DEC1', ogr.OFTString))
    layer.CreateField(ogr.FieldDefn('DEC2', ogr.OFTString))

    ds = open_datasource()
    layer = ds.GetLayerByName(layer_name)
    layer_defn = layer.GetLayerDefn()
    field_SINT = layer_defn.GetFieldDefn(layer_defn.GetFieldIndex('SINT'))
    field_DEC1 = layer_defn.GetFieldDefn(layer_defn.GetFieldIndex('DEC1'))
    field_DEC2 = layer_defn.GetFieldDefn(layer_defn.GetFieldIndex('DEC2'))
    assert field_SINT.GetType() == ogr.OFTInteger
    assert field_SINT.GetWidth() == 0
    assert field_DEC1.GetType() == ogr.OFTReal
    assert field_DEC1.GetWidth() == 10
    assert field_DEC1.GetPrecision() == 5
    assert field_DEC2.GetType() == ogr.OFTReal
    assert field_DEC2.GetWidth() == 20
    assert field_DEC2.GetPrecision() == 0


###############################################################################
# Run test_ogrsf

def test_ogr_hana_24(ogrsf_path):
    conn_str = get_connection_str() + ';SCHEMA=' + gdaltest.hana_schema_name
    ret = gdaltest.runexternal(ogrsf_path + ' "' + 'HANA:' + conn_str + '" TPOLY')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1


###############################################################################
# Run test_ogrsf with -sql

def test_ogr_hana_25(ogrsf_path):
    conn_str = get_connection_str() + ';SCHEMA=' + gdaltest.hana_schema_name
    ret = gdaltest.runexternal(ogrsf_path + ' "' + 'HANA:' + conn_str +
                               '" -sql "SELECT * FROM TPOLY"')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1


###############################################################################
# Test retrieving an error from ExecuteSQL()

def test_ogr_hana_26():
    ds = open_datasource()
    gdal.ErrorReset()
    with gdaltest.error_handler():
        layer = ds.ExecuteSQL('SELECT FROM')
    assert gdal.GetLastErrorMsg() != ''
    assert layer is None


###############################################################################
# Test array types

def test_ogr_hana_27():
    conn = create_connection()

    def get_str(s):
        if s is None:
            return "NULL"
        if isinstance(s, str):
            return "'{}'".format(s)
        return str(s)

    def test_array_type(arr_type, arr_values, expected_type, expected_sub_type = ogr.OFSTNone):
        layer_name = get_test_name()
        table_name = f'"{gdaltest.hana_schema_name}"."{layer_name}"'
        execute_sql(conn, f'CREATE COLUMN TABLE {table_name} ( COL1 INT PRIMARY KEY, COL2 {arr_type} ARRAY )')
        str_values = ', '.join(get_str(e) for e in arr_values)
        execute_sql(conn, f"INSERT INTO {table_name} VALUES ( 1, ARRAY ( {str_values} ) )")

        ds = open_datasource(0)
        layer = ds.GetLayerByName(layer_name)
        layer_defn = layer.GetLayerDefn()
        field_defn = layer_defn.GetFieldDefn(layer_defn.GetFieldIndex('COL2'))
        assert field_defn.GetType() == expected_type
        if field_defn.GetSubType() != ogr.OFSTNone:
            assert field_defn.GetSubType() == expected_sub_type

        feat = layer.GetNextFeature()
        values = feat['COL2']
        assert len(values) == len(arr_values)
        for i in range(0, len(arr_values)):
            assert values[i] == arr_values[i]

        execute_sql(conn, f'DROP TABLE {table_name}')

    test_array_type('BOOLEAN', [True, False], ogr.OFTIntegerList, ogr.OFSTBoolean)
    test_array_type('SMALLINT', [-1, 7982], ogr.OFTIntegerList, ogr.OFSTInt16)
    test_array_type('INT', [-1, 0, 1, 2, 2147483647, 4], ogr.OFTIntegerList)
    test_array_type('BIGINT', [-1, 0, 9223372036854775807], ogr.OFTInteger64List)
    test_array_type('DOUBLE', [-1.0002, 0.0, 4.6828734], ogr.OFTRealList)
    test_array_type('NVARCHAR(300)', ['str1', '', 'str2'], ogr.OFTStringList)
    test_array_type('VARCHAR(100)', ['str1', '', 'str2'], ogr.OFTStringList)


###############################################################################
# Test DETECT_GEOMETRY_TYPE open options

def test_ogr_hana_28():
    ds_with_gt = open_datasource(0, 'DETECT_GEOMETRY_TYPE=YES')
    layer = ds_with_gt.GetLayerByName('TPOLY')
    assert layer.GetLayerDefn().GetGeomFieldDefn(0).GetType() == ogr.wkbPolygon, \
        'Returned wrong geometry type'
    ds_without_gt = open_datasource(0, 'DETECT_GEOMETRY_TYPE=NO')
    layer = ds_without_gt.GetLayerByName('TPOLY')
    assert layer.GetLayerDefn().GetGeomFieldDefn(0).GetType() == ogr.wkbUnknown, \
        'Returned wrong geometry type'


###############################################################################
# Test CreateGeomField

def test_ogr_hana_29():
    ds_ro = open_datasource(0)
    layer = ds_ro.GetLayerByName('TPOLY')
    with gdaltest.error_handler():
        assert layer.CreateGeomField(ogr.GeomFieldDefn('GEOM_FIELD', ogr.wkbPoint)) == ogr.OGRERR_FAILURE

    layer_name = get_test_name()
    ds_rw = open_datasource(1)
    create_tpoly_table(ds_rw, layer_name)

    layer = ds_rw.GetLayerByName(layer_name)
    with gdaltest.error_handler():
        # unsupported geometry type
        assert layer.CreateGeomField(ogr.GeomFieldDefn('GEOM_FIELD', ogr.wkbCompoundCurve)) == ogr.OGRERR_FAILURE
        # duplicate field name
        assert layer.CreateGeomField(ogr.GeomFieldDefn('OGR_GEOMETRY', ogr.wkbPoint)) == ogr.OGRERR_FAILURE
        # undefined srs
        assert layer.CreateGeomField(ogr.GeomFieldDefn('GEOM_FIELD', ogr.wkbPoint)) == ogr.OGRERR_FAILURE

    gfld_defn = ogr.GeomFieldDefn('', ogr.wkbPolygon)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    gfld_defn.SetSpatialRef(srs)
    assert layer.CreateGeomField(gfld_defn) == ogr.OGRERR_NONE
    layer_defn = layer.GetLayerDefn()
    assert layer_defn.GetGeomFieldCount() == 2
    assert layer_defn.GetGeomFieldDefn(1).GetType() == ogr.wkbPolygon, 'Returned wrong geometry type'
    assert layer_defn.GetGeomFieldDefn(1).GetName() == 'OGR_GEOMETRY_1', 'Returned wrong geometry field name'


###############################################################################
# Test DateTime time zones

def test_ogr_hana_30():
    ds = open_datasource(1)

    layer_name = get_test_name()
    layer = ds.CreateLayer(layer_name)
    layer.CreateField(ogr.FieldDefn('DT', ogr.OFTDateTime))
    for val in ['2020/01/01 01:34:56', '2020/01/01 01:34:56+00', '2020/01/01 01:34:56.789+02']:
        feat = ogr.Feature(layer.GetLayerDefn())
        feat.SetField('DT', val)
        layer.CreateFeature(feat)

    ds = open_datasource(0)
    layer = ds.GetLayerByName(layer_name)
    for val in ['2020/01/01 01:34:56', '2020/01/01 01:34:56', '2019/12/31 23:34:56.789']:
        feat = layer.GetNextFeature()
        assert feat.GetField('DT') == val


###############################################################################
#  Create a table from data/poly.shp

def create_tpoly_table(ds, layer_name='TPOLY'):
    with gdaltest.error_handler():
        ds.ExecuteSQL('DELLAYER:%s' % layer_name)

    shp_ds = ogr.Open('data/poly.shp')
    shp_layer = shp_ds.GetLayer(0)

    ######################################################
    # Create Layer
    layer = ds.CreateLayer(layer_name, srs=shp_layer.GetSpatialRef())

    ######################################################
    # Check layer name

    assert layer.GetName() == layer_name, \
        pytest.fail('GetName() returned %s instead of %s' % (layer.GetName(), layer_name))

    ######################################################
    # Check capabilities

    for capabilities in [ogr.OLCFastFeatureCount,
                         ogr.OLCFastSpatialFilter,
                         ogr.OLCFastGetExtent,
                         ogr.OLCCreateField,
                         ogr.OLCCreateGeomField,
                         ogr.OLCDeleteFeature,
                         ogr.OLCAlterFieldDefn,
                         ogr.OLCRandomWrite,
                         ogr.OLCTransactions]:
        assert layer.TestCapability(capabilities)

    ######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def(layer,
                                   [('AREA', ogr.OFTReal),
                                    ('EAS_ID', ogr.OFTInteger64),
                                    ('PRFEDEA', ogr.OFTString),
                                    ('SHORTNAME', ogr.OFTString, 8)])

    ######################################################
    # Check fields

    feat_def = layer.GetLayerDefn()
    assert feat_def.GetGeomFieldCount() == 1, 'geometry field not found'
    assert feat_def.GetFieldCount() == 4, \
        'GetFieldCount() returned %d instead of %d' % (4, feat_def.GetFieldCount())

    ######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature(feature_def=layer.GetLayerDefn())

    feat = shp_layer.GetNextFeature()
    while feat is not None:
        dst_feat.SetFrom(feat)
        layer.CreateFeature(dst_feat)
        feat = shp_layer.GetNextFeature()


###############################################################################
# Helper methods

def get_connection_str():
    uri = gdal.GetConfigOption('OGR_HANA_CONNECTION_STRING', None)
    if uri is not None:
        conn_str = uri + ';ENCRYPT=YES;SSL_VALIDATE_CERTIFICATE=false;CHAR_AS_UTF8=1'
    else:
        conn_str = 'HANA:autotest'

    return conn_str


def create_connection():
    conn_str = get_connection_str()
    conn_params = dict(item.split("=") for item in conn_str.split(";"))

    with gdaltest.error_handler():
        conn = dbapi.connect(address=conn_params['HOST'], port=conn_params['PORT'], user=conn_params['USER'],
                             password=conn_params['PASSWORD'], ENCRYPT=conn_params['ENCRYPT'],
                             sslValidateCertificate=conn_params['SSL_VALIDATE_CERTIFICATE'], CHAR_AS_UTF8=1)

    if conn is None:
        pytest.skip()
    conn.setautocommit(False)
    return conn


def get_test_name():
    name = environ.get('PYTEST_CURRENT_TEST').split(':')[-1].split(' ')[0]
    return name.replace('test_', '').upper()


def execute_sql(conn, sql):
    cursor = conn.cursor()
    assert cursor
    cursor.execute(sql)
    cursor.close()
    conn.commit()


def execute_sql_scalar(conn, sql):
    cursor = conn.cursor()
    assert cursor
    cursor.execute(sql)
    res = cursor.fetchone()[0]
    cursor.close()
    conn.commit()
    return res


def open_datasource(update=0, open_opts=None):
    conn_str = 'HANA:' + get_connection_str() + ';SCHEMA=' + gdaltest.hana_schema_name
    if open_opts is None:
        return ogr.Open(conn_str, update=update)
    else:
        return gdal.OpenEx(conn_str, update, open_options=[open_opts])


def check_bboxes(actual, expected, max_error=0.001):
    minx = abs(actual[0] - expected[0])
    maxx = abs(actual[1] - expected[1])
    miny = abs(actual[2] - expected[2])
    maxy = abs(actual[3] - expected[3])

    if max(minx, maxx, miny, maxy) > max_error:
        print(actual)
        pytest.fail('Extents do not match')


def check_values(actual, expected, max_error=0.001):
    if actual is None and expected is None:
        return 1

    if isinstance(actual, float):
        dif = abs(actual - expected)
        if dif > max_error:
            return 0
    else:
        if actual != expected:
            return 0

    return 1


###############################################################################
# Reverse the vertex order

def reverse_points(poly):
    for geom_index in range(poly.GetGeometryCount()):
        ring = poly.GetGeometryRef(geom_index)
        point_count = ring.GetPointCount()

        for point_index in range(point_count // 2):
            point_rev_index = point_count - point_index - 1
            p1 = (ring.GetX(point_index), ring.GetY(point_index), ring.GetZ(point_index))
            ring.SetPoint(point_index, ring.GetX(point_rev_index), ring.GetY(point_rev_index),
                          ring.GetZ(point_rev_index))
            ring.SetPoint(point_rev_index, p1[0], p1[1], p1[2])

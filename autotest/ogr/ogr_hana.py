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
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
###############################################################################

import os

from osgeo.ogr import wkbPolygon

import gdaltest
import ogrtest
import pytest

from osgeo import ogr
from osgeo import osr
from osgeo import gdal

from hdbcli import dbapi

pytestmark = pytest.mark.require_driver('HANA')


###############################################################################
# Initialize SAP HANA data source.

def test_ogr_hana_init():
    gdaltest.hana_ds = None
    gdaltest.hana_drv = ogr.GetDriverByName('HANA')
    if gdaltest.hana_drv is None:
        pytest.skip()

    uri = gdal.GetConfigOption('OGR_HANA_CONNECTION_STRING', None)
    if uri is not None:
        gdaltest.hana_connection_string = uri
    else:
        if 'GDAL_HANA_TEST_DB' in os.environ:
            gdaltest.hana_connection_string = os.environ['GDAL_HANA_TEST_DB']
        else:
            gdaltest.hana_connection_string = 'DRIVER={\'/usr/sap/hdbclient/libodbcHDB.so\'};HOST=localhost;' \
                                              'PORT=30015;USER=SYSTEM;PASSWORD=mypassword'

    conn = create_connection(gdaltest.hana_connection_string)
    gdaltest.hana_schema_name = generate_schema_name(conn, 'gdal_test')
    execute_sql(conn, f'CREATE SCHEMA "{gdaltest.hana_schema_name}"')

    gdaltest.hana_ds = open_datasource()
    if gdaltest.hana_ds is None:
        pytest.skip()

    for capabilities in [ogr.ODsCCreateLayer,
                         ogr.ODsCDeleteLayer,
                         ogr.ODsCCreateGeomFieldAfterCreateLayer,
                         ogr.ODsCMeasuredGeometries,
                         ogr.ODsCTransactions]:
        assert gdaltest.hana_ds.TestCapability(capabilities)

    layer = gdaltest.hana_ds.ExecuteSQL("SELECT * FROM \"SYS\".\"M_DATABASE\"")
    f = layer.GetNextFeature()
    print('Version: ' + f.GetField("VERSION"))
    gdaltest.hana_ds.ReleaseResultSet(layer)


###############################################################################
#  Create a table from data/poly.shp

def test_ogr_hana_1():
    if gdaltest.hana_ds is None:
        pytest.skip()

    layer_name = 'TPOLY'
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    gdaltest.hana_ds.ExecuteSQL('DELLAYER:%s' % layer_name)
    gdal.PopErrorHandler()

    shp_ds = ogr.Open('data/poly.shp')
    shp_layer = shp_ds.GetLayer(0)

    ######################################################
    # Create Layer
    gdaltest.hana_layer = gdaltest.hana_ds.CreateLayer(layer_name, srs=shp_layer.GetSpatialRef())

    ######################################################
    # Check layer name

    assert gdaltest.hana_layer.GetName() == layer_name, \
        pytest.fail('GetName() returned %s instead of %s' % (gdaltest.hana_layer.GetName(), layer_name))

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
        assert gdaltest.hana_layer.TestCapability(capabilities)

    ######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def(gdaltest.hana_layer,
                                   [('AREA', ogr.OFTReal),
                                    ('EAS_ID', ogr.OFTInteger64),
                                    ('PRFEDEA', ogr.OFTString),
                                    ('SHORTNAME', ogr.OFTString, 8)])

    ######################################################
    # Check fields

    feat_def = gdaltest.hana_layer.GetLayerDefn()
    if feat_def.GetGeomFieldCount() != 1:
        pytest.fail('geometry field not found')

    if feat_def.GetFieldCount() != 4:
        pytest.fail('GetFieldCount() returned %d instead of %d' % (4, feat_def.GetFieldCount()))

    ######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature(feature_def=gdaltest.hana_layer.GetLayerDefn())

    feat = shp_layer.GetNextFeature()
    while feat is not None:
        dst_feat.SetFrom(feat)
        gdaltest.hana_layer.CreateFeature(dst_feat)
        feat = shp_layer.GetNextFeature()

    dst_feat.Destroy()

    shp_ds.Destroy()


###############################################################################
# Verify data that has been just written

def test_ogr_hana_2():
    if gdaltest.hana_ds is None:
        pytest.skip()

    shp_ds = ogr.Open('data/poly.shp')
    shp_layer = shp_ds.GetLayer(0)

    assert gdaltest.hana_layer.GetFeatureCount() == shp_layer.GetFeatureCount(), \
        'feature count does not match'
    assert gdaltest.hana_layer.GetSpatialRef().GetAuthorityCode(None) == shp_layer.GetSpatialRef().GetAuthorityCode(
        None), \
        'spatial ref does not match'

    gdaltest.hana_layer.SetAttributeFilter(None)
    field_count = gdaltest.hana_layer.GetLayerDefn().GetFieldCount()
    orig_feat = shp_layer.GetNextFeature()

    while orig_feat is not None:
        read_feat = gdaltest.hana_layer.GetNextFeature()

        assert read_feat.GetFieldCount() == field_count, \
            'Field count does not match'

        assert (ogrtest.check_feature_geometry(read_feat, orig_feat.GetGeometryRef(),
                                               max_error=0.001) == 0)
        for fld in range(field_count - 1):
            assert orig_feat.GetField(fld) == read_feat.GetField(fld), \
                ('Attribute %d does not match' % fld)

        read_feat.Destroy()
        orig_feat.Destroy()

        orig_feat = shp_layer.GetNextFeature()

    shp_ds.Destroy()


###############################################################################
# Test attribute filter

def test_ogr_hana_3():
    if gdaltest.hana_ds is None:
        pytest.skip()

    expect = [168, 169, 166, 165]

    gdaltest.hana_layer.SetAttributeFilter('EAS_ID > 160 AND EAS_ID < 170')
    tr = ogrtest.check_features_against_list(gdaltest.hana_layer, 'EAS_ID', expect)

    assert gdaltest.hana_layer.GetFeatureCount() == 4, \
        'GetFeatureCount() returned %d instead of 4' % gdaltest.hana_layer.GetFeatureCount()

    gdaltest.hana_layer.SetAttributeFilter(None)

    assert tr


###############################################################################
# Test spatial filter

def test_ogr_hana_4():
    if gdaltest.hana_ds is None:
        pytest.skip()

    geom = ogr.CreateGeometryFromWkt('LINESTRING(479505 4763195,480526 4762819)')
    gdaltest.hana_layer.SetSpatialFilter(geom)

    assert gdaltest.hana_layer.GetFeatureCount() == 1, \
        'GetFeatureCount() returned %d instead of 1' % gdaltest.hana_layer.GetFeatureCount()

    tr = ogrtest.check_features_against_list(gdaltest.hana_layer, 'EAS_ID', [158])

    gdaltest.hana_layer.SetSpatialFilter(None)

    assert tr


###############################################################################
# Test reading a layer extent

def test_ogr_hana_5():
    if gdaltest.hana_ds is None:
        pytest.skip()

    layer = gdaltest.hana_ds.GetLayerByName('tpoly')
    assert layer is not None, 'did not get tpoly layer'

    check_bboxes(layer.GetExtent(), (478315.53125, 481645.3125, 4762880.5, 4765610.5), 0.0001)


###############################################################################
# Test reading a SQL result layer extent

def test_ogr_hana_6():
    if gdaltest.hana_ds is None:
        pytest.skip()

    sql_lyr = gdaltest.hana_ds.ExecuteSQL('select * from tpoly')

    check_bboxes(sql_lyr.GetExtent(), (478315.53125, 481645.3125, 4762880.5, 4765610.5), 0.0001)


###############################################################################
# Test returned spatial reference

def test_ogr_hana_7():
    if gdaltest.hana_ds is None:
        pytest.skip()

    sql_layer = gdaltest.hana_ds.ExecuteSQL('SELECT * FROM tpoly')
    assert sql_layer.GetSpatialRef().GetAuthorityCode(None) == '27700', \
        'returned wrong spatial reference id'

    gdaltest.hana_ds.ReleaseResultSet(sql_layer)


###############################################################################
# Test returned geometry type

def test_ogr_hana_8():
    if gdaltest.hana_ds is None:
        pytest.skip()

    layer = gdaltest.hana_ds.ExecuteSQL('SELECT * FROM TPOLY')
    assert layer.GetLayerDefn().GetGeomFieldDefn(0).GetType() == ogr.wkbPolygon, \
        'Returned wrong geometry type'

    gdaltest.hana_ds.ReleaseResultSet(layer)


###############################################################################
# Write new features with geometries and verify them

def test_ogr_hana_9():
    if gdaltest.hana_ds is None:
        pytest.skip()

    dst_feat = ogr.Feature(feature_def=gdaltest.hana_layer.GetLayerDefn())
    wkt_list = ['10', '2', '1', '3d_1', '4', '5', '6']

    for item in wkt_list:

        wkt = open('data/wkb_wkt/' + item + '.wkt').read()
        geom = ogr.CreateGeometryFromWkt(wkt)

        ######################################################################
        # Write geometry as a new feature.

        dst_feat.SetGeometryDirectly(geom)
        dst_feat.SetField('PRFEDEA', item)
        dst_feat.SetFID(-1)
        gdaltest.hana_layer.CreateFeature(dst_feat)

        ######################################################################
        # Read back the feature and get the geometry.

        gdaltest.hana_layer.SetAttributeFilter("PRFEDEA = '%s'" % item)

        feat_read = gdaltest.hana_layer.GetNextFeature()

        if ogrtest.check_feature_geometry(feat_read, geom) != 0:
            print(item)
            print(wkt)
            pytest.fail(geom)

        feat_read.Destroy()

    dst_feat.Destroy()
    gdaltest.hana_layer.ResetReading()


###############################################################################
# Test ExecuteSQL() without geometry

def test_ogr_hana_10():
    if gdaltest.hana_ds is None:
        pytest.skip()

    gdaltest.hana_layer.SetAttributeFilter(None)

    layer = gdaltest.hana_ds.ExecuteSQL('SELECT EAS_ID FROM tpoly WHERE EAS_ID IN (158, 170) ')

    assert layer.GetFeatureCount() == 2, \
        'GetFeatureCount() returned %d instead of 2' % layer.GetFeatureCount()

    tr = ogrtest.check_features_against_list(layer, 'EAS_ID', [158, 170])

    gdaltest.hana_ds.ReleaseResultSet(layer)

    assert tr


###############################################################################
# Test ExecuteSQL() results layers without geometry


def test_ogr_hana_11():
    if gdaltest.hana_ds is None:
        pytest.skip()

    layer = gdaltest.hana_ds.ExecuteSQL('SELECT DISTINCT EAS_ID FROM TPOLY ORDER BY EAS_ID DESC')

    assert layer.GetFeatureCount() == 11

    expect = [179, 173, 172, 171, 170, 169, 168, 166, 165, 158, None]
    tr = ogrtest.check_features_against_list(layer, 'EAS_ID', expect)

    gdaltest.hana_ds.ReleaseResultSet(layer)

    assert tr


###############################################################################
# Test ExecuteSQL() results layers with geometry

def test_ogr_hana_12():
    if gdaltest.hana_ds is None:
        pytest.skip()

    wkt_id = '5'
    layer = gdaltest.hana_ds.ExecuteSQL("SELECT * FROM TPOLY WHERE PRFEDEA = '%s'" % wkt_id)
    assert layer.GetLayerDefn().GetGeomFieldCount() == 1, \
        "GetGeomFieldCount() must return 1"

    tr = ogrtest.check_features_against_list(layer, 'PRFEDEA', [wkt_id])
    if tr:
        layer.ResetReading()
        feat_read = layer.GetNextFeature()
        wkt = open('data/wkb_wkt/' + wkt_id + '.wkt').read()
        if ogrtest.check_feature_geometry(feat_read, wkt) != 0:
            tr = 0
        feat_read.Destroy()

    geom = ogr.CreateGeometryFromWkt('LINESTRING(-10 -10,0 0)')
    layer.SetSpatialFilter(geom)

    assert layer.GetFeatureCount() == 0
    assert layer.GetNextFeature() is None, 'GetNextFeature() did not return None'

    gdaltest.hana_ds.ReleaseResultSet(layer)

    assert tr


###############################################################################
# Test ExecuteSQL() with empty result set

def test_ogr_hana_13():
    if gdaltest.hana_ds is None:
        pytest.skip()

    layer = gdaltest.hana_ds.ExecuteSQL('SELECT * FROM TPOLY WHERE EAS_ID = 7892342')
    assert layer is not None, 'Expected a non None layer'

    feat = layer.GetNextFeature()
    assert feat is None, 'Expected no features'

    gdaltest.hana_ds.ReleaseResultSet(layer)


###############################################################################
# Test ExecuteSQL() with quoted table name

def test_ogr_hana_14():
    if gdaltest.hana_ds is None:
        pytest.skip()

    layer = gdaltest.hana_ds.ExecuteSQL('SELECT EAS_ID FROM "TPOLY" WHERE EAS_ID IN (158, 170) ')

    tr = ogrtest.check_features_against_list(layer, 'EAS_ID', [158, 170])

    gdaltest.hana_ds.ReleaseResultSet(layer)

    assert tr


###############################################################################
# Test GetFeature() method with an invalid id

def test_ogr_hana_15():
    if gdaltest.hana_ds is None:
        pytest.skip()

    layer = gdaltest.hana_ds.GetLayerByName('tpoly')
    assert layer.GetFeature(0) is None


###############################################################################
# Test inserting features without geometry

def test_ogr_hana_16():
    if gdaltest.hana_ds is None:
        pytest.skip()

    feat_count = gdaltest.hana_layer.GetFeatureCount()

    dst_feat = ogr.Feature(feature_def=gdaltest.hana_layer.GetLayerDefn())
    dst_feat.SetField('PRFEDEA', '7777')
    dst_feat.SetField('EAS_ID', 2000)
    dst_feat.SetFID(-1)
    gdaltest.hana_layer.CreateFeature(dst_feat)
    dst_feat.Destroy()

    assert (feat_count + 1) == gdaltest.hana_layer.GetFeatureCount(), \
        ('Feature count %d is not as expected %d' % (gdaltest.hana_layer.GetFeatureCount(), feat_count + 1))


###############################################################################
# Test reading features without geometry

def test_ogr_hana_17():
    if gdaltest.hana_ds is None:
        pytest.skip()

    gdaltest.hana_layer.SetAttributeFilter("PRFEDEA = '%s'" % 7777)
    feat_read = gdaltest.hana_layer.GetNextFeature()

    assert feat_read.GetGeometryRef() is None, \
        'NULL geometry is expected'

    gdaltest.hana_layer.SetAttributeFilter(None)


###############################################################################
# Write a feature with too long a text value for a fixed length text field.
# The driver should now truncate this (but with a debug message).  Also,
# put some crazy stuff in the value to verify that quoting and escaping
# is working smoothly.
#
# No geometry in this test.

def test_ogr_hana_18():
    if gdaltest.hana_ds is None:
        pytest.skip()

    dst_feat = ogr.Feature(feature_def=gdaltest.hana_layer.GetLayerDefn())

    dst_feat.SetField('PRFEDEA', 'CrazyKey')
    dst_feat.SetField('SHORTNAME', 'Crazy"\'Long')
    gdaltest.hana_layer.CreateFeature(dst_feat)
    dst_feat.Destroy()

    gdaltest.hana_layer.SetAttributeFilter("PRFEDEA = 'CrazyKey'")
    feat_read = gdaltest.hana_layer.GetNextFeature()

    assert feat_read is not None, 'creating crazy feature failed!'

    assert feat_read.GetField('shortname') == 'Crazy"\'L', \
        ('Vvalue not properly escaped or truncated:' +
         feat_read.GetField('shortname'))

    feat_read.Destroy()


###############################################################################
# Verify inplace update of a feature with SetFeature()

def test_ogr_hana_19():
    if gdaltest.hana_ds is None:
        pytest.skip()

    gdaltest.hana_layer.SetAttributeFilter("PRFEDEA = 'CrazyKey'")
    feat = gdaltest.hana_layer.GetNextFeature()
    gdaltest.hana_layer.SetAttributeFilter(None)

    feat.SetField('SHORTNAME', 'Reset')

    point = ogr.Geometry(ogr.wkbPoint)
    point.SetPoint(0, 5, 6, 7)
    feat.SetGeometryDirectly(point)

    if gdaltest.hana_layer.SetFeature(feat) != 0:
        feat.Destroy()
        pytest.fail('SetFeature() method failed.')

    fid = feat.GetFID()
    feat.Destroy()

    feat = gdaltest.hana_layer.GetFeature(fid)
    assert feat is not None, ('GetFeature(%d) failed.' % fid)

    shortname = feat.GetField('SHORTNAME')
    assert shortname[:5] == 'Reset', ('SetFeature() did not update SHORTNAME, got %s.'
                                      % shortname)

    if ogrtest.check_feature_geometry(feat, 'POINT(5 6 7)') != 0:
        print(feat.GetGeometryRef())
        pytest.fail('Geometry update failed')

    feat.SetGeometryDirectly(None)

    if gdaltest.hana_layer.SetFeature(feat) != 0:
        feat.Destroy()
        pytest.fail('SetFeature() method failed.')

    feat.Destroy()

    feat = gdaltest.hana_layer.GetFeature(fid)
    assert feat.GetGeometryRef() is None, \
        'Geometry update failed. null geometry expected'

    feat.SetFieldNull('SHORTNAME')
    gdaltest.hana_layer.SetFeature(feat)
    feat = gdaltest.hana_layer.GetFeature(fid)
    assert feat.IsFieldNull('SHORTNAME'), 'SHORTNAME update failed. null value expected'

    # Test updating non-existing feature
    feat.SetFID(-10)
    if gdaltest.hana_layer.SetFeature(feat) != ogr.OGRERR_NON_EXISTING_FEATURE:
        feat.Destroy()
        pytest.fail('Expected failure of SetFeature().')

    feat.Destroy()


###############################################################################
# Verify that DeleteFeature() works properly

def test_ogr_hana_20():
    if gdaltest.hana_ds is None:
        pytest.skip()

    gdaltest.hana_layer.SetAttributeFilter("PRFEDEA = 'CrazyKey'")
    feat = gdaltest.hana_layer.GetNextFeature()
    gdaltest.hana_layer.SetAttributeFilter(None)

    fid = feat.GetFID()
    feat.Destroy()

    assert gdaltest.hana_layer.DeleteFeature(fid) == 0, 'DeleteFeature() method failed.'

    gdaltest.hana_layer.SetAttributeFilter("PRFEDEA = 'CrazyKey'")
    feat = gdaltest.hana_layer.GetNextFeature()
    gdaltest.hana_layer.SetAttributeFilter(None)

    if feat is not None:
        feat.Destroy()
        pytest.fail('DeleteFeature() seems to have had no effect.')

    # Test deleting non-existing feature
    assert gdaltest.hana_layer.DeleteFeature(-10) == ogr.OGRERR_NON_EXISTING_FEATURE, \
        'Expected failure of DeleteFeature().'


###############################################################################
# Test default values

def test_ogr_hana_21():
    if gdaltest.hana_ds is None:
        return 'skip'

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    layer = gdaltest.hana_ds.CreateLayer('ogr_hana_21', srs, options=[])

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
    new_feat.Destroy()

    layer.ResetReading()

    layer_defn = layer.GetLayerDefn()

    ds = open_datasource()
    layer_new = ds.GetLayerByName('OGR_HANA_21')
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

    feat.Destroy()


###############################################################################
# Test creating a field with the fid name

def test_ogr_hana_22():
    if gdaltest.hana_ds is None:
        pytest.skip()

    layer = gdaltest.hana_ds.CreateLayer('OGR_HANA_22', geom_type=ogr.wkbNone, options=['FID=fid', 'LAUNDER=NO'])

    gdal.PushErrorHandler()
    assert layer.CreateField(ogr.FieldDefn('str', ogr.OFTString)) == 0
    assert layer.CreateField(ogr.FieldDefn('fid', ogr.OFTString)) != 0
    assert layer.CreateField(ogr.FieldDefn('fid', ogr.OFTInteger)) != 0
    gdal.PopErrorHandler()

    layer.ResetReading()


###############################################################################
# Test very large query

def test_ogr_hana_23():
    if gdaltest.hana_ds is None:
        pytest.skip()

    query = 'eas_id = 169'
    for id in range(1000):
        query = query + (' or eas_id = %d' % (id + 1000))

    gdaltest.hana_layer.SetAttributeFilter(query)
    tr = ogrtest.check_features_against_list(gdaltest.hana_layer, 'eas_id', [169])
    gdaltest.hana_layer.SetAttributeFilter(None)

    assert tr


###############################################################################
# Test COLUMN_TYPES layer creation option

def test_ogr_hana_24():
    if gdaltest.hana_ds is None:
        pytest.skip()

    layer = gdaltest.hana_ds.CreateLayer('OGR_HANA_24',
                                         options=['COLUMN_TYPES=SINT=SMALLINT,DEC1=DECIMAL(10,5),DEC2=DECIMAL(20,0)'])
    layer.CreateField(ogr.FieldDefn('SINT', ogr.OFTString))
    layer.CreateField(ogr.FieldDefn('DEC1', ogr.OFTString))
    layer.CreateField(ogr.FieldDefn('DEC2', ogr.OFTString))

    ds = open_datasource()
    layer = ds.GetLayerByName('OGR_HANA_24')
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

def test_ogr_hana_25():
    if gdaltest.hana_ds is None:
        pytest.skip()

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    conn_str = gdaltest.hana_connection_string + ';SCHEMA=' + gdaltest.hana_schema_name
    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' "' + 'HANA:' + conn_str + '" TPOLY')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1


###############################################################################
# Run test_ogrsf with -sql

def test_ogr_hana_26():
    if gdaltest.hana_ds is None:
        pytest.skip()

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    conn_str = gdaltest.hana_connection_string + ';SCHEMA=' + gdaltest.hana_schema_name
    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' "' + 'HANA:' + conn_str +
                               '" -sql "SELECT * FROM TPOLY"')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1


###############################################################################
# Test retrieving an error from ExecuteSQL()

def test_ogr_hana_27():
    if gdaltest.hana_ds is None:
        pytest.skip()

    gdal.ErrorReset()
    with gdaltest.error_handler():
        layer = gdaltest.hana_ds.ExecuteSQL('SELECT FROM')
    assert gdal.GetLastErrorMsg() != ''
    assert layer is None


###############################################################################
# Cleanup

def test_ogr_hana_cleanup():
    if gdaltest.hana_ds is None:
        return 'skip'

    # Cleanup created tables
    gdal.PushErrorHandler('CPLQuietErrorHandler')

    gdaltest.hana_ds.ExecuteSQL('DELLAYER:tpoly')
    gdaltest.hana_ds.ExecuteSQL('DELLAYER:ogr_hana_21')
    gdaltest.hana_ds.ExecuteSQL('DELLAYER:ogr_hana_22')
    gdaltest.hana_ds.ExecuteSQL('DELLAYER:ogr_hana_24')

    gdal.PopErrorHandler()

    gdaltest.hana_ds.Destroy()
    gdaltest.hana_ds = None

    conn = create_connection(gdaltest.hana_connection_string)
    execute_sql(conn, f'DROP SCHEMA "{gdaltest.hana_schema_name}" CASCADE')

    gdaltest.hana_connection_string = None
    gdaltest.hana_schema_name = None


###############################################################################
# Helper methods

def create_connection(conn_str):
    conn_params = dict(item.split("=") for item in conn_str.split(";"))
    conn = dbapi.connect(address=conn_params['HOST'], port=conn_params['PORT'], user=conn_params['USER'],
                         password=conn_params['PASSWORD'], ENCRYPT=True, sslValidateCertificate=False, CHAR_AS_UTF8=1)
    conn.setautocommit(False)
    return conn


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


def generate_schema_name(conn, prefix):
    uid = execute_sql_scalar(conn, "SELECT REPLACE(CURRENT_UTCDATE, '-', '') || '_' || BINTOHEX(SYSUUID) FROM DUMMY;")
    return '{}_{}'.format(prefix, uid)


def open_datasource(update=1):
    return ogr.Open('HANA:' + gdaltest.hana_connection_string + ';SCHEMA=' + gdaltest.hana_schema_name, update=update)


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

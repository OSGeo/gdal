#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test MySQL driver functionality.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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



import gdaltest
import ogrtest
from osgeo import ogr
from osgeo import gdal
from osgeo import osr
import pytest

# E. Rouault : this is almost a copy & paste from ogr_pg.py

#
# To create the required MySQL instance do something like:
#
#  $ mysql -u root -p
#     mysql> CREATE DATABASE autotest;
#     mysql> GRANT ALL ON autotest.* TO 'THE_USER_THAT_RUNS_AUTOTEST'@'localhost';
#

###############################################################################
# Open Database.


def test_ogr_mysql_1():

    gdaltest.mysql_ds = None

    try:
        ogr.GetDriverByName('MySQL')
    except:
        pytest.skip()

    val = gdal.GetConfigOption('OGR_MYSQL_CONNECTION_STRING', None)
    if val is not None:
        gdaltest.mysql_connection_string = val
    else:
        gdaltest.mysql_connection_string = 'MYSQL:autotest'

    gdaltest.mysql_ds = ogr.Open(gdaltest.mysql_connection_string, update=1)
    if gdaltest.mysql_ds is None:
        pytest.skip()

    sql_lyr = gdaltest.mysql_ds.ExecuteSQL("SELECT VERSION()")
    f = sql_lyr.GetNextFeature()
    print('Version: ' + f.GetField(0))
    gdaltest.is_mysql_8_or_later = int(f.GetField(0).split('.')[0]) >= 8 and f.GetField(0).find('MariaDB') < 0
    gdaltest.mysql_ds.ReleaseResultSet(sql_lyr)

###############################################################################
# Create table from data/poly.shp


def test_ogr_mysql_2():

    if gdaltest.mysql_ds is None:
        pytest.skip()

    shp_ds = ogr.Open('data/poly.shp')
    gdaltest.shp_ds = shp_ds
    shp_lyr = shp_ds.GetLayer(0)

    ######################################################
    # Create Layer
    gdaltest.mysql_lyr = gdaltest.mysql_ds.CreateLayer('tpoly', srs=shp_lyr.GetSpatialRef(),
                                                       options=[])

    ######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def(gdaltest.mysql_lyr,
                                   [('AREA', ogr.OFTReal),
                                    ('EAS_ID', ogr.OFTInteger),
                                    ('PRFEDEA', ogr.OFTString),
                                    ('SHORTNAME', ogr.OFTString, 8),
                                    ('INT64', ogr.OFTInteger64)])

    ######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature(feature_def=gdaltest.mysql_lyr.GetLayerDefn())

    feat = shp_lyr.GetNextFeature()
    gdaltest.poly_feat = []

    while feat is not None:

        gdaltest.poly_feat.append(feat)

        dst_feat.SetFrom(feat)
        dst_feat.SetField('INT64', 1234567890123)
        gdaltest.mysql_lyr.CreateFeature(dst_feat)

        feat = shp_lyr.GetNextFeature()

    dst_feat.Destroy()

    assert gdaltest.mysql_lyr.GetFeatureCount() == shp_lyr.GetFeatureCount(), \
        'not matching feature count'

    assert gdaltest.mysql_lyr.GetSpatialRef().GetAuthorityCode(None) == shp_lyr.GetSpatialRef().GetAuthorityCode(None), \
        'not matching spatial ref'

###############################################################################
# Test reading a layer extent


def test_ogr_mysql_19():

    if gdaltest.mysql_ds is None:
        pytest.skip()

    layer = gdaltest.mysql_ds.GetLayerByName('tpoly')
    if layer is None:
        pytest.fail('did not get tpoly layer')

    extent = layer.GetExtent()
    expect = (478315.53125, 481645.3125, 4762880.5, 4765610.5)

    minx = abs(extent[0] - expect[0])
    maxx = abs(extent[1] - expect[1])
    miny = abs(extent[2] - expect[2])
    maxy = abs(extent[3] - expect[3])

    if max(minx, maxx, miny, maxy) > 0.0001:
        print(extent)
        pytest.fail('Extents do not match')

###############################################################################
# Verify that stuff we just wrote is still OK.


def test_ogr_mysql_3():
    if gdaltest.mysql_ds is None:
        pytest.skip()

    assert gdaltest.mysql_lyr.GetGeometryColumn() == 'SHAPE'

    assert gdaltest.mysql_lyr.GetFeatureCount() == 10

    expect = [168, 169, 166, 158, 165]

    gdaltest.mysql_lyr.SetAttributeFilter('eas_id < 170')
    tr = ogrtest.check_features_against_list(gdaltest.mysql_lyr,
                                             'eas_id', expect)

    assert gdaltest.mysql_lyr.GetFeatureCount() == 5

    gdaltest.mysql_lyr.SetAttributeFilter(None)

    for i in range(len(gdaltest.poly_feat)):
        orig_feat = gdaltest.poly_feat[i]
        read_feat = gdaltest.mysql_lyr.GetNextFeature()

        assert (ogrtest.check_feature_geometry(read_feat, orig_feat.GetGeometryRef(),
                                          max_error=0.001) == 0)

        for fld in range(3):
            assert orig_feat.GetField(fld) == read_feat.GetField(fld), \
                ('Attribute %d does not match' % fld)
        assert read_feat.GetField('INT64') == 1234567890123

        read_feat.Destroy()
        orig_feat.Destroy()

    gdaltest.poly_feat = None
    gdaltest.shp_ds.Destroy()

    assert tr

###############################################################################
# Write more features with a bunch of different geometries, and verify the
# geometries are still OK.


def test_ogr_mysql_4():

    if gdaltest.mysql_ds is None:
        pytest.skip()

    # E. Rouault : the mySQL driver doesn't seem to like adding new features and
    # iterating over a query at the same time.
    # If trying to do so, we get the 'Commands out of sync' error.

    wkt_list = ['10', '2', '1', '4', '5', '6']

    gdaltest.mysql_lyr.ResetReading()

    feature_def = gdaltest.mysql_lyr.GetLayerDefn()

    for item in wkt_list:
        dst_feat = ogr.Feature(feature_def)

        wkt = open('data/wkb_wkt/' + item + '.wkt').read()
        geom = ogr.CreateGeometryFromWkt(wkt)

        ######################################################################
        # Write geometry as a new Oracle feature.

        dst_feat.SetGeometryDirectly(geom)
        dst_feat.SetField('PRFEDEA', item)
        gdaltest.mysql_lyr.CreateFeature(dst_feat)

        dst_feat.Destroy()

    # FIXME : The source wkt polygons of '4' and '6' are not closed and
    # mySQL return them as closed, so the check_feature_geometry returns FALSE
    # Checking them after closing the rings again returns TRUE.

    wkt_list = ['10', '2', '1', '5', '4', '6']

    for item in wkt_list:
        wkt = open('data/wkb_wkt/' + item + '.wkt').read()
        geom = ogr.CreateGeometryFromWkt(wkt)

        ######################################################################
        # Read back the feature and get the geometry.

        gdaltest.mysql_lyr.SetAttributeFilter("PRFEDEA = '%s'" % item)
        feat_read = gdaltest.mysql_lyr.GetNextFeature()

        if ogrtest.check_feature_geometry(feat_read, geom) != 0:
            print('Geometry changed. Closing rings before trying again for wkt #', item)
            print('(before):', geom.ExportToWkt())
            geom.CloseRings()
            print('(after) :', geom.ExportToWkt())
            assert ogrtest.check_feature_geometry(feat_read, geom) == 0

        feat_read.Destroy()

    
###############################################################################
# Test ExecuteSQL() results layers without geometry.


def test_ogr_mysql_5():

    if gdaltest.mysql_ds is None:
        pytest.skip()

    # E. Rouault : unlike PostgreSQL driver : None is sorted in last position
    expect = [179, 173, 172, 171, 170, 169, 168, 166, 165, 158, None]

    sql_lyr = gdaltest.mysql_ds.ExecuteSQL('select distinct eas_id from tpoly order by eas_id desc')

    assert sql_lyr.GetFeatureCount() == 11

    tr = ogrtest.check_features_against_list(sql_lyr, 'eas_id', expect)

    gdaltest.mysql_ds.ReleaseResultSet(sql_lyr)

    assert tr

###############################################################################
# Test ExecuteSQL() results layers with geometry.


def test_ogr_mysql_6():

    if gdaltest.mysql_ds is None:
        pytest.skip()

    sql_lyr = gdaltest.mysql_ds.ExecuteSQL("select * from tpoly where prfedea = '2'")

    tr = ogrtest.check_features_against_list(sql_lyr, 'prfedea', ['2'])
    if tr:
        sql_lyr.ResetReading()
        feat_read = sql_lyr.GetNextFeature()
        if ogrtest.check_feature_geometry(feat_read, 'MULTILINESTRING ((5.00121349 2.99853132,5.00121349 1.99853133),(5.00121349 1.99853133,5.00121349 0.99853133),(3.00121351 1.99853127,5.00121349 1.99853133),(5.00121349 1.99853133,6.00121348 1.99853135))') != 0:
            tr = 0
        feat_read.Destroy()
    sql_lyr.ResetReading()

    geom = ogr.CreateGeometryFromWkt(
        'LINESTRING(-10 -10,0 0)')
    sql_lyr.SetSpatialFilter(geom)
    geom.Destroy()

    assert sql_lyr.GetFeatureCount() == 0

    assert sql_lyr.GetNextFeature() is None, 'GetNextFeature() did not return None'

    gdaltest.mysql_ds.ReleaseResultSet(sql_lyr)

    assert tr

###############################################################################
# Test spatial filtering.


def test_ogr_mysql_7():

    if gdaltest.mysql_ds is None:
        pytest.skip()

    gdaltest.mysql_lyr.SetAttributeFilter(None)

    geom = ogr.CreateGeometryFromWkt(
        'LINESTRING(479505 4763195,480526 4762819)')
    gdaltest.mysql_lyr.SetSpatialFilter(geom)
    geom.Destroy()

    assert gdaltest.mysql_lyr.GetFeatureCount() == 1

    tr = ogrtest.check_features_against_list(gdaltest.mysql_lyr, 'eas_id',
                                             [158])

    gdaltest.mysql_lyr.SetAttributeFilter('eas_id = 158')

    assert gdaltest.mysql_lyr.GetFeatureCount() == 1

    gdaltest.mysql_lyr.SetAttributeFilter(None)

    gdaltest.mysql_lyr.SetSpatialFilter(None)

    assert tr

###############################################################################
# Write a feature with too long a text value for a fixed length text field.
# The driver should now truncate this (but with a debug message).  Also,
# put some crazy stuff in the value to verify that quoting and escaping
# is working smoothly.
#
# No geometry in this test.


def test_ogr_mysql_8():

    if gdaltest.mysql_ds is None:
        pytest.skip()

    dst_feat = ogr.Feature(feature_def=gdaltest.mysql_lyr.GetLayerDefn())

    dst_feat.SetField('PRFEDEA', 'CrazyKey')
    dst_feat.SetField('SHORTNAME', 'Crazy"\'Long')
    # We are obliged to create a fake geometry
    dst_feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT(0 0)'))
    gdaltest.mysql_lyr.CreateFeature(dst_feat)
    dst_feat.Destroy()

    gdaltest.mysql_lyr.SetAttributeFilter("PRFEDEA = 'CrazyKey'")
    feat_read = gdaltest.mysql_lyr.GetNextFeature()

    assert feat_read is not None, 'creating crazy feature failed!'

    assert feat_read.GetField('shortname') == 'Crazy"\'L', \
        ('Vvalue not properly escaped or truncated:' +
                             feat_read.GetField('shortname'))

    feat_read.Destroy()

###############################################################################
# Verify inplace update of a feature with SetFeature().


def test_ogr_mysql_9():

    if gdaltest.mysql_ds is None:
        pytest.skip()

    gdaltest.mysql_lyr.SetAttributeFilter("PRFEDEA = 'CrazyKey'")
    feat = gdaltest.mysql_lyr.GetNextFeature()
    gdaltest.mysql_lyr.SetAttributeFilter(None)

    feat.SetField('SHORTNAME', 'Reset')

    point = ogr.Geometry(ogr.wkbPoint25D)
    point.SetPoint(0, 5, 6)
    feat.SetGeometryDirectly(point)

    if gdaltest.mysql_lyr.SetFeature(feat) != 0:
        feat.Destroy()
        pytest.fail('SetFeature() method failed.')

    fid = feat.GetFID()
    feat.Destroy()

    feat = gdaltest.mysql_lyr.GetFeature(fid)
    assert feat is not None, ('GetFeature(%d) failed.' % fid)

    shortname = feat.GetField('SHORTNAME')
    assert shortname[:5] == 'Reset', ('SetFeature() did not update SHORTNAME, got %s.'
                             % shortname)

    if ogrtest.check_feature_geometry(feat, 'POINT(5 6)') != 0:
        print(feat.GetGeometryRef())
        pytest.fail('Geometry update failed')

    # Test updating non-existing feature
    feat.SetFID(-10)
    if gdaltest.mysql_lyr.SetFeature(feat) != ogr.OGRERR_NON_EXISTING_FEATURE:
        feat.Destroy()
        pytest.fail('Expected failure of SetFeature().')

    # Test deleting non-existing feature
    if gdaltest.mysql_lyr.DeleteFeature(-10) != ogr.OGRERR_NON_EXISTING_FEATURE:
        feat.Destroy()
        pytest.fail('Expected failure of DeleteFeature().')

    feat.Destroy()

###############################################################################
# Verify that DeleteFeature() works properly.


def test_ogr_mysql_10():

    if gdaltest.mysql_ds is None:
        pytest.skip()

    gdaltest.mysql_lyr.SetAttributeFilter("PRFEDEA = 'CrazyKey'")
    feat = gdaltest.mysql_lyr.GetNextFeature()
    gdaltest.mysql_lyr.SetAttributeFilter(None)

    fid = feat.GetFID()
    feat.Destroy()

    assert gdaltest.mysql_lyr.DeleteFeature(fid) == 0, 'DeleteFeature() method failed.'

    gdaltest.mysql_lyr.SetAttributeFilter("PRFEDEA = 'CrazyKey'")
    feat = gdaltest.mysql_lyr.GetNextFeature()
    gdaltest.mysql_lyr.SetAttributeFilter(None)

    if feat is None:
        return

    feat.Destroy()
    pytest.fail('DeleteFeature() seems to have had no effect.')


###############################################################################
# Test very large query.

def test_ogr_mysql_15():

    if gdaltest.mysql_ds is None:
        pytest.skip()

    expect = [169]

    query = 'eas_id = 169'

    for i in range(1000):
        query = query + (' or eas_id = %d' % (i + 1000))

    gdaltest.mysql_lyr.SetAttributeFilter(query)
    tr = ogrtest.check_features_against_list(gdaltest.mysql_lyr,
                                             'eas_id', expect)
    gdaltest.mysql_lyr.SetAttributeFilter(None)

    assert tr


###############################################################################
# Test very large statement.

def test_ogr_mysql_16():

    if gdaltest.mysql_ds is None:
        pytest.skip()

    expect = [169]

    query = 'eas_id = 169'

    for ident in range(1000):
        query = query + (' or eas_id = %d' % (ident + 1000))

    statement = 'select eas_id from tpoly where ' + query

    lyr = gdaltest.mysql_ds.ExecuteSQL(statement)

    tr = ogrtest.check_features_against_list(lyr, 'eas_id', expect)

    gdaltest.mysql_ds.ReleaseResultSet(lyr)

    assert tr

###############################################################################
# Test requesting a non-existent table by name (bug 1480).


def test_ogr_mysql_17():

    if gdaltest.mysql_ds is None:
        pytest.skip()

    count = gdaltest.mysql_ds.GetLayerCount()
    layer = gdaltest.mysql_ds.GetLayerByName('JunkTableName')
    assert layer is None, 'got layer for non-existent table!'

    assert count == gdaltest.mysql_ds.GetLayerCount(), \
        'layer count changed unexpectedly.'

###############################################################################
# Test getting a layer by name that was not previously a layer.


def ogr_mysql_18():

    if gdaltest.mysql_ds is None:
        pytest.skip()

    count = gdaltest.mysql_ds.GetLayerCount()
    layer = gdaltest.mysql_ds.GetLayerByName('geometry_columns')
    assert layer is not None, 'did not get geometry_columns layer'

    assert count + 1 == gdaltest.mysql_ds.GetLayerCount(), \
        'layer count unexpectedly unchanged.'

###############################################################################


def test_ogr_mysql_20():

    if gdaltest.mysql_ds is None:
        pytest.skip()

    layer = gdaltest.mysql_ds.CreateLayer('select', options=[])
    ogrtest.quick_create_layer_def(layer,
                                   [('desc', ogr.OFTString),
                                    ('select', ogr.OFTString)])
    dst_feat = ogr.Feature(feature_def=layer.GetLayerDefn())

    dst_feat.SetField('desc', 'desc')
    dst_feat.SetField('select', 'select')
    # We are obliged to create a fake geometry
    dst_feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT(0 1)'))
    layer.CreateFeature(dst_feat)
    dst_feat.Destroy()

    layer = gdaltest.mysql_ds.GetLayerByName('select')
    layer.ResetReading()
    feat = layer.GetNextFeature()
    if feat.desc == 'desc' and feat.select == 'select':
        return
    pytest.fail()

###############################################################################
# Test inserting NULL geometries into a table with a spatial index -> must FAIL


def test_ogr_mysql_21():

    if gdaltest.mysql_ds is None:
        pytest.skip()

    layer = gdaltest.mysql_ds.CreateLayer('tablewithspatialindex', geom_type=ogr.wkbPoint, options=[])
    ogrtest.quick_create_layer_def(layer, [('name', ogr.OFTString)])
    dst_feat = ogr.Feature(feature_def=layer.GetLayerDefn())
    dst_feat.SetField('name', 'name')

    # The insertion MUST fail
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    layer.CreateFeature(dst_feat)
    gdal.PopErrorHandler()

    dst_feat.Destroy()

    layer.ResetReading()
    feat = layer.GetNextFeature()
    assert feat is None

###############################################################################
# Test inserting NULL geometries into a table without a spatial index


def test_ogr_mysql_22():

    if gdaltest.mysql_ds is None:
        pytest.skip()

    layer = gdaltest.mysql_ds.CreateLayer('tablewithoutspatialindex', geom_type=ogr.wkbPoint,
                                          options=['SPATIAL_INDEX=NO'])
    ogrtest.quick_create_layer_def(layer, [('name', ogr.OFTString)])
    dst_feat = ogr.Feature(feature_def=layer.GetLayerDefn())
    dst_feat.SetField('name', 'name')

    layer.CreateFeature(dst_feat)

    dst_feat.Destroy()

    layer.ResetReading()
    feat = layer.GetNextFeature()
    assert feat is not None

###############################################################################
# Check for right precision


def test_ogr_mysql_23():

    if gdaltest.mysql_ds is None:
        pytest.skip()

    fields = ('zero', 'widthonly', 'onedecimal', 'twentynine', 'thirtyone')
    values = (1, 2, 1.1, 0.12345678901234567890123456789, 0.1234567890123456789012345678901)
    precision = (0, 0, 1, 29, 0)

    ######################################################
    # Create a layer with a single feature through SQL

    if gdaltest.is_mysql_8_or_later:
        gdaltest.mysql_lyr = gdaltest.mysql_ds.ExecuteSQL("SELECT ROUND(1.1,0) AS zero, ROUND(2.0, 0) AS widthonly, ROUND(1.1,1) AS onedecimal, ROUND(0.12345678901234567890123456789,29) AS twentynine, ST_GeomFromText(CONVERT('POINT(1.0 2.0)',CHAR)) as the_geom;")
    else:
        gdaltest.mysql_lyr = gdaltest.mysql_ds.ExecuteSQL("SELECT ROUND(1.1,0) AS zero, ROUND(2.0, 0) AS widthonly, ROUND(1.1,1) AS onedecimal, ROUND(0.12345678901234567890123456789,29) AS twentynine, GeomFromText(CONVERT('POINT(1.0 2.0)',CHAR)) as the_geom;")

    feat = gdaltest.mysql_lyr.GetNextFeature()
    assert feat is not None

    ######################################################
    # Check the values and the precisions
    for i in range(4):
        assert feat.GetFieldIndex(fields[i]) >= 0, 'field not found'
        assert feat.GetField(feat.GetFieldIndex(fields[i])) == values[i], \
            'value not right'
        assert feat.GetFieldDefnRef(feat.GetFieldIndex(fields[i])).GetPrecision() == precision[i], \
            'precision not right'

    gdaltest.mysql_ds.ReleaseResultSet(gdaltest.mysql_lyr)
    gdaltest.mysql_lyr = None

###############################################################################
# Run test_ogrsf


def test_ogr_mysql_24():

    if gdaltest.mysql_ds is None:
        pytest.skip()

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + " '" + gdaltest.mysql_connection_string + "' tpoly")

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

###############################################################################
# Test 64 bit FID


def test_ogr_mysql_72():

    if gdaltest.mysql_ds is None:
        pytest.skip()

    # Regular layer with 32 bit IDs
    lyr = gdaltest.mysql_ds.CreateLayer('ogr_mysql_72', geom_type=ogr.wkbNone)
    assert lyr.GetMetadataItem(ogr.OLMD_FID64) is None
    lyr.CreateField(ogr.FieldDefn('foo'))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(123456789012345)
    f.SetField(0, 'bar')
    assert lyr.CreateFeature(f) == 0
    f = lyr.GetFeature(123456789012345)
    assert f is not None

    lyr = gdaltest.mysql_ds.CreateLayer('ogr_mysql_72', geom_type=ogr.wkbNone, options=['FID64=YES', 'OVERWRITE=YES'])
    assert lyr.GetMetadataItem(ogr.OLMD_FID64) is not None
    lyr.CreateField(ogr.FieldDefn('foo'))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(123456789012345)
    f.SetField(0, 'bar')
    assert lyr.CreateFeature(f) == 0
    assert lyr.SetFeature(f) == 0

    gdaltest.mysql_ds = None
    # Test with normal protocol
    gdaltest.mysql_ds = ogr.Open(gdaltest.mysql_connection_string, update=1)
    lyr = gdaltest.mysql_ds.GetLayerByName('ogr_mysql_72')
    assert lyr.GetMetadataItem(ogr.OLMD_FID64) is not None
    f = lyr.GetNextFeature()
    if f.GetFID() != 123456789012345:
        f.DumpReadable()
        pytest.fail()

    
###############################################################################
# Test nullable


def test_ogr_mysql_25():

    if gdaltest.mysql_ds is None:
        pytest.skip()

    lyr = gdaltest.mysql_ds.CreateLayer('ogr_mysql_25', geom_type=ogr.wkbPoint, options=[])
    field_defn = ogr.FieldDefn('field_not_nullable', ogr.OFTString)
    field_defn.SetNullable(0)
    lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn('field_nullable', ogr.OFTString)
    lyr.CreateField(field_defn)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('field_not_nullable', 'not_null')
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT(0 0)'))
    lyr.CreateFeature(f)
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
    if False:  # pylint: disable=using-constant-test
        # hum mysql seems OK with unset non-nullable fields ??
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT(0 0)'))
        gdal.PushErrorHandler()
        ret = lyr.CreateFeature(f)
        gdal.PopErrorHandler()
        assert ret != 0
        f = None

    gdaltest.mysql_ds = None
    gdaltest.mysql_ds = ogr.Open(gdaltest.mysql_connection_string, update=1)
    lyr = gdaltest.mysql_ds.GetLayerByName('ogr_mysql_25')
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_not_nullable')).IsNullable() == 0
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_nullable')).IsNullable() == 1
    assert lyr.GetLayerDefn().GetGeomFieldDefn(0).IsNullable() == 0

###############################################################################
# Test default values


def test_ogr_mysql_26():

    if gdaltest.mysql_ds is None:
        pytest.skip()

    lyr = gdaltest.mysql_ds.CreateLayer('ogr_mysql_26', geom_type=ogr.wkbPoint, options=[])

    field_defn = ogr.FieldDefn('field_string', ogr.OFTString)
    field_defn.SetDefault("'a''b'")
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn('field_string_null', ogr.OFTString)
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

    # field_defn = ogr.FieldDefn( 'field_date', ogr.OFTDate )
    # field_defn.SetDefault("CURRENT_DATE")
    # lyr.CreateField(field_defn)

    # field_defn = ogr.FieldDefn( 'field_time', ogr.OFTTime )
    # field_defn.SetDefault("CURRENT_TIME")
    # lyr.CreateField(field_defn)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFieldNull('field_string_null')
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT(0 0)'))
    lyr.CreateFeature(f)
    f = None

    gdaltest.mysql_ds = None
    gdaltest.mysql_ds = ogr.Open(gdaltest.mysql_connection_string, update=1)
    lyr = gdaltest.mysql_ds.GetLayerByName('ogr_mysql_26')
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_string')).GetDefault() == "'a''b'"
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_int')).GetDefault() == '123'
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_real')).GetDefault() == '1.23'
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_nodefault')).GetDefault() is None
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_datetime')).GetDefault() == 'CURRENT_TIMESTAMP'
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_datetime2')).GetDefault() == "'2015/06/30 12:34:56'"
    # if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_date')).GetDefault() != "CURRENT_DATE":
    #    gdaltest.post_reason('fail')
    #    print(lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_date')).GetDefault())
    #    return 'fail'
    # if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_time')).GetDefault() != "CURRENT_TIME":
    #    gdaltest.post_reason('fail')
    #    return 'fail'
    f = lyr.GetNextFeature()
    if f.GetField('field_string') != 'a\'b' or f.GetField('field_int') != 123 or \
       f.GetField('field_real') != 1.23 or \
       not f.IsFieldNull('field_string_null') or \
       not f.IsFieldNull('field_nodefault') or not f.IsFieldSet('field_datetime') or \
       f.GetField('field_datetime2') != '2015/06/30 12:34:56':
        f.DumpReadable()
        pytest.fail()

    gdal.Unlink('/vsimem/ogr_gpkg_24.gpkg')

###############################################################################
# Test created table indecs

def test_ogr_mysql_27():

    if gdaltest.mysql_ds is None:
        pytest.skip()

    if not gdaltest.is_mysql_8_or_later:
        pytest.skip()

    layer = gdaltest.mysql_ds.GetLayerByName('tpoly')
    if layer is None:
        pytest.skip('did not get tpoly layer')

    sql_lyr = gdaltest.mysql_ds.ExecuteSQL('SHOW CREATE TABLE tpoly')
    f = sql_lyr.GetNextFeature()
    field = f.GetField(1)
    res = False
    for line in field.splitlines():
        if 'geometry' in line:
            if "SRID" in line:
                res = True
            else:
                res = False
    if not res:
        print('{}'.format(field))
        pytest.fail("Not found SRID definition with GEOMETORY field.")
    gdaltest.mysql_ds.ReleaseResultSet(sql_lyr)

###############################################################################
#


def test_ogr_mysql_longlat():

    if gdaltest.mysql_ds is None:
        pytest.skip()

    srs = osr.SpatialReference()
    srs.SetFromUserInput('EPSG:4326')
    lyr = gdaltest.mysql_ds.CreateLayer('ogr_mysql_longlat',
                                        geom_type=ogr.wkbPoint,
                                        srs=srs,
                                        options=[])
    f = ogr.Feature(lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt('POINT(150 2)')
    f.SetGeometry(geom)
    lyr.CreateFeature(f)

    lyr.SetSpatialFilterRect(149.5, 1.5, 150.5, 2.5)
    f = lyr.GetNextFeature()
    assert ogrtest.check_feature_geometry(f, geom) == 0

    extent = lyr.GetExtent()
    expect = (150.0, 150.0, 2.0, 2.0)

    minx = abs(extent[0] - expect[0])
    maxx = abs(extent[1] - expect[1])
    miny = abs(extent[2] - expect[2])
    maxy = abs(extent[3] - expect[3])

    if max(minx, maxx, miny, maxy) > 0.0001:
        print(extent)
        pytest.fail('Extents do not match')

    if gdaltest.is_mysql_8_or_later:
        sql_lyr = gdaltest.mysql_ds.ExecuteSQL('SHOW CREATE TABLE ogr_mysql_longlat')
        f = sql_lyr.GetNextFeature()
        field = f.GetField(1)
        res = False
        for line in field.splitlines():
            if 'geometry' in line:
                if "SRID" in line:
                    res = True
                else:
                    res = False
        if not res:
            print('{}'.format(field))
            pytest.fail("Not found SRID definition with GEOMETORY field.")
        gdaltest.mysql_ds.ReleaseResultSet(sql_lyr)

###############################################################################
# Test writing and reading back geometries

@pytest.mark.xfail(reason="MariaDB has a known issue MDEV-21401")
def test_ogr_mysql_28():

    if gdaltest.mysql_ds is None:
        pytest.skip()

    wkts = ogrtest.get_wkt_data_series(with_z=True, with_m=True, with_gc=True, with_circular=True, with_surface=False)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)

    for i, wkt in enumerate(wkts):
        gdaltest.num_mysql_28 = i + 1
        geom = ogr.CreateGeometryFromWkt(wkt)
        lyr = gdaltest.mysql_ds.CreateLayer('ogr_mysql_28_%d' % i, geom_type=geom.GetGeometryType(), srs=srs)
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(geom)
        lyr.CreateFeature(f)
        f = None
        #
        layer = gdaltest.mysql_ds.GetLayerByName('ogr_mysql_28_%d' % i)
        if layer is None:
            pytest.fail('did not get ogr_mysql_28_%d layer' % i)
        feat = layer.GetNextFeature()
        assert feat is not None
        feat = None


@pytest.mark.xfail(reason='MySQL does not support POLYHEDRALSURFACE.')
def test_ogr_mysql_29():

    if gdaltest.mysql_ds is None:
        pytest.skip()

    wkts = ogrtest.get_wkt_data_series(with_z=False, with_m=False, with_gc=False, with_circular=False, with_surface=True)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)

    for i, wkt in enumerate(wkts):
        gdaltest.num_mysql_29 = i + 1
        geom = ogr.CreateGeometryFromWkt(wkt)
        lyr = gdaltest.mysql_ds.CreateLayer('ogr_mysql_29_%d' % i, geom_type=geom.GetGeometryType(), srs=srs)
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(geom)
        lyr.CreateFeature(f)
        f = None
        #
        layer = gdaltest.mysql_ds.GetLayerByName('ogr_mysql_29_%d' % i)
        if layer is None:
            pytest.fail('did not get ogr_mysql_29_%d layer' % i)
        feat = layer.GetNextFeature()
        assert feat is not None
        feat = None


###############################################################################
#

def test_ogr_mysql_cleanup():

    if gdaltest.mysql_ds is None:
        pytest.skip()

    gdaltest.mysql_ds.ExecuteSQL('DROP TABLE tpoly')
    gdaltest.mysql_ds.ExecuteSQL('DROP TABLE `select`')
    gdaltest.mysql_ds.ExecuteSQL('DROP TABLE tablewithspatialindex')
    with gdaltest.error_handler():
        gdaltest.mysql_ds.ExecuteSQL('DROP TABLE tablewithoutspatialindex')
    gdaltest.mysql_ds.ExecuteSQL('DROP TABLE geometry_columns')
    if not gdaltest.is_mysql_8_or_later:
        gdaltest.mysql_ds.ExecuteSQL('DROP TABLE spatial_ref_sys')
    gdaltest.mysql_ds.ExecuteSQL('DROP TABLE ogr_mysql_72')
    gdaltest.mysql_ds.ExecuteSQL('DROP TABLE ogr_mysql_25')
    gdaltest.mysql_ds.ExecuteSQL('DROP TABLE ogr_mysql_26')
    gdaltest.mysql_ds.ExecuteSQL('DROP TABLE ogr_mysql_longlat')
    for i in range(gdaltest.num_mysql_28):
        gdaltest.mysql_ds.ExecuteSQL('DROP TABLE ogr_mysql_28_%d' % i)
    for i in range(gdaltest.num_mysql_29):
        gdaltest.mysql_ds.ExecuteSQL('DROP TABLE ogr_mysql_29_%d' % i)
    gdaltest.mysql_ds.Destroy()
    gdaltest.mysql_ds = None




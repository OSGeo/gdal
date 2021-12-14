#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test SQLite driver functionality.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2014, Even Rouault <even dot rouault at spatialys.com>
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
import pytest

from osgeo import gdal
from osgeo import ogr
from osgeo import osr
import gdaltest
import ogrtest


pytestmark = [
    pytest.mark.require_driver('SQLite'),
]


###############################################################################
# Test if SpatiaLite is available

@pytest.fixture(autouse=True, scope='module')
def setup():
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogr.GetDriverByName('SQLite').CreateDataSource('/vsimem/foo.db', options=['SPATIALITE=YES'])
    gdaltest.spatialite_version = None
    if ds is not None:
        sql_lyr = ds.ExecuteSQL("SELECT spatialite_version()")
        feat = sql_lyr.GetNextFeature()
        gdaltest.spatialite_version = feat.GetFieldAsString(0)
        print('Spatialite : %s' % gdaltest.spatialite_version)
        ds.ReleaseResultSet(sql_lyr)
    ds = None
    gdal.Unlink('/vsimem/foo.db')
    gdal.PopErrorHandler()


@pytest.fixture()
def require_spatialite(setup):
    if gdaltest.spatialite_version is None:
        pytest.skip('Spatialite not available')
    return gdaltest.spatialite_version


@pytest.fixture(params=['no-spatialite', 'spatialite'])
def with_and_without_spatialite(request):
    if request.param == 'spatialite':
        return gdaltest.spatialite_version
    else:
        return None


###############################################################################
# Create a fresh database.


def test_ogr_sqlite_1():
    gdaltest.sl_ds = None

    sqlite_dr = ogr.GetDriverByName('SQLite')
    if sqlite_dr is None:
        pytest.skip()

    try:
        os.remove('tmp/sqlite_test.db')
    except OSError:
        pass

    # This is to speed-up the runtime of tests on EXT4 filesystems
    # Do not use this for production environment if you care about data safety
    # w.r.t system/OS crashes, unless you know what you are doing.
    gdal.SetConfigOption('OGR_SQLITE_SYNCHRONOUS', 'OFF')

    gdaltest.sl_ds = sqlite_dr.CreateDataSource('tmp/sqlite_test.db')

    assert gdaltest.sl_ds is not None

###############################################################################
# Create table from data/poly.shp


def test_ogr_sqlite_2():

    if gdaltest.sl_ds is None:
        pytest.skip()

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    gdaltest.sl_ds.ExecuteSQL('DELLAYER:tpoly')
    gdal.PopErrorHandler()

    # Test invalid FORMAT
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr = gdaltest.sl_ds.CreateLayer('will_fail', options=['FORMAT=FOO'])
    gdal.PopErrorHandler()
    assert lyr is None, 'layer creation should have failed'

    # Test creating a layer with an existing name
    lyr = gdaltest.sl_ds.CreateLayer('a_layer')
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr = gdaltest.sl_ds.CreateLayer('a_layer')
    gdal.PopErrorHandler()
    assert lyr is None, 'layer creation should have failed'

    # Test OVERWRITE=YES
    lyr = gdaltest.sl_ds.CreateLayer('a_layer', options=['FID=my_fid', 'GEOMETRY_NAME=mygeom', 'OVERWRITE=YES'])
    assert lyr is not None, 'layer creation should have succeeded'

    ######################################################
    # Create Layer
    gdaltest.sl_lyr = gdaltest.sl_ds.CreateLayer('tpoly')

    ######################################################
    # Setup Schema

    fields = [('AREA', ogr.OFTReal),
              ('EAS_ID', ogr.OFTInteger),
              ('PRFEDEA', ogr.OFTString),
              ('BINCONTENT', ogr.OFTBinary),
              ('INT64', ogr.OFTInteger64)]

    ogrtest.quick_create_layer_def(gdaltest.sl_lyr,
                                   fields)
    fld_defn = ogr.FieldDefn('fld_boolean', ogr.OFTInteger)
    fld_defn.SetSubType(ogr.OFSTBoolean)
    gdaltest.sl_lyr.CreateField(fld_defn)

    ######################################################
    # Reopen database to be sure that the data types are properly read
    # even if no record are written

    gdaltest.sl_ds = None
    gdaltest.sl_ds = ogr.Open('tmp/sqlite_test.db', update=1)
    gdaltest.sl_lyr = gdaltest.sl_ds.GetLayerByName('tpoly')
    assert gdaltest.sl_lyr.GetGeometryColumn() == 'GEOMETRY'

    for field_desc in fields:
        feature_def = gdaltest.sl_lyr.GetLayerDefn()
        field_defn = feature_def.GetFieldDefn(feature_def.GetFieldIndex(field_desc[0]))
        if field_defn.GetType() != field_desc[1]:
            print('Expected type for %s is %s, not %s' %
                  (field_desc[0], field_defn.GetFieldTypeName(field_defn.GetType()),
                   field_defn.GetFieldTypeName(field_desc[1])))
    field_defn = feature_def.GetFieldDefn(feature_def.GetFieldIndex('fld_boolean'))
    assert field_defn.GetType() == ogr.OFTInteger and field_defn.GetSubType() == ogr.OFSTBoolean
    field_defn = feature_def.GetFieldDefn(feature_def.GetFieldIndex('INT64'))
    assert field_defn.GetType() == ogr.OFTInteger64

    assert gdaltest.sl_ds.GetLayerByName('a_layer').GetGeometryColumn() == 'mygeom'
    assert gdaltest.sl_ds.GetLayerByName('a_layer').GetFIDColumn() == 'my_fid'

    ######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature(feature_def)

    shp_ds = ogr.Open('data/poly.shp')
    gdaltest.shp_ds = shp_ds
    shp_lyr = shp_ds.GetLayer(0)

    feat = shp_lyr.GetNextFeature()
    gdaltest.poly_feat = []

    gdaltest.sl_lyr.StartTransaction()

    while feat is not None:

        gdaltest.poly_feat.append(feat)

        dst_feat.SetFrom(feat)
        dst_feat.SetField('int64', 1234567890123)
        gdaltest.sl_lyr.CreateFeature(dst_feat)

        feat = shp_lyr.GetNextFeature()

    gdaltest.sl_lyr.CommitTransaction()

###############################################################################
# Verify that stuff we just wrote is still OK.


def test_ogr_sqlite_3():
    if gdaltest.sl_ds is None:
        pytest.skip()

    assert gdaltest.sl_lyr.GetFeatureCount() == 10

    expect = [168, 169, 166, 158, 165]

    gdaltest.sl_lyr.SetAttributeFilter('eas_id < 170')
    tr = ogrtest.check_features_against_list(gdaltest.sl_lyr,
                                             'eas_id', expect)

    assert gdaltest.sl_lyr.GetFeatureCount() == 5

    gdaltest.sl_lyr.SetAttributeFilter(None)

    for i in range(len(gdaltest.poly_feat)):
        orig_feat = gdaltest.poly_feat[i]
        read_feat = gdaltest.sl_lyr.GetNextFeature()

        assert read_feat is not None, 'Did not get as many features as expected.'

        assert (ogrtest.check_feature_geometry(read_feat, orig_feat.GetGeometryRef(),
                                          max_error=0.001) == 0)

        for fld in range(3):
            assert orig_feat.GetField(fld) == read_feat.GetField(fld), \
                ('Attribute %d does not match' % fld)
        if read_feat.GetField('int64') != 1234567890123:
            read_feat.DumpReadable()
            pytest.fail()

    gdaltest.poly_feat = None
    gdaltest.shp_ds = None

    assert tr


###############################################################################
# Test retrieving layers


def test_ogr_sqlite_layers():

    if gdaltest.sl_ds is None:
        pytest.skip()

    assert gdaltest.sl_ds.GetLayerCount() == 2, 'did not get expected layer count'

    lyr = gdaltest.sl_ds.GetLayer(0)
    assert lyr is not None
    assert lyr.GetName() == 'a_layer', 'did not get expected layer name'
    assert lyr.GetGeomType() == ogr.wkbUnknown, 'did not get expected layer geometry type'
    assert lyr.GetFeatureCount() == 0, 'did not get expected feature count'

    lyr = gdaltest.sl_ds.GetLayer(1)
    assert lyr is not None
    assert lyr.GetName() == 'tpoly', 'did not get expected layer name'
    assert lyr.GetGeomType() == ogr.wkbUnknown, 'did not get expected layer geometry type'
    assert lyr.GetFeatureCount() == 10, 'did not get expected feature count'

    # Test LIST_ALL_TABLES=YES open option
    sl_ds_all_table = gdal.OpenEx('tmp/sqlite_test.db', gdal.OF_VECTOR | gdal.OF_UPDATE,
                                 open_options=['LIST_ALL_TABLES=YES'])
    assert sl_ds_all_table.GetLayerCount() == 5, 'did not get expected layer count'
    lyr = sl_ds_all_table.GetLayer(0)
    assert lyr is not None
    assert lyr.GetName() == 'a_layer', 'did not get expected layer name'
    assert not sl_ds_all_table.IsLayerPrivate(0)

    lyr = sl_ds_all_table.GetLayer(1)
    assert lyr is not None
    assert lyr.GetName() == 'tpoly', 'did not get expected layer name'
    assert not sl_ds_all_table.IsLayerPrivate(1)

    lyr = sl_ds_all_table.GetLayer(2)
    assert lyr is not None
    assert lyr.GetName() == 'geometry_columns', 'did not get expected layer name'
    assert sl_ds_all_table.IsLayerPrivate(2)

    lyr = sl_ds_all_table.GetLayer(3)
    assert lyr is not None
    assert lyr.GetName() == 'spatial_ref_sys', 'did not get expected layer name'
    assert sl_ds_all_table.IsLayerPrivate(3)

    lyr = sl_ds_all_table.GetLayer(4)
    assert lyr is not None
    assert lyr.GetName() == 'sqlite_sequence', 'did not get expected layer name'
    assert sl_ds_all_table.IsLayerPrivate(4)



###############################################################################
# Write more features with a bunch of different geometries, and verify the
# geometries are still OK.


def test_ogr_sqlite_4():

    if gdaltest.sl_ds is None:
        pytest.skip()

    dst_feat = ogr.Feature(feature_def=gdaltest.sl_lyr.GetLayerDefn())
    wkt_list = ['10', '2', '1', '3d_1', '4', '5', '6']

    for item in wkt_list:

        wkt = open('data/wkb_wkt/' + item + '.wkt').read()
        geom = ogr.CreateGeometryFromWkt(wkt)

        ######################################################################
        # Write geometry as a new feature.

        dst_feat.SetGeometryDirectly(geom)
        dst_feat.SetField('PRFEDEA', item)
        dst_feat.SetFID(-1)
        gdaltest.sl_lyr.CreateFeature(dst_feat)

        ######################################################################
        # Read back the feature and get the geometry.

        gdaltest.sl_lyr.SetAttributeFilter("PRFEDEA = '%s'" % item)
        feat_read = gdaltest.sl_lyr.GetNextFeature()

        assert feat_read is not None, 'Did not get as many features as expected.'

        assert ogrtest.check_feature_geometry(feat_read, geom) == 0


###############################################################################
# Test ExecuteSQL() results layers without geometry.


def test_ogr_sqlite_5():

    if gdaltest.sl_ds is None:
        pytest.skip()

    expect = [179, 173, 172, 171, 170, 169, 168, 166, 165, 158, None]

    sql_lyr = gdaltest.sl_ds.ExecuteSQL('select distinct eas_id from tpoly order by eas_id desc')

    assert sql_lyr.GetFeatureCount() == 11

    tr = ogrtest.check_features_against_list(sql_lyr, 'eas_id', expect)

    gdaltest.sl_ds.ReleaseResultSet(sql_lyr)

    assert tr

###############################################################################
# Test ExecuteSQL() results layers with geometry.


def test_ogr_sqlite_6():

    if gdaltest.sl_ds is None:
        pytest.skip()

    sql_lyr = gdaltest.sl_ds.ExecuteSQL("select * from tpoly where prfedea = '2'")

    tr = ogrtest.check_features_against_list(sql_lyr, 'prfedea', ['2'])
    if tr:
        sql_lyr.ResetReading()
        feat_read = sql_lyr.GetNextFeature()
        if ogrtest.check_feature_geometry(feat_read, 'MULTILINESTRING ((5.00121349 2.99853132,5.00121349 1.99853133),(5.00121349 1.99853133,5.00121349 0.99853133),(3.00121351 1.99853127,5.00121349 1.99853133),(5.00121349 1.99853133,6.00121348 1.99853135))') != 0:
            tr = 0

    gdaltest.sl_ds.ReleaseResultSet(sql_lyr)

    assert tr

###############################################################################
# Test spatial filtering.


def test_ogr_sqlite_7():

    if gdaltest.sl_ds is None:
        pytest.skip()

    gdaltest.sl_lyr.SetAttributeFilter(None)

    geom = ogr.CreateGeometryFromWkt(
        'LINESTRING(479505 4763195,480526 4762819)')
    gdaltest.sl_lyr.SetSpatialFilter(geom)
    geom.Destroy()

    assert gdaltest.sl_lyr.GetFeatureCount() == 1

    tr = ogrtest.check_features_against_list(gdaltest.sl_lyr, 'eas_id',
                                             [158])

    gdaltest.sl_lyr.SetAttributeFilter('eas_id = 158')

    assert gdaltest.sl_lyr.GetFeatureCount() == 1

    gdaltest.sl_lyr.SetAttributeFilter(None)

    gdaltest.sl_lyr.SetSpatialFilter(None)

    assert tr

###############################################################################
# Test transactions with rollback.


def test_ogr_sqlite_8():

    if gdaltest.sl_ds is None:
        pytest.skip()

    ######################################################################
    # Prepare working feature.

    dst_feat = ogr.Feature(feature_def=gdaltest.sl_lyr.GetLayerDefn())
    dst_feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT(10 20)'))

    dst_feat.SetField('PRFEDEA', 'rollbacktest')

    ######################################################################
    # Create it, but rollback the transaction.

    gdaltest.sl_lyr.StartTransaction()
    gdaltest.sl_lyr.CreateFeature(dst_feat)
    gdaltest.sl_lyr.RollbackTransaction()

    ######################################################################
    # Verify that it is not in the layer.

    gdaltest.sl_lyr.SetAttributeFilter("PRFEDEA = 'rollbacktest'")
    feat_read = gdaltest.sl_lyr.GetNextFeature()
    gdaltest.sl_lyr.SetAttributeFilter(None)

    assert feat_read is None, 'Unexpectedly got rollbacktest feature.'

    ######################################################################
    # Create it, and commit the transaction.

    gdaltest.sl_lyr.StartTransaction()
    gdaltest.sl_lyr.CreateFeature(dst_feat)
    gdaltest.sl_lyr.CommitTransaction()

    ######################################################################
    # Verify that it is not in the layer.

    gdaltest.sl_lyr.SetAttributeFilter("PRFEDEA = 'rollbacktest'")
    feat_read = gdaltest.sl_lyr.GetNextFeature()
    gdaltest.sl_lyr.SetAttributeFilter(None)

    assert feat_read is not None, 'Failed to get committed feature.'

    feat_read.Destroy()
    dst_feat.Destroy()

###############################################################################
# Test SetFeature()


def test_ogr_sqlite_9():

    if gdaltest.sl_ds is None:
        pytest.skip()

    ######################################################################
    # Read feature with EAS_ID 158.

    gdaltest.sl_lyr.SetAttributeFilter("eas_id = 158")
    feat_read = gdaltest.sl_lyr.GetNextFeature()
    gdaltest.sl_lyr.SetAttributeFilter(None)

    assert feat_read is not None, 'did not find eas_id 158!'

    ######################################################################
    # Modify the PRFEDEA value, and reset it.

    feat_read.SetField('PRFEDEA', 'SetWorked')
    err = gdaltest.sl_lyr.SetFeature(feat_read)
    assert err == 0, ('SetFeature() reported error %d' % err)

    ######################################################################
    # Read feature with EAS_ID 158 and check that PRFEDEA was altered.

    gdaltest.sl_lyr.SetAttributeFilter("eas_id = 158")
    feat_read_2 = gdaltest.sl_lyr.GetNextFeature()
    gdaltest.sl_lyr.SetAttributeFilter(None)

    assert feat_read_2 is not None, 'did not find eas_id 158!'

    if feat_read_2.GetField('PRFEDEA') != 'SetWorked':
        feat_read_2.DumpReadable()
        pytest.fail('PRFEDEA apparently not reset as expected.')

    # Test updating non-existing feature
    feat_read.SetFID(-10)
    assert gdaltest.sl_lyr.SetFeature(feat_read) == ogr.OGRERR_NON_EXISTING_FEATURE, \
        'Expected failure of SetFeature().'

    # Test deleting non-existing feature
    assert gdaltest.sl_lyr.DeleteFeature(-10) == ogr.OGRERR_NON_EXISTING_FEATURE, \
        'Expected failure of DeleteFeature().'

    feat_read.Destroy()
    feat_read_2.Destroy()

###############################################################################
# Test GetFeature()


def test_ogr_sqlite_10():

    if gdaltest.sl_ds is None:
        pytest.skip()

    ######################################################################
    # Read feature with EAS_ID 158.

    gdaltest.sl_lyr.SetAttributeFilter("eas_id = 158")
    feat_read = gdaltest.sl_lyr.GetNextFeature()
    gdaltest.sl_lyr.SetAttributeFilter(None)

    assert feat_read is not None, 'did not find eas_id 158!'

    ######################################################################
    # Now read the feature by FID.

    feat_read_2 = gdaltest.sl_lyr.GetFeature(feat_read.GetFID())

    assert feat_read_2 is not None, ('did not find FID %d' % feat_read.GetFID())

    if feat_read_2.GetField('PRFEDEA') != feat_read.GetField('PRFEDEA'):
        feat_read.DumpReadable()
        feat_read_2.DumpReadable()
        pytest.fail('GetFeature() result seems to not match expected.')


###############################################################################
# Test FORMAT=WKB creation option


def test_ogr_sqlite_11():

    if gdaltest.sl_ds is None:
        pytest.skip()

    ######################################################
    # Create Layer with WKB geometry
    gdaltest.sl_lyr = gdaltest.sl_ds.CreateLayer('geomwkb', options=['FORMAT=WKB'])

    geom = ogr.CreateGeometryFromWkt('POINT(0 1)')
    dst_feat = ogr.Feature(feature_def=gdaltest.sl_lyr.GetLayerDefn())
    dst_feat.SetGeometry(geom)
    gdaltest.sl_lyr.CreateFeature(dst_feat)
    dst_feat = None

    # Test adding a column to see if geometry is preserved (#3471)
    gdaltest.sl_lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))

    ######################################################
    # Reopen DB
    gdaltest.sl_ds = None
    gdaltest.sl_ds = ogr.Open('tmp/sqlite_test.db', update=1)
    gdaltest.sl_lyr = gdaltest.sl_ds.GetLayerByName('geomwkb')

    feat_read = gdaltest.sl_lyr.GetNextFeature()
    assert ogrtest.check_feature_geometry(feat_read, geom, max_error=0.001) == 0

    gdaltest.sl_lyr.ResetReading()

###############################################################################
# Test FORMAT=WKT creation option


def test_ogr_sqlite_12():

    if gdaltest.sl_ds is None:
        pytest.skip()

    ######################################################
    # Create Layer with WKT geometry
    gdaltest.sl_lyr = gdaltest.sl_ds.CreateLayer('geomwkt', options=['FORMAT=WKT'])

    geom = ogr.CreateGeometryFromWkt('POINT(0 1)')
    dst_feat = ogr.Feature(feature_def=gdaltest.sl_lyr.GetLayerDefn())
    dst_feat.SetGeometry(geom)
    gdaltest.sl_lyr.CreateFeature(dst_feat)
    dst_feat = None

    # Test adding a column to see if geometry is preserved (#3471)
    gdaltest.sl_lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))

    ######################################################
    # Reopen DB
    gdaltest.sl_ds = None
    gdaltest.sl_ds = ogr.Open('tmp/sqlite_test.db', update=1)
    gdaltest.sl_lyr = gdaltest.sl_ds.GetLayerByName('geomwkt')

    feat_read = gdaltest.sl_lyr.GetNextFeature()
    assert ogrtest.check_feature_geometry(feat_read, geom, max_error=0.001) == 0
    feat_read = None

    gdaltest.sl_lyr.ResetReading()

    sql_lyr = gdaltest.sl_ds.ExecuteSQL("select * from geomwkt")

    feat_read = sql_lyr.GetNextFeature()
    assert ogrtest.check_feature_geometry(feat_read, geom, max_error=0.001) == 0
    feat_read = None

    feat_read = sql_lyr.GetFeature(0)
    assert ogrtest.check_feature_geometry(feat_read, geom, max_error=0.001) == 0
    feat_read = None

    gdaltest.sl_ds.ReleaseResultSet(sql_lyr)

###############################################################################
# Test SRID support


def test_ogr_sqlite_13():

    if gdaltest.sl_ds is None:
        pytest.skip()

    ######################################################
    # Create Layer with EPSG:4326
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    gdaltest.sl_lyr = gdaltest.sl_ds.CreateLayer('wgs84layer', srs=srs)

    ######################################################
    # Reopen DB
    gdaltest.sl_ds = None
    gdaltest.sl_ds = ogr.Open('tmp/sqlite_test.db', update=1)
    gdaltest.sl_lyr = gdaltest.sl_ds.GetLayerByName('wgs84layer')

    assert gdaltest.sl_lyr.GetSpatialRef().IsSame(srs, options = ['IGNORE_DATA_AXIS_TO_SRS_AXIS_MAPPING=YES']), 'SRS is not the one expected.'

    ######################################################
    # Create second layer with very approximative EPSG:4326
    srs = osr.SpatialReference()
    srs.SetFromUserInput('GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433],AUTHORITY["EPSG","4326"]]')
    gdaltest.sl_lyr = gdaltest.sl_ds.CreateLayer('wgs84layer_approx', srs=srs)

    # Must still be 1
    sql_lyr = gdaltest.sl_ds.ExecuteSQL("SELECT COUNT(*) AS count FROM spatial_ref_sys")
    feat = sql_lyr.GetNextFeature()
    assert feat.GetFieldAsInteger('count') == 1
    gdaltest.sl_ds.ReleaseResultSet(sql_lyr)


###############################################################################
# Test all column types

def test_ogr_sqlite_14():

    if gdaltest.sl_ds is None:
        pytest.skip()

    gdaltest.sl_lyr = gdaltest.sl_ds.CreateLayer('testtypes')
    ogrtest.quick_create_layer_def(gdaltest.sl_lyr,
                                   [('INTEGER', ogr.OFTInteger),
                                    ('FLOAT', ogr.OFTReal),
                                    ('STRING', ogr.OFTString),
                                    ('BLOB', ogr.OFTBinary),
                                    ('BLOB2', ogr.OFTBinary)])

    dst_feat = ogr.Feature(feature_def=gdaltest.sl_lyr.GetLayerDefn())

    dst_feat.SetField('INTEGER', 1)
    dst_feat.SetField('FLOAT', 1.2)
    dst_feat.SetField('STRING', 'myString\'a')
    dst_feat.SetFieldBinaryFromHexString('BLOB', '0001FF')

    gdaltest.sl_lyr.CreateFeature(dst_feat)

    ######################################################
    # Reopen DB
    gdaltest.sl_ds = None
    gdaltest.sl_ds = ogr.Open('tmp/sqlite_test.db', update=1)
    gdaltest.sl_lyr = gdaltest.sl_ds.GetLayerByName('testtypes')

    # Duplicate the first record
    dst_feat = ogr.Feature(feature_def=gdaltest.sl_lyr.GetLayerDefn())
    feat_read = gdaltest.sl_lyr.GetNextFeature()
    dst_feat.SetFrom(feat_read)
    gdaltest.sl_lyr.CreateFeature(dst_feat)

    # Check the 2 records
    gdaltest.sl_lyr.ResetReading()
    for _ in range(2):
        feat_read = gdaltest.sl_lyr.GetNextFeature()
        assert (feat_read.GetField('INTEGER') == 1 and \
           feat_read.GetField('FLOAT') == 1.2 and \
           feat_read.GetField('STRING') == 'myString\'a' and \
           feat_read.GetFieldAsString('BLOB') == '0001FF')

    gdaltest.sl_lyr.ResetReading()

###############################################################################
# Test FORMAT=SPATIALITE layer creation option


def test_ogr_sqlite_15():

    if gdaltest.sl_ds is None:
        pytest.skip()

    ######################################################
    # Create Layer with SPATIALITE geometry
    with gdaltest.error_handler():
        gdaltest.sl_lyr = gdaltest.sl_ds.CreateLayer('geomspatialite', options=['FORMAT=SPATIALITE'])

    geoms = [ogr.CreateGeometryFromWkt('POINT(0 1)'),
             ogr.CreateGeometryFromWkt('MULTIPOINT EMPTY'),
             ogr.CreateGeometryFromWkt('MULTIPOINT (0 1,2 3)'),
             ogr.CreateGeometryFromWkt('LINESTRING EMPTY'),
             ogr.CreateGeometryFromWkt('LINESTRING (1 2,3 4)'),
             ogr.CreateGeometryFromWkt('MULTILINESTRING EMPTY'),
             ogr.CreateGeometryFromWkt('MULTILINESTRING ((1 2,3 4),(5 6,7 8))'),
             ogr.CreateGeometryFromWkt('POLYGON EMPTY'),
             ogr.CreateGeometryFromWkt('POLYGON ((1 2,3 4))'),
             ogr.CreateGeometryFromWkt('POLYGON ((1 2,3 4),(5 6,7 8))'),
             ogr.CreateGeometryFromWkt('MULTIPOLYGON EMPTY'),
             ogr.CreateGeometryFromWkt('MULTIPOLYGON (((1 2,3 4)),((5 6,7 8)))'),
             ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION EMPTY'),
             ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION (POLYGON ((1 2,3 4)),POLYGON ((5 6,7 8)))'),
             ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION (POLYGON ((1 2,3 4)),POINT(0 1))')]

    gdaltest.sl_lyr.StartTransaction()

    for geom in geoms:
        dst_feat = ogr.Feature(feature_def=gdaltest.sl_lyr.GetLayerDefn())
        dst_feat.SetGeometry(geom)
        gdaltest.sl_lyr.CreateFeature(dst_feat)

    gdaltest.sl_lyr.CommitTransaction()

    ######################################################
    # Reopen DB
    gdaltest.sl_ds = None
    gdaltest.sl_ds = ogr.Open('tmp/sqlite_test.db')

    # Test creating a layer on a read-only DB
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr = gdaltest.sl_ds.CreateLayer('will_fail')
    gdal.PopErrorHandler()
    assert lyr is None, 'layer creation should have failed'

    gdaltest.sl_lyr = gdaltest.sl_ds.GetLayerByName('geomspatialite')

    for geom in geoms:
        feat_read = gdaltest.sl_lyr.GetNextFeature()
        assert ogrtest.check_feature_geometry(feat_read, geom, max_error=0.001) == 0

    gdaltest.sl_lyr.ResetReading()

    sql_lyr = gdaltest.sl_ds.ExecuteSQL("select * from geomspatialite")

    feat_read = sql_lyr.GetNextFeature()
    assert ogrtest.check_feature_geometry(feat_read, geoms[0], max_error=0.001) == 0

    feat_read = sql_lyr.GetFeature(0)
    assert ogrtest.check_feature_geometry(feat_read, geoms[0], max_error=0.001) == 0

    gdaltest.sl_ds.ReleaseResultSet(sql_lyr)


###############################################################################
# Test reading geometries in FGF (FDO Geometry Format) binary representation.

def test_ogr_sqlite_16():

    if gdaltest.sl_ds is None:
        pytest.skip()

    ######################################################
    # Reopen DB in update
    gdaltest.sl_ds = None
    gdaltest.sl_ds = ogr.Open('tmp/sqlite_test.db', update=1)

    # Hand create a table with FGF geometry
    gdaltest.sl_ds.ExecuteSQL("INSERT INTO geometry_columns (f_table_name, f_geometry_column, geometry_type, coord_dimension, geometry_format) VALUES ('fgf_table', 'GEOMETRY', 0, 2, 'FGF')")
    gdaltest.sl_ds.ExecuteSQL("CREATE TABLE fgf_table (OGC_FID INTEGER PRIMARY KEY, GEOMETRY BLOB)")
    gdaltest.sl_ds.ExecuteSQL("INSERT INTO fgf_table (OGC_FID, GEOMETRY) VALUES (1, X'0100000000000000000000000000F03F0000000000000040')")
    gdaltest.sl_ds.ExecuteSQL("INSERT INTO fgf_table (OGC_FID, GEOMETRY) VALUES (2, X'020000000000000000000000')")
    gdaltest.sl_ds.ExecuteSQL("INSERT INTO fgf_table (OGC_FID, GEOMETRY) VALUES (3, X'020000000000000002000000000000000000F03F000000000000004000000000000008400000000000001040')")
    gdaltest.sl_ds.ExecuteSQL("INSERT INTO fgf_table (OGC_FID, GEOMETRY) VALUES (4, X'030000000000000000000000')")
    gdaltest.sl_ds.ExecuteSQL("INSERT INTO fgf_table (OGC_FID, GEOMETRY) VALUES (5, X'03000000000000000200000002000000000000000000F03F00000000000000400000000000000840000000000000104000000000')")
    gdaltest.sl_ds.ExecuteSQL("INSERT INTO fgf_table (OGC_FID, GEOMETRY) VALUES (6, X'0700000000000000')")
    gdaltest.sl_ds.ExecuteSQL("INSERT INTO fgf_table (OGC_FID, GEOMETRY) VALUES (7, X'070000000200000003000000000000000200000002000000000000000000F03F0000000000000040000000000000084000000000000010400000000003000000000000000200000002000000000000000000F03F00000000000000400000000000000840000000000000104000000000')")
    gdaltest.sl_ds.ExecuteSQL("INSERT INTO fgf_table (OGC_FID, GEOMETRY) VALUES (8, X'0100000001000000000000000000F03F00000000000000400000000000000840')")

    # invalid geometries
    gdaltest.sl_ds.ExecuteSQL("INSERT INTO fgf_table (OGC_FID, GEOMETRY) VALUES (9, X'0700000001000000')")
    gdaltest.sl_ds.ExecuteSQL("INSERT INTO fgf_table (OGC_FID, GEOMETRY) VALUES (10,X'060000000100000001')")
    gdaltest.sl_ds.ExecuteSQL("INSERT INTO fgf_table (OGC_FID, GEOMETRY) VALUES (11,X'06000000010000000100000000000000000000000000F03F0000000000000040')")

    ######################################################
    # Reopen DB
    gdaltest.sl_ds = None
    gdaltest.sl_ds = ogr.Open('tmp/sqlite_test.db', update=1)
    gdaltest.sl_lyr = gdaltest.sl_ds.GetLayerByName('fgf_table')

    feat = gdaltest.sl_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    assert geom.ExportToWkt() == 'POINT (1 2)'

    feat = gdaltest.sl_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    assert geom.ExportToWkt() == 'LINESTRING EMPTY'

    feat = gdaltest.sl_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    assert geom.ExportToWkt() == 'LINESTRING (1 2,3 4)'

    feat = gdaltest.sl_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    assert geom.ExportToWkt() == 'POLYGON EMPTY'

    feat = gdaltest.sl_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    assert geom.ExportToWkt() == 'POLYGON ((1 2,3 4))'

    feat = gdaltest.sl_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    assert geom.ExportToWkt() == 'GEOMETRYCOLLECTION EMPTY'

    feat = gdaltest.sl_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    assert geom.ExportToWkt() == 'GEOMETRYCOLLECTION (POLYGON ((1 2,3 4)),POLYGON ((1 2,3 4)))'

    feat = gdaltest.sl_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    assert geom.ExportToWkt() == 'POINT (1 2 3)'

    # Test invalid geometries
    for _ in range(3):
        feat = gdaltest.sl_lyr.GetNextFeature()
        geom = feat.GetGeometryRef()
        assert geom is None

    gdaltest.sl_lyr.ResetReading()

###############################################################################
# Test SPATIALITE dataset creation option


def test_ogr_sqlite_17(require_spatialite):

    if gdaltest.sl_ds is None:
        pytest.skip()

    ######################################################
    # Create dataset with SPATIALITE geometry

    with gdaltest.error_handler():
        ds = ogr.GetDriverByName('SQLite').CreateDataSource('tmp/spatialite_test.db', options=['SPATIALITE=YES'])

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr = ds.CreateLayer('will_fail', options=['FORMAT=WKB'])
    gdal.PopErrorHandler()
    assert lyr is None, 'layer creation should have failed'

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    lyr = ds.CreateLayer('geomspatialite', srs=srs)

    geom = ogr.CreateGeometryFromWkt('POINT(0 1)')

    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetGeometry(geom)
    lyr.CreateFeature(dst_feat)

    ######################################################
    # Reopen DB
    ds = None
    ds = ogr.Open('tmp/spatialite_test.db')
    lyr = ds.GetLayerByName('geomspatialite')

    feat_read = lyr.GetNextFeature()
    assert ogrtest.check_feature_geometry(feat_read, geom, max_error=0.001) == 0

    srs = lyr.GetSpatialRef()
    wkt = srs.ExportToWkt()
    assert wkt.find('4326') != -1, 'did not identify correctly SRS'


###############################################################################
# Create a layer with a non EPSG SRS into a SPATIALITE DB (#3506)


def test_ogr_sqlite_18(require_spatialite):
    if gdaltest.sl_ds is None:
        pytest.skip()

    ds = ogr.Open('tmp/spatialite_test.db', update=1)
    srs = osr.SpatialReference()
    srs.SetFromUserInput('+proj=vandg')
    lyr = ds.CreateLayer('nonepsgsrs', srs=srs)

    ######################################################
    # Reopen DB
    ds = None
    ds = ogr.Open('tmp/spatialite_test.db')

    lyr = ds.GetLayerByName('nonepsgsrs')
    srs = lyr.GetSpatialRef()
    wkt = srs.ExportToWkt()
    assert wkt.find('VanDerGrinten') != -1, 'did not identify correctly SRS'

    sql_lyr = ds.ExecuteSQL("SELECT * FROM spatial_ref_sys ORDER BY srid DESC LIMIT 1")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField('auth_name') != 'OGR' or \
       feat.GetField('proj4text').find('+proj=vandg') != 0:
        feat.DumpReadable()
        pytest.fail()
    gdaltest.sl_ds.ReleaseResultSet(sql_lyr)

###############################################################################
# Create a SpatiaLite DB with INIT_WITH_EPSG=YES


def test_ogr_sqlite_19(require_spatialite):

    if gdaltest.sl_ds is None:
        pytest.skip()

    if int(gdal.VersionInfo('VERSION_NUM')) < 1800:
        pytest.skip()

    if require_spatialite != '2.3.1':
        pytest.skip()

    ds = ogr.GetDriverByName('SQLite').CreateDataSource('tmp/spatialite_test_with_epsg.db', options=['SPATIALITE=YES', 'INIT_WITH_EPSG=YES'])

    # EPSG:26632 has a ' character in it's WKT representation
    srs = osr.SpatialReference()
    srs.SetFromUserInput('EPSG:26632')
    ds.CreateLayer('test', srs=srs)

    ds = None
    ds = ogr.Open('tmp/spatialite_test_with_epsg.db')

    sql_lyr = ds.ExecuteSQL("select count(*) from spatial_ref_sys")
    feat = sql_lyr.GetNextFeature()
    nb_srs = feat.GetFieldAsInteger(0)
    ds.ReleaseResultSet(sql_lyr)

    # Currently the injection of the EPSG DB as proj.4 strings adds 3915 entries
    assert nb_srs >= 3915, 'did not get expected SRS count'


###############################################################################
# Create a SpatiaLite DB with INIT_WITH_EPSG=NO


def test_ogr_sqlite_19_bis(require_spatialite):

    if gdaltest.sl_ds is None:
        pytest.skip()

    spatialite_major_ver = int(require_spatialite.split('.')[0])
    if spatialite_major_ver < 4:
        pytest.skip()

    ds = ogr.GetDriverByName('SQLite').CreateDataSource('/vsimem/spatialite_test_without_epsg.db', options=['SPATIALITE=YES', 'INIT_WITH_EPSG=NO'])

    # EPSG:26632 has a ' character in it's WKT representation
    srs = osr.SpatialReference()
    srs.SetFromUserInput('EPSG:26632')
    ds.CreateLayer('test', srs=srs)

    ds = None
    ds = ogr.Open('/vsimem/spatialite_test_without_epsg.db')

    sql_lyr = ds.ExecuteSQL("select count(*) from spatial_ref_sys")
    feat = sql_lyr.GetNextFeature()
    nb_srs = feat.GetFieldAsInteger(0)
    ds.ReleaseResultSet(sql_lyr)

    assert nb_srs == 1, 'did not get expected SRS count'

    gdal.Unlink('/vsimem/spatialite_test_without_epsg.db')

###############################################################################
# Create a regular DB with INIT_WITH_EPSG=YES


def test_ogr_sqlite_20():

    if gdaltest.sl_ds is None:
        pytest.skip()

    gdal.Unlink('tmp/non_spatialite_test_with_epsg.db')

    ds = ogr.GetDriverByName('SQLite').CreateDataSource('tmp/non_spatialite_test_with_epsg.db', options=['INIT_WITH_EPSG=YES'])

    # EPSG:26632 has a ' character in it's WKT representation
    srs = osr.SpatialReference()
    srs.SetFromUserInput('EPSG:26632')
    ds.CreateLayer('test', srs=srs)

    ds = None
    ds = ogr.Open('tmp/non_spatialite_test_with_epsg.db')

    sql_lyr = ds.ExecuteSQL("select count(*) from spatial_ref_sys")
    feat = sql_lyr.GetNextFeature()
    nb_srs = feat.GetFieldAsInteger(0)
    ds.ReleaseResultSet(sql_lyr)

    # Currently the injection of the EPSG DB as proj.4 strings adds 3945 entries
    assert nb_srs >= 3945, 'did not get expected SRS count'


###############################################################################
# Test CopyLayer() from a table layer (#3617)


def test_ogr_sqlite_21():

    if gdaltest.sl_ds is None:
        pytest.skip()

    src_lyr = gdaltest.sl_ds.GetLayerByName('tpoly')
    copy_lyr = gdaltest.sl_ds.CopyLayer(src_lyr, 'tpoly_2')

    src_lyr_count = src_lyr.GetFeatureCount()
    copy_lyr_count = copy_lyr.GetFeatureCount()
    assert src_lyr_count == copy_lyr_count, 'did not get same number of features'

###############################################################################
# Test CopyLayer() from a result layer (#3617)


def test_ogr_sqlite_22():

    if gdaltest.sl_ds is None:
        pytest.skip()

    src_lyr = gdaltest.sl_ds.ExecuteSQL('select * from tpoly')
    copy_lyr = gdaltest.sl_ds.CopyLayer(src_lyr, 'tpoly_3')

    src_lyr_count = src_lyr.GetFeatureCount()
    copy_lyr_count = copy_lyr.GetFeatureCount()
    assert src_lyr_count == copy_lyr_count, 'did not get same number of features'

    gdaltest.sl_ds.ReleaseResultSet(src_lyr)

###############################################################################
# Test ignored fields works ok


def test_ogr_sqlite_23():

    if gdaltest.sl_ds is None:
        pytest.skip()

    shp_layer = gdaltest.sl_ds.GetLayerByName('tpoly')
    shp_layer.SetIgnoredFields(['AREA'])

    shp_layer.ResetReading()
    feat = shp_layer.GetNextFeature()

    assert not feat.IsFieldSet('AREA'), 'got area despite request to ignore it.'

    assert feat.GetFieldAsInteger('EAS_ID') == 168, 'missing or wrong eas_id'

    wkt = 'POLYGON ((479819.84375 4765180.5,479690.1875 4765259.5,479647.0 4765369.5,479730.375 4765400.5,480039.03125 4765539.5,480035.34375 4765558.5,480159.78125 4765610.5,480202.28125 4765482.0,480365.0 4765015.5,480389.6875 4764950.0,480133.96875 4764856.5,480080.28125 4764979.5,480082.96875 4765049.5,480088.8125 4765139.5,480059.90625 4765239.5,480019.71875 4765319.5,479980.21875 4765409.5,479909.875 4765370.0,479859.875 4765270.0,479819.84375 4765180.5))'
    assert (ogrtest.check_feature_geometry(feat, wkt,
                                      max_error=0.00000001) == 0)

    fd = shp_layer.GetLayerDefn()
    fld = fd.GetFieldDefn(0)  # area
    assert fld.IsIgnored(), 'AREA unexpectedly not marked as ignored.'

    fld = fd.GetFieldDefn(1)  # eas_id
    assert not fld.IsIgnored(), 'EASI unexpectedly marked as ignored.'

    assert not fd.IsGeometryIgnored(), 'geometry unexpectedly ignored.'

    assert not fd.IsStyleIgnored(), 'style unexpectedly ignored.'

    fd.SetGeometryIgnored(1)

    assert fd.IsGeometryIgnored(), 'geometry unexpectedly not ignored.'

    feat = shp_layer.GetNextFeature()

    assert feat.GetGeometryRef() is None, 'Unexpectedly got a geometry on feature 2.'

    assert not feat.IsFieldSet('AREA'), 'got area despite request to ignore it.'

    assert feat.GetFieldAsInteger('EAS_ID') == 179, 'missing or wrong eas_id'

###############################################################################
# Test that ExecuteSQL() with OGRSQL dialect doesn't forward the where clause to sqlite (#4022)


def test_ogr_sqlite_24():

    if gdaltest.sl_ds is None:
        pytest.skip()

    try:
        os.remove('tmp/test24.sqlite')
    except OSError:
        pass
    ds = ogr.GetDriverByName('SQLite').CreateDataSource('tmp/test24.sqlite')
    lyr = ds.CreateLayer('test')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(0 1)'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('LINESTRING(2 3)'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((4 5,6 7))'))
    lyr.CreateFeature(feat)
    ds = None

    ds = ogr.Open('tmp/test24.sqlite')

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr = ds.ExecuteSQL('select OGR_GEOMETRY from test')
    gdal.PopErrorHandler()
    if lyr is not None:
        ds.ReleaseResultSet(lyr)
        pytest.fail('this should not work (1)')

    lyr = ds.ExecuteSQL('select * from test')
    lyr.SetAttributeFilter("OGR_GEOMETRY = 'POLYGON'")
    feat = lyr.GetNextFeature()
    ds.ReleaseResultSet(lyr)
    assert feat is not None, 'a feature was expected (2)'

    lyr = ds.GetLayerByName('test')
    lyr.SetAttributeFilter("OGR_GEOMETRY = 'POLYGON'")
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    feat = lyr.GetNextFeature()
    gdal.PopErrorHandler()
    assert feat is None, 'a feature was not expected (3)'

    lyr = ds.ExecuteSQL('select OGR_GEOMETRY from test', dialect='OGRSQL')
    lyr.SetAttributeFilter("OGR_GEOMETRY = 'POLYGON'")
    feat = lyr.GetNextFeature()
    ds.ReleaseResultSet(lyr)
    assert feat is not None, 'a feature was expected (4)'

    lyr = ds.ExecuteSQL("select OGR_GEOMETRY from test WHERE OGR_GEOMETRY = 'POLYGON'", dialect='OGRSQL')
    feat = lyr.GetNextFeature()
    ds.ReleaseResultSet(lyr)
    assert feat is not None, 'a feature was expected (5)'

    ds = None

###############################################################################
# Test opening a /vsicurl/ DB


def test_ogr_sqlite_25():

    if gdaltest.sl_ds is None:
        pytest.skip()

    sql_lyr = gdaltest.sl_ds.ExecuteSQL("SELECT sqlite_version()")
    feat = sql_lyr.GetNextFeature()
    ogrtest.sqlite_version = feat.GetFieldAsString(0)
    print('SQLite version : %s' % ogrtest.sqlite_version)
    feat = None
    gdaltest.sl_ds.ReleaseResultSet(sql_lyr)

    drv = gdal.GetDriverByName('HTTP')

    if drv is None:
        pytest.skip()

    # Check that we have SQLite VFS support
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogr.GetDriverByName('SQLite').CreateDataSource('/vsimem/ogr_sqlite_25.db')
    gdal.PopErrorHandler()
    if ds is None:
        pytest.skip()
    ds = None
    gdal.Unlink('/vsimem/ogr_sqlite_25.db')

    gdal.SetConfigOption('GDAL_HTTP_TIMEOUT', '5')
    ds = ogr.Open('/vsicurl/http://download.osgeo.org/gdal/data/sqlite3/polygon.db')
    gdal.SetConfigOption('GDAL_HTTP_TIMEOUT', None)
    if ds is None:
        if gdaltest.gdalurlopen('http://download.osgeo.org/gdal/data/sqlite3/polygon.db', timeout=4) is None:
            pytest.skip('cannot open URL')
        pytest.fail()

    lyr = ds.GetLayerByName('polygon')
    assert lyr is not None

    assert lyr.GetLayerDefn().GetFieldCount() != 0

###############################################################################
# Test creating a :memory: DB


def test_ogr_sqlite_26():

    if gdaltest.sl_ds is None:
        pytest.skip()

    ds = ogr.GetDriverByName('SQLite').CreateDataSource(':memory:')
    sql_lyr = ds.ExecuteSQL('select count(*) from geometry_columns')
    assert sql_lyr is not None, 'expected existing geometry_columns'

    count = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    assert count == 1, 'expected existing geometry_columns'

###############################################################################
# Run test_ogrsf


def test_ogr_sqlite_27():

    if gdaltest.sl_ds is None:
        pytest.skip()

    import test_cli_utilities
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f SQLite tmp/ogr_sqlite_27.sqlite data/poly.shp --config OGR_SQLITE_SYNCHRONOUS OFF')

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' tmp/ogr_sqlite_27.sqlite')

    pos = ret.find('ERROR: poLayerFeatSRS != NULL && poSQLFeatSRS == NULL.')
    if pos != -1:
        # Detect if libsqlite3 has been built with SQLITE_HAS_COLUMN_METADATA
        # If not, that explains the error.
        ds = ogr.Open(':memory:')
        sql_lyr = ds.ExecuteSQL('SQLITE_HAS_COLUMN_METADATA()')
        feat = sql_lyr.GetNextFeature()
        val = feat.GetField(0)
        ds.ReleaseResultSet(sql_lyr)
        if val == 0:
            ret = ret[0:pos] + ret[pos + len('ERROR: poLayerFeatSRS != NULL && poSQLFeatSRS == NULL.'):]

            # And remove ERROR ret code consequently
            pos = ret.find('ERROR ret code = 1')
            if pos != -1:
                ret = ret[0:pos]

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

    # Test on a result SQL layer
    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro tmp/ogr_sqlite_27.sqlite -sql "SELECT * FROM poly"')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

###############################################################################
# Run test_ogrsf on a spatialite enabled DB


def test_ogr_sqlite_28():

    if gdaltest.sl_ds is None:
        pytest.skip()

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    # Test with a Spatialite 3.0 DB
    shutil.copy('data/sqlite/poly_spatialite.sqlite', 'tmp/poly_spatialite.sqlite')
    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' tmp/poly_spatialite.sqlite')
    os.unlink('tmp/poly_spatialite.sqlite')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

    # Test on a result SQL layer
    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/sqlite/poly_spatialite.sqlite -sql "SELECT * FROM poly"')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

    # Test with a Spatialite 4.0 DB
    shutil.copy('data/sqlite/poly_spatialite4.sqlite', 'tmp/poly_spatialite4.sqlite')
    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' tmp/poly_spatialite4.sqlite')
    os.unlink('tmp/poly_spatialite4.sqlite')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

    # Generic test
    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -driver SQLite -dsco SPATIALITE=YES')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

###############################################################################
# Test CreateFeature() with empty feature


def test_ogr_sqlite_29():

    if gdaltest.sl_ds is None:
        pytest.skip()

    try:
        os.remove('tmp/ogr_sqlite_29.sqlite')
    except OSError:
        pass
    ds = ogr.GetDriverByName('SQLite').CreateDataSource('tmp/ogr_sqlite_29.sqlite')

    lyr = ds.CreateLayer('test')
    feat = ogr.Feature(lyr.GetLayerDefn())
    assert lyr.CreateFeature(feat) == 0

    ds = None

###############################################################################
# Test ExecuteSQL() with empty result set (#4684)


def test_ogr_sqlite_30():

    if gdaltest.sl_ds is None:
        pytest.skip()

    sql_lyr = gdaltest.sl_ds.ExecuteSQL('SELECT * FROM tpoly WHERE eas_id = 12345')
    if sql_lyr is None:
        pytest.skip()

    # Test fix added in r24768
    feat = sql_lyr.GetNextFeature()
    assert feat is None

    gdaltest.sl_ds.ReleaseResultSet(sql_lyr)


###############################################################################
# Test spatial filter when SpatiaLite is available


def test_ogr_spatialite_2(require_spatialite):

    ds = ogr.Open('tmp/spatialite_test.db', update=1)
    srs = osr.SpatialReference()
    srs.SetFromUserInput('EPSG:4326')
    lyr = ds.CreateLayer('test_spatialfilter', srs=srs)
    lyr.CreateField(ogr.FieldDefn('intcol', ogr.OFTInteger))

    lyr.StartTransaction()

    for i in range(10):
        for j in range(10):
            geom = ogr.CreateGeometryFromWkt('POINT(%d %d)' % (i, j))
            dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
            dst_feat.SetGeometry(geom)
            lyr.CreateFeature(dst_feat)
            dst_feat.Destroy()

    geom = ogr.CreateGeometryFromWkt('POLYGON((0 0,0 3,3 3,3 0,0 0))')
    dst_feat = ogr.Feature(feature_def=lyr.GetLayerDefn())
    dst_feat.SetGeometry(geom)
    lyr.CreateFeature(dst_feat)
    dst_feat.Destroy()

    lyr.CommitTransaction()

    ds = None

    # Test OLCFastFeatureCount with spatial index (created by default)
    ds = ogr.Open('tmp/spatialite_test.db', update=0)
    lyr = ds.GetLayerByName('test_spatialfilter')

    extent = lyr.GetExtent()
    assert extent == (0.0, 9.0, 0.0, 9.0), 'got bad extent'

    # Test caching
    extent = lyr.GetExtent()
    assert extent == (0.0, 9.0, 0.0, 9.0), 'got bad extent'

    geom = ogr.CreateGeometryFromWkt(
        'POLYGON((2 2,2 8,8 8,8 2,2 2))')
    lyr.SetSpatialFilter(geom)

    assert lyr.TestCapability(ogr.OLCFastFeatureCount) is not False, \
        'OLCFastFeatureCount failed'
    assert lyr.TestCapability(ogr.OLCFastSpatialFilter) is not False, \
        'OLCFastSpatialFilter failed'

    assert lyr.GetFeatureCount() == 50, 'did not get expected feature count'

    # Test spatial filter with a SQL result layer without WHERE clause
    sql_lyr = ds.ExecuteSQL("SELECT * FROM 'test_spatialfilter'")

    extent = sql_lyr.GetExtent()
    assert extent == (0.0, 9.0, 0.0, 9.0), 'got bad extent'

    # Test caching
    extent = sql_lyr.GetExtent()
    assert extent == (0.0, 9.0, 0.0, 9.0), 'got bad extent'

    assert sql_lyr.TestCapability(ogr.OLCFastSpatialFilter) is not False, \
        'OLCFastSpatialFilter failed'
    sql_lyr.SetSpatialFilter(geom)
    assert sql_lyr.TestCapability(ogr.OLCFastSpatialFilter) is not False, \
        'OLCFastSpatialFilter failed'
    assert sql_lyr.GetFeatureCount() == 50, 'did not get expected feature count'
    ds.ReleaseResultSet(sql_lyr)

    # Test spatial filter with a SQL result layer with WHERE clause
    sql_lyr = ds.ExecuteSQL('SELECT * FROM test_spatialfilter WHERE 1=1')
    assert sql_lyr.TestCapability(ogr.OLCFastSpatialFilter) is not False, \
        'OLCFastSpatialFilter failed'
    sql_lyr.SetSpatialFilter(geom)
    assert sql_lyr.TestCapability(ogr.OLCFastSpatialFilter) is not False, \
        'OLCFastSpatialFilter failed'
    assert sql_lyr.GetFeatureCount() == 50, 'did not get expected feature count'
    ds.ReleaseResultSet(sql_lyr)

    # Test spatial filter with a SQL result layer with ORDER BY clause
    sql_lyr = ds.ExecuteSQL('SELECT * FROM test_spatialfilter ORDER BY intcol')

    extent = sql_lyr.GetExtent()
    assert extent == (0.0, 9.0, 0.0, 9.0), 'got bad extent'

    # Test caching
    extent = sql_lyr.GetExtent()
    assert extent == (0.0, 9.0, 0.0, 9.0), 'got bad extent'

    assert sql_lyr.TestCapability(ogr.OLCFastSpatialFilter) is not False, \
        'OLCFastSpatialFilter failed'
    sql_lyr.SetSpatialFilter(geom)
    assert sql_lyr.TestCapability(ogr.OLCFastSpatialFilter) is not False, \
        'OLCFastSpatialFilter failed'
    assert sql_lyr.GetFeatureCount() == 50, 'did not get expected feature count'
    ds.ReleaseResultSet(sql_lyr)

    # Test spatial filter with a SQL result layer with WHERE and ORDER BY clause
    sql_lyr = ds.ExecuteSQL('SELECT * FROM test_spatialfilter WHERE 1 = 1 ORDER BY intcol')

    extent = sql_lyr.GetExtent()
    assert extent == (0.0, 9.0, 0.0, 9.0), 'got bad extent'

    # Test caching
    extent = sql_lyr.GetExtent()
    assert extent == (0.0, 9.0, 0.0, 9.0), 'got bad extent'

    assert sql_lyr.TestCapability(ogr.OLCFastSpatialFilter) is not False, \
        'OLCFastSpatialFilter failed'
    sql_lyr.SetSpatialFilter(geom)
    assert sql_lyr.TestCapability(ogr.OLCFastSpatialFilter) is not False, \
        'OLCFastSpatialFilter failed'
    assert sql_lyr.GetFeatureCount() == 50, 'did not get expected feature count'
    ds.ReleaseResultSet(sql_lyr)

    # Remove spatial index
    ds = None
    ds = ogr.Open('tmp/spatialite_test.db', update=1)
    sql_lyr = ds.ExecuteSQL("SELECT DisableSpatialIndex('test_spatialfilter', 'Geometry')")
    sql_lyr.GetFeatureCount()
    feat = sql_lyr.GetNextFeature()
    ret = feat.GetFieldAsInteger(0)
    ds.ReleaseResultSet(sql_lyr)

    assert ret == 1, 'DisableSpatialIndex failed'

    ds.ExecuteSQL("VACUUM")

    ds.Destroy()

    # Test OLCFastFeatureCount without spatial index
    ds = ogr.Open('tmp/spatialite_test.db')
    lyr = ds.GetLayerByName('test_spatialfilter')

    geom = ogr.CreateGeometryFromWkt(
        'POLYGON((2 2,2 8,8 8,8 2,2 2))')
    lyr.SetSpatialFilter(geom)
    geom.Destroy()

    assert lyr.TestCapability(ogr.OLCFastFeatureCount) is not True
    assert lyr.TestCapability(ogr.OLCFastSpatialFilter) is not True

    assert lyr.GetFeatureCount() == 50

    ds.Destroy()

###############################################################################
# Test VirtualShape feature of SpatiaLite


def test_ogr_spatialite_3(require_spatialite):

    ds = ogr.Open('tmp/spatialite_test.db', update=1)
    ds.ExecuteSQL('CREATE VIRTUAL TABLE testpoly USING VirtualShape(data/shp/testpoly, CP1252, -1)')
    ds.Destroy()

    ds = ogr.Open('tmp/spatialite_test.db')
    lyr = ds.GetLayerByName('testpoly')
    assert lyr is not None

    lyr.SetSpatialFilterRect(-400, 22, -120, 400)

    tr = ogrtest.check_features_against_list(lyr, 'FID',
                                             [0, 4, 8])

    ds.Destroy()

    assert tr

###############################################################################
# Test updating a spatialite DB (#3471 and #3474)


def test_ogr_spatialite_4(require_spatialite):

    ds = ogr.Open('tmp/spatialite_test.db', update=1)

    lyr = ds.ExecuteSQL('SELECT * FROM sqlite_master')
    nb_sqlite_master_objects_before = lyr.GetFeatureCount()
    ds.ReleaseResultSet(lyr)

    lyr = ds.ExecuteSQL('SELECT * FROM idx_geomspatialite_GEOMETRY')
    nb_idx_before = lyr.GetFeatureCount()
    ds.ReleaseResultSet(lyr)

    lyr = ds.GetLayerByName('geomspatialite')
    lyr.CreateField(ogr.FieldDefn("foo", ogr.OFTString))

    lyr = ds.ExecuteSQL('SELECT * FROM geomspatialite')
    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    assert geom is not None and geom.ExportToWkt() == 'POINT (0 1)'
    feat.Destroy()
    ds.ReleaseResultSet(lyr)

    # Check that triggers and index are restored (#3474)
    lyr = ds.ExecuteSQL('SELECT * FROM sqlite_master')
    nb_sqlite_master_objects_after = lyr.GetFeatureCount()
    ds.ReleaseResultSet(lyr)

    assert nb_sqlite_master_objects_before == nb_sqlite_master_objects_after, \
        ('nb_sqlite_master_objects_before=%d, nb_sqlite_master_objects_after=%d' % (nb_sqlite_master_objects_before, nb_sqlite_master_objects_after))

    # Add new feature
    lyr = ds.GetLayerByName('geomspatialite')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT(100 -100)'))
    lyr.CreateFeature(feat)
    feat.Destroy()

    # Check that the trigger is functional (#3474).
    lyr = ds.ExecuteSQL('SELECT * FROM idx_geomspatialite_GEOMETRY')
    nb_idx_after = lyr.GetFeatureCount()
    ds.ReleaseResultSet(lyr)

    assert nb_idx_before + 1 == nb_idx_after, \
        ('nb_idx_before=%d, nb_idx_after=%d' % (nb_idx_before, nb_idx_after))


###############################################################################
# Test writing and reading back spatialite geometries (#4092)
# Test writing and reading back spatialite geometries in compressed form


@pytest.mark.parametrize(
    'bUseComprGeom',
    [False, True],
    ids=['dont-compress-geometries', 'compress-geometries']
)
def test_ogr_spatialite_5(require_spatialite, bUseComprGeom):
    if bUseComprGeom and require_spatialite == '2.3.1':
        pytest.skip()

    try:
        os.remove('tmp/ogr_spatialite_5.sqlite')
    except OSError:
        pass
    ds = ogr.GetDriverByName('SQLite').CreateDataSource('tmp/ogr_spatialite_5.sqlite', options=['SPATIALITE=YES'])

    geometries = [
        # 'POINT EMPTY',
        'POINT (1 2)',
        'POINT Z (1 2 3)',
        'POINT M (1 2 3)',
        'POINT ZM (1 2 3 4)',
        'LINESTRING EMPTY',
        'LINESTRING (1 2)',
        'LINESTRING (1 2,3 4)',
        'LINESTRING (1 2,3 4,5 6)',
        'LINESTRING Z (1 2 3,4 5 6)',
        'LINESTRING Z (1 2 3,4 5 6,7 8 9)',
        'LINESTRING M (1 2 3,4 5 6)',
        'LINESTRING M (1 2 3,4 5 6,7 8 9)',
        'LINESTRING ZM (1 2 3 4,5 6 7 8)',
        'LINESTRING ZM (1 2 3 4,5 6 7 8,9 10 11 12)',
        'POLYGON EMPTY',
        'POLYGON ((1 2,1 3,2 3,2 2,1 2))',
        'POLYGON Z ((1 2 10,1 3 -10,2 3 20,2 2 -20,1 2 10))',
        'POLYGON M ((1 2 10,1 3 -10,2 3 20,2 2 -20,1 2 10))',
        'POLYGON ZM ((1 2 10 20,1 3 -10 -20,2 3 20 30,2 2 -20 -30,1 2 10 20))',
        'POLYGON ((1 2,1 3,2 3,2 2,1 2),(1.25 2.25,1.25 2.75,1.75 2.75,1.75 2.25,1.25 2.25))',
        'MULTIPOINT EMPTY',
        'MULTIPOINT ((1 2),(3 4))',
        'MULTIPOINT Z ((1 2 3),(4 5 6))',
        'MULTIPOINT M ((1 2 3),(4 5 6))',
        'MULTIPOINT ZM ((1 2 3 4),(5 6 7 8))',
        'MULTILINESTRING EMPTY',
        'MULTILINESTRING ((1 2,3 4),(5 6,7 8))',
        'MULTILINESTRING Z ((1 2 3,4 5 6),(7 8 9,10 11 12))',
        'MULTILINESTRING M ((1 2 3,4 5 6),(7 8 9,10 11 12))',
        'MULTILINESTRING ZM ((1 2 3 4,5 6 7 8),(9 10 11 12,13 14 15 16))',
        'MULTIPOLYGON EMPTY',
        'MULTIPOLYGON (((1 2,1 3,2 3,2 2,1 2)),((-1 -2,-1 -3,-2 -3,-2 -2,-1 -2)))',
        'MULTIPOLYGON (((1 2,1 3,2 3,2 2,1 2),(1.25 2.25,1.25 2.75,1.75 2.75,1.75 2.25,1.25 2.25)),((-1 -2,-1 -3,-2 -3,-2 -2,-1 -2)))',
        'MULTIPOLYGON Z (((1 2 -4,1 3 -3,2 3 -3,2 2 -3,1 2 -6)),((-1 -2 0,-1 -3 0,-2 -3 0,-2 -2 0,-1 -2 0)))',
        'MULTIPOLYGON M (((1 2 -4,1 3 -3,2 3 -3,2 2 -3,1 2 -6)),((-1 -2 0,-1 -3 0,-2 -3 0,-2 -2 0,-1 -2 0)))',
        'MULTIPOLYGON ZM (((1 2 -4 -40,1 3 -3 -30,2 3 -3 -30,2 2 -3 30,1 2 -6 -60)),((-1 -2 0 0,-1 -3 0 0,-2 -3 0 0,-2 -2 0 0,-1 -2 0 0)))',
        'GEOMETRYCOLLECTION EMPTY',
        # 'GEOMETRYCOLLECTION (GEOMETRYCOLLECTION EMPTY)',
        'GEOMETRYCOLLECTION (POINT (1 2))',
        'GEOMETRYCOLLECTION Z (POINT Z (1 2 3))',
        'GEOMETRYCOLLECTION M (POINT M (1 2 3))',
        'GEOMETRYCOLLECTION ZM (POINT ZM (1 2 3 4))',
        'GEOMETRYCOLLECTION (LINESTRING (1 2,3 4))',
        'GEOMETRYCOLLECTION Z (LINESTRING Z (1 2 3,4 5 6))',
        'GEOMETRYCOLLECTION (POLYGON ((1 2,1 3,2 3,2 2,1 2)))',
        'GEOMETRYCOLLECTION Z (POLYGON Z ((1 2 10,1 3 -10,2 3 20,2 2 -20,1 2 10)))',
        'GEOMETRYCOLLECTION (POINT (1 2),LINESTRING (1 2,3 4),POLYGON ((1 2,1 3,2 3,2 2,1 2)))',
        'GEOMETRYCOLLECTION Z (POINT Z (1 2 3),LINESTRING Z (1 2 3,4 5 6),POLYGON Z ((1 2 10,1 3 -10,2 3 20,2 2 -20,1 2 10)))',
    ]

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)

    num_layer = 0
    for wkt in geometries:
        # print(wkt)
        geom = ogr.CreateGeometryFromWkt(wkt)
        if bUseComprGeom:
            options = ['COMPRESS_GEOM=YES']
        else:
            options = []
        lyr = ds.CreateLayer('test%d' % num_layer, geom_type=geom.GetGeometryType(), srs=srs, options=options)
        feat = ogr.Feature(lyr.GetLayerDefn())
        # print(geom)
        feat.SetGeometry(geom)
        lyr.CreateFeature(feat)
        num_layer = num_layer + 1

    ds = None

    ds = ogr.Open('tmp/ogr_spatialite_5.sqlite')
    num_layer = 0
    for wkt in geometries:
        geom = ogr.CreateGeometryFromWkt(wkt)
        lyr = ds.GetLayer(num_layer)
        assert lyr.GetGeomType() == geom.GetGeometryType()
        feat = lyr.GetNextFeature()
        got_wkt = feat.GetGeometryRef().ExportToIsoWkt()
        # Spatialite < 2.4 only supports 2D geometries
        if gdaltest.spatialite_version == '2.3.1' and (geom.GetGeometryType() & ogr.wkb25DBit) != 0:
            geom.SetCoordinateDimension(2)
            expected_wkt = geom.ExportToIsoWkt()
            assert got_wkt == expected_wkt
        elif got_wkt != wkt:
            pytest.fail('got %s, expected %s' % (got_wkt, wkt))

        num_layer = num_layer + 1

    if bUseComprGeom:
        num_layer = 0
        for wkt in geometries:
            if wkt.find('EMPTY') == -1 and wkt.find('POINT') == -1:
                sql_lyr = ds.ExecuteSQL("SELECT GEOMETRY == CompressGeometry(GEOMETRY) FROM test%d" % num_layer)
                feat = sql_lyr.GetNextFeature()
                val = feat.GetFieldAsInteger(0)
                if wkt != 'LINESTRING (1 2)':
                    if val != 1:
                        print(wkt)
                        print(val)
                        ds.ReleaseResultSet(sql_lyr)
                        pytest.fail('did not get expected compressed geometry')
                else:
                    if val != 0:
                        print(val)
                        ds.ReleaseResultSet(sql_lyr)
                        pytest.fail(wkt)
                feat = None
                ds.ReleaseResultSet(sql_lyr)
            num_layer = num_layer + 1

    ds = None


###############################################################################
# Test spatialite spatial views


def test_ogr_spatialite_6(require_spatialite):
    if gdaltest.spatialite_version.startswith('2.3'):
        pytest.skip()

    try:
        os.remove('tmp/ogr_spatialite_6.sqlite')
    except OSError:
        pass
    ds = ogr.GetDriverByName('SQLite').CreateDataSource('tmp/ogr_spatialite_6.sqlite', options=['SPATIALITE=YES'])

    if int(gdaltest.spatialite_version[0:gdaltest.spatialite_version.find('.')]) >= 4:
        layername = 'regular_layer'
        layername_single = 'regular_layer'
        viewname = 'view_of_regular_layer'
        viewname_single = 'view_of_regular_layer'
        thegeom_single = 'the_geom'
        pkid_single = 'pk_id'
    else:
        layername = 'regular_\'layer'
        layername_single = 'regular_\'\'layer'
        viewname = 'view_of_\'regular_layer'
        viewname_single = 'view_of_\'\'regular_layer'
        thegeom_single = 'the_"''geom'
        pkid_single = 'pk_"''id'

    # Create regular layer
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    lyr = ds.CreateLayer(layername, geom_type=ogr.wkbPoint, srs=srs, options=['LAUNDER=NO'])

    geometryname = lyr.GetGeometryColumn()

    lyr.CreateField(ogr.FieldDefn("int'col", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("realcol", ogr.OFTReal))

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 12)
    feat.SetField(1, 34.56)
    geom = ogr.CreateGeometryFromWkt('POINT(2 49)')
    feat.SetGeometryDirectly(geom)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 12)
    feat.SetField(1, 34.56)
    geom = ogr.CreateGeometryFromWkt('POINT(3 50)')
    feat.SetGeometryDirectly(geom)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 34)
    feat.SetField(1, 56.78)
    geom = ogr.CreateGeometryFromWkt('POINT(-30000 -50000)')
    feat.SetGeometryDirectly(geom)
    lyr.CreateFeature(feat)
    geom = ogr.CreateGeometryFromWkt('POINT(3 50)')
    feat.SetGeometryDirectly(geom)
    lyr.SetFeature(feat)

    # Create spatial view
    ds.ExecuteSQL("CREATE VIEW \"%s\" AS SELECT OGC_FID AS '%s', %s AS '%s', \"int'col\", realcol FROM \"%s\"" % (viewname, pkid_single, geometryname, thegeom_single, layername))

    if int(gdaltest.spatialite_version[0:gdaltest.spatialite_version.find('.')]) >= 4:
        ds.ExecuteSQL("INSERT INTO views_geometry_columns(view_name, view_geometry, view_rowid, f_table_name, f_geometry_column, read_only) VALUES " +
                      "('%s', '%s', '%s', '%s', Lower('%s'), 1)" % (viewname_single, thegeom_single, pkid_single, layername_single, geometryname))
    else:
        ds.ExecuteSQL("INSERT INTO views_geometry_columns(view_name, view_geometry, view_rowid, f_table_name, f_geometry_column) VALUES " +
                      "('%s', '%s', '%s', '%s', '%s')" % (viewname_single, thegeom_single, pkid_single, layername_single, geometryname))

    ds = None

    # Test spatial view
    ds = ogr.Open('tmp/ogr_spatialite_6.sqlite')
    lyr = ds.GetLayerByName(layername)
    view_lyr = ds.GetLayerByName(viewname)
    assert view_lyr.GetFIDColumn() == pkid_single, view_lyr.GetGeometryColumn()
    assert view_lyr.GetGeometryColumn() == thegeom_single
    assert view_lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "int'col"
    assert view_lyr.GetGeomType() == lyr.GetGeomType()
    assert view_lyr.GetFeatureCount() == lyr.GetFeatureCount()
    assert view_lyr.GetSpatialRef().IsSame(lyr.GetSpatialRef()) == 1
    feat = view_lyr.GetFeature(3)
    if feat.GetFieldAsInteger(0) != 34:
        feat.DumpReadable()
        pytest.fail()
    if feat.GetFieldAsDouble(1) != 56.78:
        feat.DumpReadable()
        pytest.fail()
    view_lyr.SetAttributeFilter('"int\'col" = 34')
    view_lyr.SetSpatialFilterRect(2.5, 49.5, 3.5, 50.5)
    feat = view_lyr.GetNextFeature()
    if feat.GetFID() != 3:
        feat.DumpReadable()
        pytest.fail()
    if feat.GetGeometryRef().ExportToWkt() != 'POINT (3 50)':
        feat.DumpReadable()
        pytest.fail()
    ds = None

    # Remove spatial index
    ds = ogr.Open('tmp/ogr_spatialite_6.sqlite', update=1)
    sql_lyr = ds.ExecuteSQL("SELECT DisableSpatialIndex('%s', '%s')" % (layername_single, geometryname))
    ds.ReleaseResultSet(sql_lyr)
    ds.ExecuteSQL("DROP TABLE \"idx_%s_%s\"" % (layername, geometryname))
    ds = None

    # Test spatial view again
    ds = ogr.Open('tmp/ogr_spatialite_6.sqlite')
    view_lyr = ds.GetLayerByName(viewname)
    view_lyr.SetAttributeFilter('"int\'col" = 34')
    view_lyr.SetSpatialFilterRect(2.5, 49.5, 3.5, 50.5)
    feat = view_lyr.GetNextFeature()
    if feat.GetFID() != 3:
        feat.DumpReadable()
        pytest.fail()
    ds = None

###############################################################################
# Test VirtualShape:xxx.shp


def test_ogr_spatialite_7(require_spatialite):
    ds = ogr.Open('VirtualShape:data/poly.shp')
    assert ds is not None

    lyr = ds.GetLayerByName('poly')
    assert lyr is not None

    assert lyr.GetGeomType() == ogr.wkbPolygon

    f = lyr.GetNextFeature()
    assert f.GetGeometryRef() is not None

###############################################################################
# Test tables with multiple geometry columns (#4768)


def test_ogr_spatialite_8(require_spatialite):
    if require_spatialite.startswith('2.3'):
        pytest.skip()

    try:
        os.remove('tmp/ogr_spatialite_8.sqlite')
    except OSError:
        pass
    ds = ogr.GetDriverByName('SQLite').CreateDataSource('tmp/ogr_spatialite_8.sqlite', options=['SPATIALITE=YES'])
    lyr = ds.CreateLayer('test', geom_type=ogr.wkbNone)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    fld = ogr.GeomFieldDefn('geom1', ogr.wkbPoint)
    fld.SetSpatialRef(srs)
    lyr.CreateGeomField(fld)
    fld = ogr.GeomFieldDefn('geom2', ogr.wkbLineString)
    fld.SetSpatialRef(srs)
    lyr.CreateGeomField(fld)
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('foo', 'bar')
    f.SetGeomFieldDirectly(0, ogr.CreateGeometryFromWkt('POINT(0 -1)'))
    f.SetGeomFieldDirectly(1, ogr.CreateGeometryFromWkt('LINESTRING(0 -1,2 3)'))
    lyr.CreateFeature(f)
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f.GetGeomFieldRef('geom1').ExportToWkt() != 'POINT (0 -1)' or \
       f.GetGeomFieldRef('geom2').ExportToWkt() != 'LINESTRING (0 -1,2 3)':
        f.DumpReadable()
        pytest.fail()
    f.SetGeomFieldDirectly(0, ogr.CreateGeometryFromWkt('POINT(0 1)'))
    f.SetGeomFieldDirectly(1, ogr.CreateGeometryFromWkt('LINESTRING(0 1,2 3)'))
    lyr.SetFeature(f)
    f = None
    ds.ExecuteSQL('CREATE VIEW view_test_geom1 AS SELECT OGC_FID AS pk_id, foo, geom1 AS renamed_geom1 FROM test')

    if int(gdaltest.spatialite_version[0:gdaltest.spatialite_version.find('.')]) >= 4:
        readonly_col = ', read_only'
        readonly_val = ', 1'
    else:
        readonly_col = ''
        readonly_val = ''

    ds.ExecuteSQL(("INSERT INTO views_geometry_columns(view_name, view_geometry, view_rowid, f_table_name, f_geometry_column%s) VALUES " % readonly_col) +
                  ("('view_test_geom1', 'renamed_geom1', 'pk_id', 'test', 'geom1'%s)" % readonly_val))
    ds.ExecuteSQL('CREATE VIEW view_test_geom2 AS SELECT OGC_FID AS pk_id, foo, geom2 AS renamed_geom2 FROM test')
    ds.ExecuteSQL(("INSERT INTO views_geometry_columns(view_name, view_geometry, view_rowid, f_table_name, f_geometry_column%s) VALUES " % readonly_col) +
                  ("('view_test_geom2', 'renamed_geom2', 'pk_id', 'test', 'geom2'%s)" % readonly_val))
    ds = None

    ds = ogr.Open('tmp/ogr_spatialite_8.sqlite')

    lyr = ds.GetLayerByName('test(geom1)')
    view_lyr = ds.GetLayerByName('view_test_geom1')
    assert lyr.GetLayerDefn().GetFieldCount() == 1
    assert view_lyr.GetLayerDefn().GetFieldCount() == 1
    assert lyr.GetGeometryColumn() == 'geom1'
    assert view_lyr.GetGeometryColumn() == 'renamed_geom1'
    assert lyr.GetGeomType() == ogr.wkbPoint
    assert view_lyr.GetGeomType() == lyr.GetGeomType()
    assert view_lyr.GetFeatureCount() == lyr.GetFeatureCount()
    feat = view_lyr.GetFeature(1)
    if feat.GetFieldAsString(0) != 'bar':
        feat.DumpReadable()
        pytest.fail()
    feat = None
    view_lyr.SetSpatialFilterRect(-1, -1, 10, 10)
    feat = view_lyr.GetNextFeature()
    if feat.GetFID() != 1:
        feat.DumpReadable()
        pytest.fail()
    if feat.GetGeometryRef().ExportToWkt() != 'POINT (0 1)':
        feat.DumpReadable()
        pytest.fail()
    feat = None

    lyr = ds.GetLayerByName('test(geom2)')
    view_lyr = ds.GetLayerByName('view_test_geom2')
    assert lyr.GetLayerDefn().GetFieldCount() == 1
    assert view_lyr.GetLayerDefn().GetFieldCount() == 1
    assert lyr.GetGeometryColumn() == 'geom2'
    assert view_lyr.GetGeometryColumn() == 'renamed_geom2'
    assert lyr.GetGeomType() == ogr.wkbLineString
    assert view_lyr.GetGeomType() == lyr.GetGeomType()
    assert view_lyr.GetFeatureCount() == lyr.GetFeatureCount()
    feat = view_lyr.GetFeature(1)
    if feat.GetFieldAsString(0) != 'bar':
        feat.DumpReadable()
        pytest.fail()
    feat = None
    view_lyr.SetSpatialFilterRect(-1, -1, 10, 10)
    feat = view_lyr.GetNextFeature()
    if feat.GetFID() != 1:
        feat.DumpReadable()
        pytest.fail()
    if feat.GetGeometryRef().ExportToWkt() != 'LINESTRING (0 1,2 3)':
        feat.DumpReadable()
        pytest.fail()
    feat = None

    sql_lyr = ds.ExecuteSQL('SELECT foo, geom2 FROM test')
    sql_lyr.SetSpatialFilterRect(-1, -1, 10, 10)
    feat = sql_lyr.GetNextFeature()
    if feat.GetGeometryRef().ExportToWkt() != 'LINESTRING (0 1,2 3)':
        feat.DumpReadable()
        pytest.fail()
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    with gdaltest.error_handler():
        lyr = ds.GetLayerByName('invalid_layer_name(geom1)')
    assert lyr is None

    lyr = ds.GetLayerByName('test')
    assert lyr.GetLayerDefn().GetFieldCount() == 1
    assert lyr.GetLayerDefn().GetGeomFieldCount() == 2
    assert lyr.GetLayerDefn().GetGeomFieldDefn(0).GetName() == 'geom1'
    assert lyr.GetLayerDefn().GetGeomFieldDefn(1).GetName() == 'geom2'
    assert lyr.GetLayerDefn().GetGeomFieldDefn(0).GetType() == ogr.wkbPoint
    assert lyr.GetLayerDefn().GetGeomFieldDefn(1).GetType() == ogr.wkbLineString
    lyr.SetSpatialFilterRect(-1, -1, 10, 10)
    feat = lyr.GetNextFeature()
    if feat.GetFID() != 1:
        feat.DumpReadable()
        pytest.fail()
    if feat.GetGeomFieldRef(0).ExportToWkt() != 'POINT (0 1)':
        feat.DumpReadable()
        pytest.fail()
    if feat.GetGeomFieldRef(1).ExportToWkt() != 'LINESTRING (0 1,2 3)':
        feat.DumpReadable()
        pytest.fail()
    feat = None

    lyr.SetSpatialFilterRect(1, -1, -1, 10, 10)
    feat = lyr.GetNextFeature()
    if feat.GetFID() != 1:
        feat.DumpReadable()
        pytest.fail()
    feat = None

    ds = None

###############################################################################
# Test tables with multiple geometry columns (#4768)


def test_ogr_sqlite_31():

    if gdaltest.sl_ds is None:
        pytest.skip()

    try:
        os.remove('tmp/ogr_sqlite_31.sqlite')
    except OSError:
        pass
    ds = ogr.GetDriverByName('SQLite').CreateDataSource('tmp/ogr_sqlite_31.sqlite')
    lyr = ds.CreateLayer('test', geom_type=ogr.wkbNone)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    fld = ogr.GeomFieldDefn('geom1', ogr.wkbPoint)
    fld.SetSpatialRef(srs)
    lyr.CreateGeomField(fld)
    fld = ogr.GeomFieldDefn('geom2', ogr.wkbLineString)
    fld.SetSpatialRef(srs)
    lyr.CreateGeomField(fld)
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('foo', 'bar')
    f.SetGeomFieldDirectly(0, ogr.CreateGeometryFromWkt('POINT(0 1)'))
    f.SetGeomFieldDirectly(1, ogr.CreateGeometryFromWkt('LINESTRING(0 1,2 3)'))
    lyr.CreateFeature(f)
    f = None
    ds = None

    ds = ogr.Open('tmp/ogr_sqlite_31.sqlite')

    lyr = ds.GetLayerByName('test(geom1)')
    assert lyr.GetLayerDefn().GetFieldCount() == 1
    assert lyr.GetGeometryColumn() == 'geom1'
    assert lyr.GetGeomType() == ogr.wkbPoint
    lyr.SetSpatialFilterRect(-1, -1, 10, 10)
    feat = lyr.GetNextFeature()
    if feat.GetFID() != 1:
        feat.DumpReadable()
        pytest.fail()
    if feat.GetGeometryRef().ExportToWkt() != 'POINT (0 1)':
        feat.DumpReadable()
        pytest.fail()
    feat = None

    lyr = ds.GetLayerByName('test(geom2)')
    assert lyr.GetLayerDefn().GetFieldCount() == 1
    assert lyr.GetGeometryColumn() == 'geom2'
    assert lyr.GetGeomType() == ogr.wkbLineString
    lyr.SetSpatialFilterRect(-1, -1, 10, 10)
    feat = lyr.GetNextFeature()
    if feat.GetFID() != 1:
        feat.DumpReadable()
        pytest.fail()
    if feat.GetGeometryRef().ExportToWkt() != 'LINESTRING (0 1,2 3)':
        feat.DumpReadable()
        pytest.fail()
    feat = None

    ds = None

###############################################################################
# Test datetime support


def test_ogr_sqlite_32():

    if gdaltest.sl_ds is None:
        pytest.skip()

    try:
        os.remove('tmp/ogr_sqlite_32.sqlite')
    except OSError:
        pass
    ds = ogr.GetDriverByName('SQLite').CreateDataSource('tmp/ogr_sqlite_32.sqlite')
    lyr = ds.CreateLayer('test')
    field_defn = ogr.FieldDefn('datetimefield', ogr.OFTDateTime)
    lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn('datefield', ogr.OFTDate)
    lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn('timefield', ogr.OFTTime)
    lyr.CreateField(field_defn)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('datetimefield', '2012/08/23 21:24:00  ')
    feat.SetField('datefield', '2012/08/23  ')
    feat.SetField('timefield', '21:24:00  ')
    lyr.CreateFeature(feat)
    feat = None

    ds = None

    ds = ogr.Open('tmp/ogr_sqlite_32.sqlite')
    lyr = ds.GetLayer(0)

    assert lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTDateTime
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetType() == ogr.OFTDate
    assert lyr.GetLayerDefn().GetFieldDefn(2).GetType() == ogr.OFTTime

    feat = lyr.GetNextFeature()
    if feat.GetField('datetimefield') != '2012/08/23 21:24:00' or \
            feat.GetField('datefield') != '2012/08/23' or \
            feat.GetField('timefield') != '21:24:00':
        feat.DumpReadable()
        pytest.fail()
    feat = None

    ds = None

###############################################################################
# Test SRID layer creation option


def test_ogr_sqlite_33(with_and_without_spatialite):
    if gdaltest.sl_ds is None:
        pytest.skip()

    try:
        os.remove('tmp/ogr_sqlite_33.sqlite')
    except OSError:
        pass
    if not with_and_without_spatialite:
        options = []
    else:
        if gdaltest.spatialite_version.find('2.3') == 0:
            return
        options = ['SPATIALITE=YES']

    ds = ogr.GetDriverByName('SQLite').CreateDataSource('tmp/ogr_sqlite_33.sqlite', options=options)

    if not with_and_without_spatialite:
        # To make sure that the entry is added in spatial_ref_sys
        srs = osr.SpatialReference()
        srs.ImportFromEPSG(4326)
        lyr = ds.CreateLayer('test1', srs=srs)

    # Test with existing entry
    lyr = ds.CreateLayer('test2', options=['SRID=4326'])

    # Test with non-existing entry
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr = ds.CreateLayer('test3', options=['SRID=123456'])
    gdal.PopErrorHandler()
    ds = None

    ds = ogr.Open('tmp/ogr_sqlite_33.sqlite')
    lyr = ds.GetLayerByName('test2')
    srs = lyr.GetSpatialRef()
    if srs.ExportToWkt().find('4326') == -1:
        pytest.fail('failure')

    # 123456 should be referenced in geometry_columns...
    sql_lyr = ds.ExecuteSQL('SELECT * from geometry_columns WHERE srid=123456')
    feat = sql_lyr.GetNextFeature()
    is_none = feat is None
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    assert not is_none

    # ... but not in spatial_ref_sys
    sql_lyr = ds.ExecuteSQL('SELECT * from spatial_ref_sys WHERE srid=123456')
    feat = sql_lyr.GetNextFeature()
    is_none = feat is None
    feat = None
    ds.ReleaseResultSet(sql_lyr)

    assert is_none


###############################################################################
# Test REGEXP support (#4823)


def test_ogr_sqlite_34():

    if gdaltest.sl_ds is None:
        pytest.skip()

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    sql_lyr = gdaltest.sl_ds.ExecuteSQL("SELECT 'a' REGEXP 'a'")
    gdal.PopErrorHandler()
    if sql_lyr is None:
        pytest.skip()
    feat = sql_lyr.GetNextFeature()
    val = feat.GetField(0)
    gdaltest.sl_ds.ReleaseResultSet(sql_lyr)
    assert val == 1

    # Evaluates to FALSE
    sql_lyr = gdaltest.sl_ds.ExecuteSQL("SELECT 'b' REGEXP 'a'")
    feat = sql_lyr.GetNextFeature()
    val = feat.GetField(0)
    gdaltest.sl_ds.ReleaseResultSet(sql_lyr)
    assert val == 0

    # NULL left-member
    sql_lyr = gdaltest.sl_ds.ExecuteSQL("SELECT NULL REGEXP 'a'")
    feat = sql_lyr.GetNextFeature()
    val = feat.GetField(0)
    gdaltest.sl_ds.ReleaseResultSet(sql_lyr)
    assert val == 0

    # NULL regexp
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    sql_lyr = gdaltest.sl_ds.ExecuteSQL("SELECT 'a' REGEXP NULL")
    gdal.PopErrorHandler()
    assert sql_lyr is None

    # Invalid regexp
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    sql_lyr = gdaltest.sl_ds.ExecuteSQL("SELECT 'a' REGEXP '['")
    gdal.PopErrorHandler()
    assert sql_lyr is None

    # Adds another pattern
    sql_lyr = gdaltest.sl_ds.ExecuteSQL("SELECT 'b' REGEXP 'b'")
    feat = sql_lyr.GetNextFeature()
    val = feat.GetField(0)
    gdaltest.sl_ds.ReleaseResultSet(sql_lyr)
    assert val == 1

    # Test cache
    for _ in range(2):
        for i in range(17):
            regexp = chr(ord('a') + i)
            sql_lyr = gdaltest.sl_ds.ExecuteSQL("SELECT '%s' REGEXP '%s'" % (regexp, regexp))
            feat = sql_lyr.GetNextFeature()
            val = feat.GetField(0)
            gdaltest.sl_ds.ReleaseResultSet(sql_lyr)
            assert val == 1


###############################################################################
# Test SetAttributeFilter() on SQL result layer


def test_ogr_sqlite_35(with_and_without_spatialite):

    if gdaltest.sl_ds is None:
        pytest.skip()

    if with_and_without_spatialite:
        if gdaltest.spatialite_version.find('2.3') >= 0:
            pytest.skip()
        options = ['SPATIALITE=YES']
    else:
        options = []

    try:
        os.remove('tmp/ogr_sqlite_35.sqlite')
    except OSError:
        pass

    ds = ogr.GetDriverByName('SQLite').CreateDataSource('tmp/ogr_sqlite_35.sqlite', options=options)
    lyr = ds.CreateLayer('test')
    field_defn = ogr.FieldDefn('foo', ogr.OFTString)
    lyr.CreateField(field_defn)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('foo', 'bar')
    feat.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 1)"))
    lyr.CreateFeature(feat)
    feat = None

    for sql in ["SELECT * FROM test",
                "SELECT * FROM test GROUP BY foo",
                "SELECT * FROM test ORDER BY foo",
                "SELECT * FROM test LIMIT 1",
                "SELECT * FROM test WHERE 1=1",
                "SELECT * FROM test WHERE 1=1 GROUP BY foo",
                "SELECT * FROM test WHERE 1=1 ORDER BY foo",
                "SELECT * FROM test WHERE 1=1 LIMIT 1"]:
        sql_lyr = ds.ExecuteSQL(sql)

        sql_lyr.SetAttributeFilter("foo = 'bar'")
        sql_lyr.ResetReading()
        feat = sql_lyr.GetNextFeature()
        assert feat is not None
        feat = None

        sql_lyr.SetAttributeFilter("foo = 'baz'")
        sql_lyr.ResetReading()
        feat = sql_lyr.GetNextFeature()
        assert feat is None
        feat = None

        sql_lyr.SetAttributeFilter(None)
        sql_lyr.ResetReading()
        feat = sql_lyr.GetNextFeature()
        assert feat is not None
        feat = None

        sql_lyr.SetSpatialFilterRect(0, 0, 2, 2)
        sql_lyr.SetAttributeFilter("foo = 'bar'")
        sql_lyr.ResetReading()
        feat = sql_lyr.GetNextFeature()
        assert feat is not None
        feat = None

        sql_lyr.SetSpatialFilterRect(1.5, 1.5, 2, 2)
        sql_lyr.SetAttributeFilter("foo = 'bar'")
        sql_lyr.ResetReading()
        feat = sql_lyr.GetNextFeature()
        assert feat is None
        feat = None

        sql_lyr.SetSpatialFilterRect(0, 0, 2, 2)
        sql_lyr.SetAttributeFilter(None)
        sql_lyr.ResetReading()
        feat = sql_lyr.GetNextFeature()
        assert feat is not None
        feat = None

        ds.ReleaseResultSet(sql_lyr)

    ds = None

###############################################################################
# Test FID64 support


def test_ogr_sqlite_36():

    if gdaltest.sl_ds is None:
        pytest.skip()

    try:
        os.remove('tmp/ogr_sqlite_36.sqlite')
    except OSError:
        pass

    ds = ogr.GetDriverByName('SQLite').CreateDataSource('tmp/ogr_sqlite_36.sqlite')
    lyr = ds.CreateLayer('test')
    field_defn = ogr.FieldDefn('foo', ogr.OFTString)
    lyr.CreateField(field_defn)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('foo', 'bar')
    feat.SetFID(1234567890123)
    lyr.CreateFeature(feat)
    feat = None

    ds = None

    ds = ogr.Open('tmp/ogr_sqlite_36.sqlite')
    lyr = ds.GetLayer(0)
    assert lyr.GetMetadataItem('') is None

    ds = ogr.Open('tmp/ogr_sqlite_36.sqlite')
    lyr = ds.GetLayer(0)
    assert lyr.GetMetadataItem(ogr.OLMD_FID64) is not None
    f = lyr.GetNextFeature()
    assert f.GetFID() == 1234567890123

    ds = ogr.Open('tmp/ogr_sqlite_36.sqlite')
    lyr = ds.GetLayer(0)
    assert ogr.OLMD_FID64 in lyr.GetMetadata()

###############################################################################
# Test not nullable fields


def test_ogr_sqlite_37():

    if gdaltest.sl_ds is None:
        pytest.skip()

    try:
        os.remove('tmp/ogr_sqlite_37.sqlite')
    except OSError:
        pass

    ds = ogr.GetDriverByName('SQLite').CreateDataSource('tmp/ogr_sqlite_37.sqlite')
    lyr = ds.CreateLayer('test', geom_type=ogr.wkbNone)
    field_defn = ogr.FieldDefn('field_not_nullable', ogr.OFTString)
    field_defn.SetNullable(0)
    lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn('field_nullable', ogr.OFTString)
    lyr.CreateField(field_defn)
    field_defn = ogr.GeomFieldDefn('geomfield_not_nullable', ogr.wkbPoint)
    field_defn.SetNullable(0)
    lyr.CreateGeomField(field_defn)
    field_defn = ogr.GeomFieldDefn('geomfield_nullable', ogr.wkbPoint)
    lyr.CreateGeomField(field_defn)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('field_not_nullable', 'not_null')
    f.SetGeomFieldDirectly('geomfield_not_nullable', ogr.CreateGeometryFromWkt('POINT(0 0)'))
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
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT(0 0)'))
    gdal.PushErrorHandler()
    ret = lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    assert ret != 0
    f = None

    ds = None

    ds = ogr.Open('tmp/ogr_sqlite_37.sqlite', update=1)
    lyr = ds.GetLayerByName('test')
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_not_nullable')).IsNullable() == 0
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_nullable')).IsNullable() == 1
    assert lyr.GetLayerDefn().GetGeomFieldDefn(lyr.GetLayerDefn().GetGeomFieldIndex('geomfield_not_nullable')).IsNullable() == 0
    assert lyr.GetLayerDefn().GetGeomFieldDefn(lyr.GetLayerDefn().GetGeomFieldIndex('geomfield_nullable')).IsNullable() == 1

    # Turn not null into nullable
    src_fd = lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_not_nullable'))
    fd = ogr.FieldDefn('now_nullable', src_fd.GetType())
    fd.SetNullable(1)
    lyr.AlterFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_not_nullable'), fd, ogr.ALTER_ALL_FLAG)
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('now_nullable')).IsNullable() == 1

    # Turn nullable into not null, but remove NULL values first
    ds.ExecuteSQL("UPDATE test SET field_nullable = '' WHERE field_nullable IS NULL")
    src_fd = lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_nullable'))
    fd = ogr.FieldDefn('now_nullable', src_fd.GetType())
    fd.SetName('now_not_nullable')
    fd.SetNullable(0)
    lyr.AlterFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_nullable'), fd, ogr.ALTER_ALL_FLAG)
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('now_not_nullable')).IsNullable() == 0

    ds = None

    ds = ogr.Open('tmp/ogr_sqlite_37.sqlite')
    lyr = ds.GetLayerByName('test')
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('now_not_nullable')).IsNullable() == 0
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('now_nullable')).IsNullable() == 1
    assert lyr.GetLayerDefn().GetGeomFieldDefn(lyr.GetLayerDefn().GetGeomFieldIndex('geomfield_not_nullable')).IsNullable() == 0
    assert lyr.GetLayerDefn().GetGeomFieldDefn(lyr.GetLayerDefn().GetGeomFieldIndex('geomfield_nullable')).IsNullable() == 1
    ds = None

###############################################################################
# Test  default values


def test_ogr_sqlite_38():

    if gdaltest.sl_ds is None:
        pytest.skip()

    try:
        os.remove('tmp/ogr_sqlite_38.sqlite')
    except OSError:
        pass

    ds = ogr.GetDriverByName('SQLite').CreateDataSource('tmp/ogr_sqlite_38.sqlite')
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

    field_defn = ogr.FieldDefn('field_datetime3', ogr.OFTDateTime)
    field_defn.SetDefault("(strftime('%Y-%m-%dT%H:%M:%fZ','now'))")
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn('field_datetime4', ogr.OFTDateTime)
    field_defn.SetDefault("'2015/06/30 12:34:56.123'")
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn('field_date', ogr.OFTDate)
    field_defn.SetDefault("CURRENT_DATE")
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn('field_time', ogr.OFTTime)
    field_defn.SetDefault("CURRENT_TIME")
    lyr.CreateField(field_defn)

    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    f = None

    ds = ogr.Open('tmp/ogr_sqlite_38.sqlite', update=1)
    lyr = ds.GetLayerByName('test')
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_string')).GetDefault() == "'a''b'"
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_int')).GetDefault() == '123'
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_real')).GetDefault() == '1.23'
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_nodefault')).GetDefault() is None
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_datetime')).GetDefault() == 'CURRENT_TIMESTAMP'
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_datetime2')).GetDefault() == "'2015/06/30 12:34:56'"
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_datetime3')).GetDefault() == "(strftime('%Y-%m-%dT%H:%M:%fZ','now'))"
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_datetime4')).GetDefault() == "'2015/06/30 12:34:56.123'"
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_date')).GetDefault() == "CURRENT_DATE"
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_time')).GetDefault() == "CURRENT_TIME"
    f = lyr.GetNextFeature()
    if f.GetField('field_string') != 'a\'b' or f.GetField('field_int') != 123 or \
       f.GetField('field_real') != 1.23 or \
       not f.IsFieldNull('field_nodefault') or not f.IsFieldSet('field_datetime') or \
       f.GetField('field_datetime2') != '2015/06/30 12:34:56' or \
       f.GetField('field_datetime4') != '2015/06/30 12:34:56.123' or \
       not f.IsFieldSet('field_datetime3') or \
       not f.IsFieldSet('field_date') or not f.IsFieldSet('field_time'):
        f.DumpReadable()
        pytest.fail()

    # Change DEFAULT value
    src_fd = lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_string'))
    fd = ogr.FieldDefn('field_string', src_fd.GetType())
    fd.SetDefault("'c'")
    lyr.AlterFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_string'), fd, ogr.ALTER_DEFAULT_FLAG)
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_string')).GetDefault() == "'c'"

    # Drop DEFAULT value
    src_fd = lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_int'))
    fd = ogr.FieldDefn('field_int', src_fd.GetType())
    fd.SetDefault(None)
    lyr.AlterFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_int'), fd, ogr.ALTER_DEFAULT_FLAG)
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_int')).GetDefault() is None

    ds = None

    ds = ogr.Open('tmp/ogr_sqlite_38.sqlite', update=1)
    lyr = ds.GetLayerByName('test')
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_string')).GetDefault() == "'c'"
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_int')).GetDefault() is None

    ds = None

###############################################################################
# Test spatial filters with point extent


def test_ogr_spatialite_9(require_spatialite):
    ds = ogr.GetDriverByName('SQLite').CreateDataSource('/vsimem/ogr_spatialite_9.sqlite', options=['SPATIALITE=YES'])
    lyr = ds.CreateLayer('point', geom_type=ogr.wkbPoint)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT(1 2)'))
    lyr.CreateFeature(feat)
    lyr.SetSpatialFilterRect(1, 2, 1, 2)
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat is not None
    ds = None
    ogr.GetDriverByName('SQLite').DeleteDataSource('/vsimem/ogr_spatialite_9.sqlite')

###############################################################################
# Test not nullable fields


def test_ogr_spatialite_10(require_spatialite):
    try:
        os.remove('tmp/ogr_spatialite_10.sqlite')
    except OSError:
        pass

    ds = ogr.GetDriverByName('SQLite').CreateDataSource('tmp/ogr_spatialite_10.sqlite', options=['SPATIALITE=YES'])
    lyr = ds.CreateLayer('test', geom_type=ogr.wkbNone)
    field_defn = ogr.FieldDefn('field_not_nullable', ogr.OFTString)
    field_defn.SetNullable(0)
    lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn('field_nullable', ogr.OFTString)
    lyr.CreateField(field_defn)
    field_defn = ogr.GeomFieldDefn('geomfield_not_nullable', ogr.wkbPoint)
    field_defn.SetNullable(0)
    lyr.CreateGeomField(field_defn)
    field_defn = ogr.GeomFieldDefn('geomfield_nullable', ogr.wkbPoint)
    lyr.CreateGeomField(field_defn)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('field_not_nullable', 'not_null')
    f.SetGeomFieldDirectly('geomfield_not_nullable', ogr.CreateGeometryFromWkt('POINT(0 0)'))
    lyr.CreateFeature(f)
    f = None
    ds = None

    ds = ogr.Open('tmp/ogr_spatialite_10.sqlite')
    lyr = ds.GetLayerByName('test')
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_not_nullable')).IsNullable() == 0
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_nullable')).IsNullable() == 1
    assert lyr.GetLayerDefn().GetGeomFieldDefn(lyr.GetLayerDefn().GetGeomFieldIndex('geomfield_not_nullable')).IsNullable() == 0
    assert lyr.GetLayerDefn().GetGeomFieldDefn(lyr.GetLayerDefn().GetGeomFieldIndex('geomfield_nullable')).IsNullable() == 1
    ds = None


###############################################################################
# Test creating a field with the fid name

def test_ogr_sqlite_39():

    if gdaltest.sl_ds is None:
        pytest.skip()

    ds = ogr.GetDriverByName('SQLite').CreateDataSource('/vsimem/ogr_sqlite_39.sqlite')
    lyr = ds.CreateLayer('test', geom_type=ogr.wkbNone, options=['FID=myfid'])

    lyr.CreateField(ogr.FieldDefn('str', ogr.OFTString))
    gdal.PushErrorHandler()
    ret = lyr.CreateField(ogr.FieldDefn('myfid', ogr.OFTString))
    gdal.PopErrorHandler()
    assert ret != 0

    ret = lyr.CreateField(ogr.FieldDefn('myfid', ogr.OFTInteger))
    assert ret == 0
    lyr.CreateField(ogr.FieldDefn('str2', ogr.OFTString))

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('str', 'first string')
    feat.SetField('myfid', 10)
    feat.SetField('str2', 'second string')
    ret = lyr.CreateFeature(feat)
    assert ret == 0
    assert feat.GetFID() == 10

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('str2', 'second string')
    ret = lyr.CreateFeature(feat)
    assert ret == 0
    if feat.GetFID() < 0:
        feat.DumpReadable()
        pytest.fail()
    if feat.GetField('myfid') != feat.GetFID():
        feat.DumpReadable()
        pytest.fail()

    feat.SetField('str', 'foo')
    ret = lyr.SetFeature(feat)
    assert ret == 0

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetFID(1)
    feat.SetField('myfid', 10)
    gdal.PushErrorHandler()
    ret = lyr.CreateFeature(feat)
    gdal.PopErrorHandler()
    assert ret != 0

    gdal.PushErrorHandler()
    ret = lyr.SetFeature(feat)
    gdal.PopErrorHandler()
    assert ret != 0

    feat.UnsetField('myfid')
    gdal.PushErrorHandler()
    ret = lyr.SetFeature(feat)
    gdal.PopErrorHandler()
    assert ret != 0

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f.GetFID() != 10 or f.GetField('str') != 'first string' or f.GetField('str2') != 'second string' or f.GetField('myfid') != 10:
        f.DumpReadable()
        pytest.fail()
    f = None

    ds = None

    ogr.GetDriverByName('SQLite').DeleteDataSource('/vsimem/ogr_sqlite_39.sqlite')

###############################################################################
# Test dataset transactions


def test_ogr_sqlite_40(with_and_without_spatialite):

    if gdaltest.sl_ds is None:
        pytest.skip()

    if with_and_without_spatialite:
        options = ['SPATIALITE=YES']
    else:
        options = []
    ds = ogr.GetDriverByName('SQLite').CreateDataSource('/vsimem/ogr_sqlite_40.sqlite', options=options)

    assert ds.TestCapability(ogr.ODsCTransactions) == 1

    ret = ds.StartTransaction()
    assert ret == 0
    gdal.PushErrorHandler()
    ret = ds.StartTransaction()
    gdal.PopErrorHandler()
    assert ret != 0

    lyr = ds.CreateLayer('test', geom_type=ogr.wkbPoint)
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    ret = ds.RollbackTransaction()
    assert ret == 0
    gdal.PushErrorHandler()
    ret = ds.RollbackTransaction()
    gdal.PopErrorHandler()
    assert ret != 0
    ds = None

    ds = ogr.Open('/vsimem/ogr_sqlite_40.sqlite', update=1)
    assert ds.GetLayerCount() == 0
    ret = ds.StartTransaction()
    assert ret == 0
    gdal.PushErrorHandler()
    ret = ds.StartTransaction()
    gdal.PopErrorHandler()
    assert ret != 0

    lyr = ds.CreateLayer('test', geom_type=ogr.wkbPoint)
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    ret = ds.CommitTransaction()
    assert ret == 0
    gdal.PushErrorHandler()
    ret = ds.CommitTransaction()
    gdal.PopErrorHandler()
    assert ret != 0
    ds = None

    ds = ogr.Open('/vsimem/ogr_sqlite_40.sqlite', update=1)
    assert ds.GetLayerCount() == 1
    lyr = ds.GetLayerByName('test')

    ds.StartTransaction()
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f is not None
    assert lyr.GetFeatureCount() == 1
    ds.RollbackTransaction()
    assert lyr.GetFeatureCount() == 0

    ds.StartTransaction()
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f is not None and f.GetFID() == 1
    ds.CommitTransaction()
    # the cursor is still valid after CommitTransaction(), which isn't the case for other backends such as PG !
    f = lyr.GetNextFeature()
    assert f is not None and f.GetFID() == 2
    assert lyr.GetFeatureCount() == 2

    ds.StartTransaction()
    lyr = ds.CreateLayer('test2', geom_type=ogr.wkbPoint)
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT(0 0)'))
    ret = lyr.CreateFeature(f)
    ds.CommitTransaction()
    assert ret == 0

    ds.StartTransaction()
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT(0 0)'))
    ret = lyr.CreateFeature(f)
    ds.CommitTransaction()
    assert ret == 0

    ds.StartTransaction()
    lyr = ds.CreateLayer('test3', geom_type=ogr.wkbPoint)
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT(0 0)'))
    ret = lyr.CreateFeature(f)

    # ds.CommitTransaction()
    ds.ReleaseResultSet(ds.ExecuteSQL('SELECT 1'))
    # ds = None
    # ds = ogr.Open('/vsimem/ogr_gpkg_26.gpkg', update = 1)
    # lyr = ds.GetLayerByName('test3')
    # ds.StartTransaction()

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT(0 0)'))
    ret = lyr.CreateFeature(f)
    ds.CommitTransaction()
    assert ret == 0

    ds = None

    ogr.GetDriverByName('SQLite').DeleteDataSource('/vsimem/ogr_sqlite_40.sqlite')

###############################################################################
# Test reading dates from Julian day floating point representation


def test_ogr_sqlite_41():

    if gdaltest.sl_ds is None:
        pytest.skip()
    ds = ogr.GetDriverByName('SQLite').CreateDataSource('/vsimem/ogr_sqlite_41.sqlite', options=['METADATA=NO'])
    ds.ExecuteSQL('CREATE TABLE test(a_date DATETIME);')
    ds.ExecuteSQL("INSERT INTO test(a_date) VALUES (strftime('%J', '2015-04-30 12:34:56'))")
    ds = None

    ds = ogr.Open('/vsimem/ogr_sqlite_41.sqlite')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f['a_date'] == '2015/04/30 12:34:56'

    ds = None

    ogr.GetDriverByName('SQLite').DeleteDataSource('/vsimem/ogr_sqlite_41.sqlite')

###############################################################################
# Test ExecuteSQL() heuristics (#6107)


def test_ogr_sqlite_42():

    if gdaltest.sl_ds is None:
        pytest.skip()

    ds = ogr.GetDriverByName('SQLite').CreateDataSource('/vsimem/ogr_sqlite_42.sqlite')
    lyr = ds.CreateLayer("aab")
    lyr.CreateField(ogr.FieldDefn("id", ogr.OFTInteger))
    f = ogr.Feature(lyr.GetLayerDefn())
    f['id'] = 1
    lyr.CreateFeature(f)
    lyr = None

    sql_lyr = ds.ExecuteSQL('SELECT id FROM aab')
    sql_lyr.SetAttributeFilter('id = 1')
    f = sql_lyr.GetNextFeature()
    assert f is not None
    ds.ReleaseResultSet(sql_lyr)

    sql_lyr = ds.ExecuteSQL('SELECT id FROM "aab"')
    sql_lyr.SetAttributeFilter('id = 1')
    f = sql_lyr.GetNextFeature()
    assert f is not None
    ds.ReleaseResultSet(sql_lyr)

    lyr = ds.CreateLayer('with"quotes')
    lyr.CreateField(ogr.FieldDefn("id", ogr.OFTInteger))
    f = ogr.Feature(lyr.GetLayerDefn())
    f['id'] = 1
    lyr.CreateFeature(f)
    lyr = None

    sql_lyr = ds.ExecuteSQL('SELECT id FROM "with""quotes"')
    sql_lyr.SetAttributeFilter('id = 1')
    f = sql_lyr.GetNextFeature()
    assert f is not None
    ds.ReleaseResultSet(sql_lyr)

    # Too complex to analyze
    sql_lyr = ds.ExecuteSQL('SELECT id FROM "with""quotes" UNION ALL SELECT id FROM aab')
    sql_lyr.SetAttributeFilter('id = 1')
    f = sql_lyr.GetNextFeature()
    assert f is not None
    ds.ReleaseResultSet(sql_lyr)

    ds = None

    ogr.GetDriverByName('SQLite').DeleteDataSource('/vsimem/ogr_sqlite_42.sqlite')

###############################################################################
# Test file:foo?mode=memory&cache=shared (#6150)


def test_ogr_sqlite_43():

    if gdaltest.sl_ds is None:
        pytest.skip()

    # Only available since sqlite 3.8.0
    version = ogrtest.sqlite_version.split('.')
    if not (len(version) >= 3 and int(version[0]) * 10000 + int(version[1]) * 100 + int(version[2]) >= 30800):
        pytest.skip()

    ds = ogr.Open('file:foo?mode=memory&cache=shared')
    assert ds is not None

###############################################################################
# Test reading/writing StringList, etc..


def test_ogr_sqlite_44():

    if gdaltest.sl_ds is None:
        pytest.skip()

    gdal.FileFromMemBuffer('/vsimem/ogr_sqlite_44.csvt', 'JsonStringList,JsonIntegerList,JsonInteger64List,JsonRealList,WKT')
    gdal.FileFromMemBuffer('/vsimem/ogr_sqlite_44.csv',
                           """stringlist,intlist,int64list,reallist,WKT
"[""a"",null]","[1]","[1234567890123]","[0.125]",
""")

    gdal.VectorTranslate('/vsimem/ogr_sqlite_44.sqlite', '/vsimem/ogr_sqlite_44.csv', format='SQLite')
    gdal.VectorTranslate('/vsimem/ogr_sqlite_44_out.csv', '/vsimem/ogr_sqlite_44.sqlite', format='CSV', layerCreationOptions=['CREATE_CSVT=YES', 'LINEFORMAT=LF'])

    f = gdal.VSIFOpenL('/vsimem/ogr_sqlite_44_out.csv', 'rb')
    assert f is not None
    data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
    gdal.VSIFCloseL(f)

    assert data.startswith('stringlist,intlist,int64list,reallist,wkt\n"[ ""a"", """" ]",[ 1 ],[ 1234567890123 ],[ 0.125')

    f = gdal.VSIFOpenL('/vsimem/ogr_sqlite_44_out.csvt', 'rb')
    data = gdal.VSIFReadL(1, 10000, f).decode('ascii')
    gdal.VSIFCloseL(f)

    assert data.startswith('JSonStringList,JSonIntegerList,JSonInteger64List,JSonRealList')

    gdal.Unlink('/vsimem/ogr_sqlite_44.csv')
    gdal.Unlink('/vsimem/ogr_sqlite_44.csvt')
    gdal.Unlink('/vsimem/ogr_sqlite_44.sqlite')
    gdal.Unlink('/vsimem/ogr_sqlite_44_out.csv')
    gdal.Unlink('/vsimem/ogr_sqlite_44_out.csvt')

###############################################################################
# Test WAL and opening in read-only (#6776)


def test_ogr_sqlite_45():

    if gdaltest.sl_ds is None:
        pytest.skip()

    # Only available since sqlite 3.7.0
    version = ogrtest.sqlite_version.split('.')
    if not (len(version) >= 3 and int(version[0]) * 10000 + int(version[1]) * 100 + int(version[2]) >= 30700):
        pytest.skip()

    ds = ogr.GetDriverByName('SQLite').CreateDataSource('tmp/ogr_sqlite_45.db')
    sql_lyr = ds.ExecuteSQL('PRAGMA journal_mode = WAL')
    ds.ReleaseResultSet(sql_lyr)
    sql_lyr = ds.ExecuteSQL('SELECT * FROM sqlite_master')
    ds.ReleaseResultSet(sql_lyr)
    assert os.path.exists('tmp/ogr_sqlite_45.db-wal')
    shutil.copy('tmp/ogr_sqlite_45.db', 'tmp/ogr_sqlite_45_bis.db')
    shutil.copy('tmp/ogr_sqlite_45.db-shm', 'tmp/ogr_sqlite_45_bis.db-shm')
    shutil.copy('tmp/ogr_sqlite_45.db-wal', 'tmp/ogr_sqlite_45_bis.db-wal')
    ds = None
    assert not os.path.exists('tmp/ogr_sqlite_45.db-wal')

    ds = ogr.Open('tmp/ogr_sqlite_45_bis.db')
    ds = None
    assert not os.path.exists('tmp/ogr_sqlite_45_bis.db-wal')

    gdal.Unlink('tmp/ogr_sqlite_45.db')
    gdal.Unlink('tmp/ogr_sqlite_45_bis.db')


###############################################################################
# Test creating unsupported geometry types

def test_ogr_spatialite_11(require_spatialite):
    ds = ogr.GetDriverByName('SQLite').CreateDataSource('/vsimem/ogr_spatialite_11.sqlite', options=['SPATIALITE=YES'])

    # Will be converted to LineString
    lyr = ds.CreateLayer('test', geom_type=ogr.wkbCurve)
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    f = None

    lyr = ds.CreateLayer('test2', geom_type=ogr.wkbNone)
    with gdaltest.error_handler():
        res = lyr.CreateGeomField(ogr.GeomFieldDefn('foo', ogr.wkbCurvePolygon))
    assert res != 0

    ds = None

    gdal.Unlink('/vsimem/ogr_spatialite_11.sqlite')

###############################################################################
# Test opening a .sql file


def test_ogr_spatialite_12(require_spatialite):
    if gdal.GetDriverByName('SQLite').GetMetadataItem("ENABLE_SQL_SQLITE_FORMAT") != 'YES':
        pytest.skip()

    ds = ogr.Open('data/sqlite/poly_spatialite.sqlite.sql')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f is not None


###############################################################################

def test_ogr_sqlite_iterate_and_update():

    if gdaltest.sl_ds is None:
        pytest.skip()

    filename = "/vsimem/ogr_sqlite_iterate_and_update.db"
    ds = ogr.GetDriverByName('SQLite').CreateDataSource(filename)
    lyr = ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn('strfield'))
    f = ogr.Feature(lyr.GetLayerDefn())
    f['strfield'] = 'foo'
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f['strfield'] = 'bar'
    lyr.CreateFeature(f)
    lyr.ResetReading()
    for f in lyr:
        f['strfield'] += "_updated"
        lyr.SetFeature(f)
    lyr.ResetReading()
    for f in lyr:
        assert f['strfield'].endswith('_updated')
    ds = None

    gdal.Unlink(filename)

###############################################################################
# Test unique constraints on fields


def test_ogr_sqlite_unique():

    if gdaltest.is_travis_branch('trusty_32bit') or gdaltest.is_travis_branch('trusty_clang'):
        pytest.skip('gcc too old')

    ds = ogr.GetDriverByName('SQLite').CreateDataSource('/vsimem/ogr_gpkg_unique.db')
    lyr = ds.CreateLayer('test', geom_type=ogr.wkbNone)

    # Default: no unique constraints
    field_defn = ogr.FieldDefn('field_default', ogr.OFTString)
    lyr.CreateField(field_defn)

    # Explicit: no unique constraints
    field_defn = ogr.FieldDefn('field_no_unique', ogr.OFTString)
    field_defn.SetUnique(0)
    lyr.CreateField(field_defn)

    # Explicit: unique constraints
    field_defn = ogr.FieldDefn('field_unique', ogr.OFTString)
    field_defn.SetUnique(1)
    lyr.CreateField(field_defn)

    # Now check for getters
    layerDefinition = lyr.GetLayerDefn()
    fldDef = layerDefinition.GetFieldDefn(0)
    assert not fldDef.IsUnique()
    fldDef = layerDefinition.GetFieldDefn(1)
    assert not fldDef.IsUnique()
    fldDef = layerDefinition.GetFieldDefn(2)
    assert fldDef.IsUnique()


    # Create another layer from SQL to test quoting of fields
    # and indexes
    # Note: leave create table in a single line because of regex spaces testing
    sql = (
        'CREATE TABLE IF NOT EXISTS "test2" ( "fid" INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, "field_default" TEXT, "field_no_unique" TEXT, "field_unique" TEXT UNIQUE,`field unique2` TEXT UNIQUE,field_unique3 TEXT UNIQUE, FIELD_UNIQUE_INDEX TEXT, `field unique index2`, "field_unique_index3" TEXT, NOT_UNIQUE TEXT);',
        'CREATE UNIQUE INDEX test2_unique_idx ON test2(field_unique_index);', # field_unique_index in lowercase whereas in uppercase in CREATE TABLE statement
        'CREATE UNIQUE INDEX test2_unique_idx2 ON test2(`field unique index2`);',
        'CREATE UNIQUE INDEX test2_unique_idx3 ON test2("field_unique_index3");',
    )

    for s in sql:
        ds.ExecuteSQL(s)

    ds = None

    # Reload
    ds = ogr.Open('/vsimem/ogr_gpkg_unique.db')

    lyr = ds.GetLayerByName('test')

    layerDefinition = lyr.GetLayerDefn()
    fldDef = layerDefinition.GetFieldDefn(0)
    assert not fldDef.IsUnique()
    fldDef = layerDefinition.GetFieldDefn(1)
    assert not fldDef.IsUnique()
    fldDef = layerDefinition.GetFieldDefn(2)
    assert fldDef.IsUnique()

    lyr = ds.GetLayerByName('test2')

    layerDefinition = lyr.GetLayerDefn()
    fldDef = layerDefinition.GetFieldDefn(0)
    assert not fldDef.IsUnique()
    fldDef = layerDefinition.GetFieldDefn(1)
    assert not fldDef.IsUnique()
    fldDef = layerDefinition.GetFieldDefn(2)
    assert fldDef.IsUnique()
    fldDef = layerDefinition.GetFieldDefn(3)
    assert fldDef.IsUnique()
    fldDef = layerDefinition.GetFieldDefn(4)
    assert fldDef.IsUnique()

    # Check the last 3 field where the unique constraint is defined
    # from an index
    fldDef = layerDefinition.GetFieldDefn(5)
    assert fldDef.IsUnique()
    fldDef = layerDefinition.GetFieldDefn(6)
    assert fldDef.IsUnique()
    fldDef = layerDefinition.GetFieldDefn(7)
    assert fldDef.IsUnique()

    fldDef = layerDefinition.GetFieldDefn(8)
    assert not fldDef.IsUnique()

    ds = None

    gdal.Unlink('/vsimem/ogr_gpkg_unique.db')

###############################################################################
# Test PRELUDE_STATEMENTS open option


def test_ogr_sqlite_prelude_statements(require_spatialite):

    ds = gdal.OpenEx('data/sqlite/poly_spatialite.sqlite',
                     open_options=["PRELUDE_STATEMENTS=ATTACH DATABASE 'data/sqlite/poly_spatialite.sqlite' AS other"])
    sql_lyr = ds.ExecuteSQL('SELECT * FROM poly JOIN other.poly USING (eas_id)')
    assert sql_lyr.GetFeatureCount() == 10
    ds.ReleaseResultSet(sql_lyr)

###############################################################################
# Test INTEGER_OR_TEXT affinity


def test_ogr_sqlite_integer_or_text():

    ds = ogr.GetDriverByName('SQLite').CreateDataSource(':memory:')
    ds.ExecuteSQL('CREATE TABLE foo(c INTEGER_OR_TEXT)')
    ds.ExecuteSQL('INSERT INTO foo VALUES (5)')
    ds.ExecuteSQL("INSERT INTO foo VALUES ('five')")

    sql_lyr = ds.ExecuteSQL('SELECT typeof(c) FROM foo')
    f = sql_lyr.GetNextFeature()
    assert f.GetField(0) == 'integer'
    ds.ReleaseResultSet(sql_lyr)

    lyr = ds.GetLayer('foo')
    assert lyr.GetLayerDefn().GetFieldCount() == 1
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTString
    f = lyr.GetNextFeature()
    assert f['c'] == '5'
    f = lyr.GetNextFeature()
    assert f['c'] == 'five'

###############################################################################
# Test better guessing of columns in a view


def test_ogr_sqlite_view_type():

    ds = ogr.GetDriverByName('SQLite').CreateDataSource(':memory:')
    ds.ExecuteSQL('CREATE TABLE t(c INTEGER)')
    ds.ExecuteSQL('CREATE TABLE u(d TEXT)')
    ds.ExecuteSQL("CREATE VIEW v AS SELECT c FROM t UNION ALL SELECT NULL FROM u")

    lyr = ds.GetLayer('v')
    assert lyr.GetLayerDefn().GetFieldCount() == 1
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTInteger

    ds.ExecuteSQL('INSERT INTO t VALUES(1)')
    f = lyr.GetNextFeature()
    assert f['c'] == 1
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f['c'] == 1


###############################################################################
# Test table WITHOUT ROWID


def test_ogr_sqlite_without_rowid():

    tmpfilename = '/vsimem/without_rowid.db'
    try:
        ds = ogr.GetDriverByName('SQLite').CreateDataSource(tmpfilename)
        ds.ExecuteSQL('CREATE TABLE t(key TEXT NOT NULL PRIMARY KEY, value TEXT) WITHOUT ROWID')
        ds = None

        ds = ogr.Open(tmpfilename, update=1)
        lyr = ds.GetLayer('t')
        assert lyr.GetFIDColumn() == ''
        assert lyr.GetLayerDefn().GetFieldCount() == 2

        f = ogr.Feature(lyr.GetLayerDefn())
        f['key'] = 'foo'
        f['value'] = 'bar'
        assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
        assert f.GetFID() == -1 # hard to do best

        assert lyr.GetFeatureCount() == 1

        f = lyr.GetNextFeature()
        assert f['key'] == 'foo'
        assert f['value'] == 'bar'
        assert f.GetFID() == 0 # somewhat arbitrary

        f = lyr.GetFeature(0)
        assert f['key'] == 'foo'

        ds = None
    finally:
        gdal.Unlink(tmpfilename)


###############################################################################
# Test table in STRICT mode (sqlite >= 3.37)


def test_ogr_sqlite_strict():

    if 'FORCE_SQLITE_STRICT' not in os.environ and \
        'STRICT' not in gdal.GetDriverByName('SQLite').GetMetadataItem(gdal.DMD_CREATIONOPTIONLIST):
        pytest.skip('sqlite >= 3.37 required')

    tmpfilename = '/vsimem/strict.db'
    try:
        ds = ogr.GetDriverByName('SQLite').CreateDataSource(tmpfilename)
        lyr = ds.CreateLayer('t', options=['STRICT=YES'])
        lyr.CreateField(ogr.FieldDefn('int_field', ogr.OFTInteger))
        lyr.CreateField(ogr.FieldDefn('int64_field', ogr.OFTInteger64))
        lyr.CreateField(ogr.FieldDefn('text_field', ogr.OFTString))
        lyr.CreateField(ogr.FieldDefn('blob_field', ogr.OFTBinary))
        ds = None

        ds = ogr.Open(tmpfilename, update=1)
        sql_lyr = ds.ExecuteSQL("SELECT sql FROM sqlite_master WHERE name='t'")
        f = sql_lyr.GetNextFeature()
        sql = f['sql']
        ds.ReleaseResultSet(sql_lyr)
        assert ') STRICT' in sql

        lyr = ds.GetLayer('t')
        lyr.CreateField(ogr.FieldDefn('real_field', ogr.OFTReal))
        lyr.CreateField(ogr.FieldDefn('datetime_field', ogr.OFTDateTime))
        lyr.CreateField(ogr.FieldDefn('date_field', ogr.OFTDate))
        lyr.CreateField(ogr.FieldDefn('time_field', ogr.OFTTime))
        ds = None

        ds = ogr.Open(tmpfilename, update=1)
        lyr = ds.GetLayer('t')
        layer_defn = lyr.GetLayerDefn()
        assert layer_defn.GetFieldCount() == 8
        assert layer_defn.GetFieldDefn(0).GetType() == ogr.OFTInteger
        assert layer_defn.GetFieldDefn(1).GetType() == ogr.OFTInteger64
        assert layer_defn.GetFieldDefn(2).GetType() == ogr.OFTString
        assert layer_defn.GetFieldDefn(3).GetType() == ogr.OFTBinary
        assert layer_defn.GetFieldDefn(4).GetType() == ogr.OFTReal
        assert layer_defn.GetFieldDefn(5).GetType() == ogr.OFTDateTime
        assert layer_defn.GetFieldDefn(6).GetType() == ogr.OFTDate
        assert layer_defn.GetFieldDefn(7).GetType() == ogr.OFTTime

        ds = None
    finally:
        gdal.Unlink(tmpfilename)


###############################################################################
#


def test_ogr_sqlite_cleanup():

    if gdaltest.sl_ds is None:
        pytest.skip()

    gdaltest.sl_ds.ExecuteSQL('DELLAYER:tpoly')
    gdaltest.sl_ds.ExecuteSQL('DELLAYER:tpoly_2')
    gdaltest.sl_ds.ExecuteSQL('DELLAYER:tpoly_3')
    gdaltest.sl_ds.ExecuteSQL('DELLAYER:geomwkb')
    gdaltest.sl_ds.ExecuteSQL('DELLAYER:geomwkt')
    gdaltest.sl_ds.ExecuteSQL('DELLAYER:geomspatialite')
    gdaltest.sl_ds.ExecuteSQL('DELLAYER:wgs84layer')
    gdaltest.sl_ds.ExecuteSQL('DELLAYER:wgs84layer_approx')
    gdaltest.sl_ds.ExecuteSQL('DELLAYER:testtypes')
    gdaltest.sl_ds.ExecuteSQL('DELLAYER:fgf_table')

    gdaltest.sl_ds = None

    gdaltest.shp_ds = None

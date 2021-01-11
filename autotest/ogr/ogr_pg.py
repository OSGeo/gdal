#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test PostGIS driver functionality.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
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

import os
import sys
import shutil
import time
import threading

import pytest

import gdaltest
import ogrtest
from osgeo import ogr
from osgeo import osr
from osgeo import gdal


pytestmark = pytest.mark.require_driver('PostgreSQL')


###############################################################################
# Return true if 'layer_name' is one of the reported layers of pg_ds


def ogr_pg_check_layer_in_list(ds, layer_name):

    for i in range(0, ds.GetLayerCount()):
        name = ds.GetLayer(i).GetName()
        if name == layer_name:
            return True
    return False

#
# To create the required PostGIS instance do something like:
#
#  $ createdb autotest
#  $ createlang plpgsql autotest
#  $ psql autotest < ~/postgis.sql
#

###############################################################################
# Run tests with PostGIS enabled and then with PostGIS disabled


@pytest.fixture(
    params=['postgis', 'no-postgis'],
    scope='module',
    autouse=True
)
def with_and_without_postgis(request):
    test_ogr_pg_1()
    if gdaltest.pg_ds is None:
        pytest.skip()

    with_postgis = request.param == 'postgis'

    if with_postgis and not gdaltest.pg_has_postgis:
        pytest.skip()

    with gdaltest.config_option("PG_USE_POSTGIS", 'YES' if with_postgis else 'NO'):
        yield with_postgis


###############################################################################
# Open Database.


def test_ogr_pg_1():
    gdaltest.pg_ds = None
    gdaltest.pg_use_copy = gdal.GetConfigOption('PG_USE_COPY', None)
    val = gdal.GetConfigOption('OGR_PG_CONNECTION_STRING', None)
    if val is not None:
        gdaltest.pg_connection_string = val
    else:
        gdaltest.pg_connection_string = 'dbname=autotest'
    # gdaltest.pg_connection_string='dbname=autotest-postgis1.4'
    # gdaltest.pg_connection_string='dbname=autotest port=5432'
    # gdaltest.pg_connection_string='dbname=autotest-postgis2.0'
    # gdaltest.pg_connection_string='dbname=autotest host=127.0.0.1 port=5433 user=postgres'
    # gdaltest.pg_connection_string='dbname=autotest host=127.0.0.1 port=5434 user=postgres'
    # gdaltest.pg_connection_string='dbname=autotest port=5435 host=127.0.0.1'
    # 7.4
    # gdaltest.pg_connection_string='dbname=autotest port=5436 user=postgres'

    try:
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        gdaltest.pg_ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
        gdal.PopErrorHandler()
    except:
        gdaltest.pg_ds = None
        gdal.PopErrorHandler()

    if gdaltest.pg_ds is None:
        pytest.skip()

    sql_lyr = gdaltest.pg_ds.ExecuteSQL('SELECT version()')
    feat = sql_lyr.GetNextFeature()
    version_str = feat.GetFieldAsString('version')
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

    gdaltest.pg_retrieve_fid = False
    gdaltest.pg_version = (0,)
    v = version_str
    pos = v.find(' ') # "PostgreSQL 12.0beta1" or "PostgreSQL 12.2 ...."
    if pos > 0:
        v = v[pos+1:]
        pos = v.find('beta')
        if pos > 0:
            v = v[0:pos]
        pos = v.find(' ')
        if pos > 0:
            v = v[0:pos]
        gdaltest.pg_version = tuple([int(x) for x in v.split('.')])
        #print(gdaltest.pg_version)
        if gdaltest.pg_version >= (8,2):
            gdaltest.pg_retrieve_fid = True

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    sql_lyr = gdaltest.pg_ds.ExecuteSQL('SHOW standard_conforming_strings')
    gdal.PopErrorHandler()
    gdaltest.pg_quote_with_E = sql_lyr is not None
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    sql_lyr = gdaltest.pg_ds.ExecuteSQL('SELECT postgis_version()')
    gdaltest.pg_has_postgis = sql_lyr is not None
    gdaltest.pg_has_postgis_2 = False
    if gdaltest.pg_has_postgis:
        feat = sql_lyr.GetNextFeature()
        version_str = feat.GetFieldAsString('postgis_version')
        gdaltest.pg_has_postgis_2 = (float(version_str[0:3]) >= 2.0)
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)
    gdal.PopErrorHandler()

    if gdaltest.pg_has_postgis:
        if gdal.GetConfigOption('PG_USE_POSTGIS', 'YES') == 'YES':
            print('PostGIS available !')
        else:
            gdaltest.pg_has_postgis = False
            gdaltest.pg_has_postgis_2 = False
            print('PostGIS available but will NOT be used because of PG_USE_POSTGIS=NO !')
    else:
        gdaltest.pg_has_postgis = False
        print('PostGIS NOT available !')

    
###############################################################################
# Create table from data/poly.shp


def test_ogr_pg_2():

    if gdaltest.pg_ds is None:
        pytest.skip()

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:tpoly')
    gdal.PopErrorHandler()

    ######################################################
    # Create Layer
    gdaltest.pg_lyr = gdaltest.pg_ds.CreateLayer('tpoly',
                                                 options=['DIM=3'])

    ######################################################
    # Check capabilities

    if gdaltest.pg_has_postgis:
        assert gdaltest.pg_lyr.TestCapability(ogr.OLCFastSpatialFilter)
        assert gdaltest.pg_lyr.TestCapability(ogr.OLCFastGetExtent)
    else:
        assert not gdaltest.pg_lyr.TestCapability(ogr.OLCFastSpatialFilter)
        assert not gdaltest.pg_lyr.TestCapability(ogr.OLCFastGetExtent)

    assert gdaltest.pg_lyr.TestCapability(ogr.OLCRandomRead)
    assert gdaltest.pg_lyr.TestCapability(ogr.OLCFastFeatureCount)
    assert gdaltest.pg_lyr.TestCapability(ogr.OLCFastSetNextByIndex)
    try:
        ogr.OLCStringsAsUTF8
        assert gdaltest.pg_lyr.TestCapability(ogr.OLCStringsAsUTF8)
    except:
        pass
    assert gdaltest.pg_lyr.TestCapability(ogr.OLCSequentialWrite)
    assert gdaltest.pg_lyr.TestCapability(ogr.OLCCreateField)
    assert gdaltest.pg_lyr.TestCapability(ogr.OLCRandomWrite)
    assert gdaltest.pg_lyr.TestCapability(ogr.OLCTransactions)

    ######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def(gdaltest.pg_lyr,
                                   [('AREA', ogr.OFTReal),
                                    ('EAS_ID', ogr.OFTInteger),
                                    ('PRFEDEA', ogr.OFTString),
                                    ('SHORTNAME', ogr.OFTString, 8),
                                    ('REALLIST', ogr.OFTRealList)])

    ######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature(feature_def=gdaltest.pg_lyr.GetLayerDefn())

    shp_ds = ogr.Open('data/poly.shp')
    shp_lyr = shp_ds.GetLayer(0)

    feat = shp_lyr.GetNextFeature()
    gdaltest.poly_feat = []

    expected_fid = 1

    while feat is not None:

        gdaltest.poly_feat.append(feat)

        dst_feat.SetFrom(feat)
        gdaltest.pg_lyr.CreateFeature(dst_feat)
        if gdaltest.pg_retrieve_fid:
            got_fid = dst_feat.GetFID()
            assert got_fid == expected_fid, \
                ("didn't get expected fid : %d instead of %d" % (got_fid, expected_fid))

        expected_fid = expected_fid + 1

        feat = shp_lyr.GetNextFeature()

    dst_feat.Destroy()


###############################################################################
# Test reading a layer extent


def test_ogr_pg_19():

    if gdaltest.pg_ds is None:
        pytest.skip()

    layer = gdaltest.pg_ds.GetLayerByName('tpoly')
    assert layer is not None, 'did not get tpoly layer'

    extent = layer.GetExtent()
    expect = (478315.53125, 481645.3125, 4762880.5, 4765610.5)

    minx = abs(extent[0] - expect[0])
    maxx = abs(extent[1] - expect[1])
    miny = abs(extent[2] - expect[2])
    maxy = abs(extent[3] - expect[3])

    if max(minx, maxx, miny, maxy) > 0.0001:
        print(extent)
        pytest.fail('Extents do not match')

    estimated_extent = layer.GetExtent(force=0)
    if not gdaltest.pg_has_postgis:
        # The OGRLayer default implementation in force = 0 returns error
        if estimated_extent != (0, 0, 0, 0):
            print(extent)
            pytest.fail('Wrong estimated extent')
    else:
        # Better testing needed ?
        if estimated_extent == (0, 0, 0, 0):
            print(extent)
            pytest.fail('Wrong estimated extent')

    
###############################################################################
# Test reading a SQL result layer extent


def test_ogr_pg_19_2():

    if gdaltest.pg_ds is None:
        pytest.skip()

    sql_lyr = gdaltest.pg_ds.ExecuteSQL('select * from tpoly')

    extent = sql_lyr.GetExtent()
    expect = (478315.53125, 481645.3125, 4762880.5, 4765610.5)

    minx = abs(extent[0] - expect[0])
    maxx = abs(extent[1] - expect[1])
    miny = abs(extent[2] - expect[2])
    maxy = abs(extent[3] - expect[3])

    assert max(minx, maxx, miny, maxy) <= 0.0001, 'Extents do not match'

    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)


###############################################################################
# Verify that stuff we just wrote is still OK.


def test_ogr_pg_3():
    if gdaltest.pg_ds is None:
        pytest.skip()

    assert gdaltest.pg_lyr.GetFeatureCount() == 10

    expect = [168, 169, 166, 158, 165]

    gdaltest.pg_lyr.SetAttributeFilter('eas_id < 170')
    tr = ogrtest.check_features_against_list(gdaltest.pg_lyr,
                                             'eas_id', expect)

    assert gdaltest.pg_lyr.GetFeatureCount() == 5

    gdaltest.pg_lyr.SetAttributeFilter(None)

    for i in range(len(gdaltest.poly_feat)):
        orig_feat = gdaltest.poly_feat[i]
        read_feat = gdaltest.pg_lyr.GetNextFeature()

        assert (ogrtest.check_feature_geometry(read_feat, orig_feat.GetGeometryRef(),
                                          max_error=0.001) == 0)

        for fld in range(3):
            assert orig_feat.GetField(fld) == read_feat.GetField(fld), \
                ('Attribute %d does not match' % fld)

        read_feat.Destroy()
        orig_feat.Destroy()

    gdaltest.poly_feat = None

    assert tr

###############################################################################
# Write more features with a bunch of different geometries, and verify the
# geometries are still OK.


def test_ogr_pg_4():

    if gdaltest.pg_ds is None:
        pytest.skip()

    dst_feat = ogr.Feature(feature_def=gdaltest.pg_lyr.GetLayerDefn())
    wkt_list = ['10', '2', '1', '3d_1', '4', '5', '6']

    for item in wkt_list:

        wkt = open('data/wkb_wkt/' + item + '.wkt').read()
        geom = ogr.CreateGeometryFromWkt(wkt)

        ######################################################################
        # Write geometry as a new Oracle feature.

        dst_feat.SetGeometryDirectly(geom)
        dst_feat.SetField('PRFEDEA', item)
        dst_feat.SetFID(-1)
        gdaltest.pg_lyr.CreateFeature(dst_feat)

        ######################################################################
        # Read back the feature and get the geometry.

        gdaltest.pg_lyr.SetAttributeFilter("PRFEDEA = '%s'" % item)
        feat_read = gdaltest.pg_lyr.GetNextFeature()
        geom_read = feat_read.GetGeometryRef()

        if ogrtest.check_feature_geometry(feat_read, geom) != 0:
            print(item)
            print(wkt)
            pytest.fail(geom_read)

        feat_read.Destroy()

    dst_feat.Destroy()
    gdaltest.pg_lyr.ResetReading()  # to close implicit transaction

###############################################################################
# Test ExecuteSQL() results layers without geometry.


def test_ogr_pg_5():

    if gdaltest.pg_ds is None:
        pytest.skip()

    expect = [None, 179, 173, 172, 171, 170, 169, 168, 166, 165, 158]

    sql_lyr = gdaltest.pg_ds.ExecuteSQL('select distinct eas_id from tpoly order by eas_id desc')

    assert sql_lyr.GetFeatureCount() == 11

    tr = ogrtest.check_features_against_list(sql_lyr, 'eas_id', expect)

    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

    assert tr

###############################################################################
# Test ExecuteSQL() results layers with geometry.


def test_ogr_pg_6():

    if gdaltest.pg_ds is None:
        pytest.skip()

    sql_lyr = gdaltest.pg_ds.ExecuteSQL("select * from tpoly where prfedea = '2'")

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

    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

    assert tr

###############################################################################
# Test spatial filtering.


def test_ogr_pg_7():

    if gdaltest.pg_ds is None:
        pytest.skip()

    gdaltest.pg_lyr.SetAttributeFilter(None)

    geom = ogr.CreateGeometryFromWkt(
        'LINESTRING(479505 4763195,480526 4762819)')
    gdaltest.pg_lyr.SetSpatialFilter(geom)
    geom.Destroy()

    assert gdaltest.pg_lyr.GetFeatureCount() == 1

    tr = ogrtest.check_features_against_list(gdaltest.pg_lyr, 'eas_id',
                                             [158])

    gdaltest.pg_lyr.SetAttributeFilter('eas_id = 158')

    assert gdaltest.pg_lyr.GetFeatureCount() == 1

    gdaltest.pg_lyr.SetAttributeFilter(None)

    gdaltest.pg_lyr.SetSpatialFilter(None)

    assert tr

###############################################################################
# Write a feature with too long a text value for a fixed length text field.
# The driver should now truncate this (but with a debug message).  Also,
# put some crazy stuff in the value to verify that quoting and escaping
# is working smoothly.
#
# No geometry in this test.


def test_ogr_pg_8():

    if gdaltest.pg_ds is None:
        pytest.skip()

    dst_feat = ogr.Feature(feature_def=gdaltest.pg_lyr.GetLayerDefn())

    dst_feat.SetField('PRFEDEA', 'CrazyKey')
    dst_feat.SetField('SHORTNAME', 'Crazy"\'Long')
    gdaltest.pg_lyr.CreateFeature(dst_feat)
    dst_feat.Destroy()

    gdaltest.pg_lyr.SetAttributeFilter("PRFEDEA = 'CrazyKey'")
    feat_read = gdaltest.pg_lyr.GetNextFeature()

    assert feat_read is not None, 'creating crazy feature failed!'

    assert feat_read.GetField('shortname') == 'Crazy"\'L', \
        ('Vvalue not properly escaped or truncated:' +
                             feat_read.GetField('shortname'))

    feat_read.Destroy()

###############################################################################
# Verify inplace update of a feature with SetFeature().


def test_ogr_pg_9():

    if gdaltest.pg_ds is None:
        pytest.skip()

    gdaltest.pg_lyr.SetAttributeFilter("PRFEDEA = 'CrazyKey'")
    feat = gdaltest.pg_lyr.GetNextFeature()
    gdaltest.pg_lyr.SetAttributeFilter(None)

    feat.SetField('SHORTNAME', 'Reset')

    point = ogr.Geometry(ogr.wkbPoint25D)
    point.SetPoint(0, 5, 6, 7)
    feat.SetGeometryDirectly(point)

    if gdaltest.pg_lyr.SetFeature(feat) != 0:
        feat.Destroy()
        pytest.fail('SetFeature() method failed.')

    fid = feat.GetFID()
    feat.Destroy()

    feat = gdaltest.pg_lyr.GetFeature(fid)
    assert feat is not None, ('GetFeature(%d) failed.' % fid)

    shortname = feat.GetField('SHORTNAME')
    assert shortname[:5] == 'Reset', ('SetFeature() did not update SHORTNAME, got %s.'
                             % shortname)

    if ogrtest.check_feature_geometry(feat, 'POINT(5 6 7)') != 0:
        print(feat.GetGeometryRef())
        pytest.fail('Geometry update failed')

    feat.SetGeometryDirectly(None)

    if gdaltest.pg_lyr.SetFeature(feat) != 0:
        feat.Destroy()
        pytest.fail('SetFeature() method failed.')

    feat.Destroy()

    feat = gdaltest.pg_lyr.GetFeature(fid)
    assert feat.GetGeometryRef() is None, \
        'Geometry update failed. null geometry expected'

    feat.SetFieldNull('SHORTNAME')
    gdaltest.pg_lyr.SetFeature(feat)
    feat = gdaltest.pg_lyr.GetFeature(fid)
    assert feat.IsFieldNull('SHORTNAME'), 'SHORTNAME update failed. null value expected'

    # Test updating non-existing feature
    feat.SetFID(-10)
    if gdaltest.pg_lyr.SetFeature(feat) != ogr.OGRERR_NON_EXISTING_FEATURE:
        feat.Destroy()
        pytest.fail('Expected failure of SetFeature().')

    feat.Destroy()

###############################################################################
# Verify that DeleteFeature() works properly.


def test_ogr_pg_10():

    if gdaltest.pg_ds is None:
        pytest.skip()

    gdaltest.pg_lyr.SetAttributeFilter("PRFEDEA = 'CrazyKey'")
    feat = gdaltest.pg_lyr.GetNextFeature()
    gdaltest.pg_lyr.SetAttributeFilter(None)

    fid = feat.GetFID()
    feat.Destroy()

    assert gdaltest.pg_lyr.DeleteFeature(fid) == 0, 'DeleteFeature() method failed.'

    gdaltest.pg_lyr.SetAttributeFilter("PRFEDEA = 'CrazyKey'")
    feat = gdaltest.pg_lyr.GetNextFeature()
    gdaltest.pg_lyr.SetAttributeFilter(None)

    if feat is not None:
        feat.Destroy()
        pytest.fail('DeleteFeature() seems to have had no effect.')

    # Test deleting non-existing feature
    assert gdaltest.pg_lyr.DeleteFeature(-10) == ogr.OGRERR_NON_EXISTING_FEATURE, \
        'Expected failure of DeleteFeature().'

###############################################################################
# Create table from data/poly.shp in INSERT mode.


def test_ogr_pg_11():

    if gdaltest.pg_ds is None:
        pytest.skip()

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:tpolycopy')
    gdal.PopErrorHandler()

    gdal.SetConfigOption('PG_USE_COPY', 'NO')

    ######################################################
    # Create Layer
    gdaltest.pgc_lyr = gdaltest.pg_ds.CreateLayer('tpolycopy',
                                                  options=['DIM=3'])

    ######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def(gdaltest.pgc_lyr,
                                   [('AREA', ogr.OFTReal),
                                    ('EAS_ID', ogr.OFTInteger),
                                    ('PRFEDEA', ogr.OFTString),
                                    ('SHORTNAME', ogr.OFTString, 8)])

    ######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature(feature_def=gdaltest.pgc_lyr.GetLayerDefn())

    shp_ds = ogr.Open('data/poly.shp')
    shp_lyr = shp_ds.GetLayer(0)

    feat = shp_lyr.GetNextFeature()
    gdaltest.poly_feat = []

    while feat is not None:

        gdaltest.poly_feat.append(feat)

        dst_feat.SetFrom(feat)
        gdaltest.pgc_lyr.CreateFeature(dst_feat)

        feat = shp_lyr.GetNextFeature()

    dst_feat.Destroy()

    gdal.SetConfigOption('PG_USE_COPY', gdaltest.pg_use_copy)

###############################################################################
# Verify that stuff we just wrote is still OK.


def test_ogr_pg_12():
    if gdaltest.pg_ds is None:
        pytest.skip()

    gdaltest.pgc_lyr.ResetReading()
    gdaltest.pgc_lyr.SetAttributeFilter(None)

    for i in range(len(gdaltest.poly_feat)):
        orig_feat = gdaltest.poly_feat[i]
        read_feat = gdaltest.pgc_lyr.GetNextFeature()

        assert (ogrtest.check_feature_geometry(read_feat, orig_feat.GetGeometryRef(),
                                          max_error=0.001) == 0)

        for fld in range(3):
            assert orig_feat.GetField(fld) == read_feat.GetField(fld), \
                ('Attribute %d does not match' % fld)

        read_feat.Destroy()
        orig_feat.Destroy()

    gdaltest.poly_feat = None
    gdaltest.pgc_lyr.ResetReading()  # to close implicit transaction

###############################################################################
# Create a table with some date fields.


def test_ogr_pg_13():

    if gdaltest.pg_ds is None:
        pytest.skip()

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:datetest')
    gdal.PopErrorHandler()

    ######################################################
    # Create Table
    lyr = gdaltest.pg_ds.CreateLayer('datetest')

    ######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def(lyr, [('ogrdate', ogr.OFTDate),
                                         ('ogrtime', ogr.OFTTime),
                                         ('ogrdatetime', ogr.OFTDateTime)])

    ######################################################
    # add some custom date fields.
    gdaltest.pg_ds.ExecuteSQL('ALTER TABLE datetest ADD COLUMN tsz timestamp with time zone')
    gdaltest.pg_ds.ExecuteSQL('ALTER TABLE datetest ADD COLUMN ts timestamp without time zone')
    gdaltest.pg_ds.ExecuteSQL('ALTER TABLE datetest ADD COLUMN dt date')
    gdaltest.pg_ds.ExecuteSQL('ALTER TABLE datetest ADD COLUMN tm time')

    ######################################################
    # Create a populated records.
    gdaltest.pg_ds.ExecuteSQL("INSERT INTO datetest ( ogrdate, ogrtime, ogrdatetime, tsz, ts, dt, tm) VALUES ( '2005-10-12 10:41:33-05', '2005-10-12 10:41:33-05', '2005-10-12 10:41:33-05', '2005-10-12 10:41:33-05','2005-10-12 10:41:33-05','2005-10-12 10:41:33-05','2005-10-12 10:41:33-05' )")

###############################################################################
# Verify that stuff we just wrote is still OK.
# Fetch in several timezones to test our timezone processing.


def test_ogr_pg_14():
    if gdaltest.pg_ds is None:
        pytest.skip()

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)

    ds.ExecuteSQL('set timezone to "UTC"')

    lyr = ds.GetLayerByName('datetest')

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('ogrdatetime') != '2005/10/12 15:41:33+00' \
       or feat.GetFieldAsString('ogrdate') != '2005/10/12' \
       or feat.GetFieldAsString('ogrtime') != '10:41:33' \
       or feat.GetFieldAsString('tsz') != '2005/10/12 15:41:33+00' \
       or feat.GetFieldAsString('ts') != '2005/10/12 10:41:33' \
       or feat.GetFieldAsString('dt') != '2005/10/12' \
       or feat.GetFieldAsString('tm') != '10:41:33':
        feat.DumpReadable()
        pytest.fail('UTC value wrong')

    sql_lyr = ds.ExecuteSQL("select * from pg_timezone_names where name = 'Canada/Newfoundland'")
    if sql_lyr is None:
        has_tz = True
    else:
        has_tz = sql_lyr.GetFeatureCount() != 0
        ds.ReleaseResultSet(sql_lyr)

    if has_tz:
        ds.ExecuteSQL('set timezone to "Canada/Newfoundland"')

        lyr.ResetReading()

        feat = lyr.GetNextFeature()

        if feat.GetFieldAsString('ogrdatetime') != '2005/10/12 13:11:33-0230' \
                or feat.GetFieldAsString('tsz') != '2005/10/12 13:11:33-0230' \
                or feat.GetFieldAsString('ts') != '2005/10/12 10:41:33' \
                or feat.GetFieldAsString('dt') != '2005/10/12' \
                or feat.GetFieldAsString('tm') != '10:41:33':
            feat.DumpReadable()
            pytest.fail('Newfoundland value wrong')

    ds.ExecuteSQL('set timezone to "+5"')

    lyr.ResetReading()

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('ogrdatetime') != '2005/10/12 20:41:33+05' \
       or feat.GetFieldAsString('tsz') != '2005/10/12 20:41:33+05':
        feat.DumpReadable()
        pytest.fail('+5 value wrong')

    feat = None
    ds.Destroy()

###############################################################################
# Test very large query.


def test_ogr_pg_15():

    if gdaltest.pg_ds is None:
        pytest.skip()

    expect = [169]

    query = 'eas_id = 169'

    for ident in range(1000):
        query = query + (' or eas_id = %d' % (ident + 1000))

    gdaltest.pg_lyr.SetAttributeFilter(query)
    tr = ogrtest.check_features_against_list(gdaltest.pg_lyr,
                                             'eas_id', expect)
    gdaltest.pg_lyr.SetAttributeFilter(None)

    assert tr


###############################################################################
# Test very large statement.

def test_ogr_pg_16():

    if gdaltest.pg_ds is None:
        pytest.skip()

    expect = [169]

    query = 'eas_id = 169'

    for ident in range(1000):
        query = query + (' or eas_id = %d' % (ident + 1000))

    statement = 'select eas_id from tpoly where ' + query

    lyr = gdaltest.pg_ds.ExecuteSQL(statement)

    tr = ogrtest.check_features_against_list(lyr, 'eas_id', expect)

    gdaltest.pg_ds.ReleaseResultSet(lyr)

    assert tr

###############################################################################
# Test requesting a non-existent table by name (bug 1480).


def test_ogr_pg_17():

    if gdaltest.pg_ds is None:
        pytest.skip()

    count = gdaltest.pg_ds.GetLayerCount()
    try:
        layer = gdaltest.pg_ds.GetLayerByName('JunkTableName')
    except:
        layer = None

    assert layer is None, 'got layer for non-existent table!'

    assert count == gdaltest.pg_ds.GetLayerCount(), 'layer count changed unexpectedly.'

###############################################################################
# Test getting a layer by name that was not previously a layer.


def test_ogr_pg_18():

    if gdaltest.pg_ds is None or not gdaltest.pg_has_postgis:
        pytest.skip()

    count = gdaltest.pg_ds.GetLayerCount()
    layer = gdaltest.pg_ds.GetLayerByName('geometry_columns')
    assert layer is not None, 'did not get geometry_columns layer'

    assert count + 1 == gdaltest.pg_ds.GetLayerCount(), \
        'layer count unexpectedly unchanged.'


###############################################################################
# Test reading 4-dim geometry in EWKT format


def test_ogr_pg_20():

    if gdaltest.pg_ds is None or not gdaltest.pg_has_postgis:
        pytest.skip()

    #
    # Prepare test layer with 4-dim geometries.
    #

    # Collection of test geometry pairs:
    # ( <EWKT>, <WKT> ) <=> ( <tested>, <expected> )
    geometries = (
        ('POINT (10 20 5 5)',
         'POINT ZM (10 20 5 5)'),
        ('LINESTRING (10 10 1 2,20 20 3 4,30 30 5 6,40 40 7 8)',
         'LINESTRING ZM (10 10 1 2,20 20 3 4,30 30 5 6,40 40 7 8)'),
        ('POLYGON ((0 0 1 2,4 0 3 4,4 4 5 6,0 4 7 8,0 0 1 2))',
         'POLYGON ZM ((0 0 1 2,4 0 3 4,4 4 5 6,0 4 7 8,0 0 1 2))'),
        ('MULTIPOINT (10 20 5 5,30 30 7 7)',
         'MULTIPOINT ZM ((10 20 5 5),(30 30 7 7))'),
        ('MULTILINESTRING ((10 10 1 2,20 20 3 4),(30 30 5 6,40 40 7 8))',
         'MULTILINESTRING ZM ((10 10 1 2,20 20 3 4),(30 30 5 6,40 40 7 8))'),
        ('MULTIPOLYGON(((0 0 0 1,4 0 0 1,4 4 0 1,0 4 0 1,0 0 0 1),(1 1 0 5,2 1 0 5,2 2 0 5,1 2 0 5,1 1 0 5)),((-1 -1 0 10,-1 -2 0 10,-2 -2 0 10,-2 -1 0 10,-1 -1 0 10)))',
         'MULTIPOLYGON ZM (((0 0 0 1,4 0 0 1,4 4 0 1,0 4 0 1,0 0 0 1),(1 1 0 5,2 1 0 5,2 2 0 5,1 2 0 5,1 1 0 5)),((-1 -1 0 10,-1 -2 0 10,-2 -2 0 10,-2 -1 0 10,-1 -1 0 10)))'),
        ('GEOMETRYCOLLECTION(POINT(2 3 11 101),LINESTRING(2 3 12 102,3 4 13 103))',
         'GEOMETRYCOLLECTION ZM (POINT ZM (2 3 11 101),LINESTRING ZM (2 3 12 102,3 4 13 103))'),
        ('TRIANGLE ((0 0 0 0,100 0 100 1,0 100 100 0,0 0 0 0))',
         'TRIANGLE ZM ((0 0 0 0,100 0 100 1,0 100 100 0,0 0 0 0))'),
        ('TIN (((0 0 0 0,0 0 1 0,0 1 0 0,0 0 0 0)),((0 0 0 0,0 1 0 0,1 1 0 0,0 0 0 0)))',
         'TIN ZM (((0 0 0 0,0 0 1 0,0 1 0 0,0 0 0 0)),((0 0 0 0,0 1 0 0,1 1 0 0,0 0 0 0)))'),
        ('POLYHEDRALSURFACE (((0 0 0 0,0 0 1 0,0 1 1 0,0 1 0 0,0 0 0 0)),((0 0 0 0,0 1 0 0,1 1 0 0,1 0 0 0,0 0 0 0)),((0 0 0 0,1 0 0 0,1 0 1 0,0 0 1 0,0 0 0 0)),((1 1 0 0,1 1 1 0,1 0 1 0,1 0 0 0,1 1 0 0)),((0 1 0 0,0 1 1 0,1 1 1 0,1 1 0 0,0 1 0 0)),((0 0 1 0,1 0 1 0,1 1 1 0,0 1 1 0,0 0 1 0)))',
         'POLYHEDRALSURFACE ZM (((0 0 0 0,0 0 1 0,0 1 1 0,0 1 0 0,0 0 0 0)),((0 0 0 0,0 1 0 0,1 1 0 0,1 0 0 0,0 0 0 0)),((0 0 0 0,1 0 0 0,1 0 1 0,0 0 1 0,0 0 0 0)),((1 1 0 0,1 1 1 0,1 0 1 0,1 0 0 0,1 1 0 0)),((0 1 0 0,0 1 1 0,1 1 1 0,1 1 0 0,0 1 0 0)),((0 0 1 0,1 0 1 0,1 1 1 0,0 1 1 0,0 0 1 0)))')
    )

    # This layer is also used in ogr_pg_21() test.
    gdaltest.pg_ds.ExecuteSQL("CREATE TABLE testgeom (ogc_fid integer)")

    # XXX - mloskot - if 'public' is omitted, then OGRPGDataSource::DeleteLayer fails, line 438
    sql_lyr = gdaltest.pg_ds.ExecuteSQL("SELECT AddGeometryColumn('public','testgeom','wkb_geometry',-1,'GEOMETRY',4)")
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)
    for i, geom in enumerate(geometries):
        gdaltest.pg_ds.ExecuteSQL("INSERT INTO testgeom (ogc_fid,wkb_geometry) \
                                    VALUES (%d,GeomFromEWKT('%s'))" % (i, geom[0]))

    # We need to re-read layers
    gdaltest.pg_ds.Destroy()
    gdaltest.pg_ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    assert gdaltest.pg_ds is not None, 'can not re-open datasource'

    #
    # Test reading 4-dim geometries normalized to OGC WKT form.
    #

    layer = gdaltest.pg_ds.GetLayerByName('testgeom')
    assert layer is not None, 'did not get testgeom layer'

    # Test updating the geometries
    for i in range(len(geometries)):
        feat = layer.GetFeature(i)
        layer.SetFeature(feat)

    # Test we get them back as expected
    for i, geoms in enumerate(geometries):
        feat = layer.GetFeature(i)
        geom = feat.GetGeometryRef()
        assert geom is not None, ('did not get geometry, expected %s' % geoms[1])
        wkt = geom.ExportToIsoWkt()
        feat.Destroy()
        feat = None

        assert wkt == geoms[1], \
            ('WKT do not match: expected %s, got %s' % (geoms[1], wkt))

    layer = None

###############################################################################
# Test reading 4-dimension geometries in EWKB format


def test_ogr_pg_21():

    if gdaltest.pg_ds is None or not gdaltest.pg_has_postgis:
        pytest.skip()

    layer = gdaltest.pg_ds.ExecuteSQL("SELECT wkb_geometry FROM testgeom")
    assert layer is not None, 'did not get testgeom layer'

    feat = layer.GetNextFeature()
    while feat is not None:
        geom = feat.GetGeometryRef()
        if ogr.GT_HasZ(geom.GetGeometryType()) == 0 or ogr.GT_HasM(geom.GetGeometryType()) == 0:
            feat.Destroy()
            feat = None
            gdaltest.pg_ds.ReleaseResultSet(layer)
            layer = None
            pytest.fail('expected feature with type >3000')

        feat.Destroy()
        feat = layer.GetNextFeature()

    feat = None
    gdaltest.pg_ds.ReleaseResultSet(layer)
    layer = None

###############################################################################
# Check if the sub geometries of TIN and POLYHEDRALSURFACE are valid


def test_ogr_pg_21_subgeoms():

    if gdaltest.pg_ds is None or not gdaltest.pg_has_postgis:
        pytest.skip()

    subgeom_PS = ['POLYGON ZM ((0 0 0 0,0 0 1 0,0 1 1 0,0 1 0 0,0 0 0 0))',
                  'POLYGON ZM ((0 0 0 0,0 1 0 0,1 1 0 0,1 0 0 0,0 0 0 0))',
                  'POLYGON ZM ((0 0 0 0,1 0 0 0,1 0 1 0,0 0 1 0,0 0 0 0))',
                  'POLYGON ZM ((1 1 0 0,1 1 1 0,1 0 1 0,1 0 0 0,1 1 0 0))',
                  'POLYGON ZM ((0 1 0 0,0 1 1 0,1 1 1 0,1 1 0 0,0 1 0 0))',
                  'POLYGON ZM ((0 0 1 0,1 0 1 0,1 1 1 0,0 1 1 0,0 0 1 0))']

    subgeom_TIN = ['TRIANGLE ZM ((0 0 0 0,0 0 1 0,0 1 0 0,0 0 0 0))',
                   'TRIANGLE ZM ((0 0 0 0,0 1 0 0,1 1 0 0,0 0 0 0))']

    layer = gdaltest.pg_ds.GetLayerByName('testgeom')
    for i in range(8, 10):
        feat = layer.GetFeature(i)
        geom = feat.GetGeometryRef()
        assert geom is not None, 'did not get the expected geometry'
        if geom.GetGeometryName() == "POLYHEDRALSURFACE":
            for j in range(0, geom.GetGeometryCount()):
                sub_geom = geom.GetGeometryRef(j)
                subgeom_wkt = sub_geom.ExportToIsoWkt()
                assert subgeom_wkt == subgeom_PS[j], \
                    ('did not get the expected subgeometry, expected %s' % (subgeom_PS[j]))
        if geom.GetGeometryName() == "TIN":
            for j in range(0, geom.GetGeometryCount()):
                sub_geom = geom.GetGeometryRef(j)
                subgeom_wkt = sub_geom.ExportToIsoWkt()
                assert subgeom_wkt == subgeom_TIN[j], \
                    ('did not get the expected subgeometry, expected %s' % (subgeom_TIN[j]))
        feat.Destroy()
        feat = None

    
###############################################################################
# Check if the 3d geometries of TIN, Triangle and POLYHEDRALSURFACE are valid


def test_ogr_pg_21_3d_geometries(with_and_without_postgis):

    if gdaltest.pg_ds is None or not with_and_without_postgis:
        pytest.skip()

    gdaltest.pg_ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)

    gdaltest.pg_ds.ExecuteSQL("CREATE TABLE zgeoms (field_no integer)")
    sql_lyr = gdaltest.pg_ds.ExecuteSQL("SELECT AddGeometryColumn('public','zgeoms','wkb_geometry',-1,'GEOMETRY',3)")
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)
    wkt_list = ['POLYHEDRALSURFACE (((0 0 0,0 0 1,0 1 1,0 1 0,0 0 0)),((0 0 0,0 1 0,1 1 0,1 0 0,0 0 0)),((0 0 0,1 0 0,1 0 1,0 0 1,0 0 0)),((1 1 0,1 1 1,1 0 1,1 0 0,1 1 0)),((0 1 0,0 1 1,1 1 1,1 1 0,0 1 0)),((0 0 1,1 0 1,1 1 1,0 1 1,0 0 1)))',
                'TIN (((0 0 0,0 0 1,0 1 0,0 0 0)),((0 0 0,0 1 0,1 1 0,0 0 0)))',
                'TRIANGLE ((48 36 84,32 54 64,86 11 54,48 36 84))']

    wkt_expected = ['POLYHEDRALSURFACE Z (((0 0 0,0 0 1,0 1 1,0 1 0,0 0 0)),((0 0 0,0 1 0,1 1 0,1 0 0,0 0 0)),((0 0 0,1 0 0,1 0 1,0 0 1,0 0 0)),((1 1 0,1 1 1,1 0 1,1 0 0,1 1 0)),((0 1 0,0 1 1,1 1 1,1 1 0,0 1 0)),((0 0 1,1 0 1,1 1 1,0 1 1,0 0 1)))',
                    'TIN Z (((0 0 0,0 0 1,0 1 0,0 0 0)),((0 0 0,0 1 0,1 1 0,0 0 0)))',
                    'TRIANGLE Z ((48 36 84,32 54 64,86 11 54,48 36 84))']

    for i in range(0, 3):
        gdaltest.pg_ds.ExecuteSQL("INSERT INTO zgeoms (field_no, wkb_geometry) VALUES (%d,GeomFromEWKT('%s'))" % (i, wkt_list[i]))

    gdaltest.pg_ds.Destroy()
    gdaltest.pg_ds = None

    gdaltest.pg_ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    assert gdaltest.pg_ds is not None, 'Cannot open the dataset'

    layer = gdaltest.pg_ds.GetLayerByName('zgeoms')
    assert layer is not None, 'No layer received'

    for i in range(0, 3):
        feat = layer.GetFeature(i)
        geom = feat.GetGeometryRef()

        wkt = geom.ExportToIsoWkt()

        assert wkt == wkt_expected[i], \
            ('Unexpected WKT, expected %s and got %s' % (wkt_expected[i], wkt))

    gdaltest.pg_ds.ExecuteSQL("DROP TABLE zgeoms")

###############################################################################
# Create table from data/poly.shp under specified SCHEMA
# This test checks if schema support and schema name quoting works well.


def test_ogr_pg_22():

    if gdaltest.pg_ds is None:
        pytest.skip()

    ######################################################
    # Create Schema

    schema_name = 'AutoTest-schema'
    layer_name = schema_name + '.tpoly'

    gdaltest.pg_ds.ExecuteSQL('CREATE SCHEMA \"' + schema_name + '\"')

    ######################################################
    # Create Layer
    gdaltest.pg_lyr = gdaltest.pg_ds.CreateLayer(layer_name,
                                                 options=[
                                                     'DIM=3',
                                                     'SCHEMA=' + schema_name]
                                                )

    ######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def(gdaltest.pg_lyr,
                                   [('AREA', ogr.OFTReal),
                                    ('EAS_ID', ogr.OFTInteger),
                                    ('PRFEDEA', ogr.OFTString),
                                    ('SHORTNAME', ogr.OFTString, 8)])

    ######################################################
    # Copy 3 features from the poly.shp

    dst_feat = ogr.Feature(feature_def=gdaltest.pg_lyr.GetLayerDefn())

    shp_ds = ogr.Open('data/poly.shp')
    shp_lyr = shp_ds.GetLayer(0)

    # Insert 3 features only
    for ident in range(0, 3):
        feat = shp_lyr.GetFeature(ident)
        dst_feat.SetFrom(feat)
        gdaltest.pg_lyr.CreateFeature(dst_feat)

    dst_feat.Destroy()

    # Test if test layer under custom schema is listed

    found = ogr_pg_check_layer_in_list(gdaltest.pg_ds, layer_name)

    assert found is not False, ('layer from schema \'' + schema_name + '\' not listed')

###############################################################################
# Create table with all data types


def test_ogr_pg_23():

    if gdaltest.pg_ds is None:
        pytest.skip()

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:datatypetest')
    gdal.PopErrorHandler()

    ######################################################
    # Create Table
    lyr = gdaltest.pg_ds.CreateLayer('datatypetest')

    ######################################################
    # Setup Schema
    # ogrtest.quick_create_layer_def( lyr, None )

    ######################################################
    # add some custom date fields.
    gdaltest.pg_ds.ExecuteSQL('ALTER TABLE datatypetest ADD COLUMN my_numeric numeric')
    gdaltest.pg_ds.ExecuteSQL('ALTER TABLE datatypetest ADD COLUMN my_numeric5 numeric(5)')
    gdaltest.pg_ds.ExecuteSQL('ALTER TABLE datatypetest ADD COLUMN my_numeric5_3 numeric(5,3)')
    # gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE datatypetest ADD COLUMN my_bool bool' )
    fld = ogr.FieldDefn('my_bool', ogr.OFTInteger)
    fld.SetSubType(ogr.OFSTBoolean)
    lyr.CreateField(fld)
    # gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE datatypetest ADD COLUMN my_int2 int2' )
    fld = ogr.FieldDefn('my_int2', ogr.OFTInteger)
    fld.SetSubType(ogr.OFSTInt16)
    lyr.CreateField(fld)
    gdaltest.pg_ds.ExecuteSQL('ALTER TABLE datatypetest ADD COLUMN my_int4 int4')
    lyr.CreateField(ogr.FieldDefn('my_int8', ogr.OFTInteger64))
    # gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE datatypetest ADD COLUMN my_float4 float4' )
    fld = ogr.FieldDefn('my_float4', ogr.OFTReal)
    fld.SetSubType(ogr.OFSTFloat32)
    lyr.CreateField(fld)
    gdaltest.pg_ds.ExecuteSQL('ALTER TABLE datatypetest ADD COLUMN my_float8 float8')
    gdaltest.pg_ds.ExecuteSQL('ALTER TABLE datatypetest ADD COLUMN my_real real')
    gdaltest.pg_ds.ExecuteSQL('ALTER TABLE datatypetest ADD COLUMN my_char char')
    gdaltest.pg_ds.ExecuteSQL('ALTER TABLE datatypetest ADD COLUMN my_varchar character varying')
    gdaltest.pg_ds.ExecuteSQL('ALTER TABLE datatypetest ADD COLUMN my_varchar10 character varying(10)')
    gdaltest.pg_ds.ExecuteSQL('ALTER TABLE datatypetest ADD COLUMN my_text text')
    gdaltest.pg_ds.ExecuteSQL('ALTER TABLE datatypetest ADD COLUMN my_bytea bytea')
    gdaltest.pg_ds.ExecuteSQL('ALTER TABLE datatypetest ADD COLUMN my_time time')
    gdaltest.pg_ds.ExecuteSQL('ALTER TABLE datatypetest ADD COLUMN my_date date')
    gdaltest.pg_ds.ExecuteSQL('ALTER TABLE datatypetest ADD COLUMN my_timestamp timestamp without time zone')
    gdaltest.pg_ds.ExecuteSQL('ALTER TABLE datatypetest ADD COLUMN my_timestamptz timestamp with time zone')
    gdaltest.pg_ds.ExecuteSQL('ALTER TABLE datatypetest ADD COLUMN my_chararray char(1)[]')
    gdaltest.pg_ds.ExecuteSQL('ALTER TABLE datatypetest ADD COLUMN my_textarray text[]')
    gdaltest.pg_ds.ExecuteSQL('ALTER TABLE datatypetest ADD COLUMN my_varchararray character varying[]')
    fld = ogr.FieldDefn('my_int2array', ogr.OFTIntegerList)
    fld.SetSubType(ogr.OFSTInt16)
    lyr.CreateField(fld)
    gdaltest.pg_ds.ExecuteSQL('ALTER TABLE datatypetest ADD COLUMN my_int4array int4[]')
    lyr.CreateField(ogr.FieldDefn('my_int8array', ogr.OFTInteger64List))
    fld = ogr.FieldDefn('my_float4array', ogr.OFTRealList)
    fld.SetSubType(ogr.OFSTFloat32)
    lyr.CreateField(fld)
    gdaltest.pg_ds.ExecuteSQL('ALTER TABLE datatypetest ADD COLUMN my_float8array float8[]')
    gdaltest.pg_ds.ExecuteSQL('ALTER TABLE datatypetest ADD COLUMN my_numericarray numeric[]')
    gdaltest.pg_ds.ExecuteSQL('ALTER TABLE datatypetest ADD COLUMN my_numeric5array numeric(5)[]')
    gdaltest.pg_ds.ExecuteSQL('ALTER TABLE datatypetest ADD COLUMN my_numeric5_3array numeric(5,3)[]')
    fld = ogr.FieldDefn('my_boolarray', ogr.OFTIntegerList)
    fld.SetSubType(ogr.OFSTBoolean)
    lyr.CreateField(fld)
    ######################################################
    # Create a populated records.

    if gdaltest.pg_has_postgis:
        geom_str = "GeomFromEWKT('POINT(10 20)')"
    else:
        geom_str = "'\\\\001\\\\001\\\\000\\\\000\\\\000\\\\000\\\\000\\\\000\\\\000\\\\000\\\\000$@\\\\000\\\\000\\\\000\\\\000\\\\000\\\\0004@'"
        if gdaltest.pg_quote_with_E:
            geom_str = "E" + geom_str
    sql = "INSERT INTO datatypetest ( my_numeric, my_numeric5, my_numeric5_3, my_bool, my_int2, "
    sql += "my_int4, my_int8, my_float4, my_float8, my_real, my_char, my_varchar, "
    sql += "my_varchar10, my_text, my_bytea, my_time, my_date, my_timestamp, my_timestamptz, "
    sql += "my_chararray, my_textarray, my_varchararray, my_int2array, my_int4array, "
    sql += "my_int8array, my_float4array, my_float8array, my_numericarray, my_numeric5array, my_numeric5_3array, my_boolarray, wkb_geometry) "
    sql += "VALUES ( 1.2, 12345, 0.123, 'T', 12345, 12345678, 1234567901234, 0.123, "
    sql += "0.12345678, 0.876, 'a', 'ab', 'varchar10 ', 'abc', 'xyz', '12:34:56', "
    sql += "'2000-01-01', '2000-01-01 00:00:00', '2000-01-01 00:00:00+00', "
    sql += "'{a,b}', "
    sql += "'{aa,bb}', '{cc,dd}', '{100,200}', '{100,200}', '{1234567901234}', "
    sql += "'{100.1,200.1}', '{100.12,200.12}', ARRAY[100.12,200.12], ARRAY[10,20], ARRAY[10.12,20.12], '{1,0}', " + geom_str + " )"
    gdaltest.pg_ds.ExecuteSQL(sql)

###############################################################################


def check_value_23(layer_defn, feat):

    field_defn = layer_defn.GetFieldDefn(layer_defn.GetFieldIndex("my_numeric5"))
    assert field_defn.GetWidth() == 5 and field_defn.GetPrecision() == 0 and field_defn.GetType() == ogr.OFTInteger, \
        ('Wrong field defn for my_numeric5 : %d, %d, %d' % (field_defn.GetWidth(), field_defn.GetPrecision(), field_defn.GetType()))

    field_defn = layer_defn.GetFieldDefn(layer_defn.GetFieldIndex("my_numeric5_3"))
    assert field_defn.GetWidth() == 5 and field_defn.GetPrecision() == 3 and field_defn.GetType() == ogr.OFTReal, \
        ('Wrong field defn for my_numeric5_3 : %d, %d, %d' % (field_defn.GetWidth(), field_defn.GetPrecision(), field_defn.GetType()))

    field_defn = layer_defn.GetFieldDefn(layer_defn.GetFieldIndex("my_varchar10"))
    assert field_defn.GetWidth() == 10 and field_defn.GetPrecision() == 0, \
        ('Wrong field defn for my_varchar10 : %d, %d, %d' % (field_defn.GetWidth(), field_defn.GetPrecision(), field_defn.GetType()))

    field_defn = layer_defn.GetFieldDefn(layer_defn.GetFieldIndex("my_bool"))
    assert field_defn.GetWidth() == 1 and field_defn.GetType() == ogr.OFTInteger and field_defn.GetSubType() == ogr.OFSTBoolean, \
        ('Wrong field defn for my_bool : %d, %d, %d, %d' % (field_defn.GetWidth(), field_defn.GetPrecision(), field_defn.GetType(), field_defn.GetSubType()))

    field_defn = layer_defn.GetFieldDefn(layer_defn.GetFieldIndex("my_boolarray"))
    assert field_defn.GetType() == ogr.OFTIntegerList and field_defn.GetSubType() == ogr.OFSTBoolean, \
        ('Wrong field defn for my_boolarray : %d, %d, %d, %d' % (field_defn.GetWidth(), field_defn.GetPrecision(), field_defn.GetType(), field_defn.GetSubType()))

    field_defn = layer_defn.GetFieldDefn(layer_defn.GetFieldIndex("my_int2"))
    assert field_defn.GetType() == ogr.OFTInteger and field_defn.GetSubType() == ogr.OFSTInt16, \
        ('Wrong field defn for my_int2 : %d, %d, %d, %d' % (field_defn.GetWidth(), field_defn.GetPrecision(), field_defn.GetType(), field_defn.GetSubType()))

    field_defn = layer_defn.GetFieldDefn(layer_defn.GetFieldIndex("my_float4"))
    assert field_defn.GetType() == ogr.OFTReal and field_defn.GetSubType() == ogr.OFSTFloat32, \
        ('Wrong field defn for my_float4 : %d, %d, %d, %d' % (field_defn.GetWidth(), field_defn.GetPrecision(), field_defn.GetType(), field_defn.GetSubType()))

    field_defn = layer_defn.GetFieldDefn(layer_defn.GetFieldIndex("my_int2array"))
    assert field_defn.GetType() == ogr.OFTIntegerList and field_defn.GetSubType() == ogr.OFSTInt16, \
        ('Wrong field defn for my_int2array : %d, %d, %d, %d' % (field_defn.GetWidth(), field_defn.GetPrecision(), field_defn.GetType(), field_defn.GetSubType()))

    field_defn = layer_defn.GetFieldDefn(layer_defn.GetFieldIndex("my_float4array"))
    assert field_defn.GetType() == ogr.OFTRealList and field_defn.GetSubType() == ogr.OFSTFloat32, \
        ('Wrong field defn for my_float4array : %d, %d, %d, %d' % (field_defn.GetWidth(), field_defn.GetPrecision(), field_defn.GetType(), field_defn.GetSubType()))

    if feat.my_numeric != pytest.approx(1.2, abs=1e-8) or \
        feat.my_numeric5 != 12345 or \
        feat.my_numeric5_3 != 0.123 or \
        feat.my_bool != 1 or \
        feat.my_int2 != 12345 or \
        feat.my_int4 != 12345678 or \
        feat.my_int8 != 1234567901234 or \
        feat.my_float4 != pytest.approx(0.123, abs=1e-8) or \
        feat.my_float8 != 0.12345678 or \
        feat.my_real != pytest.approx(0.876, abs=1e-6) or \
        feat.my_char != 'a' or \
        feat.my_varchar != 'ab' or \
        feat.my_varchar10 != 'varchar10 ' or \
        feat.my_text != 'abc' or \
        feat.GetFieldAsString('my_bytea') != '78797A' or \
        feat.GetFieldAsString('my_time') != '12:34:56' or \
        feat.GetFieldAsString('my_date') != '2000/01/01' or \
        (feat.GetFieldAsString('my_timestamp') != '2000/01/01 00:00:00' and feat.GetFieldAsString('my_timestamp') != '2000/01/01 00:00:00+00') or \
        (layer_defn.GetFieldIndex('my_timestamptz') >= 0 and feat.GetFieldAsString('my_timestamptz') != '2000/01/01 00:00:00+00') or \
        feat.GetFieldAsString('my_chararray') != '(2:a,b)' or \
        feat.GetFieldAsString('my_textarray') != '(2:aa,bb)' or \
        feat.GetFieldAsString('my_varchararray') != '(2:cc,dd)' or \
        feat.GetFieldAsString('my_int2array') != '(2:100,200)' or \
        feat.GetFieldAsString('my_int4array') != '(2:100,200)' or \
        feat.my_int8array != [1234567901234] or \
        feat.GetFieldAsString('my_boolarray') != '(2:1,0)' or \
        feat.my_float4array[0] != pytest.approx(100.1, abs=1e-6) or \
        feat.my_float8array[0] != pytest.approx(100.12, abs=1e-8) or \
        feat.my_numericarray[0] != pytest.approx(100.12, abs=1e-8) or \
        feat.my_numeric5array[0] != pytest.approx(10, abs=1e-8) or \
            feat.my_numeric5_3array[0] != pytest.approx(10.12, abs=1e-8):
        feat.DumpReadable()
        pytest.fail('Wrong values')

    geom = feat.GetGeometryRef()
    assert geom is not None, 'geom is none'

    wkt = geom.ExportToWkt()
    assert wkt == 'POINT (10 20)', ('Wrong WKT :' + wkt)

###############################################################################
# Test with PG: connection


def test_ogr_pg_24():

    if gdaltest.pg_ds is None:
        pytest.skip()

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)

    ds.ExecuteSQL('set timezone to "UTC"')

    lyr = ds.GetLayerByName('datatypetest')

    feat = lyr.GetNextFeature()
    check_value_23(lyr.GetLayerDefn(), feat)

    feat = None

    ds.Destroy()

###############################################################################
# Test with PG: connection and SELECT query


def test_ogr_pg_25():

    if gdaltest.pg_ds is None:
        pytest.skip()

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)

    ds.ExecuteSQL('set timezone to "UTC"')

    sql_lyr = ds.ExecuteSQL('select * from datatypetest')

    feat = sql_lyr.GetNextFeature()
    check_value_23(sql_lyr.GetLayerDefn(), feat)

    ds.ReleaseResultSet(sql_lyr)

    feat = None

    ds.Destroy()

###############################################################################
# Duplicate all data types in INSERT mode


def test_ogr_pg_28():

    if gdaltest.pg_ds is None:
        pytest.skip()

    gdal.SetConfigOption('PG_USE_COPY', "NO")

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds.ExecuteSQL('DELLAYER:datatypetest2')
    gdal.PopErrorHandler()

    ds.ExecuteSQL('set timezone to "UTC"')

    src_lyr = ds.GetLayerByName('datatypetest')

    dst_lyr = ds.CreateLayer('datatypetest2')

    src_lyr.ResetReading()

    for i in range(src_lyr.GetLayerDefn().GetFieldCount()):
        field_defn = src_lyr.GetLayerDefn().GetFieldDefn(i)
        dst_lyr.CreateField(field_defn)

    dst_feat = ogr.Feature(feature_def=dst_lyr.GetLayerDefn())

    feat = src_lyr.GetNextFeature()
    assert feat is not None

    dst_feat.SetFrom(feat)
    assert dst_lyr.CreateFeature(dst_feat) == 0, 'CreateFeature failed.'

    dst_feat.Destroy()

    src_lyr = None
    dst_lyr = None

    ds.Destroy()

    gdal.SetConfigOption('PG_USE_COPY', gdaltest.pg_use_copy)

###############################################################################
# Test with PG: connection


def test_ogr_pg_29():

    if gdaltest.pg_ds is None:
        pytest.skip()

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)

    ds.ExecuteSQL('set timezone to "UTC"')

    lyr = ds.GetLayerByName('datatypetest2')

    # my_timestamp has now a time zone...
    feat = lyr.GetNextFeature()
    check_value_23(lyr.GetLayerDefn(), feat)

    geom = feat.GetGeometryRef()
    wkt = geom.ExportToWkt()
    assert wkt == 'POINT (10 20)', ('Wrong WKT :' + wkt)

    feat = None

    ds.Destroy()

###############################################################################
# Duplicate all data types in PG_USE_COPY mode


def test_ogr_pg_30():

    if gdaltest.pg_ds is None:
        pytest.skip()

    gdal.SetConfigOption('PG_USE_COPY', 'YES')

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds.ExecuteSQL('DELLAYER:datatypetest2')
    gdal.PopErrorHandler()

    ds.ExecuteSQL('set timezone to "UTC"')

    src_lyr = ds.GetLayerByName('datatypetest')

    dst_lyr = ds.CreateLayer('datatypetest2')

    src_lyr.ResetReading()

    for i in range(src_lyr.GetLayerDefn().GetFieldCount()):
        field_defn = src_lyr.GetLayerDefn().GetFieldDefn(i)
        dst_lyr.CreateField(field_defn)

    dst_feat = ogr.Feature(feature_def=dst_lyr.GetLayerDefn())

    feat = src_lyr.GetNextFeature()
    assert feat is not None

    dst_feat.SetFrom(feat)
    assert dst_lyr.CreateFeature(dst_feat) == 0, 'CreateFeature failed.'

    dst_feat.Destroy()

    ds.Destroy()

    gdal.SetConfigOption('PG_USE_COPY', gdaltest.pg_use_copy)


###############################################################################
# Test the tables= connection string option

def test_ogr_pg_31():

    if gdaltest.pg_ds is None:
        pytest.skip()

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    lyr = gdaltest.pg_ds.CreateLayer('test_for_tables_equal_param', geom_type=ogr.wkbPoint, srs=srs, options=['OVERWRITE=YES'])
    lyr.StartTransaction()
    for i in range(501):
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(0 0)'))
        lyr.CreateFeature(f)
    lyr.CommitTransaction()

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string + ' tables=tpoly,tpolycopy', update=1)

    assert ds is not None and ds.GetLayerCount() == 2

    sql_lyr = ds.ExecuteSQL('SELECT * FROM test_for_tables_equal_param')
    i = 0
    while True:
        f = sql_lyr.GetNextFeature()
        if f is None:
            break
        i = i + 1
    ds.ReleaseResultSet(sql_lyr)
    assert i == 501

    ds.Destroy()

###############################################################################
# Test approximate srtext (#2123, #3508)


def test_ogr_pg_32():

    if gdaltest.pg_ds is None or not gdaltest.pg_has_postgis:
        pytest.skip()

    gdaltest.pg_ds.ExecuteSQL("DELETE FROM spatial_ref_sys")

    ######################################################
    # Create Layer with EPSG:4326
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)

    gdaltest.pg_lyr = gdaltest.pg_ds.CreateLayer('testsrtext', srs=srs)

    sql_lyr = gdaltest.pg_ds.ExecuteSQL("SELECT COUNT(*) FROM spatial_ref_sys")
    feat = sql_lyr.GetNextFeature()
    if feat.count != 1:
        feat.DumpReadable()
        pytest.fail('did not get expected count after step (1)')
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

    ######################################################
    # Create second layer with very approximative EPSG:4326

    srs = osr.SpatialReference()
    srs.SetFromUserInput('GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433],AUTHORITY["EPSG","4326"]]')
    gdaltest.pg_lyr = gdaltest.pg_ds.CreateLayer('testsrtext2', srs=srs)

    # Must still be 1
    sql_lyr = gdaltest.pg_ds.ExecuteSQL("SELECT COUNT(*) FROM spatial_ref_sys")
    feat = sql_lyr.GetNextFeature()
    if feat.count != 1:
        feat.DumpReadable()
        pytest.fail('did not get expected count after step (2)')
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

    ######################################################
    # Create third layer with very approximative EPSG:4326 but without authority

    srs = osr.SpatialReference()
    srs.SetFromUserInput("""GEOGCS["GCS_WGS_1984",DATUM["WGS_1984",SPHEROID["WGS_1984",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["Degree",0.017453292519943295]]""")
    gdaltest.pg_lyr = gdaltest.pg_ds.CreateLayer('testsrtext3', srs=srs)

    # Must still be 1
    sql_lyr = gdaltest.pg_ds.ExecuteSQL("SELECT COUNT(*) FROM spatial_ref_sys")
    feat = sql_lyr.GetNextFeature()
    if feat.count != 1:
        feat.DumpReadable()
        pytest.fail('did not get expected count after step (3)')
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

    ######################################################
    # Create Layer with EPSG:26632

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(26632)

    gdaltest.pg_lyr = gdaltest.pg_ds.CreateLayer('testsrtext4', geom_type=ogr.wkbPoint, srs=srs)
    feat = ogr.Feature(gdaltest.pg_lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(0 0)'))
    gdaltest.pg_lyr.CreateFeature(feat)
    feat = None
    sr = gdaltest.pg_lyr.GetSpatialRef()
    assert sr.ExportToWkt().find('26632') != -1, 'did not get expected SRS'

    sql_lyr = gdaltest.pg_ds.ExecuteSQL("SELECT COUNT(*) FROM spatial_ref_sys")
    feat = sql_lyr.GetNextFeature()
    # Must be 2 now
    if feat.count != 2:
        feat.DumpReadable()
        pytest.fail('did not get expected count after step (4)')
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

    ######################################################
    # Test GetSpatialRef() on SQL layer (#4644)

    sql_lyr = gdaltest.pg_ds.ExecuteSQL('SELECT * FROM testsrtext4')
    sr = sql_lyr.GetSpatialRef()
    assert sr.ExportToWkt().find('26632') != -1, 'did not get expected SRS'
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

    ######################################################
    # Test getting SRS and geom type without requiring to fetch the layer defn

    for i in range(2):
        # sys.stderr.write('BEFORE OPEN\n')
        ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
        # sys.stderr.write('AFTER Open\n')
        lyr = ds.GetLayerByName('testsrtext4')
        # sys.stderr.write('AFTER GetLayerByName\n')
        if i == 0:
            sr = lyr.GetSpatialRef()
            # sys.stderr.write('AFTER GetSpatialRef\n')
            geom_type = lyr.GetGeomType()
            # sys.stderr.write('AFTER GetGeomType\n')
        else:
            geom_type = lyr.GetGeomType()
            # sys.stderr.write('AFTER GetGeomType\n')
            sr = lyr.GetSpatialRef()
            # sys.stderr.write('AFTER GetSpatialRef\n')

        assert sr.ExportToWkt().find('26632') != -1, 'did not get expected SRS'
        assert geom_type == ogr.wkbPoint, 'did not get expected geom type'

        ds = None

    ######################################################
    # Create Layer with non EPSG SRS

    srs = osr.SpatialReference()
    srs.SetFromUserInput('+proj=vandg')

    gdaltest.pg_lyr = gdaltest.pg_ds.CreateLayer('testsrtext5', srs=srs)

    sql_lyr = gdaltest.pg_ds.ExecuteSQL("SELECT COUNT(*) FROM spatial_ref_sys")
    feat = sql_lyr.GetNextFeature()
    # Must be 3 now
    if feat.count != 3:
        feat.DumpReadable()
        pytest.fail('did not get expected count after step (5)')
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

###############################################################################
# Test encoding as UTF8


def test_ogr_pg_33():

    if gdaltest.pg_ds is None:
        pytest.skip()

    gdaltest.pg_lyr = gdaltest.pg_ds.GetLayerByName('tpoly')
    assert gdaltest.pg_lyr is not None, 'did not get tpoly layer'

    dst_feat = ogr.Feature(feature_def=gdaltest.pg_lyr.GetLayerDefn())
    # eacute in UTF8 : 0xc3 0xa9
    dst_feat.SetField('SHORTNAME', '\xc3\xa9')
    gdaltest.pg_lyr.CreateFeature(dst_feat)
    dst_feat.Destroy()

###############################################################################
# Test encoding as Latin1


def test_ogr_pg_34():

    if gdaltest.pg_ds is None:
        pytest.skip()

    # We only test that on Linux since setting os.environ['XXX']
    # is not guaranteed to have effects on system not supporting putenv
    if sys.platform.startswith('linux'):
        os.environ['PGCLIENTENCODING'] = 'LATIN1'
        test_ogr_pg_1()
        del os.environ['PGCLIENTENCODING']

        # For some unknown reasons, some servers don't like forcing LATIN1
        # but prefer LATIN9 instead...
        if gdaltest.pg_ds is None:
            os.environ['PGCLIENTENCODING'] = 'LATIN9'
            test_ogr_pg_1()
            del os.environ['PGCLIENTENCODING']
    else:
        gdaltest.pg_ds.ExecuteSQL('SET client_encoding = LATIN1')

    gdaltest.pg_lyr = gdaltest.pg_ds.GetLayerByName('tpoly')
    assert gdaltest.pg_lyr is not None, 'did not get tpoly layer'

    dst_feat = ogr.Feature(feature_def=gdaltest.pg_lyr.GetLayerDefn())
    # eacute in Latin1 : 0xe9
    dst_feat.SetField('SHORTNAME', '\xe9')
    gdaltest.pg_lyr.CreateFeature(dst_feat)
    dst_feat.Destroy()


###############################################################################
# Test for buffer overflows

def test_ogr_pg_35():

    if gdaltest.pg_ds is None:
        pytest.skip()

    gdal.PushErrorHandler()
    try:
        gdaltest.pg_lyr = gdaltest.pg_ds.CreateLayer('testoverflows')
        ogrtest.quick_create_layer_def(gdaltest.pg_lyr, [('0123456789' * 1000, ogr.OFTReal)])
        # To trigger actual layer creation
        gdaltest.pg_lyr.ResetReading()
    except:
        pass
    finally:
        gdal.PopErrorHandler()

    gdal.PushErrorHandler()
    try:
        gdaltest.pg_lyr = gdaltest.pg_ds.CreateLayer('testoverflows', options=['OVERWRITE=YES', 'GEOMETRY_NAME=' + ('0123456789' * 1000)])
        # To trigger actual layer creation
        gdaltest.pg_lyr.ResetReading()
    except:
        pass
    finally:
        gdal.PopErrorHandler()

    
###############################################################################
# Test support for inherited tables : tables inherited from a Postgis Table


def test_ogr_pg_36():

    if gdaltest.pg_ds is None:
        pytest.skip()

    if gdaltest.pg_has_postgis:
        lyr = gdaltest.pg_ds.CreateLayer('table36_base', geom_type=ogr.wkbPoint)
    else:
        lyr = gdaltest.pg_ds.CreateLayer('table36_base')

    gdaltest.pg_ds.ExecuteSQL('CREATE TABLE table36_inherited ( col1 CHAR(1) ) INHERITS ( table36_base )')
    gdaltest.pg_ds.ExecuteSQL('CREATE TABLE table36_inherited2 ( col2 CHAR(1) ) INHERITS ( table36_inherited )')

    # Test fix for #3636 when 2 inherited tables with same name exist in 2 different schemas
    if gdaltest.pg_has_postgis:
        # lyr = gdaltest.pg_ds.CreateLayer( 'table36_base', geom_type = ogr.wkbLineString, options = ['SCHEMA=AutoTest-schema'] )
        lyr = gdaltest.pg_ds.CreateLayer('AutoTest-schema.table36_base', geom_type=ogr.wkbLineString)
    else:
        lyr = gdaltest.pg_ds.CreateLayer('table36_base', options=['SCHEMA=AutoTest-schema'])

    gdaltest.pg_ds.ExecuteSQL('CREATE TABLE "AutoTest-schema"."table36_inherited" ( col3 CHAR(1) ) INHERITS ( "AutoTest-schema".table36_base )')
    gdaltest.pg_ds.ExecuteSQL('CREATE TABLE "AutoTest-schema"."table36_inherited2" ( col4 CHAR(1) ) INHERITS ( "AutoTest-schema".table36_inherited )')

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)

    found = ogr_pg_check_layer_in_list(ds, 'table36_inherited')
    assert found is not False, 'layer table36_inherited not listed'

    found = ogr_pg_check_layer_in_list(ds, 'table36_inherited2')
    assert found is not False, 'layer table36_inherited2 not listed'

    lyr = ds.GetLayerByName('table36_inherited2')
    assert lyr is not None
    assert not gdaltest.pg_has_postgis or lyr.GetGeomType() == ogr.wkbPoint, \
        'wrong geometry type for layer table36_inherited2'

    lyr = ds.GetLayerByName('AutoTest-schema.table36_inherited2')
    assert lyr is not None
    assert not gdaltest.pg_has_postgis or lyr.GetGeomType() == ogr.wkbLineString, \
        'wrong geometry type for layer AutoTest-schema.table36_inherited2'

    ds.Destroy()


def test_ogr_pg_36_bis():

    if gdaltest.pg_ds is None:
        pytest.skip()

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string + ' TABLES=table36_base', update=1)

    found = ogr_pg_check_layer_in_list(ds, 'table36_inherited')
    assert found is not True, 'layer table36_inherited is listed but it should not'

    lyr = ds.GetLayerByName('table36_inherited')
    assert lyr is not None
    assert not gdaltest.pg_has_postgis or lyr.GetGeomType() == ogr.wkbPoint

    ds.Destroy()

###############################################################################
# Test support for inherited tables : Postgis table inherited from a non-Postgis table


def test_ogr_pg_37():

    if gdaltest.pg_ds is None or not gdaltest.pg_has_postgis:
        pytest.skip()

    gdaltest.pg_ds.ExecuteSQL('CREATE TABLE table37_base ( col1 CHAR(1) )')
    gdaltest.pg_ds.ExecuteSQL('CREATE TABLE table37_inherited ( col2 CHAR(1) ) INHERITS ( table37_base )')
    sql_lyr = gdaltest.pg_ds.ExecuteSQL("SELECT AddGeometryColumn('public','table37_inherited','wkb_geometry',-1,'POINT',2)")
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)

    found = ogr_pg_check_layer_in_list(ds, 'table37_inherited')
    assert found is not False, 'layer table37_inherited not listed'

    lyr = ds.GetLayerByName('table37_inherited')
    assert lyr is not None
    assert not gdaltest.pg_has_postgis or lyr.GetGeomType() == ogr.wkbPoint

    ds.Destroy()

###############################################################################
# Test support for multiple geometry columns (RFC 41)


def test_ogr_pg_38():
    if gdaltest.pg_ds is None or not gdaltest.pg_has_postgis:
        pytest.skip()

    sql_lyr = gdaltest.pg_ds.ExecuteSQL("SELECT AddGeometryColumn('public','table37_base','pointBase',-1,'POINT',2)")
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

    sql_lyr = gdaltest.pg_ds.ExecuteSQL("SELECT AddGeometryColumn('public','table37_inherited','point25D',-1,'POINT',3)")
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)

    # Check for the layer with the wkb_geometry column
    found = ogr_pg_check_layer_in_list(ds, 'table37_inherited')
    assert found is not False, 'layer table37_inherited not listed'

    lyr = ds.GetLayerByName('table37_inherited')
    assert lyr is not None
    gfld_defn = lyr.GetLayerDefn().GetGeomFieldDefn(lyr.GetLayerDefn().GetGeomFieldIndex("wkb_geometry"))
    assert gfld_defn is not None
    assert gfld_defn.GetType() == ogr.wkbPoint
    if lyr.GetLayerDefn().GetGeomFieldCount() != 3:
        for i in range(lyr.GetLayerDefn().GetGeomFieldCount()):
            print(lyr.GetLayerDefn().GetGeomFieldDefn(i).GetName())
        pytest.fail(lyr.GetLayerDefn().GetGeomFieldCount())

    # Explicit query to 'table37_inherited(wkb_geometry)' should also work
    lyr = ds.GetLayerByName('table37_inherited(wkb_geometry)')
    assert lyr is not None

    lyr = ds.GetLayerByName('table37_inherited(pointBase)')
    assert lyr is not None
    assert lyr.GetGeomType() == ogr.wkbPoint
    assert lyr.GetGeometryColumn() == 'pointBase', 'wrong geometry column name'

    lyr = ds.GetLayerByName('table37_inherited(point25D)')
    assert lyr is not None
    assert lyr.GetGeomType() == ogr.wkbPoint25D
    assert lyr.GetGeometryColumn() == 'point25D', 'wrong geometry column name'

    ds.Destroy()

    # Check for the layer with the new 'point25D' geometry column when tables= is specified
    ds = ogr.Open('PG:' + gdaltest.pg_connection_string + ' tables=table37_inherited(point25D)', update=1)

    lyr = ds.GetLayerByName('table37_inherited(point25D)')
    assert lyr is not None
    assert lyr.GetGeomType() == ogr.wkbPoint25D
    assert lyr.GetGeometryColumn() == 'point25D', 'wrong geometry column name'

    ds.Destroy()

###############################################################################
# Test support for named views


def test_ogr_pg_39():
    if gdaltest.pg_ds is None:
        pytest.skip()

    if not gdaltest.pg_has_postgis:
        gdaltest.pg_ds.ExecuteSQL("CREATE VIEW testview AS SELECT * FROM table36_base")
        ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
        found = ogr_pg_check_layer_in_list(ds, 'testview')
        assert found is not False, 'layer testview not listed'
        ds.Destroy()
        return

    gdaltest.pg_ds.ExecuteSQL("CREATE VIEW testview AS SELECT * FROM table37_inherited")
    if not gdaltest.pg_has_postgis_2:
        gdaltest.pg_ds.ExecuteSQL("INSERT INTO geometry_columns VALUES ( '', 'public', 'testview', 'wkb_geometry', 2, -1, 'POINT') ")
    gdaltest.pg_ds.ExecuteSQL("INSERT INTO table37_inherited (col1, col2, wkb_geometry) VALUES ( 'a', 'b', GeomFromEWKT('POINT (0 1)') )")

    # Check for the layer
    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    found = ogr_pg_check_layer_in_list(ds, 'testview')
    assert found is not False, 'layer testview not listed'

    lyr = ds.GetLayerByName('testview')
    assert lyr is not None
    if gdaltest.pg_has_postgis:
        gfld_defn = lyr.GetLayerDefn().GetGeomFieldDefn(lyr.GetLayerDefn().GetGeomFieldIndex("wkb_geometry"))
        assert gfld_defn is not None
        assert gfld_defn.GetType() == ogr.wkbPoint

    feat = lyr.GetNextFeature()
    assert feat is not None, 'no feature'

    assert feat.GetGeomFieldRef("wkb_geometry") is not None and feat.GetGeomFieldRef("wkb_geometry").ExportToWkt() == 'POINT (0 1)', \
        ('bad geometry %s' % feat.GetGeometryRef().ExportToWkt())

    ds.Destroy()

    # Test another geometry column
    if not gdaltest.pg_has_postgis_2:
        gdaltest.pg_ds.ExecuteSQL("INSERT INTO geometry_columns VALUES ( '', 'public', 'testview', 'point25D', 3, -1, 'POINT') ")
    gdaltest.pg_ds.ExecuteSQL("UPDATE table37_inherited SET \"point25D\" = GeomFromEWKT('POINT (0 1 2)') ")

    # Check for the layer
    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    found = ogr_pg_check_layer_in_list(ds, 'testview')
    assert found is not False, 'layer testview not listed'

    lyr = ds.GetLayerByName('testview(point25D)')
    assert lyr is not None
    assert not gdaltest.pg_has_postgis or lyr.GetGeomType() == ogr.wkbPoint25D

    try:
        assert lyr.GetGeometryColumn() == 'point25D', 'wrong geometry column name'
    except:
        pass

    feat = lyr.GetNextFeature()
    assert feat is not None, 'no feature'

    assert feat.GetGeometryRef() is not None and feat.GetGeometryRef().ExportToWkt() == 'POINT (0 1 2)', \
        ('bad geometry %s' % feat.GetGeometryRef().ExportToWkt())

    ds.Destroy()

###############################################################################
# Test GetFeature() with an invalid id


def test_ogr_pg_40():
    if gdaltest.pg_ds is None:
        pytest.skip()

    layer = gdaltest.pg_ds.GetLayerByName('tpoly')
    assert layer.GetFeature(0) is None

###############################################################################
# Test active_schema= option


def test_ogr_pg_41():
    if gdaltest.pg_ds is None:
        pytest.skip()

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string + ' active_schema=AutoTest-schema', update=1)

    # tpoly without schema refers to the active schema, that is to say AutoTest-schema
    found = ogr_pg_check_layer_in_list(ds, 'tpoly')
    assert found is not False, 'layer tpoly not listed'

    layer = ds.GetLayerByName('tpoly')
    assert layer.GetFeatureCount() == 3, 'wrong feature count'

    found = ogr_pg_check_layer_in_list(ds, 'AutoTest-schema.tpoly')
    assert found is not True, 'layer AutoTest-schema.tpoly is listed, but should not'

    layer = ds.GetLayerByName('AutoTest-schema.tpoly')
    assert layer.GetFeatureCount() == 3, 'wrong feature count'

    found = ogr_pg_check_layer_in_list(ds, 'public.tpoly')
    assert found is not False, 'layer tpoly not listed'

    layer = ds.GetLayerByName('public.tpoly')
    assert layer.GetFeatureCount() == 19, 'wrong feature count'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:test41')
    gdal.PopErrorHandler()

    ds.CreateLayer('test41')

    found = ogr_pg_check_layer_in_list(ds, 'test41')
    assert found is not False, 'layer test41 not listed'

    layer = ds.GetLayerByName('test41')
    assert layer.GetFeatureCount() == 0, 'wrong feature count'

    layer = ds.GetLayerByName('AutoTest-schema.test41')
    assert layer.GetFeatureCount() == 0, 'wrong feature count'

    ds.Destroy()

###############################################################################
# Test schemas= option


def test_ogr_pg_42():
    if gdaltest.pg_ds is None:
        pytest.skip()

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string + ' schemas=AutoTest-schema', update=1)

    # tpoly without schema refers to the active schema, that is to say AutoTest-schema
    found = ogr_pg_check_layer_in_list(ds, 'tpoly')
    assert found is not False, 'layer tpoly not listed'

    layer = ds.GetLayerByName('tpoly')
    assert layer.GetFeatureCount() == 3, 'wrong feature count'

    found = ogr_pg_check_layer_in_list(ds, 'AutoTest-schema.tpoly')
    assert found is not True, 'layer AutoTest-schema.tpoly is listed, but should not'

    layer = ds.GetLayerByName('AutoTest-schema.tpoly')
    assert layer.GetFeatureCount() == 3, 'wrong feature count'

    found = ogr_pg_check_layer_in_list(ds, 'public.tpoly')
    assert found is not True, 'layer public.tpoly is listed, but should not'

    layer = ds.GetLayerByName('public.tpoly')
    assert layer.GetFeatureCount() == 19, 'wrong feature count'

    found = ogr_pg_check_layer_in_list(ds, 'test41')
    assert found is not False, 'layer test41 not listed'

    layer = ds.GetLayerByName('test41')
    assert layer.GetFeatureCount() == 0, 'wrong feature count'

    layer = ds.GetLayerByName('AutoTest-schema.test41')
    assert layer.GetFeatureCount() == 0, 'wrong feature count'

    ds.Destroy()


###############################################################################
# Test schemas= option

def test_ogr_pg_43():
    if gdaltest.pg_ds is None:
        pytest.skip()

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string + " application_name='foo\\\\ \\'bar' schemas = 'public,AutoTest-schema'", update=1)

    # tpoly without schema refers to the active schema, that is to say public
    found = ogr_pg_check_layer_in_list(ds, 'tpoly')
    assert found is not False, 'layer tpoly not listed'

    layer = ds.GetLayerByName('tpoly')
    assert layer.GetFeatureCount() == 19, 'wrong feature count'

    found = ogr_pg_check_layer_in_list(ds, 'AutoTest-schema.tpoly')
    assert found is not False, 'layer AutoTest-schema.tpoly not listed'

    layer = ds.GetLayerByName('AutoTest-schema.tpoly')
    assert layer.GetFeatureCount() == 3, 'wrong feature count'

    ds.Destroy()

###############################################################################
# Test for table and column names that need quoting (#2945)


def test_ogr_pg_44():

    if gdaltest.pg_ds is None:
        pytest.skip()

    gdaltest.pg_lyr = gdaltest.pg_ds.CreateLayer('select', options=['OVERWRITE=YES', 'GEOMETRY_NAME=where', 'DIM=3'])
    ogrtest.quick_create_layer_def(gdaltest.pg_lyr, [('from', ogr.OFTReal)])
    feat = ogr.Feature(gdaltest.pg_lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (0.5 0.5 1)'))
    gdaltest.pg_lyr.CreateFeature(feat)
    feat.Destroy()

    gdaltest.pg_ds.ExecuteSQL('ALTER TABLE "select" RENAME COLUMN "ogc_fid" to "AND"')

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    layer = ds.GetLayerByName('select')
    geom = ogr.CreateGeometryFromWkt('POLYGON((0 0,0 1,1 1,1 0,0 0))')
    layer.SetSpatialFilter(geom)
    geom.Destroy()
    assert layer.GetFeatureCount() == 1
    feat = layer.GetNextFeature()
    assert feat.GetGeometryRef().ExportToWkt() == 'POINT (0.5 0.5 1)'

    feat = layer.GetFeature(1)
    assert feat.GetGeometryRef().ExportToWkt() == 'POINT (0.5 0.5 1)'

    sql_lyr = ds.ExecuteSQL('SELECT * FROM "select"')
    geom = ogr.CreateGeometryFromWkt('POLYGON((0 0,0 1,1 1,1 0,0 0))')
    sql_lyr.SetSpatialFilter(geom)
    geom.Destroy()
    assert sql_lyr.GetFeatureCount() == 1
    feat = sql_lyr.GetNextFeature()
    assert feat.GetGeometryRef().ExportToWkt() == 'POINT (0.5 0.5 1)'
    ds.ReleaseResultSet(sql_lyr)

    ds.Destroy()

###############################################################################
# Test SetNextByIndex (#3117)


def test_ogr_pg_45():

    if gdaltest.pg_ds is None:
        pytest.skip()

    lyr = gdaltest.pg_ds.GetLayerByName('tpoly')

    assert lyr.TestCapability(ogr.OLCFastSetNextByIndex), \
        'OLCFastSetNextByIndex returned false'

    nb_feat = lyr.GetFeatureCount()
    tab_feat = [None for i in range(nb_feat)]
    for i in range(nb_feat):
        tab_feat[i] = lyr.GetNextFeature()

    lyr.SetNextByIndex(2)
    feat = lyr.GetNextFeature()
    assert feat.GetFID() == tab_feat[2].GetFID(), \
        'SetNextByIndex(2) did not return expected feature'

    feat = lyr.GetNextFeature()
    assert feat.GetFID() == tab_feat[3].GetFID(), 'did not get expected feature'

###############################################################################
# Test that we can read more than 500 features and update each one
# with SetFeature()


def test_ogr_pg_46():

    if gdaltest.pg_ds is None:
        pytest.skip()

    nFeatures = 1000

    # Create a table with nFeatures records
    lyr = gdaltest.pg_ds.CreateLayer('bigtable')
    field_defn = ogr.FieldDefn("field1", ogr.OFTInteger)
    lyr.CreateField(field_defn)
    field_defn.Destroy()

    feature_defn = lyr.GetLayerDefn()

    lyr.StartTransaction()
    for i in range(nFeatures):
        feat = ogr.Feature(feature_defn)
        feat.SetField(0, i)
        lyr.CreateFeature(feat)
        feat.Destroy()
    lyr.CommitTransaction()

    # Check that we can read more than 500 features and update each one
    # with SetFeature()
    count = 0
    sqllyr = gdaltest.pg_ds.ExecuteSQL('SELECT * FROM bigtable ORDER BY OGC_FID ASC')
    feat = sqllyr.GetNextFeature()
    while feat is not None:
        expected_val = count
        assert feat.GetFieldAsInteger(0) == expected_val, \
            ('expected value was %d. Got %d' % (expected_val, feat.GetFieldAsInteger(0)))
        feat.SetField(0, -count)
        lyr.SetFeature(feat)
        feat.Destroy()

        count = count + 1

        feat = sqllyr.GetNextFeature()

    assert count == nFeatures, ('did not get expected %d features' % nFeatures)

    # Check that 1 feature has been correctly updated
    sqllyr.SetNextByIndex(1)
    feat = sqllyr.GetNextFeature()
    expected_val = -1
    assert feat.GetFieldAsInteger(0) == expected_val, \
        ('expected value was %d. Got %d' % (expected_val, feat.GetFieldAsInteger(0)))
    feat.Destroy()

    gdaltest.pg_ds.ReleaseResultSet(sqllyr)

###############################################################################
# Test that we can handle 'geography' column type introduced in PostGIS 1.5


def test_ogr_pg_47():

    if gdaltest.pg_ds is None:
        pytest.skip()

    if not gdaltest.pg_has_postgis:
        pytest.skip()

    # Create table with geography column
    gdaltest.pg_ds.ExecuteSQL("DELETE FROM spatial_ref_sys")
    gdaltest.pg_ds.ExecuteSQL("""INSERT INTO "spatial_ref_sys" ("srid","auth_name","auth_srid","srtext","proj4text") VALUES (4326,'EPSG',4326,'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.01745329251994328,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4326"]]','+proj=longlat +ellps=WGS84 +datum=WGS84 +no_defs ')""")

    if gdaltest.pg_ds.GetLayerByName('geography_columns') is None:
        pytest.skip('autotest database must be created with PostGIS >= 1.5')

    gdaltest.pg_ds = None
    gdaltest.pg_ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    lyr = gdaltest.pg_ds.CreateLayer('test_geog', srs=srs, options=['GEOM_TYPE=geography', 'GEOMETRY_NAME=my_geog'])
    field_defn = ogr.FieldDefn("test_string", ogr.OFTString)
    lyr.CreateField(field_defn)
    field_defn.Destroy()

    feature_defn = lyr.GetLayerDefn()

    # Create feature
    feat = ogr.Feature(feature_defn)
    feat.SetField(0, "test_string")
    geom = ogr.CreateGeometryFromWkt('POINT (3 50)')
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)
    feat.Destroy()

    # Update feature
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    geom = ogr.CreateGeometryFromWkt('POINT (2 49)')
    feat.SetGeometry(geom)
    lyr.SetFeature(feat)
    feat.Destroy()

    # Re-open DB
    gdaltest.pg_ds.Destroy()
    gdaltest.pg_ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)

    # Check if the layer is listed
    found = ogr_pg_check_layer_in_list(gdaltest.pg_ds, 'test_geog')
    assert found is not False, 'layer test_geog not listed'

    # Check that the layer is recorder in geometry_columns table
    geography_columns_lyr = gdaltest.pg_ds.ExecuteSQL("SELECT * FROM geography_columns WHERE f_table_name = 'test_geog'")
    feat = geography_columns_lyr.GetNextFeature()
    if feat.GetFieldAsString('f_geography_column') != 'my_geog':
        feat.DumpReadable()
        pytest.fail()
    gdaltest.pg_ds.ReleaseResultSet(geography_columns_lyr)

    # Get the layer by name
    lyr = gdaltest.pg_ds.GetLayerByName('test_geog')
    assert lyr.GetExtent() == (2.0, 2.0, 49.0, 49.0), 'bad extent for test_geog'

    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    assert geom.ExportToWkt() == 'POINT (2 49)', 'bad geometry for test_geog'
    feat.Destroy()

    # Check with result set
    sql_lyr = gdaltest.pg_ds.ExecuteSQL('SELECT * FROM test_geog')
    assert sql_lyr.GetExtent() == (2.0, 2.0, 49.0, 49.0), \
        'bad extent for SELECT * FROM test_geog'

    feat = sql_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    assert geom.ExportToWkt() == 'POINT (2 49)', \
        'bad geometry for SELECT * FROM test_geog'
    feat.Destroy()
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

    # Check ST_AsText
    sql_lyr = gdaltest.pg_ds.ExecuteSQL('SELECT ST_AsText(my_geog) FROM test_geog')
    feat = sql_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    assert geom.ExportToWkt() == 'POINT (2 49)', \
        'bad geometry for SELECT ST_AsText(my_geog) FROM test_geog'
    feat.Destroy()
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

    # Check ST_AsBinary
    sql_lyr = gdaltest.pg_ds.ExecuteSQL('SELECT ST_AsBinary(my_geog) FROM test_geog')
    feat = sql_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    assert geom.ExportToWkt() == 'POINT (2 49)', \
        'bad geometry for SELECT ST_AsBinary(my_geog) FROM test_geog'
    feat.Destroy()
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

###############################################################################
# Test that we can read a table without any primary key (#2082)
# Test also the effect of PG_LIST_ALL_TABLES with a non spatial table in a
# PostGIS DB.


def test_ogr_pg_48():

    if gdaltest.pg_ds is None:
        pytest.skip()

    gdaltest.pg_ds.ExecuteSQL("CREATE TABLE no_pk_table (gid serial NOT NULL, other_id INTEGER)")
    gdaltest.pg_ds.ExecuteSQL("INSERT INTO no_pk_table (gid, other_id) VALUES (1, 10)")

    gdaltest.pg_ds.Destroy()
    gdaltest.pg_ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)

    found = ogr_pg_check_layer_in_list(gdaltest.pg_ds, 'no_pk_table')
    if gdaltest.pg_has_postgis:
        # Non spatial table in a PostGIS db -> not listed ...
        assert not found, 'layer no_pk_table unexpectedly listed'

        # ... but should be found on explicit request
        lyr = gdaltest.pg_ds.GetLayer('no_pk_table')
        assert lyr is not None, 'could not get no_pk_table'

        # Try again by setting PG_LIST_ALL_TABLES=YES
        gdal.SetConfigOption('PG_LIST_ALL_TABLES', 'YES')
        gdaltest.pg_ds.Destroy()
        gdaltest.pg_ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
        gdal.SetConfigOption('PG_LIST_ALL_TABLES', None)
        found = ogr_pg_check_layer_in_list(gdaltest.pg_ds, 'no_pk_table')

        assert found is not False, 'layer no_pk_table not listed'

        # Test LIST_ALL_TABLES=YES open option
        gdaltest.pg_ds.Destroy()
        gdaltest.pg_ds = gdal.OpenEx('PG:' + gdaltest.pg_connection_string, gdal.OF_VECTOR | gdal.OF_UPDATE, open_options=['LIST_ALL_TABLES=YES'])
        found = ogr_pg_check_layer_in_list(gdaltest.pg_ds, 'no_pk_table')

    assert found is not False, 'layer no_pk_table not listed'

    lyr = gdaltest.pg_ds.GetLayer('no_pk_table')
    assert lyr is not None, 'could not get no_pk_table'

    sr = lyr.GetSpatialRef()
    assert sr is None, 'did not get expected SRS'

    feat = lyr.GetNextFeature()
    assert feat is not None, 'did not get feature'

    assert lyr.GetFIDColumn() == '', 'got a non NULL FID column'

    if feat.GetFID() != 0:
        feat.DumpReadable()
        pytest.fail('did not get expected FID')

    if feat.GetFieldAsInteger('gid') != 1:
        feat.DumpReadable()
        pytest.fail('did not get expected gid')

    if feat.GetFieldAsInteger('other_id') != 10:
        feat.DumpReadable()
        pytest.fail('did not get expected other_id')

    
###############################################################################
# Go on with previous test but set PGSQL_OGR_FID this time


def test_ogr_pg_49():

    if gdaltest.pg_ds is None:
        pytest.skip()

    gdal.SetConfigOption('PGSQL_OGR_FID', 'other_id')
    gdaltest.pg_ds = None
    gdaltest.pg_ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    lyr = gdaltest.pg_ds.GetLayer('no_pk_table')
    gdal.SetConfigOption('PGSQL_OGR_FID', None)

    feat = lyr.GetNextFeature()
    lyr.ResetReading()  # to close implicit transaction

    assert lyr.GetFIDColumn() == 'other_id', 'did not get expected FID column'

    if feat.GetFID() != 10:
        feat.DumpReadable()
        pytest.fail('did not get expected FID')

    
###############################################################################
# Write and read NaN values (#3667)
# This tests writing using COPY and INSERT


def test_ogr_pg_50():

    if gdaltest.pg_ds is None:
        pytest.skip()

    gdaltest.pg_lyr = gdaltest.pg_ds.GetLayerByName('tpoly')
    assert gdaltest.pg_lyr is not None, 'did not get tpoly layer'

    feature_def = gdaltest.pg_lyr.GetLayerDefn()
    dst_feat = ogr.Feature(feature_def)

    try:
        dst_feat.SetFieldDoubleList
        bHasSetFieldDoubleList = True
    except:
        bHasSetFieldDoubleList = False

    for option in ['NO', 'YES']:
        gdal.SetConfigOption('PG_USE_COPY', option)
        gdaltest.pg_lyr.ResetReading()
        for value in ['NaN', 'Inf', '-Inf']:
            dst_feat.SetField('AREA', float(value))
            dst_feat.SetField('PRFEDEA', value)
            dst_feat.SetField('SHORTNAME', option)
            if bHasSetFieldDoubleList:
                dst_feat.SetFieldDoubleList(feature_def.GetFieldIndex('REALLIST'), [float(value), float(value)])
            dst_feat.SetFID(-1)
            gdaltest.pg_lyr.CreateFeature(dst_feat)

    gdal.SetConfigOption('PG_USE_COPY', gdaltest.pg_use_copy)
    dst_feat.Destroy()

    for option in ['NO', 'YES']:
        for value in ['NaN', 'Inf', '-Inf']:
            gdaltest.pg_lyr.SetAttributeFilter('PRFEDEA = \'' + value + '\' AND SHORTNAME = \'' + option + '\'')
            feat = gdaltest.pg_lyr.GetNextFeature()
            got_val = feat.GetField('AREA')
            if value == 'NaN':
                if not gdaltest.isnan(got_val):
                    gdaltest.pg_lyr.ResetReading()  # to close implicit transaction
                    pytest.fail(feat.GetFieldAsString('AREA') + ' returned for AREA instead of ' + value)
            elif got_val != float(value):
                gdaltest.pg_lyr.ResetReading()  # to close implicit transaction
                pytest.fail(feat.GetFieldAsString('AREA') + ' returned for AREA instead of ' + value)

            if bHasSetFieldDoubleList:
                got_val = feat.GetFieldAsDoubleList(feature_def.GetFieldIndex('REALLIST'))
                if value == 'NaN':
                    if not gdaltest.isnan(got_val[0]) or not gdaltest.isnan(got_val[1]):
                        gdaltest.pg_lyr.ResetReading()  # to close implicit transaction
                        pytest.fail(feat.GetFieldAsString('REALLIST') + ' returned for REALLIST instead of ' + value)
                elif got_val[0] != float(value) or got_val[1] != float(value):
                    gdaltest.pg_lyr.ResetReading()  # to close implicit transaction
                    pytest.fail(feat.GetFieldAsString('REALLIST') + ' returned for REALLIST instead of ' + value)

    gdaltest.pg_lyr.ResetReading()  # to close implicit transaction

###############################################################################
# Run test_ogrsf


def test_ogr_pg_51():

    if gdaltest.pg_ds is None:
        pytest.skip()

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' "' + 'PG:' + gdaltest.pg_connection_string + '" tpoly testview')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

###############################################################################
# Run test_ogrsf with -sql


def test_ogr_pg_52():

    if gdaltest.pg_ds is None:
        pytest.skip()

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' "' + 'PG:' + gdaltest.pg_connection_string + '" -sql "SELECT * FROM tpoly"')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

###############################################################################
# Test creating a layer with explicitly wkbNone geometry type.


def test_ogr_pg_53():

    if gdaltest.pg_ds is None:
        pytest.skip()

    lyr = gdaltest.pg_ds.CreateLayer('no_geometry_table', geom_type=ogr.wkbNone, options=['OVERWRITE=YES'])
    field_defn = ogr.FieldDefn('foo')
    lyr.CreateField(field_defn)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 'bar')
    lyr.CreateFeature(feat)

    lyr.ResetReading()  # force above feature to be committed

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string)

    assert not (gdaltest.pg_has_postgis is True and ogr_pg_check_layer_in_list(ds, 'no_geometry_table') is True), \
        'did not expected no_geometry_table to be listed at that point'

    lyr = ds.GetLayerByName('no_geometry_table')
    feat = lyr.GetNextFeature()
    assert feat.GetField(0) == 'bar'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr = ds.CreateLayer('no_geometry_table', geom_type=ogr.wkbNone)
    gdal.PopErrorHandler()
    assert lyr is None, 'layer creation should have failed'

    lyr = ds.CreateLayer('no_geometry_table', geom_type=ogr.wkbNone, options=['OVERWRITE=YES'])
    field_defn = ogr.FieldDefn('baz')
    lyr.CreateField(field_defn)

    ds = None
    ds = ogr.Open('PG:' + gdaltest.pg_connection_string)

    lyr = ds.CreateLayer('no_geometry_table', geom_type=ogr.wkbNone, options=['OVERWRITE=YES'])
    field_defn = ogr.FieldDefn('bar')
    lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn('baz')
    lyr.CreateField(field_defn)
    assert lyr is not None

    ds = None

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
    lyr = ds.GetLayerByName('no_geometry_table')
    assert lyr.GetLayerDefn().GetFieldCount() == 2

###############################################################################
# Check that we can overwrite a non-spatial geometry table (#4012)


def test_ogr_pg_53_bis():
    import test_cli_utilities
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    f = open('tmp/no_geometry_table.csv', 'wt')
    f.write('foo,bar\n')
    f.write('"baz","foo"\n')
    f.close()
    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f PostgreSQL "' + 'PG:' + gdaltest.pg_connection_string + '" tmp/no_geometry_table.csv -overwrite')

    os.unlink('tmp/no_geometry_table.csv')

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
    lyr = ds.GetLayerByName('no_geometry_table')
    feat = lyr.GetNextFeature()
    assert feat.GetField(0) == 'baz'

###############################################################################
# Test reading AsEWKB()


def test_ogr_pg_54():

    if gdaltest.pg_ds is None:
        pytest.skip()

    if not gdaltest.pg_has_postgis:
        pytest.skip()

    if gdaltest.pg_has_postgis_2:
        sql_lyr = gdaltest.pg_ds.ExecuteSQL("SELECT ST_AsEWKB(GeomFromEWKT('POINT (0 1 2)'))")
    else:
        sql_lyr = gdaltest.pg_ds.ExecuteSQL("SELECT AsEWKB(GeomFromEWKT('POINT (0 1 2)'))")
    feat = sql_lyr.GetNextFeature()
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

    geom = feat.GetGeometryRef()
    assert geom.ExportToWkt() == 'POINT (0 1 2)'

###############################################################################
# Test reading geoms as Base64 encoded strings


def test_ogr_pg_55():

    if gdaltest.pg_ds is None:
        pytest.skip()

    if not gdaltest.pg_has_postgis:
        pytest.skip()

    layer = gdaltest.pg_ds.CreateLayer('ogr_pg_55', options=['DIM=3'])
    feat = ogr.Feature(layer.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (1 2 3)'))
    layer.CreateFeature(feat)
    feat = None

    layer.ResetReading()  # force above feature to be committed

    old_val = gdal.GetConfigOption('PG_USE_BASE64')
    gdal.SetConfigOption('PG_USE_BASE64', 'YES')
    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    layer = ds.GetLayerByName('ogr_pg_55')
    feat = layer.GetNextFeature()
    gdal.SetConfigOption('PG_USE_BASE64', old_val)
    assert feat.GetGeometryRef().ExportToWkt() == 'POINT (1 2 3)'
    ds = None

###############################################################################
# Test insertion of features with first field being a 0-character string in a
# non-spatial table and without FID in COPY mode (#4040)


def test_ogr_pg_56():

    if gdaltest.pg_ds is None:
        pytest.skip()

    gdaltest.pg_ds.ExecuteSQL('CREATE TABLE ogr_pg_56 ( bar varchar, baz varchar ) WITH (OIDS=FALSE)')

    gdal.SetConfigOption('PG_USE_COPY', 'YES')

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    lyr = ds.GetLayerByName('ogr_pg_56')

    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, '')
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(1, '')
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, '')
    feat.SetField(1, '')
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 'bar')
    feat.SetField(1, '')
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, '')
    feat.SetField(1, 'baz')
    lyr.CreateFeature(feat)

    gdal.SetConfigOption('PG_USE_COPY', gdaltest.pg_use_copy)

    ds = None

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
    lyr = ds.GetLayerByName('ogr_pg_56')

    feat = lyr.GetNextFeature()
    if feat.GetField(0) is not None or feat.GetField(1) is not None:
        feat.DumpReadable()
        pytest.fail('did not get expected value for feat %d' % feat.GetFID())

    feat = lyr.GetNextFeature()
    if feat.GetField(0) != '' or feat.GetField(1) is not None:
        feat.DumpReadable()
        pytest.fail('did not get expected value for feat %d' % feat.GetFID())

    feat = lyr.GetNextFeature()
    if feat.GetField(0) is not None or feat.GetField(1) != '':
        feat.DumpReadable()
        pytest.fail('did not get expected value for feat %d' % feat.GetFID())

    feat = lyr.GetNextFeature()
    if feat.GetField(0) != '' or feat.GetField(1) != '':
        feat.DumpReadable()
        pytest.fail('did not get expected value for feat %d' % feat.GetFID())

    feat = lyr.GetNextFeature()
    if feat.GetField(0) != 'bar' or feat.GetField(1) != '':
        feat.DumpReadable()
        pytest.fail('did not get expected value for feat %d' % feat.GetFID())

    feat = lyr.GetNextFeature()
    if feat.GetField(0) != '' or feat.GetField(1) != 'baz':
        feat.DumpReadable()
        pytest.fail('did not get expected value for feat %d' % feat.GetFID())

    ds = None

###############################################################################
# Test inserting an empty feature


def test_ogr_pg_57():

    if gdaltest.pg_ds is None:
        pytest.skip()

    lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_57')
    lyr.CreateField(ogr.FieldDefn('acolumn', ogr.OFTString))
    feat = ogr.Feature(lyr.GetLayerDefn())
    ret = lyr.CreateFeature(feat)
    feat = None

    assert ret == 0

###############################################################################
# Test RFC35


def test_ogr_pg_58():

    if gdaltest.pg_ds is None:
        pytest.skip()

    lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_58')
    lyr.CreateField(ogr.FieldDefn('strcolumn', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('aintcolumn', ogr.OFTInteger))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('aintcolumn', 12345)
    lyr.CreateFeature(feat)
    feat = None

    assert lyr.TestCapability(ogr.OLCReorderFields) == 0
    assert lyr.TestCapability(ogr.OLCAlterFieldDefn) == 1
    assert lyr.TestCapability(ogr.OLCDeleteField) == 1

    fd = ogr.FieldDefn('anotherstrcolumn', ogr.OFTString)
    fd.SetWidth(5)
    lyr.AlterFieldDefn(lyr.GetLayerDefn().GetFieldIndex('aintcolumn'), fd, ogr.ALTER_ALL_FLAG)

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat.GetField('anotherstrcolumn') == '12345', 'failed (1)'

    gdaltest.pg_ds = None
    gdaltest.pg_ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    lyr = gdaltest.pg_ds.GetLayer('ogr_pg_58')

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    assert feat.GetField('anotherstrcolumn') == '12345', 'failed (2)'
    feat = None
    lyr.ResetReading()  # to close implicit transaction

    assert lyr.DeleteField(lyr.GetLayerDefn().GetFieldIndex('anotherstrcolumn')) == 0, \
        'failed (3)'

    gdaltest.pg_ds = None
    gdaltest.pg_ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    lyr = gdaltest.pg_ds.GetLayer('ogr_pg_58')

    assert lyr.GetLayerDefn().GetFieldCount() == 1, 'failed (4)'

###############################################################################
# Check that we can use -nln with a layer name that is recognized by GetLayerByName()
# but which is not the layer name.


def test_ogr_pg_59():

    if gdaltest.pg_ds is None:
        pytest.skip()

    import test_cli_utilities
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    gdaltest.runexternal(
        test_cli_utilities.get_ogr2ogr_path() +
        ' -append -f PostgreSQL "' + 'PG:' + gdaltest.pg_connection_string +
        '" data/poly.shp -nln public.tpoly')

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
    lyr = ds.GetLayerByName('tpoly')
    fc = lyr.GetFeatureCount()
    ds = None

    assert fc == 35, 'did not get expected feature count'

###############################################################################
# Test that we can insert a feature that has a FID on a table with a
# non-incrementing PK.


def test_ogr_pg_60():

    if gdaltest.pg_ds is None:
        pytest.skip()

    sql_lyr = gdaltest.pg_ds.ExecuteSQL("CREATE TABLE ogr_pg_60(id integer,"
                                        "name varchar(50),primary key (id)) "
                                        "without oids")
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

    gdaltest.pg_ds = None
    gdaltest.pg_ds = ogr.Open('PG:' + gdaltest.pg_connection_string,
                              update=1)
    lyr = gdaltest.pg_ds.GetLayerByName('ogr_pg_60')
    assert lyr.GetFIDColumn() == 'id', 'did not get expected name for FID column'

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetFID(100)
    feat.SetField('name', 'a_name')
    lyr.CreateFeature(feat)
    assert feat.GetFID() == 100, 'bad FID value'

    feat = lyr.GetFeature(100)
    assert feat is not None, 'did not get feature'

###############################################################################
# Test insertion of features with FID set in COPY mode (#4495)


def test_ogr_pg_61():

    if gdaltest.pg_ds is None:
        pytest.skip()

    gdaltest.pg_ds.ExecuteSQL('CREATE TABLE ogr_pg_61 ( id integer NOT NULL PRIMARY KEY, bar varchar )')

    gdal.SetConfigOption('PG_USE_COPY', 'YES')

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    lyr = ds.GetLayerByName('ogr_pg_61')

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetFID(10)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetFID(20)
    feat.SetField(0, 'baz')
    lyr.CreateFeature(feat)

    gdal.SetConfigOption('PG_USE_COPY', gdaltest.pg_use_copy)

    ds = None

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
    lyr = ds.GetLayerByName('ogr_pg_61')

    feat = lyr.GetFeature(10)
    if not feat.IsFieldNull(0):
        feat.DumpReadable()
        pytest.fail('did not get expected value for feat %d' % feat.GetFID())

    feat = lyr.GetFeature(20)
    if feat.GetField(0) != 'baz':
        feat.DumpReadable()
        pytest.fail('did not get expected value for feat %d' % feat.GetFID())

    ds = None

###############################################################################
# Test ExecuteSQL() and getting SRID of the request (#4699)


def test_ogr_pg_62():

    if gdaltest.pg_ds is None:
        pytest.skip()

    if not gdaltest.pg_has_postgis:
        pytest.skip()

    # Test on a regular request in a table
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:testsrtext2')
    gdaltest.pg_ds.CreateLayer('testsrtext2', srs=srs)

    sql_lyr = gdaltest.pg_ds.ExecuteSQL('SELECT * FROM testsrtext2')
    got_srs = sql_lyr.GetSpatialRef()
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)
    assert not (got_srs is None or got_srs.ExportToWkt().find('32631') == -1)

    # Test a request on a table, but with a geometry column not in the table
    sql_lyr = gdaltest.pg_ds.ExecuteSQL("SELECT eas_id, GeomFromEWKT('SRID=4326;POINT(0 1)') FROM tpoly")
    got_srs = sql_lyr.GetSpatialRef()
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)
    assert not (got_srs is None or got_srs.ExportToWkt().find('4326') == -1)

###############################################################################
# Test COLUMN_TYPES layer creation option (#4788)


def test_ogr_pg_63():

    if gdaltest.pg_ds is None:
        pytest.skip()

    # No need to test it in the non PostGIS case
    if not gdaltest.pg_has_postgis:
        pytest.skip()

    lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_63', options=['COLUMN_TYPES=foo=int8,bar=numeric(10,5),baz=hstore,baw=numeric(20,0)'])
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('bar', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('baw', ogr.OFTString))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('foo', '123')
    feat.SetField('baw', '123456789012345')
    lyr.StartTransaction()
    lyr.CreateFeature(feat)
    lyr.CommitTransaction()
    feat = None
    lyr = None

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
    lyr = ds.GetLayerByName('ogr_pg_63')
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('foo')).GetType() == ogr.OFTInteger64
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('bar')).GetType() == ogr.OFTReal

    feat = lyr.GetNextFeature()
    assert feat.GetField('foo') == 123
    assert feat.GetField('baw') == 123456789012345

###############################################################################
# Test OGR_TRUNCATE config. option (#5091)


def test_ogr_pg_64():

    if gdaltest.pg_ds is None:
        pytest.skip()

    # No need to test it in the non PostGIS case
    if not gdaltest.pg_has_postgis:
        pytest.skip()

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    lyr = ds.GetLayerByName('ogr_pg_63')

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('foo', '124')
    lyr.CreateFeature(feat)

    assert lyr.GetFeatureCount() == 2

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    lyr = ds.GetLayerByName('ogr_pg_63')

    gdal.SetConfigOption('OGR_TRUNCATE', 'YES')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('foo', '125')
    lyr.CreateFeature(feat)

    gdal.SetConfigOption('OGR_TRUNCATE', None)

    # Just one feature because of truncation
    assert lyr.GetFeatureCount() == 1

###############################################################################
# Test RFC 41


def test_ogr_pg_65():

    if gdaltest.pg_ds is None:
        pytest.skip()

    if not gdaltest.pg_has_postgis:
        pytest.skip()

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    assert ds.TestCapability(ogr.ODsCCreateGeomFieldAfterCreateLayer) != 0
    lyr = ds.CreateLayer('ogr_pg_65', geom_type=ogr.wkbNone)
    assert lyr.TestCapability(ogr.OLCCreateGeomField) != 0

    gfld_defn = ogr.GeomFieldDefn('geom1', ogr.wkbPoint)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    gfld_defn.SetSpatialRef(srs)
    assert lyr.CreateGeomField(gfld_defn) == 0

    gfld_defn = ogr.GeomFieldDefn('geom2', ogr.wkbLineString25D)
    assert lyr.CreateGeomField(gfld_defn) == 0

    gfld_defn = ogr.GeomFieldDefn('geom3', ogr.wkbPoint)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    gfld_defn.SetSpatialRef(srs)
    assert lyr.CreateGeomField(gfld_defn) == 0

    lyr.CreateField(ogr.FieldDefn('intfield', ogr.OFTInteger))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('intfield', 123)
    feat.SetGeomFieldDirectly('geom1', ogr.CreateGeometryFromWkt('POINT (1 2)'))
    feat.SetGeomFieldDirectly('geom2', ogr.CreateGeometryFromWkt('LINESTRING (3 4 5,6 7 8)'))
    assert lyr.CreateFeature(feat) == 0
    feat = ogr.Feature(lyr.GetLayerDefn())
    assert lyr.CreateFeature(feat) == 0
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat.GetField('intfield') != 123 or \
       feat.GetGeomFieldRef('geom1').ExportToWkt() != 'POINT (1 2)' or \
       feat.GetGeomFieldRef('geom2').ExportToWkt() != 'LINESTRING (3 4 5,6 7 8)':
        feat.DumpReadable()
        pytest.fail()
    feat = lyr.GetNextFeature()
    if feat.GetGeomFieldRef('geom1') is not None or \
       feat.GetGeomFieldRef('geom2') is not None:
        feat.DumpReadable()
        pytest.fail()

    ds = None
    for i in range(2):
        ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
        if i == 0:
            lyr = ds.GetLayerByName('ogr_pg_65')
        else:
            lyr = ds.ExecuteSQL('SELECT * FROM ogr_pg_65')
        assert lyr.GetLayerDefn().GetGeomFieldDefn(0).GetName() == 'geom1'
        assert i != 0 or lyr.GetLayerDefn().GetGeomFieldDefn(0).GetType() == ogr.wkbPoint
        assert lyr.GetLayerDefn().GetGeomFieldDefn(0).GetSpatialRef().ExportToWkt().find('4326') >= 0
        assert lyr.GetLayerDefn().GetGeomFieldDefn(1).GetName() == 'geom2'
        assert i != 0 or lyr.GetLayerDefn().GetGeomFieldDefn(1).GetType() == ogr.wkbLineString25D
        assert lyr.GetLayerDefn().GetGeomFieldDefn(1).GetSpatialRef() is None
        assert not (i == 0 and lyr.GetLayerDefn().GetGeomFieldDefn(2).GetSpatialRef().ExportToWkt().find('32631') < 0)
        feat = lyr.GetNextFeature()
        if feat.GetField('intfield') != 123 or \
                feat.GetGeomFieldRef('geom1').ExportToWkt() != 'POINT (1 2)' or \
                feat.GetGeomFieldRef('geom2').ExportToWkt() != 'LINESTRING (3 4 5,6 7 8)':
            feat.DumpReadable()
            pytest.fail()
        feat = lyr.GetNextFeature()
        if feat.GetGeomFieldRef('geom1') is not None or \
           feat.GetGeomFieldRef('geom2') is not None:
            feat.DumpReadable()
            pytest.fail()
        if i == 1:
            ds.ReleaseResultSet(lyr)

    gdal.SetConfigOption('PG_USE_COPY', 'YES')
    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    lyr = ds.GetLayerByName('ogr_pg_65')
    lyr.DeleteFeature(1)
    lyr.DeleteFeature(2)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeomFieldDirectly('geom1', ogr.CreateGeometryFromWkt('POINT (3 4)'))
    feat.SetGeomFieldDirectly('geom2', ogr.CreateGeometryFromWkt('LINESTRING (4 5 6,7 8 9)'))
    assert lyr.CreateFeature(feat) == 0
    feat = ogr.Feature(lyr.GetLayerDefn())
    assert lyr.CreateFeature(feat) == 0
    gdal.SetConfigOption('PG_USE_COPY', gdaltest.pg_use_copy)

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
    lyr = ds.GetLayerByName('ogr_pg_65')
    feat = lyr.GetNextFeature()
    if feat.GetGeomFieldRef('geom1').ExportToWkt() != 'POINT (3 4)' or \
       feat.GetGeomFieldRef('geom2').ExportToWkt() != 'LINESTRING (4 5 6,7 8 9)':
        feat.DumpReadable()
        pytest.fail()
    feat = lyr.GetNextFeature()
    if feat.GetGeomFieldRef('geom1') is not None or \
            feat.GetGeomFieldRef('geom2') is not None:
        feat.DumpReadable()
        pytest.fail()

    import test_cli_utilities
    if test_cli_utilities.get_ogr2ogr_path() is not None:
        gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -update "' + 'PG:' + gdaltest.pg_connection_string + '" "' + 'PG:' + gdaltest.pg_connection_string + '" ogr_pg_65 -nln ogr_pg_65_copied -overwrite')
        ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
        lyr = ds.GetLayerByName('ogr_pg_65_copied')
        assert lyr.GetLayerDefn().GetGeomFieldDefn(0).GetSpatialRef().ExportToWkt().find('4326') >= 0
        assert lyr.GetLayerDefn().GetGeomFieldDefn(1).GetSpatialRef() is None
        assert lyr.GetLayerDefn().GetGeomFieldDefn(2).GetSpatialRef().ExportToWkt().find('32631') >= 0

    
###############################################################################
# Run test_ogrsf


def test_ogr_pg_66():

    if gdaltest.pg_ds is None:
        pytest.skip()

    if not gdaltest.pg_has_postgis:
        pytest.skip()

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' "' + 'PG:' + gdaltest.pg_connection_string + '" ogr_pg_65')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

###############################################################################
# Test retrieving SRID from values (#5131)


def test_ogr_pg_67():

    if gdaltest.pg_ds is None:
        pytest.skip()

    if not gdaltest.pg_has_postgis:
        pytest.skip()

    lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_67')
    lyr.ResetReading()  # to trigger layer creation
    lyr = None

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    lyr = ds.GetLayerByName('ogr_pg_67')
    assert lyr.GetSpatialRef() is None
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)
    feat = None
    ds = None

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    lyr = ds.GetLayerByName('ogr_pg_67')
    assert lyr.GetSpatialRef() is None
    ds.ExecuteSQL("ALTER TABLE ogr_pg_67 DROP CONSTRAINT enforce_srid_wkb_geometry")
    ds.ExecuteSQL("UPDATE ogr_pg_67 SET wkb_geometry = ST_GeomFromEWKT('SRID=4326;POINT(0 1)')")
    ds = None

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    lyr = ds.GetLayerByName('ogr_pg_67')
    assert lyr.GetSpatialRef() is not None
    ds = None

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string + ' tables=fake', update=1)
    lyr = ds.GetLayerByName('ogr_pg_67')
    assert lyr.GetSpatialRef() is not None
    ds = None

###############################################################################
# Test retrieving SRID from values even if we don't have select rights on geometry_columns (#5131)


def test_ogr_pg_68():

    if gdaltest.pg_ds is None:
        pytest.skip()

    if not gdaltest.pg_has_postgis:
        pytest.skip()

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)

    lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_68', srs=srs)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT(0 0)'))
    lyr.CreateFeature(feat)
    lyr = None

    sql_lyr = gdaltest.pg_ds.ExecuteSQL('SELECT current_user')
    feat = sql_lyr.GetNextFeature()
    current_user = feat.GetField(0)
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

    gdaltest.pg_ds.ExecuteSQL("REVOKE SELECT ON geometry_columns FROM %s" % current_user)

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string + ' tables=fake', update=1)
    lyr = ds.GetLayerByName('ogr_pg_68')
    got_srs = None
    if lyr is not None:
        got_srs = lyr.GetSpatialRef()

    sql_lyr = ds.ExecuteSQL("select * from ( select 'SRID=4326;POINT(0 0)'::geometry as g ) as _xx")
    got_srs2 = None
    if sql_lyr is not None:
        got_srs2 = sql_lyr.GetSpatialRef()
    ds.ReleaseResultSet(sql_lyr)

    ds = None

    gdaltest.pg_ds.ExecuteSQL("GRANT SELECT ON geometry_columns TO %s" % current_user)

    assert got_srs is not None

    assert got_srs2 is not None

###############################################################################
# Test deferred loading of tables (#5450)


def has_run_load_tables(ds):
    return int(ds.GetMetadataItem("bHasLoadTables", "_DEBUG_"))


def test_ogr_pg_69():

    if gdaltest.pg_ds is None:
        pytest.skip()

    gdaltest.pg_ds = None
    gdaltest.pg_ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
    assert not has_run_load_tables(gdaltest.pg_ds)
    gdaltest.pg_ds.GetLayerByName('tpoly')
    assert not has_run_load_tables(gdaltest.pg_ds)
    sql_lyr = gdaltest.pg_ds.ExecuteSQL('SELECT * FROM tpoly')
    assert not has_run_load_tables(gdaltest.pg_ds)
    feat = sql_lyr.GetNextFeature()
    assert not has_run_load_tables(gdaltest.pg_ds)
    del feat
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

    gdaltest.pg_ds.GetLayer(0)
    assert has_run_load_tables(gdaltest.pg_ds)

    # Test that we can find a layer with non lowercase
    gdaltest.pg_ds = None
    gdaltest.pg_ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
    assert gdaltest.pg_ds.GetLayerByName('TPOLY') is not None

###############################################################################
# Test historical non-differed creation of tables (#5547)


def test_ogr_pg_70():

    if gdaltest.pg_ds is None:
        pytest.skip()

    gdal.SetConfigOption('OGR_PG_DEFERRED_CREATION', 'NO')
    lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_70')
    gdal.SetConfigOption('OGR_PG_DEFERRED_CREATION', None)

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
    lyr2 = ds.GetLayerByName('ogr_pg_70')
    assert lyr2 is not None
    ds = None

    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
    lyr2 = ds.GetLayerByName('ogr_pg_70')
    assert lyr2.GetLayerDefn().GetFieldCount() == 1
    ds = None

    gfld_defn = ogr.GeomFieldDefn('geom', ogr.wkbPoint)
    lyr.CreateGeomField(gfld_defn)

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
    lyr2 = ds.GetLayerByName('ogr_pg_70')
    assert lyr2.GetLayerDefn().GetGeomFieldCount() == 2
    ds = None

    if gdaltest.pg_has_postgis and gdaltest.pg_ds.GetLayerByName('geography_columns') is not None:
        print('Trying geography')

        gdaltest.pg_ds.ExecuteSQL('DELLAYER:ogr_pg_70')

        gdal.SetConfigOption('OGR_PG_DEFERRED_CREATION', 'NO')
        lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_70', options=['GEOM_TYPE=geography', 'GEOMETRY_NAME=my_geog'])
        gdal.SetConfigOption('OGR_PG_DEFERRED_CREATION', None)

        ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
        lyr2 = ds.GetLayerByName('ogr_pg_70')
        assert lyr2.GetLayerDefn().GetGeomFieldCount() == 1
        geography_columns_lyr = ds.ExecuteSQL("SELECT * FROM geography_columns WHERE f_table_name = 'ogr_pg_70' AND f_geography_column = 'my_geog'")
        assert geography_columns_lyr.GetFeatureCount() == 1
        ds.ReleaseResultSet(geography_columns_lyr)
        ds = None

    
###############################################################################
# Test interoperability of WKT/WKB with PostGIS.


def test_ogr_pg_71():

    if gdaltest.pg_ds is None:
        pytest.skip()

    if not gdaltest.pg_has_postgis:
        pytest.skip()

    curve_lyr = gdaltest.pg_ds.CreateLayer('test_curve')
    curve_lyr2 = gdaltest.pg_ds.CreateLayer('test_curve_3d', geom_type=ogr.wkbUnknown | ogr.wkb25DBit)
    # FIXME: the ResetReading() should not be necessary
    curve_lyr.ResetReading()
    curve_lyr2.ResetReading()

    for wkt in ['CIRCULARSTRING EMPTY',
                'CIRCULARSTRING Z EMPTY',
                'CIRCULARSTRING (0 1,2 3,4 5)',
                'CIRCULARSTRING Z (0 1 2,4 5 6,7 8 9)',
                'COMPOUNDCURVE EMPTY',
                'TRIANGLE ((0 0 0,100 0 100,0 100 100,0 0 0))',
                'COMPOUNDCURVE ((0 1,2 3,4 5))',
                'COMPOUNDCURVE Z ((0 1 2,4 5 6,7 8 9))',
                'COMPOUNDCURVE ((0 1,2 3,4 5),CIRCULARSTRING (4 5,6 7,8 9))',
                'COMPOUNDCURVE Z ((0 1 2,4 5 6,7 8 9),CIRCULARSTRING Z (7 8 9,10 11 12,13 14 15))',
                'CURVEPOLYGON EMPTY',
                'CURVEPOLYGON ((0 0,0 1,1 1,1 0,0 0))',
                'CURVEPOLYGON Z ((0 0 2,0 1 3,1 1 4,1 0 5,0 0 2))',
                'CURVEPOLYGON (COMPOUNDCURVE (CIRCULARSTRING (0 0,1 0,0 0)))',
                'CURVEPOLYGON Z (COMPOUNDCURVE Z (CIRCULARSTRING Z (0 0 2,1 0 3,0 0 2)))',
                'MULTICURVE EMPTY',
                'MULTICURVE (CIRCULARSTRING (0 0,1 0,0 0),(0 0,1 1))',
                'MULTICURVE Z (CIRCULARSTRING Z (0 0 1,1 0 1,0 0 1),(0 0 1,1 1 1))',
                'MULTICURVE (CIRCULARSTRING (0 0,1 0,0 0),(0 0,1 1),COMPOUNDCURVE ((0 0,1 1),CIRCULARSTRING (1 1,2 2,3 3)))',
                'MULTISURFACE EMPTY',
                'MULTISURFACE (((0 0,0 10,10 10,10 0,0 0)),CURVEPOLYGON (CIRCULARSTRING (0 0,1 0,0 0)))',
                'MULTISURFACE Z (((0 0 1,0 10 1,10 10 1,10 0 1,0 0 1)),CURVEPOLYGON Z (CIRCULARSTRING Z (0 0 1,1 0 1,0 0 1)))',
                'GEOMETRYCOLLECTION (CIRCULARSTRING (0 1,2 3,4 5),COMPOUNDCURVE ((0 1,2 3,4 5)),CURVEPOLYGON ((0 0,0 1,1 1,1 0,0 0)),MULTICURVE ((0 0,1 1)),MULTISURFACE (((0 0,0 10,10 10,10 0,0 0))))',
               ]:

        # would cause PostGIS 1.X to crash
        if not gdaltest.pg_has_postgis_2 and wkt == 'CURVEPOLYGON EMPTY':
            continue
        # Parsing error of WKT by PostGIS 1.X
        if not gdaltest.pg_has_postgis_2 and 'MULTICURVE' in wkt and 'CIRCULARSTRING' in wkt:
            continue

        postgis_in_wkt = wkt
        while True:
            z_pos = postgis_in_wkt.find('Z ')
            # PostGIS 1.X doesn't like Z in WKT
            if not gdaltest.pg_has_postgis_2 and z_pos >= 0:
                postgis_in_wkt = postgis_in_wkt[0:z_pos] + postgis_in_wkt[z_pos + 2:]
            else:
                break

        # Test parsing PostGIS WKB
        lyr = gdaltest.pg_ds.ExecuteSQL("SELECT ST_GeomFromText('%s')" % postgis_in_wkt)
        f = lyr.GetNextFeature()
        g = f.GetGeometryRef()
        out_wkt = g.ExportToWkt()
        g = None
        f = None
        gdaltest.pg_ds.ReleaseResultSet(lyr)

        expected_wkt = wkt
        if not gdaltest.pg_has_postgis_2 and 'EMPTY' in wkt:
            expected_wkt = 'GEOMETRYCOLLECTION EMPTY'
        assert out_wkt == expected_wkt

        # Test parsing PostGIS WKT
        if gdaltest.pg_has_postgis_2:
            fct = 'ST_AsText'
        else:
            fct = 'AsEWKT'

        lyr = gdaltest.pg_ds.ExecuteSQL("SELECT %s(ST_GeomFromText('%s'))" % (fct, postgis_in_wkt))
        f = lyr.GetNextFeature()
        g = f.GetGeometryRef()
        out_wkt = g.ExportToWkt()
        g = None
        f = None
        gdaltest.pg_ds.ReleaseResultSet(lyr)

        expected_wkt = wkt
        if not gdaltest.pg_has_postgis_2 and 'EMPTY' in wkt:
            expected_wkt = 'GEOMETRYCOLLECTION EMPTY'
        assert out_wkt == expected_wkt

        g = ogr.CreateGeometryFromWkt(wkt)
        if g.GetCoordinateDimension() == 2:
            active_lyr = curve_lyr
        else:
            active_lyr = curve_lyr2

        # Use our WKB export to inject into PostGIS and check that
        # PostGIS interprets it correctly by checking with ST_AsText
        f = ogr.Feature(active_lyr.GetLayerDefn())
        f.SetGeometry(g)
        ret = active_lyr.CreateFeature(f)
        assert ret == 0, wkt
        fid = f.GetFID()

        # AsEWKT() in PostGIS 1.X does not like CIRCULARSTRING EMPTY
        if not gdaltest.pg_has_postgis_2 and 'CIRCULARSTRING' in wkt and 'EMPTY' in wkt:
            continue

        lyr = gdaltest.pg_ds.ExecuteSQL("SELECT %s(wkb_geometry) FROM %s WHERE ogc_fid = %d" % (fct, active_lyr.GetName(), fid))
        f = lyr.GetNextFeature()
        g = f.GetGeometryRef()
        out_wkt = g.ExportToWkt()
        gdaltest.pg_ds.ReleaseResultSet(lyr)
        g = None
        f = None

        assert out_wkt == wkt

    
###############################################################################
# Test 64 bit FID


def test_ogr_pg_72():

    if gdaltest.pg_ds is None:
        pytest.skip()

    # Regular layer with 32 bit IDs
    lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_72')
    assert lyr.GetMetadataItem(ogr.OLMD_FID64) is None
    lyr.CreateField(ogr.FieldDefn('foo'))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(123456789012345)
    f.SetField(0, 'bar')
    assert lyr.CreateFeature(f) == 0
    f = lyr.GetFeature(123456789012345)
    assert f is not None

    lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_72', options=['FID64=YES', 'OVERWRITE=YES'])
    assert lyr.GetMetadataItem(ogr.OLMD_FID64) is not None
    lyr.CreateField(ogr.FieldDefn('foo'))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(123456789012345)
    f.SetField(0, 'bar')
    assert lyr.CreateFeature(f) == 0
    assert lyr.SetFeature(f) == 0
    gdaltest.pg_ds = None
    # Test with binary protocol
    # gdaltest.pg_ds = ogr.Open( 'PGB:' + gdaltest.pg_connection_string, update = 1 )
    # lyr = gdaltest.pg_ds.GetLayerByName('ogr_pg_72')
    # if lyr.GetMetadataItem(ogr.OLMD_FID64) is None:
    #    gdaltest.post_reason('fail')
    #    return 'fail'
    # f = lyr.GetNextFeature()
    # if f.GetFID() != 123456789012345:
    #    gdaltest.post_reason('fail')
    #    f.DumpReadable()
    #    return 'fail'
    # gdaltest.pg_ds = None
    # Test with normal protocol
    gdaltest.pg_ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    lyr = gdaltest.pg_ds.GetLayerByName('ogr_pg_72')
    assert lyr.GetMetadataItem(ogr.OLMD_FID64) is not None
    f = lyr.GetNextFeature()
    if f.GetFID() != 123456789012345:
        f.DumpReadable()
        pytest.fail()

    lyr.ResetReading()  # to close implicit transaction

###############################################################################
# Test not nullable fields


def test_ogr_pg_73():

    if gdaltest.pg_ds is None:
        pytest.skip()

    if not gdaltest.pg_has_postgis:
        pytest.skip()

    gdal.SetConfigOption('PG_USE_COPY', 'NO')

    lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_73', geom_type=ogr.wkbNone)
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

    gdal.SetConfigOption('PG_USE_COPY', gdaltest.pg_use_copy)

    lyr.ResetReading()  # force above feature to be committed

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    lyr = ds.GetLayerByName('ogr_pg_73')
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
    ds.ExecuteSQL("UPDATE ogr_pg_73 SET field_nullable = '' WHERE field_nullable IS NULL")
    src_fd = lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_nullable'))
    fd = ogr.FieldDefn('now_nullable', src_fd.GetType())
    fd.SetName('now_not_nullable')
    fd.SetNullable(0)
    lyr.AlterFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_nullable'), fd, ogr.ALTER_ALL_FLAG)
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('now_not_nullable')).IsNullable() == 0

    sql_lyr = ds.ExecuteSQL('SELECT * FROM ogr_pg_73')
    assert sql_lyr.GetLayerDefn().GetFieldDefn(sql_lyr.GetLayerDefn().GetFieldIndex('now_not_nullable')).IsNullable() == 0
    assert sql_lyr.GetLayerDefn().GetFieldDefn(sql_lyr.GetLayerDefn().GetFieldIndex('now_nullable')).IsNullable() == 1
    assert sql_lyr.GetLayerDefn().GetGeomFieldDefn(sql_lyr.GetLayerDefn().GetGeomFieldIndex('geomfield_not_nullable')).IsNullable() == 0
    assert sql_lyr.GetLayerDefn().GetGeomFieldDefn(sql_lyr.GetLayerDefn().GetGeomFieldIndex('geomfield_nullable')).IsNullable() == 1
    ds.ReleaseResultSet(sql_lyr)

###############################################################################
# Test default values


def test_ogr_pg_74():

    if gdaltest.pg_ds is None:
        pytest.skip()

    if not gdaltest.pg_has_postgis:
        pytest.skip()

    lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_74', geom_type=ogr.wkbNone)

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
    field_defn.SetDefault("'2015/06/30 12:34:56.123'")
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn('field_date', ogr.OFTDate)
    field_defn.SetDefault("CURRENT_DATE")
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn('field_time', ogr.OFTTime)
    field_defn.SetDefault("CURRENT_TIME")
    lyr.CreateField(field_defn)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFieldNull('field_string')
    f.SetField('field_int', 456)
    f.SetField('field_real', 4.56)
    f.SetField('field_datetime', '2015/06/30 12:34:56')
    f.SetField('field_datetime2', '2015/06/30 12:34:56')
    f.SetField('field_datetime3', '2015/06/30 12:34:56.123')
    f.SetField('field_date', '2015/06/30')
    f.SetField('field_time', '12:34:56')
    lyr.CreateFeature(f)
    f = None

    # Transition from COPY to INSERT
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    f = None

    # Transition from INSERT to COPY
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('field_string', 'b')
    f.SetField('field_int', 456)
    f.SetField('field_real', 4.56)
    f.SetField('field_datetime', '2015/06/30 12:34:56')
    f.SetField('field_datetime2', '2015/06/30 12:34:56')
    f.SetField('field_datetime3', '2015/06/30 12:34:56.123')
    f.SetField('field_date', '2015/06/30')
    f.SetField('field_time', '12:34:56')
    lyr.CreateFeature(f)
    f = None

    lyr.ResetReading()  # force above feature to be committed

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    ds.ExecuteSQL('set timezone to "UTC"')
    lyr = ds.GetLayerByName('ogr_pg_74')
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_string')).GetDefault() == "'a''b'"
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_int')).GetDefault() == '123'
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_real')).GetDefault() == '1.23'
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_nodefault')).GetDefault() is None
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_datetime')).GetDefault() == 'CURRENT_TIMESTAMP'
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_datetime2')).GetDefault() == "'2015/06/30 12:34:56'"
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_datetime3')).GetDefault() == "'2015/06/30 12:34:56.123'"
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_date')).GetDefault() == "CURRENT_DATE"
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_time')).GetDefault() == "CURRENT_TIME"

    f = lyr.GetNextFeature()
    if not f.IsFieldNull('field_string'):
        f.DumpReadable()
        pytest.fail()

    f = lyr.GetNextFeature()
    if f.GetField('field_string') != 'a\'b' or f.GetField('field_int') != 123 or \
       f.GetField('field_real') != 1.23 or \
       not f.IsFieldNull('field_nodefault') or not f.IsFieldSet('field_datetime') or \
       f.GetField('field_datetime2') != '2015/06/30 12:34:56+00' or \
       f.GetField('field_datetime3') != '2015/06/30 12:34:56.123+00' or \
       not f.IsFieldSet('field_date') or not f.IsFieldSet('field_time'):
        f.DumpReadable()
        pytest.fail()

    f = lyr.GetNextFeature()
    if f.GetField('field_string') != 'b':
        f.DumpReadable()
        pytest.fail()

    lyr.ResetReading()  # to close implicit transaction

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
    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    ds.ExecuteSQL('set timezone to "UTC"')
    lyr = ds.GetLayerByName('ogr_pg_74')
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_string')).GetDefault() == "'c'"
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_int')).GetDefault() is None

###############################################################################
# Test creating a field with the fid name


def test_ogr_pg_75():

    if gdaltest.pg_ds is None:
        pytest.skip()

    if not gdaltest.pg_has_postgis:
        pytest.skip()

    lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_75', geom_type=ogr.wkbNone, options=['FID=myfid'])

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

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('str', 'first string')
    feat.SetField('myfid', 12)
    feat.SetField('str2', 'second string')
    ret = lyr.CreateFeature(feat)
    assert ret == 0
    assert feat.GetFID() == 12

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f.GetFID() != 10 or f.GetField('str') != 'first string' or f.GetField('str2') != 'second string' or f.GetField('myfid') != 10:
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetFeature(f.GetFID())
    if f.GetFID() != 10 or f.GetField('str') != 'first string' or f.GetField('str2') != 'second string' or f.GetField('myfid') != 10:
        f.DumpReadable()
        pytest.fail()
    f = None
    lyr.ResetReading()  # to close implicit transaction

###############################################################################
# Test transactions RFC 54


def ogr_pg_76_get_transaction_state(ds):
    return (ds.GetMetadataItem("osDebugLastTransactionCommand", "_DEBUG_"),
            int(ds.GetMetadataItem("nSoftTransactionLevel", "_DEBUG_")),
            int(ds.GetMetadataItem("bSavePointActive", "_DEBUG_")),
            int(ds.GetMetadataItem("bUserTransactionActive", "_DEBUG_")))


def test_ogr_pg_76():

    if gdaltest.pg_ds is None:
        pytest.skip()

    assert gdaltest.pg_ds.TestCapability(ogr.ODsCTransactions) == 1

    level = int(gdaltest.pg_ds.GetMetadataItem("nSoftTransactionLevel", "_DEBUG_"))
    assert level == 0

    if gdaltest.pg_has_postgis_2:
        gdaltest.pg_ds.StartTransaction()
        lyr = gdaltest.pg_ds.CreateLayer('will_not_be_created', options=['OVERWRITE=YES'])
        lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))

        sql_lyr = gdaltest.pg_ds.ExecuteSQL("SELECT COUNT(*) FROM geometry_columns WHERE f_table_name = 'will_not_be_created'")
        f = sql_lyr.GetNextFeature()
        res = f.GetField(0)
        gdaltest.pg_ds.ReleaseResultSet(sql_lyr)
        assert res == 1

        gdaltest.pg_ds.RollbackTransaction()

        # Rollback doesn't rollback the insertion in geometry_columns if done through the AddGeometryColumn()
        sql_lyr = gdaltest.pg_ds.ExecuteSQL("SELECT COUNT(*) FROM geometry_columns WHERE f_table_name = 'will_not_be_created'")
        f = sql_lyr.GetNextFeature()
        res = f.GetField(0)
        gdaltest.pg_ds.ReleaseResultSet(sql_lyr)
        assert res == 0

    gdal.SetConfigOption('OGR_PG_CURSOR_PAGE', '1')
    lyr1 = gdaltest.pg_ds.CreateLayer('ogr_pg_76_lyr1', geom_type=ogr.wkbNone, options=['OVERWRITE=YES'])
    lyr2 = gdaltest.pg_ds.CreateLayer('ogr_pg_76_lyr2', geom_type=ogr.wkbNone, options=['OVERWRITE=YES'])
    gdal.SetConfigOption('OGR_PG_CURSOR_PAGE', None)
    lyr1.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    # lyr2.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    lyr1.CreateFeature(ogr.Feature(lyr1.GetLayerDefn()))
    lyr2.CreateFeature(ogr.Feature(lyr2.GetLayerDefn()))
    lyr1.CreateFeature(ogr.Feature(lyr1.GetLayerDefn()))
    lyr1.CreateFeature(ogr.Feature(lyr1.GetLayerDefn()))
    lyr2.CreateFeature(ogr.Feature(lyr2.GetLayerDefn()))

    level = int(gdaltest.pg_ds.GetMetadataItem("nSoftTransactionLevel", "_DEBUG_"))
    assert level == 0

    ret = ogr_pg_76_scenario1(lyr1, lyr2)
    ret = ogr_pg_76_scenario2(lyr1, lyr2)
    ret = ogr_pg_76_scenario3(lyr1, lyr2)
    ret = ogr_pg_76_scenario4(lyr1, lyr2)

    return ret

# Scenario 1 : a CreateFeature done in the middle of GetNextFeature()


def ogr_pg_76_scenario1(lyr1, lyr2):

    (_, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    assert (level, savepoint, usertransac) == (0, 0, 0)

    f = lyr1.GetNextFeature()
    assert f is not None and f.GetFID() == 1
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ('BEGIN', 1, 0, 0)

    lyr1.SetAttributeFilter("foo is NULL")
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ('COMMIT', 0, 0, 0)

    f = lyr1.GetNextFeature()
    assert f is not None and f.GetFID() == 1
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ('BEGIN', 1, 0, 0)

    f = lyr2.GetNextFeature()
    assert f is not None and f.GetFID() == 1
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ('', 2, 0, 0)

    f = lyr1.GetNextFeature()
    assert f is not None and f.GetFID() == 2

    # Check that GetFeature() doesn't reset the cursor
    f = lyr1.GetFeature(f.GetFID())
    assert f is not None and f.GetFID() == 2

    f = lyr1.GetNextFeature()
    assert f is not None and f.GetFID() == 3
    f = lyr2.GetNextFeature()
    assert f is not None and f.GetFID() == 2
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ('', 2, 0, 0)

    lyr1.CreateFeature(ogr.Feature(lyr1.GetLayerDefn()))

    lyr1.ResetReading()
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ('', 1, 0, 0)
    lyr2.ResetReading()
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ('COMMIT', 0, 0, 0)
    assert lyr1.GetFeatureCount() == 4


# Scenario 2 : a CreateFeature done in the middle of GetNextFeature(), themselves between a user transaction
def ogr_pg_76_scenario2(lyr1, lyr2):

    assert gdaltest.pg_ds.StartTransaction() == 0
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ('BEGIN', 1, 0, 1)

    # Try to re-enter a transaction
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    ret = gdaltest.pg_ds.StartTransaction()
    gdal.PopErrorHandler()
    assert not (gdal.GetLastErrorMsg() == '' or ret == 0)
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ('', 1, 0, 1)

    f = lyr1.GetNextFeature()
    assert f is not None and f.GetFID() == 1
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ('', 2, 0, 1)

    f = lyr2.GetNextFeature()
    assert f is not None and f.GetFID() == 1
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ('', 3, 0, 1)

    f = lyr1.GetNextFeature()
    assert f is not None and f.GetFID() == 2
    f = lyr1.GetNextFeature()
    assert f is not None and f.GetFID() == 3
    f = lyr2.GetNextFeature()
    assert f is not None and f.GetFID() == 2
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ('', 3, 0, 1)

    lyr1.CreateFeature(ogr.Feature(lyr1.GetLayerDefn()))

    lyr1.ResetReading()
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ('', 2, 0, 1)

    lyr2.ResetReading()
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ('', 1, 0, 1)

    assert gdaltest.pg_ds.CommitTransaction() == 0
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ('COMMIT', 0, 0, 0)

    assert gdaltest.pg_ds.StartTransaction() == 0

    assert gdaltest.pg_ds.RollbackTransaction() == 0
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ('ROLLBACK', 0, 0, 0)

    # Try to re-commit a transaction
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    ret = gdaltest.pg_ds.CommitTransaction()
    gdal.PopErrorHandler()
    assert not (gdal.GetLastErrorMsg() == '' or ret == 0)
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ('', 0, 0, 0)

    # Try to rollback a non-transaction
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    ret = gdaltest.pg_ds.RollbackTransaction()
    gdal.PopErrorHandler()
    assert not (gdal.GetLastErrorMsg() == '' or ret == 0)
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ('', 0, 0, 0)

# Scenario 3 : StartTransaction(), GetNextFeature(), CommitTransaction(), GetNextFeature()


def ogr_pg_76_scenario3(lyr1, lyr2):

    assert gdaltest.pg_ds.StartTransaction() == 0
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ('BEGIN', 1, 0, 1)

    f = lyr1.GetNextFeature()
    assert f is not None and f.GetFID() == 1
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ('', 2, 0, 1)

    assert gdaltest.pg_ds.CommitTransaction() == 0
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ('COMMIT', 0, 0, 0)

    gdal.ErrorReset()
    gdal.PushErrorHandler()
    f = lyr1.GetNextFeature()
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != '' and f is None

    # Must re-issue an explicit ResetReading()
    lyr1.ResetReading()

    f = lyr1.GetNextFeature()
    assert f is not None and f.GetFID() == 1
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ('BEGIN', 1, 0, 0)

    lyr1.ResetReading()
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ('COMMIT', 0, 0, 0)

    lyr2.ResetReading()

# Scenario 4 : GetNextFeature(), StartTransaction(), CreateFeature(), CommitTransaction(), GetNextFeature(), ResetReading()


def ogr_pg_76_scenario4(lyr1, lyr2):

    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ('', 0, 0, 0)

    f = lyr1.GetNextFeature()
    assert f is not None and f.GetFID() == 1
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ('BEGIN', 1, 0, 0)

    assert gdaltest.pg_ds.StartTransaction() == 0
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ('SAVEPOINT ogr_savepoint', 2, 1, 1)

    lyr1.CreateFeature(ogr.Feature(lyr1.GetLayerDefn()))

    f = lyr1.GetNextFeature()
    assert f is not None and f.GetFID() == 2
    f = lyr2.GetNextFeature()
    assert f is not None and f.GetFID() == 1
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ('', 3, 1, 1)

    # Check that it doesn't commit the transaction
    lyr1.SetAttributeFilter("foo is NULL")
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ('', 2, 1, 1)

    f = lyr1.GetNextFeature()
    assert f is not None and f.GetFID() == 1
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ('', 3, 1, 1)

    f = lyr2.GetNextFeature()
    assert f is not None and f.GetFID() == 2

    assert gdaltest.pg_ds.CommitTransaction() == 0
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ('RELEASE SAVEPOINT ogr_savepoint', 2, 0, 0)

    lyr2.ResetReading()

    assert gdaltest.pg_ds.StartTransaction() == 0

    assert gdaltest.pg_ds.RollbackTransaction() == 0
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ('ROLLBACK TO SAVEPOINT ogr_savepoint', 1, 0, 0)

    f = lyr1.GetNextFeature()
    assert f is not None and f.GetFID() == 2

    lyr1.ResetReading()
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    assert (lastcmd, level, savepoint, usertransac) == ('COMMIT', 0, 0, 0)

###############################################################################
# Test ogr2ogr can insert multiple layers at once


def test_ogr_pg_77():
    import test_cli_utilities
    if test_cli_utilities.get_ogr2ogr_path() is None:
        pytest.skip()

    gdaltest.pg_ds.ExecuteSQL('DELLAYER:ogr_pg_77_1')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:ogr_pg_77_2')

    try:
        shutil.rmtree('tmp/ogr_pg_77')
    except OSError:
        pass
    os.mkdir('tmp/ogr_pg_77')

    f = open('tmp/ogr_pg_77/ogr_pg_77_1.csv', 'wt')
    f.write('id,WKT\n')
    f.write('1,POINT(1 2)\n')
    f.close()
    f = open('tmp/ogr_pg_77/ogr_pg_77_2.csv', 'wt')
    f.write('id,WKT\n')
    f.write('2,POINT(1 2)\n')
    f.close()
    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f PostgreSQL "' + 'PG:' + gdaltest.pg_connection_string + '" tmp/ogr_pg_77')

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    lyr = ds.GetLayerByName('ogr_pg_77_1')
    feat = lyr.GetNextFeature()
    assert feat.GetField(0) == '1'
    feat.SetField(0, 10)
    lyr.SetFeature(feat)
    lyr = ds.GetLayerByName('ogr_pg_77_2')
    feat = lyr.GetNextFeature()
    assert feat.GetField(0) == '2'
    ds = None

    # Test fix for #6018
    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f PostgreSQL "' + 'PG:' + gdaltest.pg_connection_string + '" tmp/ogr_pg_77 -overwrite')

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
    lyr = ds.GetLayerByName('ogr_pg_77_1')
    feat = lyr.GetNextFeature()
    assert feat.GetField(0) == '1'
    ds = None

    try:
        shutil.rmtree('tmp/ogr_pg_77')
    except OSError:
        pass

    
###############################################################################
# Test manually added geometry constraints


def test_ogr_pg_78():

    if gdaltest.pg_ds is None or not gdaltest.pg_has_postgis_2:
        pytest.skip()

    gdaltest.pg_ds.ExecuteSQL("CREATE TABLE ogr_pg_78 (ID INTEGER PRIMARY KEY)")
    gdaltest.pg_ds.ExecuteSQL("ALTER TABLE ogr_pg_78 ADD COLUMN my_geom GEOMETRY")
    gdaltest.pg_ds.ExecuteSQL("ALTER TABLE ogr_pg_78 ADD CONSTRAINT ogr_pg_78_my_geom_type CHECK (geometrytype(my_geom)='POINT')")
    gdaltest.pg_ds.ExecuteSQL("ALTER TABLE ogr_pg_78 ADD CONSTRAINT ogr_pg_78_my_geom_dim CHECK (st_ndims(my_geom)=3)")
    gdaltest.pg_ds.ExecuteSQL("ALTER TABLE ogr_pg_78 ADD CONSTRAINT ogr_pg_78_my_geom_srid CHECK (st_srid(my_geom)=4326)")

    gdaltest.pg_ds.ExecuteSQL("CREATE TABLE ogr_pg_78_2 (ID INTEGER PRIMARY KEY)")
    gdaltest.pg_ds.ExecuteSQL("ALTER TABLE ogr_pg_78_2 ADD COLUMN my_geog GEOGRAPHY")
    gdaltest.pg_ds.ExecuteSQL("ALTER TABLE ogr_pg_78_2 ADD CONSTRAINT ogr_pg_78_2_my_geog_type CHECK (geometrytype(my_geog::geometry)='POINT')")
    gdaltest.pg_ds.ExecuteSQL("ALTER TABLE ogr_pg_78_2 ADD CONSTRAINT ogr_pg_78_2_my_geog_dim CHECK (st_ndims(my_geog::geometry)=3)")
    gdaltest.pg_ds.ExecuteSQL("ALTER TABLE ogr_pg_78_2 ADD CONSTRAINT ogr_pg_78_2_my_geog_srid CHECK (st_srid(my_geog::geometry)=4326)")

    gdaltest.pg_ds = None
    gdaltest.pg_ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    lc = gdaltest.pg_ds.GetLayerCount()  # force discovery of all tables
    ogr_pg_78_found = False
    ogr_pg_78_2_found = False
    for i in range(lc):
        lyr = gdaltest.pg_ds.GetLayer(i)
        if lyr.GetName() == 'ogr_pg_78':
            ogr_pg_78_found = True
            assert lyr.GetGeomType() == ogr.wkbPoint25D
            assert lyr.GetSpatialRef().ExportToWkt().find('4326') >= 0
        if lyr.GetName() == 'ogr_pg_78_2':
            ogr_pg_78_2_found = True
            assert lyr.GetGeomType() == ogr.wkbPoint25D
            assert lyr.GetSpatialRef().ExportToWkt().find('4326') >= 0
    assert ogr_pg_78_found
    assert ogr_pg_78_2_found

    gdaltest.pg_ds = None
    # Test with slow method
    gdal.SetConfigOption('PG_USE_POSTGIS2_OPTIM', 'NO')
    gdaltest.pg_ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    lc = gdaltest.pg_ds.GetLayerCount()  # force discovery of all tables
    ogr_pg_78_found = False
    ogr_pg_78_2_found = False
    for i in range(lc):
        lyr = gdaltest.pg_ds.GetLayer(i)
        if lyr.GetName() == 'ogr_pg_78':
            ogr_pg_78_found = True
            if lyr.GetGeomType() != ogr.wkbPoint25D:
                # FIXME: why does it fail suddenly on Travis ? Change of PostGIS version ?
                # But apparently not :
                # Last good: https://travis-ci.org/OSGeo/gdal/builds/60211881
                # First bad: https://travis-ci.org/OSGeo/gdal/builds/60290209
                val = gdal.GetConfigOption('TRAVIS', None)
                if val is not None:
                    print('Fails on Travis. geom_type = %d' % lyr.GetGeomType())
                else:
                    pytest.fail()
            if lyr.GetSpatialRef() is None or lyr.GetSpatialRef().ExportToWkt().find('4326') < 0:
                val = gdal.GetConfigOption('TRAVIS', None)
                if val is not None:
                    print('Fails on Travis. GetSpatialRef() = %s' % str(lyr.GetSpatialRef()))
                else:
                    pytest.fail()
        if lyr.GetName() == 'ogr_pg_78_2':
            ogr_pg_78_2_found = True
            # No logic in geography_columns to get type/coordim/srid from constraints
            # if lyr.GetGeomType() != ogr.wkbPoint25D:
            #    gdaltest.post_reason('fail')
            #    return 'fail'
            # if lyr.GetSpatialRef().ExportToWkt().find('4326') < 0:
            #    gdaltest.post_reason('fail')
            #    return 'fail'
    assert ogr_pg_78_found
    assert ogr_pg_78_2_found

###############################################################################
# Test PRELUDE_STATEMENTS and CLOSING_STATEMENTS open options


def test_ogr_pg_79():

    if gdaltest.pg_ds is None:
        pytest.skip()

    # PRELUDE_STATEMENTS starting with BEGIN (use case: pg_bouncer in transaction pooling)
    ds = gdal.OpenEx('PG:' + gdaltest.pg_connection_string,
                     gdal.OF_VECTOR | gdal.OF_UPDATE,
                     open_options=['PRELUDE_STATEMENTS=BEGIN; SET LOCAL statement_timeout TO "1h";',
                                   'CLOSING_STATEMENTS=COMMIT;'])
    sql_lyr = ds.ExecuteSQL('SHOW statement_timeout')
    f = sql_lyr.GetNextFeature()
    if f.GetField(0) != '1h':
        f.DumpReadable()
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)
    ret = ds.StartTransaction()
    assert ret == 0
    ret = ds.CommitTransaction()
    assert ret == 0
    gdal.ErrorReset()
    ds = None
    assert gdal.GetLastErrorMsg() == ''

    # random PRELUDE_STATEMENTS
    ds = gdal.OpenEx('PG:' + gdaltest.pg_connection_string,
                     gdal.OF_VECTOR | gdal.OF_UPDATE,
                     open_options=['PRELUDE_STATEMENTS=SET statement_timeout TO "1h"'])
    sql_lyr = ds.ExecuteSQL('SHOW statement_timeout')
    f = sql_lyr.GetNextFeature()
    if f.GetField(0) != '1h':
        f.DumpReadable()
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)
    ret = ds.StartTransaction()
    assert ret == 0
    ret = ds.CommitTransaction()
    assert ret == 0
    gdal.ErrorReset()
    ds = None
    assert gdal.GetLastErrorMsg() == ''

    # Test wrong PRELUDE_STATEMENTS
    with gdaltest.error_handler():
        ds = gdal.OpenEx('PG:' + gdaltest.pg_connection_string,
                         gdal.OF_VECTOR | gdal.OF_UPDATE,
                         open_options=['PRELUDE_STATEMENTS=BEGIN;error SET LOCAL statement_timeout TO "1h";',
                                       'CLOSING_STATEMENTS=COMMIT;'])
    assert ds is None

    # Test wrong CLOSING_STATEMENTS
    ds = gdal.OpenEx('PG:' + gdaltest.pg_connection_string,
                     gdal.OF_VECTOR | gdal.OF_UPDATE,
                     open_options=['PRELUDE_STATEMENTS=BEGIN; SET LOCAL statement_timeout TO "1h";',
                                   'CLOSING_STATEMENTS=COMMIT;error'])
    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = None
    assert gdal.GetLastErrorMsg() != ''

###############################################################################
# Test retrieving an error from ExecuteSQL() (#6194)


def test_ogr_pg_80(with_and_without_postgis):

    if gdaltest.pg_ds is None or not with_and_without_postgis:
        pytest.skip()

    gdal.ErrorReset()
    with gdaltest.error_handler():
        sql_lyr = gdaltest.pg_ds.ExecuteSQL('SELECT FROM')
    assert gdal.GetLastErrorMsg() != ''
    assert sql_lyr is None

###############################################################################
# Test that ogr2ogr -skip properly rollbacks transactions (#6328)


def test_ogr_pg_81(with_and_without_postgis):

    if gdaltest.pg_ds is None or not with_and_without_postgis:
        pytest.skip()

    gdaltest.pg_ds.ReleaseResultSet(gdaltest.pg_ds.ExecuteSQL("create table ogr_pg_81_1(id varchar unique, foo varchar); SELECT AddGeometryColumn('ogr_pg_81_1','dummy',-1,'POINT',2);"))
    gdaltest.pg_ds.ReleaseResultSet(gdaltest.pg_ds.ExecuteSQL("create table ogr_pg_81_2(id varchar unique, foo varchar); SELECT AddGeometryColumn('ogr_pg_81_2','dummy',-1,'POINT',2);"))

    # 0755 = 493
    gdal.Mkdir('/vsimem/ogr_pg_81', 493)
    gdal.FileFromMemBuffer('/vsimem/ogr_pg_81/ogr_pg_81_1.csv',
                           """id,foo
1,1""")

    gdal.FileFromMemBuffer('/vsimem/ogr_pg_81/ogr_pg_81_2.csv',
                           """id,foo
1,1""")

    gdal.VectorTranslate('PG:' + gdaltest.pg_connection_string, '/vsimem/ogr_pg_81', accessMode='append')

    gdal.FileFromMemBuffer('/vsimem/ogr_pg_81/ogr_pg_81_2.csv',
                           """id,foo
2,2""")

    with gdaltest.error_handler():
        gdal.VectorTranslate('PG:' + gdaltest.pg_connection_string, '/vsimem/ogr_pg_81', accessMode='append', skipFailures=True)

    gdal.Unlink('/vsimem/ogr_pg_81/ogr_pg_81_1.csv')
    gdal.Unlink('/vsimem/ogr_pg_81/ogr_pg_81_2.csv')
    gdal.Unlink('/vsimem/ogr_pg_81')

    lyr = gdaltest.pg_ds.GetLayer('ogr_pg_81_2')
    f = lyr.GetNextFeature()
    f = lyr.GetNextFeature()
    if f['id'] != '2':
        f.DumpReadable()
        pytest.fail()
    lyr.ResetReading()  # flushes implicit transaction

###############################################################################
# Test that GEOMETRY_NAME works even when the geometry column creation is
# done through CreateGeomField (#6366)
# This is important for the ogr2ogr use case when the source geometry column
# is not-nullable, and hence the CreateGeomField() interface is used.


def test_ogr_pg_82(with_and_without_postgis):

    if gdaltest.pg_ds is None or not gdaltest.pg_has_postgis or not with_and_without_postgis:
        pytest.skip()

    lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_82', geom_type=ogr.wkbNone, options=['GEOMETRY_NAME=another_name'])
    lyr.CreateGeomField(ogr.GeomFieldDefn('my_geom', ogr.wkbPoint))
    assert lyr.GetLayerDefn().GetGeomFieldDefn(0).GetName() == 'another_name'

###############################################################################
# Test ZM support


def test_ogr_pg_83(with_and_without_postgis):

    if gdaltest.pg_ds is None or not gdaltest.pg_has_postgis or not with_and_without_postgis:
        pytest.skip()

    tests = [[ogr.wkbUnknown, [], 'POINT ZM (1 2 3 4)', 'POINT (1 2)'],
             [ogr.wkbUnknown, ['DIM=XYZM'], 'POINT ZM (1 2 3 4)', 'POINT ZM (1 2 3 4)'],
             [ogr.wkbUnknown, ['DIM=XYZ'], 'POINT ZM (1 2 3 4)', 'POINT Z (1 2 3)'],
             [ogr.wkbUnknown, ['DIM=XYM'], 'POINT M (1 2 4)', 'POINT M (1 2 4)'],
             [ogr.wkbPointZM, [], 'POINT ZM (1 2 3 4)', 'POINT ZM (1 2 3 4)'],
             [ogr.wkbPoint25D, [], 'POINT ZM (1 2 3 4)', 'POINT Z (1 2 3)'],
             [ogr.wkbPointM, [], 'POINT ZM (1 2 3 4)', 'POINT M (1 2 4)'],
             [ogr.wkbUnknown, ['GEOM_TYPE=geography', 'DIM=XYM'], 'POINT ZM (1 2 3 4)', 'POINT M (1 2 4)'],
            ]

    for (geom_type, options, wkt, expected_wkt) in tests:
        lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_83', geom_type=geom_type, options=options + ['OVERWRITE=YES'])
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt(wkt))
        lyr.CreateFeature(f)
        f = None
        lyr.ResetReading()
        f = lyr.GetNextFeature()
        got_wkt = ''
        if f is not None:
            geom = f.GetGeometryRef()
            if geom is not None:
                got_wkt = geom.ExportToIsoWkt()
        assert got_wkt == expected_wkt, (geom_type, options, wkt, expected_wkt, got_wkt)
        lyr.ResetReading()  # flushes implicit transaction

        if 'GEOM_TYPE=geography' in options:
            continue
        # Cannot do AddGeometryColumn( 'GEOMETRYM', 3 ) with PostGIS 2, and doesn't accept inserting a XYM geometry
        if gdaltest.pg_has_postgis_2 and geom_type == ogr.wkbUnknown and options == ['DIM=XYM']:
            continue

        lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_83', geom_type=ogr.wkbNone, options=options + ['OVERWRITE=YES'])
        # To force table creation to happen now so that following
        # CreateGeomField() is done through a AddGeometryColumn() call
        lyr.ResetReading()
        lyr.GetNextFeature()
        lyr.CreateGeomField(ogr.GeomFieldDefn("my_geom", geom_type))
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt(wkt))
        lyr.CreateFeature(f)
        f = None
        lyr.ResetReading()
        f = lyr.GetNextFeature()
        got_wkt = ''
        if f is not None:
            geom = f.GetGeometryRef()
            if geom is not None:
                got_wkt = geom.ExportToIsoWkt()
        assert got_wkt == expected_wkt, (geom_type, options, wkt, expected_wkt, got_wkt)
        lyr.ResetReading()  # flushes implicit transaction

    
###############################################################################
# Test description


def test_ogr_pg_84(with_and_without_postgis):

    if gdaltest.pg_ds is None or not with_and_without_postgis:
        pytest.skip()

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    lyr = ds.CreateLayer('ogr_pg_84', geom_type=ogr.wkbPoint, options=['OVERWRITE=YES', 'DESCRIPTION=foo'])
    # Test that SetMetadata() and SetMetadataItem() are without effect
    lyr.SetMetadata({'DESCRIPTION': 'bar'})
    lyr.SetMetadataItem('DESCRIPTION', 'baz')
    assert lyr.GetMetadataItem('DESCRIPTION') == 'foo'
    assert lyr.GetMetadata_List() == ['DESCRIPTION=foo'], lyr.GetMetadata()
    ds = None

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    ds.GetLayerCount()  # load all layers
    lyr = ds.GetLayerByName('ogr_pg_84')
    assert lyr.GetMetadataItem('DESCRIPTION') == 'foo'
    assert lyr.GetMetadata_List() == ['DESCRIPTION=foo'], lyr.GetMetadata()
    # Set with SetMetadata()
    lyr.SetMetadata(['DESCRIPTION=bar'])
    ds = None

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    lyr = ds.GetLayerByName('ogr_pg_84')  # load just this layer
    assert lyr.GetMetadataItem('DESCRIPTION') == 'bar'
    assert lyr.GetMetadataDomainList() is not None
    # Set with SetMetadataItem()
    lyr.SetMetadataItem('DESCRIPTION', 'baz')
    ds = None

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    lyr = ds.GetLayerByName('ogr_pg_84')
    assert lyr.GetMetadataDomainList() is not None
    assert lyr.GetMetadataItem('DESCRIPTION') == 'baz'
    # Unset with SetMetadataItem()
    lyr.SetMetadataItem('DESCRIPTION', None)
    ds = None

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
    lyr = ds.GetLayerByName('ogr_pg_84')  # load just this layer
    assert lyr.GetMetadataDomainList() is None
    assert lyr.GetMetadataItem('DESCRIPTION') is None
    ds = None

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
    ds.GetLayerCount()  # load all layers
    lyr = ds.GetLayerByName('ogr_pg_84')  # load just this layer
    assert lyr.GetMetadataItem('DESCRIPTION') is None
    ds = None

###############################################################################
# Test append of several layers in PG_USE_COPY mode (#6411)


def test_ogr_pg_85(with_and_without_postgis):

    if gdaltest.pg_ds is None or not with_and_without_postgis:
        pytest.skip()

    gdaltest.pg_ds.CreateLayer('ogr_pg_85_1')
    lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_85_2')
    lyr.CreateField(ogr.FieldDefn('foo'))
    gdaltest.pg_ds.ReleaseResultSet(gdaltest.pg_ds.ExecuteSQL('SELECT 1'))  # make sure the layers are well created

    old_val = gdal.GetConfigOption('PG_USE_COPY')
    gdal.SetConfigOption('PG_USE_COPY', 'YES')
    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    ds.GetLayerCount()
    ds.StartTransaction()
    lyr = ds.GetLayerByName('ogr_pg_85_1')
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    lyr = ds.GetLayerByName('ogr_pg_85_2')
    feat_defn = lyr.GetLayerDefn()
    assert feat_defn.GetFieldCount() == 1
    f = ogr.Feature(feat_defn)
    assert lyr.CreateFeature(f) == 0
    ds.CommitTransaction()
    ds = None

    # Although test real ogr2ogr scenario
    # 0755 = 493
    gdal.Mkdir('/vsimem/ogr_pg_85', 493)
    gdal.FileFromMemBuffer('/vsimem/ogr_pg_85/ogr_pg_85_1.csv',
                           """id,foo
1,1""")

    gdal.FileFromMemBuffer('/vsimem/ogr_pg_85/ogr_pg_85_2.csv',
                           """id,foo
1,1""")

    gdal.VectorTranslate('PG:' + gdaltest.pg_connection_string, '/vsimem/ogr_pg_85', accessMode='append')

    gdal.Unlink('/vsimem/ogr_pg_85/ogr_pg_85_1.csv')
    gdal.Unlink('/vsimem/ogr_pg_85/ogr_pg_85_2.csv')
    gdal.Unlink('/vsimem/ogr_pg_85')

    gdal.SetConfigOption('PG_USE_COPY', old_val)

    lyr = gdaltest.pg_ds.GetLayerByName('ogr_pg_85_2')
    assert lyr.GetFeatureCount() == 2

###############################################################################
# Test OFTBinary


def test_ogr_pg_86(with_and_without_postgis):

    if gdaltest.pg_ds is None or not with_and_without_postgis:
        pytest.skip()

    old_val = gdal.GetConfigOption('PG_USE_COPY')

    gdal.SetConfigOption('PG_USE_COPY', 'YES')

    lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_86')
    lyr.CreateField(ogr.FieldDefn('test', ogr.OFTBinary))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFieldBinaryFromHexString('test', '3020')
    lyr.CreateFeature(f)
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f.GetField(0) != '3020':
        gdal.SetConfigOption('PG_USE_COPY', old_val)
        pytest.fail()

    gdal.SetConfigOption('PG_USE_COPY', 'NO')

    lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_86', options=['OVERWRITE=YES'])
    lyr.CreateField(ogr.FieldDefn('test', ogr.OFTBinary))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFieldBinaryFromHexString('test', '3020')
    lyr.CreateFeature(f)
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f.GetField(0) != '3020':
        gdal.SetConfigOption('PG_USE_COPY', old_val)
        pytest.fail()

    gdal.SetConfigOption('PG_USE_COPY', old_val)

###############################################################################
# Test sequence updating (#7032)


def test_ogr_pg_87(with_and_without_postgis):

    if gdaltest.pg_ds is None or not with_and_without_postgis:
        pytest.skip()

    lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_87')
    lyr.CreateField(ogr.FieldDefn('test', ogr.OFTString))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(10)
    lyr.CreateFeature(f)

    # Test updating of sequence after CreateFeatureViaCopy
    gdaltest.pg_ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    lyr = gdaltest.pg_ds.GetLayerByName('ogr_pg_87')
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    assert f.GetFID() == 11

    # Test updating of sequence after CreateFeatureViaInsert
    gdaltest.pg_ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    lyr = gdaltest.pg_ds.GetLayerByName('ogr_pg_87')
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    assert f.GetFID() == 12

###############################################################################
# Test JSON subtype


def test_ogr_pg_json():

    if gdaltest.pg_ds is None:
        pytest.skip()

    lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_json')
    fld_defn = ogr.FieldDefn('test_json', ogr.OFTString)
    fld_defn.SetSubType(ogr.OFSTJSON)
    lyr.CreateField(fld_defn)
    f = ogr.Feature(lyr.GetLayerDefn())
    f['test_json'] = '{"a": "b"}'
    lyr.CreateFeature(f)

    gdaltest.pg_ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    lyr = gdaltest.pg_ds.GetLayer('ogr_pg_json')
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetSubType() == ogr.OFSTJSON
    f = lyr.GetNextFeature()
    if f.GetField(0) != '{"a": "b"}':
        f.DumpReadable()
        pytest.fail()

    sql_lyr = gdaltest.pg_ds.ExecuteSQL('SELECT * FROM ogr_pg_json')
    assert sql_lyr.GetLayerDefn().GetFieldDefn(0).GetSubType() == ogr.OFSTJSON
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

###############################################################################
# Test generated columns


def test_ogr_pg_generated_columns():

    if gdaltest.pg_ds is None:
        pytest.skip()
    if gdaltest.pg_version < (12,):
        pytest.skip()

    gdaltest.pg_ds.ExecuteSQL("DROP TABLE IF EXISTS test_ogr_pg_generated_columns")
    gdaltest.pg_ds.ExecuteSQL("CREATE TABLE test_ogr_pg_generated_columns(id SERIAL PRIMARY KEY, unused VARCHAR, foo INTEGER, bar INTEGER GENERATED ALWAYS AS (foo+1) STORED)")
    gdaltest.pg_ds.ExecuteSQL("INSERT INTO test_ogr_pg_generated_columns VALUES (DEFAULT,NULL, 10,DEFAULT)")

    gdaltest.pg_ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    lyr = gdaltest.pg_ds.GetLayer('test_ogr_pg_generated_columns')
    f = lyr.GetNextFeature()

    assert f['foo'] == 10
    assert f['bar'] == 11
    f['foo'] = 20
    assert lyr.SetFeature(f) == 0

    f = lyr.GetFeature(1)
    assert f['foo'] == 20
    assert f['bar'] == 21
    f = None

    lyr.ResetReading()
    lyr.DeleteField(lyr.GetLayerDefn().GetFieldIndex('unused'))

    f = ogr.Feature(lyr.GetLayerDefn())
    f['foo'] = 30
    f['bar'] = 123456 # will be ignored
    assert lyr.CreateFeature(f) == 0

    f = lyr.GetFeature(2)
    assert f['foo'] == 30
    assert f['bar'] == 31

    gdaltest.pg_ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    lyr = gdaltest.pg_ds.GetLayer('test_ogr_pg_generated_columns')
    with gdaltest.config_option('PG_USE_COPY', 'YES'):
        f = ogr.Feature(lyr.GetLayerDefn())
        f['foo'] = 40
        f['bar'] = 123456 # will be ignored
        assert lyr.CreateFeature(f) == 0

    f = lyr.GetFeature(3)
    assert f['foo'] == 40
    assert f['bar'] == 41

    gdaltest.pg_ds.ExecuteSQL('DELLAYER:test_ogr_pg_generated_columns')
    gdaltest.pg_ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)

###############################################################################
# Test UNIQUE constraints


def test_ogr_pg_unique():

    if gdaltest.pg_ds is None:
        pytest.skip()

    # Create table to test UNIQUE constraints
    gdaltest.pg_ds.ExecuteSQL("DROP TABLE IF EXISTS test_ogr_pg_unique CASCADE")
    lyr = gdaltest.pg_ds.CreateLayer('test_ogr_pg_unique')

    fld_defn = ogr.FieldDefn('with_unique', ogr.OFTString)
    fld_defn.SetUnique(True)
    lyr.CreateField(fld_defn)

    fld_defn = ogr.FieldDefn('with_unique_and_explicit_unique_idx', ogr.OFTString)
    fld_defn.SetUnique(True)
    lyr.CreateField(fld_defn)

    lyr.CreateField(ogr.FieldDefn('without_unique', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('unique_on_several_col1', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('unique_on_several_col2', ogr.OFTString))
    gdaltest.pg_ds.ExecuteSQL("CREATE UNIQUE INDEX unique_idx_with_unique_and_explicit_unique_idx ON test_ogr_pg_unique(with_unique_and_explicit_unique_idx)")
    gdaltest.pg_ds.ExecuteSQL("CREATE UNIQUE INDEX unique_idx_unique_constraints ON test_ogr_pg_unique(unique_on_several_col1, unique_on_several_col2)")

    # Check after re-opening
    gdaltest.pg_ds = None
    gdaltest.pg_ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    lyr = gdaltest.pg_ds.GetLayerByName('test_ogr_pg_unique')
    feat_defn = lyr.GetLayerDefn()
    assert feat_defn.GetFieldDefn(feat_defn.GetFieldIndex('with_unique')).IsUnique()
    assert feat_defn.GetFieldDefn(feat_defn.GetFieldIndex('with_unique_and_explicit_unique_idx')).IsUnique()
    assert not feat_defn.GetFieldDefn(feat_defn.GetFieldIndex('without_unique')).IsUnique()
    assert not feat_defn.GetFieldDefn(feat_defn.GetFieldIndex('unique_on_several_col1')).IsUnique()
    assert not feat_defn.GetFieldDefn(feat_defn.GetFieldIndex('unique_on_several_col2')).IsUnique()

    # Test AlterFieldDefn()

    # Unchanged state: no unique
    fld_defn = ogr.FieldDefn('without_unique', ogr.OFTString)
    fld_defn.SetUnique(False)
    assert lyr.AlterFieldDefn(feat_defn.GetFieldIndex(fld_defn.GetName()), fld_defn, ogr.ALTER_UNIQUE_FLAG) == ogr.OGRERR_NONE
    assert not feat_defn.GetFieldDefn(feat_defn.GetFieldIndex(fld_defn.GetName())).IsUnique()

    # Unchanged state: unique
    fld_defn = ogr.FieldDefn('with_unique', ogr.OFTString)
    fld_defn.SetUnique(True)
    assert lyr.AlterFieldDefn(feat_defn.GetFieldIndex(fld_defn.GetName()), fld_defn, ogr.ALTER_UNIQUE_FLAG) == ogr.OGRERR_NONE
    assert feat_defn.GetFieldDefn(feat_defn.GetFieldIndex(fld_defn.GetName())).IsUnique()

    # no unique -> unique
    fld_defn = ogr.FieldDefn('without_unique', ogr.OFTString)
    fld_defn.SetUnique(True)
    assert lyr.AlterFieldDefn(feat_defn.GetFieldIndex(fld_defn.GetName()), fld_defn, ogr.ALTER_UNIQUE_FLAG) == ogr.OGRERR_NONE
    assert feat_defn.GetFieldDefn(feat_defn.GetFieldIndex(fld_defn.GetName())).IsUnique()

    # unique -> no unique : unsupported
    fld_defn = ogr.FieldDefn('without_unique', ogr.OFTString)
    fld_defn.SetUnique(False)
    gdal.ErrorReset()
    with gdaltest.error_handler():
        assert lyr.AlterFieldDefn(feat_defn.GetFieldIndex(fld_defn.GetName()), fld_defn, ogr.ALTER_UNIQUE_FLAG) == ogr.OGRERR_NONE
    assert gdal.GetLastErrorMsg() != ''
    assert feat_defn.GetFieldDefn(feat_defn.GetFieldIndex(fld_defn.GetName())).IsUnique()

    # Check after re-opening
    gdaltest.pg_ds = None
    gdaltest.pg_ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    lyr = gdaltest.pg_ds.GetLayerByName('test_ogr_pg_unique')
    feat_defn = lyr.GetLayerDefn()
    assert feat_defn.GetFieldDefn(feat_defn.GetFieldIndex('without_unique')).IsUnique()

    # Cleanup
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:test_ogr_pg_unique')
    gdaltest.pg_ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)

###############################################################################
# Test UUID datatype support

def test_ogr_pg_uuid():
    if gdaltest.pg_ds is None:
        pytest.skip()

    lyr = gdaltest.pg_ds.CreateLayer('test_ogr_pg_uuid')

    fd = ogr.FieldDefn('uid', ogr.OFTString)
    fd.SetSubType(ogr.OFSTUUID)

    assert lyr.CreateField(fd) == 0

    lyr.StartTransaction()
    f = ogr.Feature(lyr.GetLayerDefn())
    f['uid'] = '6f9619ff-8b86-d011-b42d-00c04fc964ff'
    lyr.CreateFeature(f)
    lyr.CommitTransaction()
    
    test_ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=0)
    lyr = test_ds.GetLayer('test_ogr_pg_uuid')
    fd = lyr.GetLayerDefn().GetFieldDefn(0)
    assert fd.GetType() == ogr.OFTString
    assert fd.GetSubType() == ogr.OFSTUUID
    f = lyr.GetNextFeature()

    assert f.GetField(0) == '6f9619ff-8b86-d011-b42d-00c04fc964ff'

    test_ds.Destroy()

###############################################################################
#


def test_ogr_pg_table_cleanup():

    if gdaltest.pg_ds is None:
        pytest.skip()

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:tpoly')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:tpolycopy')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:test_for_tables_equal_param')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:datetest')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:testgeom')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:datatypetest')
    # gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:datatypetest_withouttimestamptz' )
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:datatypetest2')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:testsrtext')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:testsrtext2')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:testsrtext3')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:testsrtext4')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:testsrtext5')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:testoverflows')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:table36_inherited2')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:table36_inherited')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:table36_base')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:table37_inherited')
    gdaltest.pg_ds.ExecuteSQL('DROP TABLE table37_base CASCADE')
    gdaltest.pg_ds.ExecuteSQL('DROP VIEW testview')
    gdaltest.pg_ds.ExecuteSQL("DELETE FROM geometry_columns WHERE f_table_name='testview'")
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:select')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:bigtable')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:test_geog')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:no_pk_table')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:no_geometry_table')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:ogr_pg_55')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:ogr_pg_56')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:ogr_pg_57')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:ogr_pg_58')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:ogr_pg_60')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:ogr_pg_61')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:ogr_pg_63')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:ogr_pg_65')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:ogr_pg_65_copied')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:ogr_pg_67')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:ogr_pg_68')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:ogr_pg_70')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:ogr_pg_72')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:ogr_pg_73')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:ogr_pg_74')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:ogr_pg_75')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:ogr_pg_76_lyr1')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:ogr_pg_76_lyr2')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:test_curve')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:test_curve_3d')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:ogr_pg_77_1')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:ogr_pg_77_2')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:ogr_pg_78')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:ogr_pg_78_2')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:ogr_pg_81_1')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:ogr_pg_81_2')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:ogr_pg_82')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:ogr_pg_83')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:ogr_pg_84')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:ogr_pg_85_1')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:ogr_pg_85_2')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:ogr_pg_86')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:ogr_pg_87')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:ogr_pg_json')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:test_ogr_pg_uuid')

    # Drop second 'tpoly' from schema 'AutoTest-schema' (do NOT quote names here)
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:AutoTest-schema.tpoly')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:AutoTest-schema.test41')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:AutoTest-schema.table36_base')
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:AutoTest-schema.table36_inherited')
    # Drop 'AutoTest-schema' (here, double quotes are required)
    gdaltest.pg_ds.ExecuteSQL('DROP SCHEMA \"AutoTest-schema\" CASCADE')
    gdal.PopErrorHandler()

###############################################################################
# Test AbortSQL

def test_abort_sql():

    if gdaltest.pg_ds is None:
        pytest.skip()

    def abortAfterDelay():
        print("Aborting SQL...")
        assert gdaltest.pg_ds.AbortSQL() == ogr.OGRERR_NONE

    t = threading.Timer(0.5, abortAfterDelay)
    t.start()

    start = time.time()

    # Long running query
    sql = "SELECT pg_sleep(3)"
    gdaltest.pg_ds.ExecuteSQL(sql)

    end = time.time()
    assert int(end - start) < 1

    # Same test with a GDAL dataset
    ds2 = gdal.OpenEx('PG:' + gdaltest.pg_connection_string, gdal.OF_VECTOR)

    def abortAfterDelay2():
        print("Aborting SQL...")
        assert ds2.AbortSQL() == ogr.OGRERR_NONE

    t = threading.Timer(0.5, abortAfterDelay2)
    t.start()

    start = time.time()

    # Long running query
    ds2.ExecuteSQL(sql)

    end = time.time()
    assert int(end - start) < 1


def test_ogr_pg_cleanup():

    if gdaltest.pg_ds is None:
        pytest.skip()

    gdaltest.pg_ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update=1)
    test_ogr_pg_table_cleanup()

    gdaltest.pg_ds.Destroy()
    gdaltest.pg_ds = None

# NOTE: The ogr_pg_19 intentionally executed after ogr_pg_2





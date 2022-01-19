#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test MSSQLSpatial driver functionality.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2018, Even Rouault <even dot rouault at spatialys.com>
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



import gdaltest
import ogrtest
from osgeo import gdal
from osgeo import ogr
from osgeo import osr
import pytest

###############################################################################
# Open Database.


def test_ogr_mssqlspatial_1():

    gdaltest.mssqlspatial_ds = None

    if ogr.GetDriverByName('MSSQLSpatial') is None:
        pytest.skip()

    gdaltest.mssqlspatial_dsname = gdal.GetConfigOption(
        'OGR_MSSQL_CONNECTION_STRING',
        # localhost doesn't work under chroot
        'MSSQL:server=127.0.0.1;database=TestDB;driver=ODBC Driver 17 for SQL Server;UID=SA;PWD=DummyPassw0rd')
    gdaltest.mssqlspatial_ds = ogr.Open(gdaltest.mssqlspatial_dsname, update=1)
    if gdaltest.mssqlspatial_ds is None:
        pytest.skip()

    # Fetch and store the major-version number of the SQL Server engine in use
    sql_lyr = gdaltest.mssqlspatial_ds.ExecuteSQL(
        'SELECT SERVERPROPERTY(\'ProductVersion\')')
    feat = sql_lyr.GetNextFeature()
    gdaltest.mssqlspatial_version = feat.GetFieldAsString(0)
    gdaltest.mssqlspatial_ds.ReleaseResultSet(sql_lyr)

    gdaltest.mssqlspatial_version_major = -1
    if '.' in gdaltest.mssqlspatial_version:
        version_major_str = gdaltest.mssqlspatial_version[
            0:gdaltest.mssqlspatial_version.find('.')]
        if version_major_str.isdigit():
            gdaltest.mssqlspatial_version_major = int(version_major_str)

    # Check whether the database server provides support for Z and M values,
    # available since SQL Server 2012
    gdaltest.mssqlspatial_has_z_m = (gdaltest.mssqlspatial_version_major >= 11)

###############################################################################
# Create table from data/poly.shp


def test_ogr_mssqlspatial_2():

    if gdaltest.mssqlspatial_ds is None:
        pytest.skip()

    shp_ds = ogr.Open('data/poly.shp')
    gdaltest.shp_ds = shp_ds
    shp_lyr = shp_ds.GetLayer(0)

    ######################################################
    # Create Layer
    gdaltest.mssqlspatial_lyr = gdaltest.mssqlspatial_ds.CreateLayer(
        'tpoly', srs=shp_lyr.GetSpatialRef())

    ######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def(gdaltest.mssqlspatial_lyr,
                                   [('AREA', ogr.OFTReal),
                                    ('EAS_ID', ogr.OFTInteger),
                                    ('PRFEDEA', ogr.OFTString),
                                    ('SHORTNAME', ogr.OFTString, 8),
                                    ('INT64', ogr.OFTInteger64)])

    ######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature(
        feature_def=gdaltest.mssqlspatial_lyr.GetLayerDefn())

    feat = shp_lyr.GetNextFeature()
    gdaltest.poly_feat = []

    while feat is not None:

        gdaltest.poly_feat.append(feat)

        dst_feat.SetFrom(feat)
        dst_feat.SetField('INT64', 1234567890123)
        gdaltest.mssqlspatial_lyr.CreateFeature(dst_feat)

        feat = shp_lyr.GetNextFeature()

    dst_feat = None

    assert gdaltest.mssqlspatial_lyr.GetFeatureCount() == shp_lyr.GetFeatureCount(), \
        'not matching feature count'

    got_srs = gdaltest.mssqlspatial_lyr.GetSpatialRef()
    expected_srs = shp_lyr.GetSpatialRef()
    assert got_srs.GetAuthorityCode(None) == expected_srs.GetAuthorityCode(None), \
        'not matching spatial ref'

###############################################################################
# Verify that stuff we just wrote is still OK.


def test_ogr_mssqlspatial_3():
    if gdaltest.mssqlspatial_ds is None:
        pytest.skip()

    assert gdaltest.mssqlspatial_lyr.GetGeometryColumn() == 'ogr_geometry'

    assert gdaltest.mssqlspatial_lyr.GetFeatureCount() == 10

    expect = [168, 169, 166, 158, 165]

    gdaltest.mssqlspatial_lyr.SetAttributeFilter('eas_id < 170')
    tr = ogrtest.check_features_against_list(gdaltest.mssqlspatial_lyr,
                                             'eas_id', expect)

    assert gdaltest.mssqlspatial_lyr.GetFeatureCount() == 5

    gdaltest.mssqlspatial_lyr.SetAttributeFilter(None)
    gdaltest.mssqlspatial_lyr.ResetReading()

    for i in range(len(gdaltest.poly_feat)):
        orig_feat = gdaltest.poly_feat[i]
        read_feat = gdaltest.mssqlspatial_lyr.GetNextFeature()

        assert (ogrtest.check_feature_geometry(read_feat, orig_feat.GetGeometryRef(),
                                          max_error=0.001) == 0)

        for fld in range(3):
            if orig_feat.GetField(fld) != read_feat.GetField(fld):
                orig_feat.DumpReadable()
                read_feat.DumpReadable()
                assert False, ('Attribute %d does not match' % fld)
        assert read_feat.GetField('INT64') == 1234567890123

        read_feat = None
        orig_feat = None

    gdaltest.poly_feat = None
    gdaltest.shp_ds = None

    assert tr

###############################################################################
# Write more features with a bunch of different geometries, and verify the
# geometries are still OK.


def test_ogr_mssqlspatial_4():
    if gdaltest.mssqlspatial_ds is None:
        pytest.skip()

    dst_feat = ogr.Feature(
        feature_def=gdaltest.mssqlspatial_lyr.GetLayerDefn())
    wkt_list = ['10', '2', '1', '4', '5', '6']

    # If the database engine supports 3D features, include one in the tests
    if gdaltest.mssqlspatial_has_z_m:
        wkt_list.append('3d_1')

    use_bcp = 'BCP' in gdal.GetDriverByName('MSSQLSpatial').GetMetadataItem(gdal.DMD_LONGNAME)
    if use_bcp:
        MSSQLSPATIAL_USE_BCP = gdal.GetConfigOption('MSSQLSPATIAL_USE_BCP', 'YES')
        if MSSQLSPATIAL_USE_BCP.upper() in ('NO', 'OFF', 'FALSE'):
            use_bcp = False

    for item in wkt_list:
        wkt_filename = 'data/wkb_wkt/' + item + '.wkt'
        wkt = open(wkt_filename).read()
        geom = ogr.CreateGeometryFromWkt(wkt)

        ######################################################################
        # Write geometry as a new feature.

        dst_feat.SetGeometryDirectly(geom)
        dst_feat.SetField('PRFEDEA', item)
        dst_feat.SetFID(-1)
        assert gdaltest.mssqlspatial_lyr.CreateFeature(dst_feat) == ogr.OGRERR_NONE, \
            ('CreateFeature failed creating feature ' +
                                 'from file "' + wkt_filename + '"')

        ######################################################################
        # Before reading back the record, verify that the newly added feature
        # is returned from the CreateFeature method with a newly assigned FID.

        if not use_bcp:
            assert dst_feat.GetFID() != -1, \
                'Assigned FID was not returned in the new feature'

        ######################################################################
        # Read back the feature and get the geometry.

        gdaltest.mssqlspatial_lyr.SetAttributeFilter("PRFEDEA = '%s'" % item)
        feat_read = gdaltest.mssqlspatial_lyr.GetNextFeature()
        geom_read = feat_read.GetGeometryRef()

        if ogrtest.check_feature_geometry(feat_read, geom) != 0:
            print(item)
            print(wkt)
            pytest.fail(geom_read)

        feat_read.Destroy()

    dst_feat.Destroy()
    gdaltest.mssqlspatial_lyr.ResetReading()  # to close implicit transaction

###############################################################################
# Run test_ogrsf


def test_ogr_mssqlspatial_test_ogrsf():

    if gdaltest.mssqlspatial_ds is None:
        pytest.skip()

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + " -ro '" + gdaltest.mssqlspatial_dsname + "' tpoly")

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

###############################################################################
# Verify features can be created in an existing table that includes a geometry
# column but is not registered in the "geometry_columns" table.


def test_ogr_mssqlspatial_create_feature_in_unregistered_table():
    if gdaltest.mssqlspatial_ds is None:
        pytest.skip()

    # Create a feature that specifies a spatial-reference system
    spatial_reference = osr.SpatialReference()
    spatial_reference.ImportFromEPSG(4326)

    feature = ogr.Feature(ogr.FeatureDefn('Unregistered'))
    feature.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (10 20)',
                                                          spatial_reference))

    # Create a table that includes a geometry column but is not registered in
    # the "geometry_columns" table
    gdaltest.mssqlspatial_ds.ExecuteSQL(
        'CREATE TABLE Unregistered'
        + '('
        +   'ObjectID int IDENTITY(1,1) NOT NULL PRIMARY KEY,'
        +   'Shape geometry NOT NULL'
        + ');')

    # Create a new MSSQLSpatial data source, one that will find the table just
    # created and make it available via GetLayerByName()
    with gdaltest.config_option('MSSQLSPATIAL_USE_GEOMETRY_COLUMNS', 'NO'):
        test_ds = ogr.Open(gdaltest.mssqlspatial_dsname, update=1)

    assert test_ds is not None, 'cannot open data source'

    # Get a layer backed by the newly created table and verify that (as it is
    # unregistered) it has no associated spatial-reference system
    unregistered_layer = test_ds.GetLayerByName('Unregistered');
    assert unregistered_layer is not None, 'did not get Unregistered layer'

    unregistered_spatial_reference = unregistered_layer.GetSpatialRef()
    assert unregistered_spatial_reference is None, \
        'layer Unregistered unexpectedly has an SRS'

    # Verify creating the feature in the layer succeeds despite the lack of an
    # associated spatial-reference system
    assert unregistered_layer.CreateFeature(feature) == ogr.OGRERR_NONE, \
        'CreateFeature failed'

    # Verify the created feature received the spatial-reference system of the
    # original, as none was associated with the table
    unregistered_layer.ResetReading()
    created_feature = unregistered_layer.GetNextFeature()
    assert created_feature is not None, 'did not get feature'

    created_feature_geometry = created_feature.GetGeometryRef()
    created_spatial_reference = created_feature_geometry.GetSpatialReference()
    assert ((created_spatial_reference == spatial_reference)
            or ((created_spatial_reference is not None)
                and created_spatial_reference.IsSame(spatial_reference, options = ['IGNORE_DATA_AXIS_TO_SRS_AXIS_MAPPING=YES'] ))), \
        'created-feature SRS does not match original'

    # Clean up
    test_ds.Destroy()
    feature.Destroy()

###############################################################################
# Test all datatype support

def test_ogr_mssqlspatial_datatypes():
    if gdaltest.mssqlspatial_ds is None:
        pytest.skip()

    lyr = gdaltest.mssqlspatial_ds.CreateLayer('test_ogr_mssql_datatypes')

    fd = ogr.FieldDefn('int32', ogr.OFTInteger)
    assert lyr.CreateField(fd) == 0

    fd = ogr.FieldDefn('int16', ogr.OFTInteger)
    fd.SetSubType(ogr.OFSTInt16)
    assert lyr.CreateField(fd) == 0

    fd = ogr.FieldDefn('int64', ogr.OFTInteger64)
    assert lyr.CreateField(fd) == 0

    fd = ogr.FieldDefn('float32', ogr.OFTReal)
    fd.SetSubType(ogr.OFSTFloat32)
    assert lyr.CreateField(fd) == 0

    fd = ogr.FieldDefn('float64', ogr.OFTReal)
    assert lyr.CreateField(fd) == 0

    fd = ogr.FieldDefn('numeric', ogr.OFTReal)
    fd.SetWidth(12)
    fd.SetPrecision(6)
    assert lyr.CreateField(fd) == 0

    fd = ogr.FieldDefn('numeric_width_9_prec_0_from_int', ogr.OFTInteger)
    fd.SetWidth(9)
    fd.SetPrecision(0)
    assert lyr.CreateField(fd) == 0

    fd = ogr.FieldDefn('numeric_width_9_prec_0_from_real', ogr.OFTReal)
    fd.SetWidth(9)
    fd.SetPrecision(0)
    assert lyr.CreateField(fd) == 0

    fd = ogr.FieldDefn('numeric_width_18_prec_0', ogr.OFTInteger64)
    fd.SetWidth(18)
    fd.SetPrecision(0)
    assert lyr.CreateField(fd) == 0

    fd = ogr.FieldDefn('string', ogr.OFTString)
    assert lyr.CreateField(fd) == 0

    fd = ogr.FieldDefn('string_limited', ogr.OFTString)
    fd.SetWidth(5)
    assert lyr.CreateField(fd) == 0

    fd = ogr.FieldDefn('date', ogr.OFTDate)
    assert lyr.CreateField(fd) == 0

    fd = ogr.FieldDefn('time', ogr.OFTTime)
    assert lyr.CreateField(fd) == 0

    fd = ogr.FieldDefn('datetime', ogr.OFTDateTime)
    assert lyr.CreateField(fd) == 0

    fd = ogr.FieldDefn('uid', ogr.OFTString)
    fd.SetSubType(ogr.OFSTUUID)
    assert lyr.CreateField(fd) == 0

    fd = ogr.FieldDefn('binary', ogr.OFTBinary)
    assert lyr.CreateField(fd) == 0

    lyr.StartTransaction()
    f = ogr.Feature(lyr.GetLayerDefn())
    f['int32'] = (1 << 31) - 1
    f['int16'] = (1 << 15) - 1
    f['int64'] = (1 << 63) - 1
    f['float32'] = 1.25
    f['float64'] = 1.25123456789
    f['numeric'] = 123456.789012
    f['numeric_width_9_prec_0_from_int'] = 123456789
    f['numeric_width_9_prec_0_from_real'] = 123456789
    f['numeric_width_18_prec_0'] = 12345678912345678
    f['string'] = 'unlimited string'
    f['string_limited'] = 'abcd\u00e9'
    f['date'] = '2021/12/11'
    f['time'] = '12:34:56'
    f['datetime'] = '2021/12/11 12:34:56'
    f['uid'] = '6F9619FF-8B86-D011-B42D-00C04FC964FF'
    f.SetFieldBinaryFromHexString('binary', '0123465789ABCDEF')
    lyr.CreateFeature(f)
    lyr.CommitTransaction()

    test_ds = ogr.Open(gdaltest.mssqlspatial_dsname, update=0)
    lyr = test_ds.GetLayer('test_ogr_mssql_datatypes')
    lyr_defn = lyr.GetLayerDefn()

    fd = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex('int32'))
    assert fd.GetType() == ogr.OFTInteger
    assert fd.GetSubType() == ogr.OFSTNone
    assert fd.GetWidth() == 0

    fd = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex('int16'))
    assert fd.GetType() == ogr.OFTInteger
    assert fd.GetSubType() == ogr.OFSTInt16
    assert fd.GetWidth() == 0

    fd = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex('int64'))
    assert fd.GetType() == ogr.OFTInteger64
    assert fd.GetSubType() == ogr.OFSTNone
    assert fd.GetWidth() == 0

    fd = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex('float32'))
    assert fd.GetType() == ogr.OFTReal
    assert fd.GetSubType() == ogr.OFSTFloat32
    assert fd.GetWidth() == 0

    fd = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex('float64'))
    assert fd.GetType() == ogr.OFTReal
    assert fd.GetSubType() == ogr.OFSTNone
    assert fd.GetWidth() == 0

    fd = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex('numeric'))
    assert fd.GetType() == ogr.OFTReal
    assert fd.GetSubType() == ogr.OFSTNone
    assert fd.GetWidth() == 12
    assert fd.GetPrecision() == 6

    fd = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex('numeric_width_9_prec_0_from_int'))
    assert fd.GetType() == ogr.OFTInteger
    assert fd.GetSubType() == ogr.OFSTNone
    assert fd.GetWidth() == 9
    assert fd.GetPrecision() == 0

    fd = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex('numeric_width_9_prec_0_from_real'))
    assert fd.GetType() == ogr.OFTInteger
    assert fd.GetSubType() == ogr.OFSTNone
    assert fd.GetWidth() == 9
    assert fd.GetPrecision() == 0

    fd = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex('numeric_width_18_prec_0'))
    assert fd.GetType() == ogr.OFTInteger64
    assert fd.GetSubType() == ogr.OFSTNone
    assert fd.GetWidth() == 18
    assert fd.GetPrecision() == 0

    fd = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex('string'))
    assert fd.GetType() == ogr.OFTString
    assert fd.GetWidth() == 0

    fd = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex('string_limited'))
    assert fd.GetType() == ogr.OFTString
    assert fd.GetWidth() == 5

    fd = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex('date'))
    assert fd.GetType() == ogr.OFTDate

    fd = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex('time'))
    # We get a string currently
    # assert fd.GetType() == ogr.OFTTime

    fd = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex('datetime'))
    assert fd.GetType() == ogr.OFTDateTime

    fd = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex('uid'))
    assert fd.GetType() == ogr.OFTString
    assert fd.GetSubType() == ogr.OFSTUUID

    fd = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex('binary'))
    assert fd.GetType() == ogr.OFTBinary

    f = lyr.GetNextFeature()
    assert f['int32'] == (1 << 31) - 1
    assert f['int16'] == (1 << 15) - 1
    assert f['int64'] == (1 << 63) - 1
    assert f['float32'] == 1.25
    assert f['float64'] == 1.25123456789
    assert f['numeric'] == 123456.789012
    assert f['numeric_width_9_prec_0_from_int'] == 123456789
    assert f['numeric_width_9_prec_0_from_real'] == 123456789
    assert f['numeric_width_18_prec_0'] == 12345678912345678
    assert f['string'] == 'unlimited string'
    assert f['string_limited'] == 'abcd\u00e9'
    assert f['date'] == '2021/12/11'
    assert f['time'] == '12:34:56' or f['time'].startswith('12:34:56.0')
    assert f['datetime'] == '2021/12/11 12:34:56'
    assert f['uid'] == '6F9619FF-8B86-D011-B42D-00C04FC964FF'
    assert f['binary'] == '0123465789ABCDEF'

    test_ds.Destroy()


###############################################################################
#


def test_ogr_mssqlspatial_cleanup():

    if gdaltest.mssqlspatial_ds is None:
        pytest.skip()

    gdaltest.mssqlspatial_ds = None

    gdaltest.mssqlspatial_ds = ogr.Open(gdaltest.mssqlspatial_dsname, update=1)
    gdaltest.mssqlspatial_ds.ExecuteSQL('DROP TABLE Unregistered')
    gdaltest.mssqlspatial_ds.ExecuteSQL('DROP TABLE tpoly')
    gdaltest.mssqlspatial_ds.ExecuteSQL('DROP TABLE test_ogr_mssql_datatypes')

    gdaltest.mssqlspatial_ds = None




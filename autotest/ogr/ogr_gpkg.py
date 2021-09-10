#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GeoPackage driver functionality.
# Author:   Paul Ramsey <pramsey@boundlessgeom.com>
#
###############################################################################
# Copyright (c) 2004, Paul Ramsey <pramsey@boundlessgeom.com>
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

import math
import os
import struct
import sys
import pytest
import time
import threading

from osgeo import gdal
from osgeo import ogr
from osgeo import osr
import gdaltest
from test_py_scripts import samples_path

pytestmark = pytest.mark.require_driver('GPKG')

###############################################################################
@pytest.fixture(autouse=True, scope='module')
def startup_and_cleanup():

    gdaltest.gpkg_dr = ogr.GetDriverByName('GPKG')

    try:
        os.remove('tmp/gpkg_test.gpkg')
    except OSError:
        pass

    # This is to speed-up the runtime of tests on EXT4 filesystems
    # Do not use this for production environment if you care about data safety
    # w.r.t system/OS crashes, unless you know what you are doing.
    gdal.SetConfigOption('OGR_SQLITE_SYNCHRONOUS', 'OFF')

    yield

    gdal.SetConfigOption('OGR_SQLITE_SYNCHRONOUS', None)

    if gdal.ReadDir('/vsimem') is not None:
        print(gdal.ReadDir('/vsimem'))
        for f in gdal.ReadDir('/vsimem'):
            gdal.Unlink('/vsimem/' + f)

    try:
        os.remove('tmp/gpkg_test.gpkg')
    except OSError:
        pass

###############################################################################
# Validate a geopackage


def _validate_check(filename):
    path = samples_path
    if path not in sys.path:
        sys.path.append(path)
    try:
        import validate_gpkg
    except ImportError:
        print('Cannot import validate_gpkg')
        return
    validate_gpkg.check(filename, extra_checks=True, warning_as_error=True)


def validate(filename, quiet=False):
    my_filename = filename
    if my_filename.startswith('/vsimem/'):
        my_filename = 'tmp/validate.gpkg'
        f = gdal.VSIFOpenL(filename, 'rb')
        if f is None:
            print('Cannot open %s' % filename)
            return False
        content = gdal.VSIFReadL(1, 10000000, f)
        gdal.VSIFCloseL(f)
        open(my_filename, 'wb').write(content)
    try:
        _validate_check(my_filename)
    except Exception as e:
        if not quiet:
            print(e)
        return False
    finally:
        if my_filename != filename:
            os.unlink(my_filename)
    return True

###############################################################################
# Create a fresh database.


def test_ogr_gpkg_1():

    gpkg_ds = gdaltest.gpkg_dr.CreateDataSource('tmp/gpkg_test.gpkg')

    assert gpkg_ds is not None

    gpkg_ds = None

    assert validate('tmp/gpkg_test.gpkg'), 'validation failed'

###############################################################################
# Re-open database to test validity


def test_ogr_gpkg_2():

    gpkg_ds = ogr.Open('tmp/gpkg_test.gpkg', update=1)

    # Should default to GPKG 1.2
    sql_lyr = gpkg_ds.ExecuteSQL('PRAGMA application_id')
    f = sql_lyr.GetNextFeature()
    if f['application_id'] != 1196444487:
        f.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)
    sql_lyr = gpkg_ds.ExecuteSQL('PRAGMA user_version')
    f = sql_lyr.GetNextFeature()
    if f['user_version'] != 10200:
        f.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)


###############################################################################
# Create a layer

def test_ogr_gpkg_3():

    gpkg_ds = ogr.Open('tmp/gpkg_test.gpkg', update=1)

    srs4326 = osr.SpatialReference()
    srs4326.ImportFromEPSG(4326)
    lyr = gpkg_ds.CreateLayer('first_layer', geom_type=ogr.wkbPoint, srs=srs4326, options=['GEOMETRY_NAME=gpkg_geometry', 'SPATIAL_INDEX=NO'])
    assert lyr is not None

    # Test creating a layer with an existing name
    lyr = gpkg_ds.CreateLayer('a_layer', options=['SPATIAL_INDEX=NO'])
    assert lyr is not None
    with gdaltest.error_handler():
        lyr = gpkg_ds.CreateLayer('a_layer', options=['SPATIAL_INDEX=NO'])
    assert lyr is None, 'layer creation should have failed'

###############################################################################
# Close and re-open to test the layer registration


def test_ogr_gpkg_4():

    assert validate('tmp/gpkg_test.gpkg'), 'validation failed'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    gpkg_ds = ogr.Open('tmp/gpkg_test.gpkg', update=1)
    gdal.PopErrorHandler()

    assert gpkg_ds is not None

    assert gpkg_ds.GetLayerCount() == 2, 'unexpected number of layers'

    lyr0 = gpkg_ds.GetLayer(0)

    assert lyr0.GetFIDColumn() == 'fid', 'unexpected FID name for layer 0'

    gpkg_ds = None
    gpkg_ds = ogr.Open('tmp/gpkg_test.gpkg', update=1)

    lyr0 = gpkg_ds.GetLayer(0)

    assert lyr0.GetName() == 'first_layer', 'unexpected layer name for layer 0'

    gpkg_ds = None
    gpkg_ds = ogr.Open('tmp/gpkg_test.gpkg', update=1)

    lyr0 = gpkg_ds.GetLayer(0)
    lyr1 = gpkg_ds.GetLayer(1)

    assert lyr0.GetLayerDefn().GetGeomFieldDefn(0).GetName() == 'gpkg_geometry', \
        'unexpected geometry field name for layer 0'

    assert lyr1.GetName() == 'a_layer', 'unexpected layer name for layer 1'

    sql_lyr = gpkg_ds.ExecuteSQL("SELECT * FROM sqlite_master WHERE name = 'gpkg_extensions'")
    assert sql_lyr.GetFeatureCount() == 0
    gpkg_ds.ReleaseResultSet(sql_lyr)


###############################################################################
# Delete a layer

def test_ogr_gpkg_5():

    gpkg_ds = ogr.Open('tmp/gpkg_test.gpkg', update=1)

    assert gpkg_ds.GetLayerCount() == 2, 'unexpected number of layers'

    with gdaltest.error_handler():
        ret = gpkg_ds.DeleteLayer(-1)
    assert ret != 0, 'expected error'

    with gdaltest.error_handler():
        ret = gpkg_ds.DeleteLayer(gpkg_ds.GetLayerCount())
    assert ret != 0, 'expected error'

    assert gpkg_ds.DeleteLayer(1) == 0, 'got error code from DeleteLayer(1)'

    assert gpkg_ds.DeleteLayer(0) == 0, 'got error code from DeleteLayer(0)'

    assert gpkg_ds.GetLayerCount() == 0, 'unexpected number of layers (not 0)'


###############################################################################
# Add fields

def test_ogr_gpkg_6():

    gpkg_ds = ogr.Open('tmp/gpkg_test.gpkg', update=1)

    srs4326 = osr.SpatialReference()
    srs4326.ImportFromEPSG(4326)
    lyr = gpkg_ds.CreateLayer('field_test_layer', geom_type=ogr.wkbPoint, srs=srs4326)
    assert lyr is not None

    field_defn = ogr.FieldDefn('dummy', ogr.OFTString)
    lyr.CreateField(field_defn)

    assert lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTString, \
        'wrong field type'

    gpkg_ds = None

    assert validate('tmp/gpkg_test.gpkg'), 'validation failed'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    gpkg_ds = ogr.Open('tmp/gpkg_test.gpkg', update=1)
    gdal.PopErrorHandler()

    assert gpkg_ds is not None

    assert gpkg_ds.GetLayerCount() == 1

    lyr = gpkg_ds.GetLayer(0)
    assert lyr.GetName() == 'field_test_layer'

    field_defn_out = lyr.GetLayerDefn().GetFieldDefn(0)
    assert field_defn_out.GetType() == ogr.OFTString, 'wrong field type after reopen'

    assert field_defn_out.GetName() == 'dummy', 'wrong field name after reopen'


###############################################################################
# Add a feature / read a feature / delete a feature

def test_ogr_gpkg_7():

    gpkg_ds = ogr.Open('tmp/gpkg_test.gpkg', update=1)

    lyr = gpkg_ds.GetLayerByName('field_test_layer')
    geom = ogr.CreateGeometryFromWkt('POINT(10 10)')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(geom)
    feat.SetField('dummy', 'a dummy value')

    assert lyr.TestCapability(ogr.OLCSequentialWrite) == 1, \
        'lyr.TestCapability(ogr.OLCSequentialWrite) != 1'

    assert lyr.CreateFeature(feat) == 0, 'cannot create feature'

    # Read back what we just inserted
    lyr.ResetReading()
    feat_read = lyr.GetNextFeature()
    assert feat_read.GetField('dummy') == 'a dummy value', 'output does not match input'

    # Only inserted one thing, so second feature should return NULL
    feat_read = lyr.GetNextFeature()
    assert feat_read is None, 'last call should return NULL'

    # Check that calling again GetNextFeature() does not reset the iterator
    feat_read = lyr.GetNextFeature()
    assert feat_read is None, 'last call should still return NULL'

    # Add another feature
    geom = ogr.CreateGeometryFromWkt('POINT(100 100)')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(geom)
    feat.SetField('dummy', 'who you calling a dummy?')
    assert lyr.CreateFeature(feat) == 0, 'cannot create feature'

    assert lyr.TestCapability(ogr.OLCRandomRead) == 1, \
        'lyr.TestCapability(ogr.OLCRandomRead) != 1'

    # Random read a feature
    feat_read_random = lyr.GetFeature(feat.GetFID())
    assert feat_read_random.GetField('dummy') == 'who you calling a dummy?', \
        'random read output does not match input'

    assert lyr.TestCapability(ogr.OLCRandomWrite) == 1, \
        'lyr.TestCapability(ogr.OLCRandomWrite) != 1'

    # Random write a feature
    feat.SetField('dummy', 'i am no dummy')
    lyr.SetFeature(feat)
    feat_read_random = lyr.GetFeature(feat.GetFID())
    assert feat_read_random.GetField('dummy') == 'i am no dummy', \
        'random read output does not match random write input'

    assert lyr.TestCapability(ogr.OLCDeleteFeature) == 1, \
        'lyr.TestCapability(ogr.OLCDeleteFeature) != 1'

    # Delete a feature
    lyr.DeleteFeature(feat.GetFID())
    assert lyr.GetFeatureCount() == 1, 'delete feature did not delete'

    # Test updating non-existing feature
    feat.SetFID(-10)
    assert lyr.SetFeature(feat) == ogr.OGRERR_NON_EXISTING_FEATURE, \
        'Expected failure of SetFeature().'

    # Test deleting non-existing feature
    assert lyr.DeleteFeature(-10) == ogr.OGRERR_NON_EXISTING_FEATURE, \
        'Expected failure of DeleteFeature().'

    # Delete the layer
    if gpkg_ds.DeleteLayer('field_test_layer') != 0:
        gdaltest.post_reason('got error code from DeleteLayer(field_test_layer)')



###############################################################################
# Test a variety of geometry feature types and attribute types

def test_ogr_gpkg_8():

    gpkg_ds = ogr.Open('tmp/gpkg_test.gpkg', update=1)

    srs = osr.SpatialReference()
    # Test a non-default SRS
    srs.ImportFromEPSG(32631)

    lyr = gpkg_ds.CreateLayer('tbl_linestring', geom_type=ogr.wkbLineString, srs=srs)
    assert lyr is not None

    lyr.StartTransaction()
    lyr.CreateField(ogr.FieldDefn('fld_integer', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('fld_string', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('fld_real', ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn('fld_date', ogr.OFTDate))
    lyr.CreateField(ogr.FieldDefn('fld_datetime', ogr.OFTDateTime))
    lyr.CreateField(ogr.FieldDefn('fld_binary', ogr.OFTBinary))
    fld_defn = ogr.FieldDefn('fld_boolean', ogr.OFTInteger)
    fld_defn.SetSubType(ogr.OFSTBoolean)
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn('fld_smallint', ogr.OFTInteger)
    fld_defn.SetSubType(ogr.OFSTInt16)
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn('fld_float', ogr.OFTReal)
    fld_defn.SetSubType(ogr.OFSTFloat32)
    lyr.CreateField(fld_defn)
    lyr.CreateField(ogr.FieldDefn('fld_integer64', ogr.OFTInteger64))

    geom = ogr.CreateGeometryFromWkt('LINESTRING(5 5,10 5,10 10,5 10)')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(geom)

    for i in range(10):
        feat.SetFID(-1)
        feat.SetField('fld_integer', 10 + i)
        feat.SetField('fld_real', 3.14159 / (i + 1))
        feat.SetField('fld_string', 'test string %d test' % i)
        feat.SetField('fld_date', '2014/05/17 ')
        feat.SetField('fld_datetime', '2014/12/31  23:59:59.999Z')
        feat.SetFieldBinaryFromHexString('fld_binary', 'fffe')
        feat.SetField('fld_boolean', 1)
        feat.SetField('fld_smallint', -32768)
        feat.SetField('fld_float', 1.23)
        feat.SetField('fld_integer64', 1000000000000 + i)

        assert lyr.CreateFeature(feat) == 0, ('cannot create feature %d' % i)
    lyr.CommitTransaction()

    feat = ogr.Feature(lyr.GetLayerDefn())
    assert lyr.CreateFeature(feat) == 0, 'cannot insert empty'

    feat.SetFID(6)
    assert lyr.SetFeature(feat) == 0, 'cannot update with empty'

    gpkg_ds = None

    assert validate('tmp/gpkg_test.gpkg'), 'validation failed'

    gpkg_ds = ogr.Open('tmp/gpkg_test.gpkg', update=1)
    lyr = gpkg_ds.GetLayerByName('tbl_linestring')
    assert lyr.GetLayerDefn().GetFieldDefn(6).GetSubType() == ogr.OFSTBoolean
    assert lyr.GetLayerDefn().GetFieldDefn(7).GetSubType() == ogr.OFSTInt16
    assert lyr.GetLayerDefn().GetFieldDefn(8).GetSubType() == ogr.OFSTFloat32
    feat = lyr.GetNextFeature()
    if feat.GetField(0) != 10 or feat.GetField(1) != 'test string 0 test' or \
       feat.GetField(2) != 3.14159 or feat.GetField(3) != '2014/05/17' or \
       feat.GetField(4) != '2014/12/31 23:59:59.999+00' or feat.GetField(5) != 'FFFE' or \
       feat.GetField(6) != 1 or feat.GetField(7) != -32768 or feat.GetField(8) != 1.23 or \
       feat.GetField(9) != 1000000000000:
        feat.DumpReadable()
        pytest.fail()

    lyr = gpkg_ds.CreateLayer('tbl_polygon', geom_type=ogr.wkbPolygon, srs=srs)
    assert lyr is not None

    lyr.StartTransaction()
    lyr.CreateField(ogr.FieldDefn('fld_datetime', ogr.OFTDateTime))
    lyr.CreateField(ogr.FieldDefn('fld_string', ogr.OFTString))

    geom = ogr.CreateGeometryFromWkt('POLYGON((5 5, 10 5, 10 10, 5 10, 5 5),(6 6, 6 7, 7 7, 7 6, 6 6))')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(geom)

    for i in range(10):
        feat.SetFID(-1)
        feat.SetField('fld_string', 'my super string %d' % i)
        feat.SetField('fld_datetime', '2010-01-01')

        assert lyr.CreateFeature(feat) == 0, ('cannot create polygon feature %d' % i)
    lyr.CommitTransaction()

    feat = lyr.GetFeature(3)
    geom_read = feat.GetGeometryRef()
    assert geom.ExportToWkt() == geom_read.ExportToWkt(), \
        'geom output not equal to geom input'

    # Test out the 3D support...
    lyr = gpkg_ds.CreateLayer('tbl_polygon25d', geom_type=ogr.wkbPolygon25D, srs=srs)
    assert lyr is not None

    lyr.CreateField(ogr.FieldDefn('fld_string', ogr.OFTString))
    geom = ogr.CreateGeometryFromWkt('POLYGON((5 5 1, 10 5 2, 10 10 3, 5 104 , 5 5 1),(6 6 4, 6 7 5, 7 7 6, 7 6 7, 6 6 4))')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    geom_read = feat.GetGeometryRef()
    assert geom.ExportToWkt() == geom_read.ExportToWkt(), \
        '3d geom output not equal to geom input'

###############################################################################
# Test support for extents and counts


def test_ogr_gpkg_9():

    gpkg_ds = ogr.Open('tmp/gpkg_test.gpkg', update=1)
    lyr = gpkg_ds.GetLayerByName('tbl_linestring')
    extent = lyr.GetExtent()
    assert extent == (5.0, 10.0, 5.0, 10.0), 'got bad extent'

    fcount = lyr.GetFeatureCount()
    assert fcount == 11, 'got bad featurecount'

###############################################################################
# Test non-SELECT SQL commands


def test_ogr_gpkg_11():

    gpkg_ds = ogr.Open('tmp/gpkg_test.gpkg', update=1)
    gpkg_ds.ExecuteSQL('CREATE INDEX tbl_linestring_fld_integer_idx ON tbl_linestring(fld_integer)')
    gpkg_ds.ExecuteSQL('ALTER TABLE tbl_linestring RENAME TO tbl_linestring_renamed;')
    gpkg_ds.ExecuteSQL('VACUUM')
    gpkg_ds = None

    gpkg_ds = ogr.Open('tmp/gpkg_test.gpkg', update=1)
    lyr = gpkg_ds.GetLayerByName('tbl_linestring_renamed')
    assert lyr is not None
    lyr.SetAttributeFilter('fld_integer = 10')
    assert lyr.GetFeatureCount() == 1

###############################################################################
# Test SELECT SQL commands


def test_ogr_gpkg_12():

    gpkg_ds = ogr.Open('tmp/gpkg_test.gpkg', update=1)
    sql_lyr = gpkg_ds.ExecuteSQL('SELECT * FROM tbl_linestring_renamed')
    assert sql_lyr.GetFIDColumn() == 'fid'
    assert sql_lyr.GetGeomType() == ogr.wkbLineString
    assert sql_lyr.GetGeometryColumn() == 'geom'
    assert sql_lyr.GetSpatialRef().ExportToWkt().find('32631') >= 0
    feat = sql_lyr.GetNextFeature()
    assert feat.GetFID() == 1
    assert sql_lyr.GetFeatureCount() == 11
    assert sql_lyr.GetLayerDefn().GetFieldCount() == 10
    assert sql_lyr.GetLayerDefn().GetFieldDefn(6).GetSubType() == ogr.OFSTBoolean
    assert sql_lyr.GetLayerDefn().GetFieldDefn(7).GetSubType() == ogr.OFSTInt16
    assert sql_lyr.GetLayerDefn().GetFieldDefn(8).GetSubType() == ogr.OFSTFloat32
    gpkg_ds.ReleaseResultSet(sql_lyr)

    sql_lyr = gpkg_ds.ExecuteSQL(
        'SELECT '
        'CAST(fid AS INTEGER) AS FID, '
        'CAST(fid AS INTEGER) AS FID, '
        '_rowid_ ,'
        'CAST(geom AS BLOB) AS GEOM, '
        'CAST(geom AS BLOB) AS GEOM, '
        'CAST(fld_integer AS INTEGER) AS FLD_INTEGER, '
        'CAST(fld_integer AS INTEGER) AS FLD_INTEGER, '
        'CAST(fld_string AS TEXT) AS FLD_STRING, '
        'CAST(fld_real AS REAL) AS FLD_REAL, '
        'CAST(fld_binary as BLOB) as FLD_BINARY, '
        'CAST(fld_integer64 AS INTEGER) AS FLD_INTEGER64 '
        'FROM tbl_linestring_renamed')
    assert sql_lyr.GetFIDColumn() == 'FID'
    assert sql_lyr.GetGeometryColumn() == 'GEOM'
    assert sql_lyr.GetLayerDefn().GetFieldCount() == 5
    assert sql_lyr.GetLayerDefn().GetFieldDefn(0).GetName() == 'FLD_INTEGER'
    assert sql_lyr.GetLayerDefn().GetFieldDefn(0).GetType() == ogr.OFTInteger
    assert sql_lyr.GetLayerDefn().GetFieldDefn(1).GetName() == 'FLD_STRING'
    assert sql_lyr.GetLayerDefn().GetFieldDefn(1).GetType() == ogr.OFTString
    assert sql_lyr.GetLayerDefn().GetFieldDefn(2).GetName() == 'FLD_REAL'
    assert sql_lyr.GetLayerDefn().GetFieldDefn(2).GetType() == ogr.OFTReal
    assert sql_lyr.GetLayerDefn().GetFieldDefn(3).GetName() == 'FLD_BINARY'
    assert sql_lyr.GetLayerDefn().GetFieldDefn(3).GetType() == ogr.OFTBinary
    assert sql_lyr.GetLayerDefn().GetFieldDefn(4).GetName() == 'FLD_INTEGER64'
    assert sql_lyr.GetLayerDefn().GetFieldDefn(4).GetType() == ogr.OFTInteger64
    gpkg_ds.ReleaseResultSet(sql_lyr)

    sql_lyr = gpkg_ds.ExecuteSQL('SELECT * FROM tbl_linestring_renamed WHERE 0=1')
    feat = sql_lyr.GetNextFeature()
    assert feat is None
    gpkg_ds.ReleaseResultSet(sql_lyr)

    for sql in ['SELECT * FROM tbl_linestring_renamed LIMIT 1',
                'SELECT * FROM tbl_linestring_renamed ORDER BY fld_integer LIMIT 1',
                'SELECT * FROM tbl_linestring_renamed UNION ALL SELECT * FROM tbl_linestring_renamed ORDER BY fld_integer LIMIT 1']:
        sql_lyr = gpkg_ds.ExecuteSQL(sql)
        feat = sql_lyr.GetNextFeature()
        assert feat is not None
        feat = sql_lyr.GetNextFeature()
        assert feat is None
        assert sql_lyr.GetFeatureCount() == 1
        gpkg_ds.ReleaseResultSet(sql_lyr)

    sql_lyr = gpkg_ds.ExecuteSQL('SELECT sqlite_version()')
    feat = sql_lyr.GetNextFeature()
    assert feat is not None
    assert sql_lyr.GetLayerDefn().GetFieldCount() == 1
    assert sql_lyr.GetLayerDefn().GetGeomFieldCount() == 0
    gpkg_ds.ReleaseResultSet(sql_lyr)

###############################################################################
# Test non-spatial tables


def test_ogr_gpkg_13():

    gpkg_ds = ogr.Open('tmp/gpkg_test.gpkg', update=1)
    lyr = gpkg_ds.CreateLayer('non_spatial', geom_type=ogr.wkbNone)
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)
    feat = None
    lyr.CreateField(ogr.FieldDefn('fld_integer', ogr.OFTInteger))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('fld_integer', 1)
    lyr.CreateFeature(feat)
    feat = None
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if not feat.IsFieldNull('fld_integer'):
        feat.DumpReadable()
        pytest.fail()
    feat = lyr.GetNextFeature()
    if feat.GetField('fld_integer') != 1:
        feat.DumpReadable()
        pytest.fail()

    # Test second aspatial layer
    lyr = gpkg_ds.CreateLayer('non_spatial2', geom_type=ogr.wkbNone)

    gpkg_ds = None
    gpkg_ds = ogr.Open('tmp/gpkg_test.gpkg', update=1)
    assert gdal.GetLastErrorMsg() == '', 'fail : warning NOT expected'
    assert gpkg_ds.GetLayerCount() == 5
    lyr = gpkg_ds.GetLayer('non_spatial')
    assert lyr.GetGeomType() == ogr.wkbNone
    feat = lyr.GetNextFeature()
    assert feat.IsFieldNull('fld_integer')
    feat = lyr.GetNextFeature()
    if feat.GetField('fld_integer') != 1:
        feat.DumpReadable()
        pytest.fail()

###############################################################################
# Add various geometries to test spatial filtering


def test_ogr_gpkg_14():

    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32631)

    gpkg_ds = ogr.Open('tmp/gpkg_test.gpkg', update=1)
    lyr = gpkg_ds.CreateLayer('point_no_spi-but-with-dashes', geom_type=ogr.wkbPoint, options=['SPATIAL_INDEX=NO'], srs=sr)
    assert lyr.TestCapability(ogr.OLCFastSpatialFilter) == 0
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1000 30000000)'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(-1000 30000000)'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1000 -30000000)'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(-1000 -30000000)'))
    lyr.CreateFeature(feat)
    # Test null geometry
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)
    # Test empty geometry
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT EMPTY'))
    lyr.CreateFeature(feat)

    f = lyr.GetFeature(5)
    if f.GetGeometryRef() is not None:
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetFeature(6)
    if f.GetGeometryRef().ExportToWkt() != 'POINT EMPTY':
        f.DumpReadable()
        pytest.fail()
    f = None

    sql_lyr = gpkg_ds.ExecuteSQL('SELECT * FROM "point_no_spi-but-with-dashes"')
    res = sql_lyr.TestCapability(ogr.OLCFastSpatialFilter)
    gpkg_ds.ReleaseResultSet(sql_lyr)
    assert res == 0

    lyr = gpkg_ds.CreateLayer('point-with-spi-and-dashes', geom_type=ogr.wkbPoint)
    assert lyr.TestCapability(ogr.OLCFastSpatialFilter) == 1
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1000 30000000)'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(-1000 30000000)'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1000 -30000000)'))
    lyr.CreateFeature(feat)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(-1000 -30000000)'))
    lyr.CreateFeature(feat)
    # Test null geometry
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)
    # Test empty geometry
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT EMPTY'))
    lyr.CreateFeature(feat)

    sql_lyr = gpkg_ds.ExecuteSQL('SELECT * FROM "point-with-spi-and-dashes"')
    res = sql_lyr.TestCapability(ogr.OLCFastSpatialFilter)
    gpkg_ds.ReleaseResultSet(sql_lyr)
    assert res == 1

    # Test spatial filer right away
    lyr.SetSpatialFilterRect(1000, 30000000, 1000, 30000000)
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    assert f is not None
    f = lyr.GetNextFeature()
    assert f is None


###############################################################################
def _has_spatialite_4_3_or_later(ds):
    has_spatialite_4_3_or_later = False
    with gdaltest.error_handler():
        sql_lyr = ds.ExecuteSQL("SELECT spatialite_version()")
        if sql_lyr:
            f = sql_lyr.GetNextFeature()
            version = f.GetField(0)
            version = '.'.join(version.split('.')[0:2])
            version = float(version)
            if version >= 4.3:
                has_spatialite_4_3_or_later = True
                # print('Spatialite 4.3 or later found')
            ds.ReleaseResultSet(sql_lyr)
    return has_spatialite_4_3_or_later

###############################################################################
# Test SQL functions


def test_ogr_gpkg_15():

    gpkg_ds = ogr.Open('tmp/gpkg_test.gpkg', update=1)
    sql_lyr = gpkg_ds.ExecuteSQL(
        'SELECT ST_IsEmpty(geom), ST_SRID(geom), ST_GeometryType(geom), ' +
        'ST_MinX(geom), ST_MinY(geom), ST_MaxX(geom), ST_MaxY(geom) FROM \"point_no_spi-but-with-dashes\" WHERE fid = 1')
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != 0 or feat.GetField(1) != 32631 or \
       feat.GetField(2) != 'POINT' or \
       feat.GetField(3) != 1000 or feat.GetField(4) != 30000000 or \
       feat.GetField(5) != 1000 or feat.GetField(6) != 30000000:
        feat.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)

    sql_lyr = gpkg_ds.ExecuteSQL(
        'SELECT ST_IsEmpty(geom), ST_SRID(geom), ST_GeometryType(geom), ' +
        'ST_MinX(geom), ST_MinY(geom), ST_MaxX(geom), ST_MaxY(geom) FROM tbl_linestring_renamed WHERE geom IS NULL')
    feat = sql_lyr.GetNextFeature()
    if not feat.IsFieldNull(0) or not feat.IsFieldNull(1) or not feat.IsFieldNull(2) or \
       not feat.IsFieldNull(3) or not feat.IsFieldNull(4) or not feat.IsFieldNull(5) or not feat.IsFieldNull(6):
        feat.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)

    for (expected_type, actual_type, expected_result) in [
        ('POINT', 'POINT', 1),
        ('LINESTRING', 'POINT', 0),
        ('GEOMETRY', 'POINT', 1),
        ('POINT', 'GEOMETRY', 0),
        ('GEOMETRYCOLLECTION', 'MULTIPOINT', 1),
            ('GEOMETRYCOLLECTION', 'POINT', 0)]:
        sql_lyr = gpkg_ds.ExecuteSQL("SELECT GPKG_IsAssignable('%s', '%s')" % (expected_type, actual_type))
        feat = sql_lyr.GetNextFeature()
        got_result = feat.GetField(0)
        gpkg_ds.ReleaseResultSet(sql_lyr)
        assert got_result == expected_result, \
            ("expected_type=%s actual_type=%s expected_result=%d got_result=%d" % (expected_type, actual_type, expected_result, got_result))

    for (sql, expected_result) in [
            ("SELECT HasSpatialIndex('point-with-spi-and-dashes', 'geom')", 1),
            ("SELECT DisableSpatialIndex('point-with-spi-and-dashes', 'geom')", 1),
            ("SELECT HasSpatialIndex('point-with-spi-and-dashes', 'geom')", 0),
            ("SELECT DisableSpatialIndex('point-with-spi-and-dashes', 'geom')", 0),
            ("SELECT CreateSpatialIndex('point-with-spi-and-dashes', 'geom')", 1),
            ("SELECT HasSpatialIndex('point-with-spi-and-dashes', 'geom')", 1),
            ("SELECT CreateSpatialIndex('point-with-spi-and-dashes', 'geom')", 0),
            ("SELECT CreateSpatialIndex('point-with-spi-and-dashes', NULL)", 0),
            ("SELECT HasSpatialIndex('point-with-spi-and-dashes', NULL)", 0),
            ("SELECT CreateSpatialIndex(NULL, 'geom')", 0),
            ("SELECT CreateSpatialIndex('bla', 'geom')", 0),
            ("SELECT CreateSpatialIndex('point-with-spi-and-dashes', 'bla')", 0),
            ("SELECT DisableSpatialIndex('point-with-spi-and-dashes', NULL)", 0),
            ("SELECT DisableSpatialIndex(NULL, 'geom')", 0),
            ("SELECT DisableSpatialIndex('bla', 'geom')", 0),
            ("SELECT DisableSpatialIndex('point-with-spi-and-dashes', 'bla')", 0),
            ("SELECT HasSpatialIndex(NULL, 'geom')", 0),
            ("SELECT HasSpatialIndex('bla', 'geom')", 0),
            ("SELECT HasSpatialIndex('point-with-spi-and-dashes', 'bla')", 0),
            ("SELECT CreateSpatialIndex('non_spatial', '')", 0),
            ("SELECT CreateSpatialIndex('point_no_spi-but-with-dashes', 'geom')", 1),
            # Final DisableSpatialIndex: will be effectively deleted at dataset closing
            ("SELECT DisableSpatialIndex('point_no_spi-but-with-dashes', 'geom')", 1),
    ]:
        if expected_result == 0:
            gdal.PushErrorHandler('CPLQuietErrorHandler')
        sql_lyr = gpkg_ds.ExecuteSQL(sql)
        if expected_result == 0:
            gdal.PopErrorHandler()
        feat = sql_lyr.GetNextFeature()
        got_result = feat.GetField(0)
        gpkg_ds.ReleaseResultSet(sql_lyr)
        assert got_result == expected_result, sql

    # NULL argument
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT SridFromAuthCRS(NULL, 4326)")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != -1:
        feat.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # NULL argument
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT SridFromAuthCRS('epsg', NULL)")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != -1:
        feat.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Existing entry
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT SridFromAuthCRS('epsg', 4326)")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != 4326:
        feat.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Non existing entry
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT SridFromAuthCRS('epsg', 1234)")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != -1:
        feat.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # NULL argument
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT ImportFromEPSG(NULL)")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != -1:
        feat.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Existing entry in gpkg_spatial_ref_sys
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT ImportFromEPSG(4326)")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != 4326:
        feat.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # New entry in gpkg_spatial_ref_sys
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT ImportFromEPSG(32633)")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != 32633:
        feat.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Invalid code
    with gdaltest.error_handler():
        sql_lyr = gpkg_ds.ExecuteSQL("SELECT ImportFromEPSG(0)")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != -1:
        feat.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # NULL argument
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT ST_Transform(NULL, 4326)")
    feat = sql_lyr.GetNextFeature()
    if feat.GetGeometryRef() is not None:
        feat.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Invalid geometry
    with gdaltest.error_handler():
        sql_lyr = gpkg_ds.ExecuteSQL("SELECT ST_Transform(x'00', 4326)")
    feat = sql_lyr.GetNextFeature()
    if feat.GetGeometryRef() is not None:
        feat.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # NULL argument
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT ST_Transform(geom, NULL) FROM tbl_linestring_renamed")
    feat = sql_lyr.GetNextFeature()
    if feat.GetGeometryRef() is not None:
        feat.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Invalid target SRID=0
    # GeoPackage: The record with an srs_id of 0 SHALL be used for undefined geographic coordinate reference systems.
    with gdaltest.error_handler():
        sql_lyr = gpkg_ds.ExecuteSQL("SELECT ST_Transform(geom, 0), ST_SRID(ST_Transform(geom, 0)) FROM tbl_linestring_renamed")
    assert sql_lyr.GetSpatialRef().ExportToWkt().find('Undefined geographic SRS') >= 0
    feat = sql_lyr.GetNextFeature()
    if feat.GetGeometryRef() is None or feat.GetField(0) != 0:
        feat.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Invalid source SRID=0
    # GeoPackage: The record with an srs_id of 0 SHALL be used for undefined geographic coordinate reference systems.
    # The source is undefined geographic coordinate reference systems (based on WGS84) and the target is WGS84,
    # and the result is an identity transformation that leaves geometry unchanged.
    src_lyr = gpkg_ds.GetLayerByName("point-with-spi-and-dashes")
    assert src_lyr.GetSpatialRef().ExportToWkt().find('Undefined geographic SRS') >= 0
    with gdaltest.error_handler():
        sql_lyr = gpkg_ds.ExecuteSQL("SELECT ST_Transform(geom, 4326), ST_SRID(ST_Transform(geom, 4326)) FROM \"point-with-spi-and-dashes\"")
    assert sql_lyr.GetSpatialRef().ExportToWkt().find('WGS_1984') >= 0
    feat = sql_lyr.GetNextFeature()
    if feat.GetGeometryRef() is None or feat.GetField(0) != 4326:
        feat.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Invalid spatialite geometry: SRID=4326,MULTIPOINT EMPTY truncated
    with gdaltest.error_handler():
        sql_lyr = gpkg_ds.ExecuteSQL("SELECT ST_Transform(x'0001E610000000000000000000000000000000000000000000000000000000000000000000007C04000000000000FE', 4326) FROM tbl_linestring_renamed")
    feat = sql_lyr.GetNextFeature()
    if feat.GetGeometryRef() is not None:
        feat.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)

    sql_lyr = gpkg_ds.ExecuteSQL("SELECT ST_Transform(geom, ST_SRID(geom)) FROM tbl_linestring_renamed")
    feat = sql_lyr.GetNextFeature()
    if feat.GetGeometryRef().ExportToWkt() != 'LINESTRING (5 5,10 5,10 10,5 10)':
        feat.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)

    sql_lyr = gpkg_ds.ExecuteSQL("SELECT ST_SRID(ST_Transform(geom, 4326)) FROM tbl_linestring_renamed")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != 4326:
        feat.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Spatialite geometry: SRID=4326,MULTIPOINT EMPTY
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT ST_SRID(ST_Transform(x'0001E610000000000000000000000000000000000000000000000000000000000000000000007C0400000000000000FE', 4326)) FROM tbl_linestring_renamed")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != 4326:
        feat.DumpReadable()
        pytest.fail()
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Error case: less than 8 bytes
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT ST_MinX(x'00')")
    feat = sql_lyr.GetNextFeature()
    if feat.IsFieldSetAndNotNull(0):
        feat.DumpReadable()
        pytest.fail()
    feat = None
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Error case: 8 wrong bytes
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT ST_MinX(x'0001020304050607')")
    feat = sql_lyr.GetNextFeature()
    if feat.IsFieldSetAndNotNull(0):
        feat.DumpReadable()
        pytest.fail()
    feat = None
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Error case: too short blob
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT ST_GeometryType(x'4750001100000000')")
    feat = sql_lyr.GetNextFeature()
    if feat.IsFieldSetAndNotNull(0):
        feat.DumpReadable()
        pytest.fail()
    feat = None
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Error case: too short blob
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT ST_GeometryType(x'475000110000000001040000')")
    feat = sql_lyr.GetNextFeature()
    if feat.IsFieldSetAndNotNull(0):
        feat.DumpReadable()
        pytest.fail()
    feat = None
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Invalid geometry, but long enough for our purpose...
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT ST_GeometryType(x'47500011000000000104000000')")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != 'MULTIPOINT':
        feat.DumpReadable()
        pytest.fail()
    feat = None
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Spatialite geometry (MULTIPOINT EMPTY)
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT ST_GeometryType(x'00010000000000000000000000000000000000000000000000000000000000000000000000007C0400000000000000FE')")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != 'MULTIPOINT':
        feat.DumpReadable()
        pytest.fail()
    feat = None
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Spatialite geometry (MULTIPOINT EMPTY)
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT ST_IsEmpty(x'00010000000000000000000000000000000000000000000000000000000000000000000000007C0400000000000000FE')")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != 1:
        feat.DumpReadable()
        pytest.fail()
    feat = None
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Error case: invalid geometry
    with gdaltest.error_handler():
        sql_lyr = gpkg_ds.ExecuteSQL("SELECT ST_GeometryType(x'475000030000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000')")
    feat = sql_lyr.GetNextFeature()
    if feat.IsFieldSetAndNotNull(0):
        feat.DumpReadable()
        pytest.fail()
    feat = None
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Error case: invalid type
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT GPKG_IsAssignable('POINT', NULL)")
    feat = sql_lyr.GetNextFeature()
    feat = None
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Error case: invalid type
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT GPKG_IsAssignable(NULL, 'POINT')")
    feat = sql_lyr.GetNextFeature()
    feat = None
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Test hstore_get_value
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT hstore_get_value('a=>b', 'a')")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != 'b':
        feat.DumpReadable()
        pytest.fail()
    feat = None
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Test hstore_get_value
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT hstore_get_value('a=>b', 'x')")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) is not None:
        feat.DumpReadable()
        pytest.fail()
    feat = None
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Error case: invalid type
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT hstore_get_value('a=>b', NULL)")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) is not None:
        feat.DumpReadable()
        pytest.fail()
    feat = None
    gpkg_ds.ReleaseResultSet(sql_lyr)

    # Error case: invalid type
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT hstore_get_value(NULL, 'a')")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) is not None:
        feat.DumpReadable()
        pytest.fail()
    feat = None
    gpkg_ds.ReleaseResultSet(sql_lyr)

    if _has_spatialite_4_3_or_later(gpkg_ds):
        sql_lyr = gpkg_ds.ExecuteSQL(
            "SELECT ST_Buffer(geom, 1e-10) FROM tbl_linestring_renamed")
        assert sql_lyr.GetGeomType() == ogr.wkbPolygon
        assert sql_lyr.GetSpatialRef().ExportToWkt().find('32631') >= 0
        gpkg_ds.ReleaseResultSet(sql_lyr)

###############################################################################
# Test unknown extensions


def test_ogr_gpkg_16():

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpk_16.gpkg')
    ds.CreateLayer('foo')
    ds.ExecuteSQL("INSERT INTO gpkg_extensions ( table_name, column_name, " +
                  "extension_name, definition, scope ) VALUES ( 'foo', 'geom', 'myext', 'some ext', 'write-only' ) ")
    ds = None

    # No warning since we open as read-only
    ds = ogr.Open('/vsimem/ogr_gpk_16.gpkg')
    lyr = ds.GetLayer(0)
    lyr.GetLayerDefn()
    ds = None
    assert gdal.GetLastErrorMsg() == '', 'fail : warning NOT expected'

    # Warning since we open as read-write
    ds = ogr.Open('/vsimem/ogr_gpk_16.gpkg', update=1)
    lyr = ds.GetLayer(0)
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr.GetLayerDefn()
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != '', 'fail : warning expected'

    ds.ExecuteSQL("UPDATE gpkg_extensions SET scope = 'read-write' WHERE extension_name = 'myext'")
    ds = None

    # Warning since we open as read-only
    ds = ogr.Open('/vsimem/ogr_gpk_16.gpkg')
    lyr = ds.GetLayer(0)
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr.GetLayerDefn()
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != '', 'fail : warning expected'

    # and also as read-write
    ds = ogr.Open('/vsimem/ogr_gpk_16.gpkg', update=1)
    lyr = ds.GetLayer(0)
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr.GetLayerDefn()
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != '', 'fail : warning expected'
    ds = None

    gdal.Unlink('/vsimem/ogr_gpk_16.gpkg')

    # Test with unsupported geometry type
    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpk_16.gpkg')
    ds.CreateLayer('foo')
    ds.ExecuteSQL("INSERT INTO gpkg_extensions ( table_name, column_name, " +
                  "extension_name, definition, scope ) VALUES ( 'foo', 'geom', 'gpkg_geom_XXXX', 'some ext', 'read-write' ) ")
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpk_16.gpkg')
    lyr = ds.GetLayer(0)
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr.GetLayerDefn()
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != '', 'fail : warning expected'

    gdal.Unlink('/vsimem/ogr_gpk_16.gpkg')

    # Test with database wide unknown extension
    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpk_16.gpkg')
    ds.CreateLayer('foo')
    ds.ExecuteSQL("INSERT INTO gpkg_extensions ( " +
                  "extension_name, definition, scope ) VALUES ( 'myext', 'some ext', 'write-only' ) ")
    ds = None

    # No warning since we open as read-only
    ds = ogr.Open('/vsimem/ogr_gpk_16.gpkg')
    lyr = ds.GetLayer(0)
    lyr.GetLayerDefn()
    assert gdal.GetLastErrorMsg() == '', 'fail : warning NOT expected'

    # Warning since we open as read-write
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogr.Open('/vsimem/ogr_gpk_16.gpkg', update=1)
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != '', 'fail : warning expected'

    ds.ExecuteSQL("UPDATE gpkg_extensions SET scope = 'read-write' WHERE extension_name = 'myext'")
    ds = None

    # Warning since we open as read-only
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogr.Open('/vsimem/ogr_gpk_16.gpkg')
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != '', 'fail : warning expected'

    # and also as read-write
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogr.Open('/vsimem/ogr_gpk_16.gpkg', update=1)
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != '', 'fail : warning expected'
    ds = None

    gdal.Unlink('/vsimem/ogr_gpk_16.gpkg')

###############################################################################
# Run INDIRECT_SQLITE dialect


def test_ogr_gpkg_17():

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_17.gpkg')
    sql_lyr = ds.ExecuteSQL("SELECT ogr_version()", dialect='INDIRECT_SQLITE')
    f = sql_lyr.GetNextFeature()
    assert f is not None
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    gdal.Unlink('/vsimem/ogr_gpkg_17.gpkg')

###############################################################################
# Test geometry type extension


def test_ogr_gpkg_18():

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_18.gpkg')
    lyr = ds.CreateLayer('wkbCircularString', geom_type=ogr.wkbCircularString)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('CIRCULARSTRING(0 0,1 0,0 0)'))
    lyr.CreateFeature(f)
    f = None
    ds = None

    assert validate('/vsimem/ogr_gpkg_18.gpkg'), 'validation failed'

    ds = ogr.Open('/vsimem/ogr_gpkg_18.gpkg')
    assert gdal.GetLastErrorMsg() == '', 'fail : warning NOT expected'

    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == ogr.wkbCircularString
    f = lyr.GetNextFeature()
    g = f.GetGeometryRef()
    assert g.GetGeometryType() == ogr.wkbCircularString

    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_extensions WHERE table_name = 'wkbCircularString' AND extension_name = 'gpkg_geom_CIRCULARSTRING'")
    assert sql_lyr.GetFeatureCount() == 1
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    gdal.Unlink('/vsimem/ogr_gpkg_18.gpkg')

    # Also test with a wkbUnknown layer and add curve geometries afterwards
    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_18.gpkg')
    lyr = ds.CreateLayer('test')
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('CIRCULARSTRING(0 0,1 0,0 0)'))
    lyr.CreateFeature(f)
    f = None

    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_extensions WHERE table_name = 'test' AND extension_name = 'gpkg_geom_CIRCULARSTRING'")
    assert sql_lyr.GetFeatureCount() == 1
    ds.ReleaseResultSet(sql_lyr)

    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_18.gpkg')
    assert gdal.GetLastErrorMsg() == '', 'fail : warning NOT expected'

    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    g = f.GetGeometryRef()
    assert g.GetGeometryType() == ogr.wkbCircularString

    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_18.gpkg', update=1)
    lyr = ds.GetLayer(0)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('CIRCULARSTRING(0 0,1 0,0 0)'))
    ret = lyr.CreateFeature(f)
    assert ret == 0 and gdal.GetLastErrorMsg() == ''
    f = None
    ds = None

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_18.gpkg')
    lyr = ds.CreateLayer('test', geom_type=ogr.wkbTriangle)
    with gdaltest.error_handler():
        # Warning 1: Registering non-standard gpkg_geom_TRIANGLE extension
        ds.FlushCache()
    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_extensions WHERE table_name = 'test' AND extension_name = 'gpkg_geom_TRIANGLE'")
    assert sql_lyr.GetFeatureCount() == 1
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    ret = validate('/vsimem/ogr_gpkg_18.gpkg', quiet=True)
    assert not ret, 'validation unexpectedly succeeded'

    # Test non-linear geometry in GeometryCollection
    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_18.gpkg')
    lyr = ds.CreateLayer('test')
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION(CIRCULARSTRING(0 0,1 0,0 0))'))
    lyr.CreateFeature(f)
    f = None
    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_extensions WHERE table_name = 'test' AND extension_name LIKE 'gpkg_geom_%'")
    assert sql_lyr.GetFeatureCount() == 1
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    gdal.Unlink('/vsimem/ogr_gpkg_18.gpkg')

###############################################################################
# Test metadata


def test_ogr_gpkg_19():

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_19.gpkg')
    assert not ds.GetMetadata()
    lyr = ds.CreateLayer('test_without_md')
    assert not lyr.GetMetadata()

    ds.SetMetadataItem('foo', 'bar')

    # GEOPACKAGE metadata domain is not allowed in a non-raster context
    gdal.PushErrorHandler()
    ds.SetMetadata(ds.GetMetadata('GEOPACKAGE'), 'GEOPACKAGE')
    ds.SetMetadataItem('foo', ds.GetMetadataItem('foo', 'GEOPACKAGE'), 'GEOPACKAGE')
    gdal.PopErrorHandler()

    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_19.gpkg')
    assert ds.GetMetadataDomainList() == ['']

    ds = ogr.Open('/vsimem/ogr_gpkg_19.gpkg')
    assert len(ds.GetMetadata()) == 1

    ds = ogr.Open('/vsimem/ogr_gpkg_19.gpkg')
    assert ds.GetMetadataItem('foo') == 'bar', ds.GetMetadata()
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_19.gpkg', update=1)
    lyr = ds.CreateLayer('test_with_md', options=['IDENTIFIER=ident', 'DESCRIPTION=desc'])
    lyr.SetMetadataItem('IDENTIFIER', 'ignored_because_of_lco')
    lyr.SetMetadataItem('DESCRIPTION', 'ignored_because_of_lco')
    lyr.SetMetadata({'IDENTIFIER': 'ignored_because_of_lco', 'DESCRIPTION': 'ignored_because_of_lco'})
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_19.gpkg')

    # Check that we don't create triggers
    sql_lyr = ds.ExecuteSQL(
        "SELECT * FROM sqlite_master WHERE type = 'trigger' AND tbl_name IN ('gpkg_metadata', 'gpkg_metadata_reference')")
    assert sql_lyr.GetFeatureCount() == 0
    ds.ReleaseResultSet(sql_lyr)

    lyr = ds.GetLayer('test_with_md')
    assert lyr.GetMetadataItem('IDENTIFIER') == 'ident'
    assert lyr.GetMetadataItem('DESCRIPTION') == 'desc'

    ds = ogr.Open('/vsimem/ogr_gpkg_19.gpkg', update=1)
    lyr = ds.GetLayer('test_with_md')
    assert lyr.GetMetadata() == {'IDENTIFIER': 'ident', 'DESCRIPTION': 'desc'}
    lyr.SetMetadataItem('IDENTIFIER', 'another_ident')
    lyr.SetMetadataItem('DESCRIPTION', 'another_desc')
    ds = None

    # FIXME? Is it expected to have a .aux.xml here ?
    gdal.Unlink('/vsimem/ogr_gpkg_19.gpkg.aux.xml')

    ds = ogr.Open('/vsimem/ogr_gpkg_19.gpkg', update=1)
    lyr = ds.GetLayer('test_with_md')
    assert lyr.GetMetadata() == {'IDENTIFIER': 'another_ident', 'DESCRIPTION': 'another_desc'}
    lyr.SetMetadataItem('foo', 'bar')
    lyr.SetMetadataItem('bar', 'baz', 'another_domain')
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_19.gpkg', update=1)
    lyr = ds.GetLayer('test_with_md')
    assert lyr.GetMetadataDomainList() == ['', 'another_domain']
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_19.gpkg', update=1)
    lyr = ds.GetLayer('test_with_md')
    assert lyr.GetMetadata() == {'IDENTIFIER': 'another_ident', 'foo': 'bar', 'DESCRIPTION': 'another_desc'}
    assert lyr.GetMetadata('another_domain') == {'bar': 'baz'}
    lyr.SetMetadata(None)
    lyr.SetMetadata(None, 'another_domain')
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_19.gpkg', update=1)
    lyr = ds.GetLayer('test_with_md')
    assert lyr.GetMetadata() == {'IDENTIFIER': 'another_ident', 'DESCRIPTION': 'another_desc'}
    assert lyr.GetMetadataDomainList() == ['']
    ds = None

    assert validate('/vsimem/ogr_gpkg_19.gpkg'), 'validation failed'

    gdal.Unlink('/vsimem/ogr_gpkg_19.gpkg')
    gdal.Unlink('/vsimem/ogr_gpkg_19.gpkg.aux.xml')

###############################################################################
# Test spatial reference system


def test_ogr_gpkg_20():

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_20.gpkg')

    # "Conflict" with EPSG:4326
    srs = osr.SpatialReference()
    srs.SetFromUserInput("""GEOGCS["my geogcs",
    DATUM["my datum",
        SPHEROID["my spheroid",1000,0]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433],
    AUTHORITY["my_org","4326"]]""")
    lyr = ds.CreateLayer('my_org_4326', srs=srs)

    # No authority node
    srs = osr.SpatialReference()
    srs.SetFromUserInput("""GEOGCS["another geogcs",
    DATUM["another datum",
        SPHEROID["another spheroid",1000,0]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]]""")
    lyr = ds.CreateLayer('without_org', srs=srs)

    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_20.gpkg')

    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_spatial_ref_sys WHERE srs_name='my geogcs' AND srs_id = 100000 AND organization='MY_ORG' AND organization_coordsys_id=4326 AND description is NULL")
    fc = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    assert fc == 1

    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_spatial_ref_sys WHERE srs_name='another geogcs' AND srs_id = 100001 AND organization='NONE' AND organization_coordsys_id=100001 AND description is NULL")
    fc = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    assert fc == 1

    lyr = ds.GetLayer('my_org_4326')
    assert lyr.GetSpatialRef().ExportToWkt().find('my geogcs') >= 0
    lyr = ds.GetLayer('without_org')
    assert lyr.GetSpatialRef().ExportToWkt().find('another geogcs') >= 0
    ds = None

    assert validate('/vsimem/ogr_gpkg_20.gpkg'), 'validation failed'

    gdal.Unlink('/vsimem/ogr_gpkg_20.gpkg')

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_20.gpkg')
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    lyr = ds.CreateLayer('foo4326', srs=srs)
    ds.ExecuteSQL("UPDATE gpkg_spatial_ref_sys SET definition='invalid', "
                  "organization='', organization_coordsys_id = 0 "
                  "WHERE srs_id = 4326")
    ds = None

    # Unable to parse srs_id '4326' well-known text 'invalid'
    with gdaltest.error_handler():
        ds = ogr.Open('/vsimem/ogr_gpkg_20.gpkg', update=1)

    ds.ExecuteSQL('DELETE FROM gpkg_spatial_ref_sys WHERE srs_id = 4326')
    ds = None
    gdal.SetConfigOption('OGR_GPKG_FOREIGN_KEY_CHECK', 'NO')
    # Warning 1: unable to read srs_id '4326' from gpkg_spatial_ref_sys
    with gdaltest.error_handler():
        ds = ogr.Open('/vsimem/ogr_gpkg_20.gpkg', update=1)
    gdal.SetConfigOption('OGR_GPKG_FOREIGN_KEY_CHECK', None)
    ds = None

    gdal.Unlink('/vsimem/ogr_gpkg_20.gpkg')

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_20.gpkg')
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    lyr = ds.CreateLayer('foo4326', srs=srs)

    ds.ExecuteSQL('DROP TABLE gpkg_spatial_ref_sys')
    ds.ExecuteSQL('CREATE TABLE gpkg_spatial_ref_sys (srs_name TEXT, '
                  'srs_id INTEGER, organization TEXT, '
                  'organization_coordsys_id INTEGER, definition TEXT)')
    ds.ExecuteSQL("INSERT INTO gpkg_spatial_ref_sys "
                  "(srs_name,srs_id,organization,organization_coordsys_id,"
                  "definition) VALUES (NULL,4326,NULL,NULL,NULL)")
    ds = None

    gdal.SetConfigOption('OGR_GPKG_FOREIGN_KEY_CHECK', 'NO')
    # Warning 1: null definition for srs_id '4326' in gpkg_spatial_ref_sys
    with gdaltest.error_handler():
        ds = ogr.Open('/vsimem/ogr_gpkg_20.gpkg', update=1)
    ds = None

    gdal.Unlink('/vsimem/ogr_gpkg_20.gpkg')


def test_ogr_gpkg_srs_non_duplication_custom_crs():

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_20.gpkg')
    srs = osr.SpatialReference()
    srs.SetFromUserInput("""GEOGCS["my custom geogcs",
    DATUM["my datum",
        SPHEROID["my spheroid",1000,0]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]]""")
    lyr = ds.CreateLayer('test', srs=srs)
    assert lyr
    lyr = ds.CreateLayer('test2', srs=srs)
    assert lyr

    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_spatial_ref_sys")
    fc = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    assert fc == 4 # srs_id 0, 1, 4326 + custom one

    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_spatial_ref_sys WHERE srs_name='my custom geogcs'")
    assert sql_lyr.GetFeatureCount() == 1
    f = sql_lyr.GetNextFeature()
    assert f['srs_id'] == 100000
    assert f['organization'] == 'NONE'
    assert f['organization_coordsys_id'] == 100000
    ds.ReleaseResultSet(sql_lyr)

    # Test now transitionning to definition_12_063 / WKT2 database structure...
    srs_3d = osr.SpatialReference()
    srs_3d.SetFromUserInput("""GEOGCRS["srs 3d",
    DATUM["some datum",
        ELLIPSOID["some ellipsoid",6378137,298.257223563,
            LENGTHUNIT["metre",1]]],
    PRIMEM["Greenwich",0,
        ANGLEUNIT["degree",0.0174532925199433]],
    CS[ellipsoidal,3],
        AXIS["geodetic latitude (Lat)",north,
            ORDER[1],
            ANGLEUNIT["degree",0.0174532925199433]],
        AXIS["geodetic longitude (Lon)",east,
            ORDER[2],
            ANGLEUNIT["degree",0.0174532925199433]],
        AXIS["ellipsoidal height (h)",up,
            ORDER[3],
            LENGTHUNIT["metre",1]]]""")
    lyr = ds.CreateLayer('test_3d', srs=srs_3d)
    assert lyr
    lyr = ds.CreateLayer('test_3d_bis', srs=srs_3d)
    assert lyr

    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_spatial_ref_sys WHERE srs_name='srs 3d'")
    assert sql_lyr.GetFeatureCount() == 1
    ds.ReleaseResultSet(sql_lyr)

    # Test again with SRS that can be represented in WKT1
    lyr = ds.CreateLayer('test3', srs=srs)
    assert lyr

    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_spatial_ref_sys WHERE srs_name='my custom geogcs'")
    assert sql_lyr.GetFeatureCount() == 1
    ds.ReleaseResultSet(sql_lyr)

    ds = None
    gdal.Unlink('/vsimem/ogr_gpkg_20.gpkg')


def test_ogr_gpkg_srs_non_consistent_with_official_definition():

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_20.gpkg')
    test_fake_4267 = osr.SpatialReference()
    test_fake_4267.SetFromUserInput("""GEOGCS["my geogcs 4267",
    DATUM["WGS_1984",
        SPHEROID["my spheroid",1000,0]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433],
    AUTHORITY["EPSG","4267"]]""")
    with gdaltest.error_handler():
        lyr = ds.CreateLayer('test_fake_4267', srs=test_fake_4267)
    assert gdal.GetLastErrorMsg() == 'Passed SRS uses EPSG:4267 identification, but its definition is not compatible with the official definition of the object. Registering it as a non-EPSG entry into the database.'
    assert lyr

    # EPSG:4326 already in the database
    test_fake_4326 = osr.SpatialReference()
    test_fake_4326.SetFromUserInput("""GEOGCS["my geogcs 4326",
    DATUM["WGS_1984",
        SPHEROID["my spheroid",1000,0]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433],
    AUTHORITY["EPSG","4326"]]""")
    with gdaltest.error_handler():
        lyr = ds.CreateLayer('test_fake_4326', srs=test_fake_4326)
    assert gdal.GetLastErrorMsg() == 'Passed SRS uses EPSG:4326 identification, but its definition is not compatible with the definition of that object already in the database. Registering it as a new entry into the database.'
    assert lyr

    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_20.gpkg', update = 1)
    lyr = ds.GetLayer('test_fake_4267')
    assert lyr.GetSpatialRef().ExportToWkt() == 'GEOGCS["my geogcs 4267",DATUM["WGS_1984",SPHEROID["my spheroid",1000,0]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4267"]]'

    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_spatial_ref_sys WHERE srs_name='my geogcs 4267'")
    assert sql_lyr.GetFeatureCount() == 1
    f = sql_lyr.GetNextFeature()
    assert f['srs_id'] == 100000
    assert f['organization'] == 'NONE'
    assert f['organization_coordsys_id'] == 100000
    ds.ReleaseResultSet(sql_lyr)

    lyr = ds.GetLayer('test_fake_4326')
    assert lyr.GetSpatialRef().ExportToWkt() == 'GEOGCS["my geogcs 4326",DATUM["WGS_1984",SPHEROID["my spheroid",1000,0]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4326"]]'

    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_spatial_ref_sys WHERE srs_name='my geogcs 4326'")
    assert sql_lyr.GetFeatureCount() == 1
    f = sql_lyr.GetNextFeature()
    assert f['srs_id'] == 100001
    assert f['organization'] == 'NONE'
    assert f['organization_coordsys_id'] == 100001
    ds.ReleaseResultSet(sql_lyr)

    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_spatial_ref_sys")
    fc_before = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)

    gdal.ErrorReset()
    lyr = ds.CreateLayer('test_fake_4267_bis', srs=test_fake_4267)
    assert gdal.GetLastErrorMsg() == ''

    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_spatial_ref_sys")
    fc_after = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)

    assert fc_before == fc_after
    ds = None

    gdal.Unlink('/vsimem/ogr_gpkg_20.gpkg')


def test_ogr_gpkg_write_srs_undefined_geographic():

    gdal.Unlink('tmp/ogr_gpkg_srs_undefined_geographic.gpkg')

    gpkg_ds = gdaltest.gpkg_dr.CreateDataSource('tmp/ogr_gpkg_srs_undefined_geographic.gpkg')
    assert gpkg_ds is not None

    # Check initial default SRS entries in gpkg_spatial_ref_sys
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT COUNT(*) FROM gpkg_spatial_ref_sys")
    gpkg_spatial_ref_sys_total = sql_lyr.GetNextFeature().GetField(0)
    assert gpkg_spatial_ref_sys_total == 3 # entries with SRS IDs: -1, 0, 4326
    gpkg_ds.ReleaseResultSet(sql_lyr)

    srs= osr.SpatialReference()
    srs.SetFromUserInput('GEOGCS["Undefined geographic SRS",DATUM["unknown",SPHEROID["unknown",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST]]')
    lyr = gpkg_ds.CreateLayer('srs_test_geographic_layer', geom_type=ogr.wkbPoint, srs=srs)
    srs_wkt = lyr.GetSpatialRef().ExportToWkt()
    assert srs_wkt.find('Undefined geographic SRS') >= 0, srs_wkt
    assert lyr.GetSpatialRef().IsGeographic()

    gpkg_ds = None
    gpkg_ds = ogr.Open('tmp/ogr_gpkg_srs_undefined_geographic.gpkg')

    # Check no new SRS entries have been inserted into gpkg_spatial_ref_sys
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT COUNT(*) FROM gpkg_spatial_ref_sys")
    assert gpkg_spatial_ref_sys_total == sql_lyr.GetNextFeature().GetField(0)
    gpkg_ds.ReleaseResultSet(sql_lyr)

    lyr = gpkg_ds.GetLayer(0)
    srs_wkt = lyr.GetSpatialRef().ExportToWkt()
    assert srs_wkt.find('Undefined geographic SRS') >= 0, srs_wkt
    assert lyr.GetSpatialRef().IsGeographic()

    gpkg_ds = None
    gdal.Unlink('tmp/ogr_gpkg_srs_undefined_geographic.gpkg')


def test_ogr_gpkg_write_srs_undefined_Cartesian():

    gdal.Unlink('tmp/ogr_gpkg_srs_Cartesian.gpkg')

    gpkg_ds = gdaltest.gpkg_dr.CreateDataSource('tmp/ogr_gpkg_srs_Cartesian.gpkg')
    assert gpkg_ds is not None

    # Check initial default SRS entries in gpkg_spatial_ref_sys
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT COUNT(*) FROM gpkg_spatial_ref_sys")
    gpkg_spatial_ref_sys_total = sql_lyr.GetNextFeature().GetField(0)
    assert gpkg_spatial_ref_sys_total == 3 # SRS with IDs: -1, 0, 4326
    gpkg_ds.ReleaseResultSet(sql_lyr)

    srs= osr.SpatialReference()
    srs.SetFromUserInput('LOCAL_CS["Undefined Cartesian SRS"]')
    lyr = gpkg_ds.CreateLayer('srs_test_Cartesian_layer', geom_type=ogr.wkbPoint, srs=srs)
    srs_wkt = lyr.GetSpatialRef().ExportToWkt()
    assert srs_wkt.find('Undefined Cartesian SRS') >= 0
    assert lyr.GetSpatialRef().IsLocal()

    gpkg_ds = None
    gpkg_ds = ogr.Open('tmp/ogr_gpkg_srs_Cartesian.gpkg')

    # Check no new SRS entries have been inserted into gpkg_spatial_ref_sys
    sql_lyr = gpkg_ds.ExecuteSQL("SELECT COUNT(*) FROM gpkg_spatial_ref_sys")
    assert gpkg_spatial_ref_sys_total == sql_lyr.GetNextFeature().GetField(0)
    gpkg_ds.ReleaseResultSet(sql_lyr)

    lyr = gpkg_ds.GetLayer(0)
    srs_wkt = lyr.GetSpatialRef().ExportToWkt()
    assert srs_wkt.find('Undefined Cartesian SRS') >= 0, srs_wkt
    assert lyr.GetSpatialRef().IsLocal()

    gpkg_ds = None
    gdal.Unlink('tmp/ogr_gpkg_srs_Cartesian.gpkg')

###############################################################################
# Test maximum width of text fields


def test_ogr_gpkg_21():

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_21.gpkg')
    lyr = ds.CreateLayer('test')
    field_defn = ogr.FieldDefn('str', ogr.OFTString)
    field_defn.SetWidth(2)
    lyr.CreateField(field_defn)
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_21.gpkg', update=1)
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetWidth() == 2
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField(0, 'ab')
    gdal.ErrorReset()
    lyr.CreateFeature(f)
    assert gdal.GetLastErrorMsg() == ''

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFieldBinaryFromHexString(0, '41E9')
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != ''

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField(0, 'abc')
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != ''

    f = lyr.GetFeature(f.GetFID())
    assert f.GetField(0) == 'abc'

    gdal.Unlink('/vsimem/ogr_gpkg_21.gpkg')

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_21.gpkg')
    lyr = ds.CreateLayer('test', options=['TRUNCATE_FIELDS=YES'])
    field_defn = ogr.FieldDefn('str', ogr.OFTString)
    field_defn.SetWidth(2)
    lyr.CreateField(field_defn)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFieldBinaryFromHexString(0, '41E9')
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != ''

    f = lyr.GetFeature(f.GetFID())
    assert f.GetField(0) == 'A_'

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField(0, 'abc')
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    assert gdal.GetLastErrorMsg() != ''

    f = lyr.GetFeature(f.GetFID())
    assert f.GetField(0) == 'ab'

    gdal.Unlink('/vsimem/ogr_gpkg_21.gpkg')

###############################################################################
# Test FID64 support


def test_ogr_gpkg_22():

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_22.gpkg')
    lyr = ds.CreateLayer('test')
    field_defn = ogr.FieldDefn('foo', ogr.OFTString)
    lyr.CreateField(field_defn)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('foo', 'bar')
    feat.SetFID(1234567890123)
    lyr.CreateFeature(feat)
    feat = None

    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_22.gpkg')
    lyr = ds.GetLayerByName('test')
    assert lyr.GetMetadataItem(ogr.OLMD_FID64) is not None
    f = lyr.GetNextFeature()
    assert f.GetFID() == 1234567890123

    gdal.Unlink('/vsimem/ogr_gpkg_22.gpkg')

###############################################################################
# Test not nullable fields


def test_ogr_gpkg_23():

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_23.gpkg')
    lyr = ds.CreateLayer('test', geom_type=ogr.wkbNone)
    field_defn = ogr.FieldDefn('field_not_nullable', ogr.OFTString)
    field_defn.SetNullable(0)
    lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn('field_nullable', ogr.OFTString)
    lyr.CreateField(field_defn)
    field_defn = ogr.GeomFieldDefn('geomfield_not_nullable', ogr.wkbPoint)
    field_defn.SetNullable(0)
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

    # Nullable geometry field
    lyr = ds.CreateLayer('test2', geom_type=ogr.wkbPoint, options=['SPATIAL_INDEX=NO'])

    # Cannot add more than one geometry field
    gdal.PushErrorHandler()
    ret = lyr.CreateGeomField(ogr.GeomFieldDefn('foo', ogr.wkbPoint))
    gdal.PopErrorHandler()
    assert ret != 0

    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    f = None

    # Not-nullable fields and geometry fields created after table creation
    lyr = ds.CreateLayer('test3', geom_type=ogr.wkbNone)

    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    f = None

    field_defn = ogr.FieldDefn('field_not_nullable', ogr.OFTString)
    field_defn.SetNullable(0)
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn('field_nullable', ogr.OFTString)
    lyr.CreateField(field_defn)

    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_contents WHERE data_type = 'features'")
    fc = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    assert fc == 2

    sql_lyr = ds.ExecuteSQL("SELECT 1 FROM sqlite_master WHERE name='gpkg_extensions'")
    has_gpkg_extensions = sql_lyr.GetFeatureCount() == 1
    ds.ReleaseResultSet(sql_lyr)
    assert not has_gpkg_extensions

    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_geometry_columns")
    fc = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    assert fc == 2

    field_defn = ogr.GeomFieldDefn('geomfield_not_nullable', ogr.wkbPoint)
    field_defn.SetNullable(0)
    lyr.CreateGeomField(field_defn)

    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_contents WHERE data_type = 'features'")
    fc = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    assert fc == 3

    sql_lyr = ds.ExecuteSQL("SELECT 1 FROM sqlite_master WHERE name='gpkg_extensions'")
    has_gpkg_extensions = sql_lyr.GetFeatureCount() == 1
    ds.ReleaseResultSet(sql_lyr)
    assert not has_gpkg_extensions

    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_geometry_columns")
    fc = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    assert fc == 3

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('field_not_nullable', 'not_null')
    f.SetGeomFieldDirectly('geomfield_not_nullable', ogr.CreateGeometryFromWkt('POINT(0 0)'))
    lyr.CreateFeature(f)
    f = None

    # Not Nullable geometry field
    lyr = ds.CreateLayer('test4', geom_type=ogr.wkbPoint, options=['GEOMETRY_NULLABLE=NO'])
    assert lyr.GetLayerDefn().GetGeomFieldDefn(0).IsNullable() == 0
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT(0 0)'))
    lyr.CreateFeature(f)
    f = None

    ds.CreateLayer('test5', geom_type=ogr.wkbNone)

    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_23.gpkg')

    lyr = ds.GetLayerByName('test5')
    field_defn = ogr.GeomFieldDefn('', ogr.wkbPoint)
    with gdaltest.error_handler():
        assert lyr.CreateGeomField(field_defn) != 0

    lyr = ds.GetLayerByName('test')
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_not_nullable')).IsNullable() == 0
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_nullable')).IsNullable() == 1
    assert lyr.GetLayerDefn().GetGeomFieldDefn(lyr.GetLayerDefn().GetGeomFieldIndex('geomfield_not_nullable')).IsNullable() == 0

    lyr = ds.GetLayerByName('test2')
    assert lyr.GetLayerDefn().GetGeomFieldDefn(0).IsNullable() == 1

    lyr = ds.GetLayerByName('test3')
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_not_nullable')).IsNullable() == 0
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_nullable')).IsNullable() == 1
    assert lyr.GetLayerDefn().GetGeomFieldDefn(lyr.GetLayerDefn().GetGeomFieldIndex('geomfield_not_nullable')).IsNullable() == 0

    lyr = ds.GetLayerByName('test4')
    assert lyr.GetLayerDefn().GetGeomFieldDefn(0).IsNullable() == 0

    ds = None

    gdal.Unlink('/vsimem/ogr_gpkg_23.gpkg')

###############################################################################
# Test unique constraints on fields


def test_ogr_gpkg_unique():

    ds = ogr.GetDriverByName('GPKG').CreateDataSource('/vsimem/ogr_gpkg_unique.gpkg')
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

    f = ogr.Feature(layerDefinition)
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    f = None

    # Test adding columns after "crystallization"
    field_defn = ogr.FieldDefn('field_unique_failure', ogr.OFTString)
    field_defn.SetUnique(1)
    # Not allowed by sqlite3. Could potentially be improved
    with gdaltest.error_handler():
        assert lyr.CreateField(field_defn) == ogr.OGRERR_FAILURE

    # Create another layer from SQL to test quoting of fields
    # and indexes
    # Note: leave create table in a single line because of regex spaces testing
    sql = (
        'CREATE TABLE IF NOT EXISTS "test2" ( "fid" INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL, "field_default" TEXT, "field_no_unique" TEXT, "field_unique" TEXT UNIQUE,`field unique2` TEXT UNIQUE,field_unique3 TEXT UNIQUE, FIELD_UNIQUE_INDEX TEXT, `field unique index2` TEXT, "field_unique_index3" TEXT, NOT_UNIQUE TEXT);',
        'CREATE UNIQUE INDEX test2_unique_idx ON test2(field_unique_index);', # field_unique_index in lowercase whereas in uppercase in CREATE TABLE statement
        'CREATE UNIQUE INDEX test2_unique_idx2 ON test2(`field unique index2`);',
        'CREATE UNIQUE INDEX test2_unique_idx3 ON test2("field_unique_index3");',
        "INSERT INTO gpkg_contents VALUES('test2','attributes','test2','','2020-05-27T12:27:30.136Z',NULL,NULL,NULL,NULL,0);",
        "INSERT INTO gpkg_ogr_contents VALUES('test2',NULL);"
    )

    for s in sql:
        ds.ExecuteSQL(s)

    ds = None

    # Reload
    ds = ogr.Open('/vsimem/ogr_gpkg_unique.gpkg')

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
    gdal.Unlink('/vsimem/ogr_gpkg_unique.gpkg')

###############################################################################
# Test default values


def test_ogr_gpkg_24():

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_24.gpkg')
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

    # This will be translated as "(strftime('%Y-%m-%dT%H:%M:%fZ','now'))"
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

    # field_defn = ogr.FieldDefn( 'field_time', ogr.OFTTime )
    # field_defn.SetDefault("CURRENT_TIME")
    # lyr.CreateField(field_defn)

    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    f = None

    # Test adding columns after "crystallization"
    field_defn = ogr.FieldDefn('field_datetime5', ogr.OFTDateTime)
    field_defn.SetDefault("'2016/06/30 12:34:56.123'")
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn('field_datetime6', ogr.OFTDateTime)
    field_defn.SetDefault("'2016/06/30 12:34:56'")
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn('field_string2', ogr.OFTString)
    field_defn.SetDefault("'X'")
    lyr.CreateField(field_defn)

    # Doesn't work currently. Would require rewriting the whole table
    # field_defn = ogr.FieldDefn( 'field_datetimeX', ogr.OFTDateTime )
    # field_defn.SetDefault("CURRENT_TIMESTAMP")
    # lyr.CreateField(field_defn)

    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_24.gpkg', update=1)
    lyr = ds.GetLayerByName('test')
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_string')).GetDefault() == "'a''b'"
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_int')).GetDefault() == '123'
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_real')).GetDefault() == '1.23'
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_nodefault')).GetDefault() is None
    # Translated from "(strftime('%Y-%m-%dT%H:%M:%fZ','now'))" to CURRENT_TIMESTAMP
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_datetime')).GetDefault() == 'CURRENT_TIMESTAMP'
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_datetime2')).GetDefault() == "'2015/06/30 12:34:56'"
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_datetime3')).GetDefault() == "CURRENT_TIMESTAMP"
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_datetime4')).GetDefault() == "'2015/06/30 12:34:56.123'"
    assert lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_date')).GetDefault() == "CURRENT_DATE"
    # if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_time')).GetDefault() != "CURRENT_TIME":
    #    gdaltest.post_reason('fail')
    #    return 'fail'
    f = lyr.GetNextFeature()
    if f.GetField('field_string') != 'a\'b' or f.GetField('field_int') != 123 or \
       f.GetField('field_real') != 1.23 or \
       not f.IsFieldNull('field_nodefault') or not f.IsFieldSet('field_datetime') or \
       f.GetField('field_datetime2') != '2015/06/30 12:34:56+00' or \
       f.GetField('field_datetime4') != '2015/06/30 12:34:56.123+00' or \
       not f.IsFieldSet('field_datetime3') or \
       not f.IsFieldSet('field_date') or \
       f.GetField('field_datetime5') != '2016/06/30 12:34:56.123+00' or \
       f.GetField('field_datetime6') != '2016/06/30 12:34:56+00' or \
       f.GetField('field_string2') != 'X':
        f.DumpReadable()
        pytest.fail()

    ds = None

    gdal.Unlink('/vsimem/ogr_gpkg_24.gpkg')

###############################################################################
# Test creating a field with the fid name


def test_ogr_gpkg_25():

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_25.gpkg')
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
    f = lyr.GetFeature(f.GetFID())
    if f.GetFID() != 10 or f.GetField('str') != 'first string' or f.GetField('str2') != 'second string' or f.GetField('myfid') != 10:
        f.DumpReadable()
        pytest.fail()
    f = None

    ds = None

    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_25.gpkg')

###############################################################################
# Test dataset transactions


def test_ogr_gpkg_26():

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_26.gpkg')

    assert ds.TestCapability(ogr.ODsCTransactions) == 1

    ret = ds.StartTransaction()
    assert ret == 0
    gdal.PushErrorHandler()
    ret = ds.StartTransaction()
    gdal.PopErrorHandler()
    assert ret != 0

    lyr = ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    ret = ds.RollbackTransaction()
    assert ret == 0
    gdal.PushErrorHandler()
    ret = ds.RollbackTransaction()
    gdal.PopErrorHandler()
    assert ret != 0
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_26.gpkg', update=1)
    assert ds.GetLayerCount() == 0
    ret = ds.StartTransaction()
    assert ret == 0
    gdal.PushErrorHandler()
    ret = ds.StartTransaction()
    gdal.PopErrorHandler()
    assert ret != 0

    lyr = ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    ret = ds.CommitTransaction()
    assert ret == 0
    gdal.PushErrorHandler()
    ret = ds.CommitTransaction()
    gdal.PopErrorHandler()
    assert ret != 0
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_26.gpkg', update=1)
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

    if False:  # pylint: disable=using-constant-test
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
        # For some reason fails with SQLite 3.6.X with 'failed to execute insert : callback requested query abort'
        # but not with later versions...
        assert ret == 0

    ds = None

    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_26.gpkg')

###############################################################################
# Test interface with Spatialite


def test_ogr_gpkg_27():

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_27.gpkg')
    gdal.PushErrorHandler()
    sql_lyr = ds.ExecuteSQL("SELECT GeomFromGPB(null)")
    gdal.PopErrorHandler()
    if sql_lyr is None:
        ds = None
        gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_27.gpkg')
        pytest.skip()
    ds.ReleaseResultSet(sql_lyr)

    lyr = ds.CreateLayer('test')
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (2 49)'))
    lyr.CreateFeature(f)
    sql_lyr = ds.ExecuteSQL('SELECT GeomFromGPB(geom) FROM test')
    f = sql_lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (2 49)':
        f.DumpReadable()
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)

    ds = None
    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_27.gpkg')

###############################################################################
# Test ogr2ogr -a_srs (as the geopackage driver doesn't clone the passed SRS
# but inc/dec its ref count, which can exhibit issues in GDALVectorTanslate())


def test_ogr_gpkg_28():

    srcDS = gdal.OpenEx('../ogr/data/poly.shp')
    ds = gdal.VectorTranslate('/vsimem/ogr_gpkg_28.gpkg', srcDS, format='GPKG', dstSRS='EPSG:4326')
    assert str(ds.GetLayer(0).GetSpatialRef()).find('1984') != -1

    ds = None
    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_28.gpkg')

###############################################################################
# Test XYM / XYZM support


def test_ogr_gpkg_29():

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_29.gpkg')
    assert ds.TestCapability(ogr.ODsCMeasuredGeometries) == 1
    lyr = ds.CreateLayer('pointm', geom_type=ogr.wkbPointM)
    assert lyr.TestCapability(ogr.OLCMeasuredGeometries) == 1
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT M (1 2 3)'))
    lyr.CreateFeature(f)
    lyr = ds.CreateLayer('pointzm', geom_type=ogr.wkbPointZM)
    assert lyr.TestCapability(ogr.OLCMeasuredGeometries) == 1
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT ZM (1 2 3 4)'))
    lyr.CreateFeature(f)
    ds = None

    assert validate('/vsimem/ogr_gpkg_29.gpkg'), 'validation failed'

    ds = ogr.Open('/vsimem/ogr_gpkg_29.gpkg', update=1)
    lyr = ds.GetLayerByName('pointm')
    assert lyr.GetGeomType() == ogr.wkbPointM
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToIsoWkt() != 'POINT M (1 2 3)':
        f.DumpReadable()
        pytest.fail()

    # Generate a XYM envelope
    ds.ExecuteSQL("UPDATE pointm SET geom = x'4750000700000000000000000000F03F000000000000F03F000000000000004000000000000000400000000000000840000000000000084001D1070000000000000000F03F00000000000000400000000000000840'")

    lyr = ds.GetLayerByName('pointzm')
    assert lyr.GetGeomType() == ogr.wkbPointZM
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToIsoWkt() != 'POINT ZM (1 2 3 4)':
        f.DumpReadable()
        pytest.fail()

    # Generate a XYZM envelope
    ds.ExecuteSQL("UPDATE pointzm SET geom = x'4750000900000000000000000000F03F000000000000F03F00000000000000400000000000000040000000000000084000000000000008400000000000001040000000000000104001B90B0000000000000000F03F000000000000004000000000000008400000000000001040'")

    ds = None

    # Check again
    ds = ogr.Open('/vsimem/ogr_gpkg_29.gpkg')
    lyr = ds.GetLayerByName('pointm')
    assert lyr.GetGeomType() == ogr.wkbPointM
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToIsoWkt() != 'POINT M (1 2 3)':
        f.DumpReadable()
        pytest.fail()
    lyr = ds.GetLayerByName('pointzm')
    assert lyr.GetGeomType() == ogr.wkbPointZM
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToIsoWkt() != 'POINT ZM (1 2 3 4)':
        f.DumpReadable()
        pytest.fail()
    ds = None

    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_29.gpkg')

###############################################################################
# Test non standard file extension (#6396)


def test_ogr_gpkg_30():

    with gdaltest.error_handler():
        ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_30.geopkg')
    assert ds is not None
    assert gdal.GetLastErrorMsg() != ''
    ds = None

    with gdaltest.error_handler():
        ds = ogr.Open('/vsimem/ogr_gpkg_30.geopkg', update=1)
    assert ds is not None
    assert gdal.GetLastErrorMsg() != ''
    ds = None

    with gdaltest.error_handler():
        gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_30.geopkg')


###############################################################################
# Test CURVE and SURFACE types


def test_ogr_gpkg_31():

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_31.gpkg')
    lyr = ds.CreateLayer('curve', geom_type=ogr.wkbCurve)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('LINESTRING (1 2,3 4)'))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('COMPOUNDCURVE ((1 2,3 4))'))
    lyr.CreateFeature(f)
    lyr = ds.CreateLayer('surface', geom_type=ogr.wkbSurface)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POLYGON ((0 0,0 1,1 1,0 0))'))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('CURVEPOLYGON ((0 0,0 1,1 1,0 0))'))
    lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_31.gpkg')
    lyr = ds.GetLayerByName('curve')
    assert lyr.GetGeomType() == ogr.wkbCurve
    lyr = ds.GetLayerByName('surface')
    assert lyr.GetGeomType() == ogr.wkbSurface
    ds = None

    assert validate('/vsimem/ogr_gpkg_31.gpkg'), 'validation failed'

    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_31.gpkg')

###############################################################################
# Run creating a non-spatial layer that isn't registered as 'aspatial' and
# read it back


def test_ogr_gpkg_32():

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_32.gpkg')
    ds.CreateLayer('aspatial', geom_type=ogr.wkbNone, options=['ASPATIAL_VARIANT=NOT_REGISTERED'])
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_32.gpkg')
    assert ds.GetLayerCount() == 1
    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_contents")
    assert sql_lyr.GetFeatureCount() == 0
    ds.ReleaseResultSet(sql_lyr)
    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_geometry_columns")
    assert sql_lyr.GetFeatureCount() == 0
    ds.ReleaseResultSet(sql_lyr)
    sql_lyr = ds.ExecuteSQL("SELECT * FROM sqlite_master WHERE name = 'gpkg_extensions'")
    assert sql_lyr.GetFeatureCount() == 0
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    assert validate('/vsimem/ogr_gpkg_32.gpkg'), 'validation failed'

    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_32.gpkg')

###############################################################################
# Test OGR_CURRENT_DATE


def test_ogr_gpkg_33():

    gdal.SetConfigOption('OGR_CURRENT_DATE', '2000-01-01T:00:00:00.000Z')
    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_33.gpkg')
    ds.CreateLayer('test', geom_type=ogr.wkbNone)
    ds = None
    gdal.SetConfigOption('OGR_CURRENT_DATE', None)

    ds = ogr.Open('/vsimem/ogr_gpkg_33.gpkg')
    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_contents WHERE last_change = '2000-01-01T:00:00:00.000Z'")
    assert sql_lyr.GetFeatureCount() == 1
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_33.gpkg')


###############################################################################
# Test rename and delete a layer registered in extensions, metadata, spatial index etc

def test_ogr_gpkg_34():

    layer_name = """weird'layer"name"""

    dbname = '/vsimem/ogr_gpkg_34.gpkg'
    ds = gdaltest.gpkg_dr.CreateDataSource(dbname)
    lyr = ds.CreateLayer(layer_name, geom_type=ogr.wkbCurvePolygon)
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('CURVEPOLYGON ((0 0,0 1,1 1,0 0))'))
    lyr.CreateFeature(f)
    lyr.SetMetadataItem('FOO', 'BAR')
    ds.ExecuteSQL("""CREATE TABLE gpkg_data_columns (
  table_name TEXT NOT NULL,
  column_name TEXT NOT NULL,
  name TEXT UNIQUE,
  title TEXT,
  description TEXT,
  mime_type TEXT,
  constraint_name TEXT,
  CONSTRAINT pk_gdc PRIMARY KEY (table_name, column_name),
  CONSTRAINT fk_gdc_tn FOREIGN KEY (table_name) REFERENCES gpkg_contents(table_name)
)""")
    ds.ExecuteSQL("INSERT INTO gpkg_data_columns VALUES('weird''layer\"name', 'foo', 'foo_constraints', NULL, NULL, NULL, NULL)")
    ds = None

    # Check that there are reference to the layer
    f = gdal.VSIFOpenL(dbname, 'rb')
    content = gdal.VSIFReadL(1, 1000000, f).decode('latin1')
    gdal.VSIFCloseL(f)

    assert layer_name in content

    ds = ogr.Open(dbname, update=1)
    new_layer_name = """weird2'layer"name"""
    with gdaltest.error_handler():
        ds.ExecuteSQL('ALTER TABLE "weird\'layer""name" RENAME TO gpkg_contents')
    assert gdal.GetLastErrorMsg() != ''
    gdal.ErrorReset()
    ds.ExecuteSQL('ALTER TABLE "weird\'layer""name" RENAME TO "weird2\'layer""name"')
    ds.ExecuteSQL('VACUUM')
    ds = None

    # Check that there is no more any reference to the layer
    f = gdal.VSIFOpenL(dbname, 'rb')
    content = gdal.VSIFReadL(1, 1000000, f).decode('latin1')
    gdal.VSIFCloseL(f)

    assert layer_name not in content
    layer_name = new_layer_name

    ds = ogr.Open(dbname, update=1)
    with gdaltest.error_handler():
        ds.ExecuteSQL('DELLAYER:does_not_exist')
    assert gdal.GetLastErrorMsg() != ''
    gdal.ErrorReset()
    ds.ExecuteSQL('DELLAYER:' + layer_name)
    assert gdal.GetLastErrorMsg() == ''
    ds.ExecuteSQL('VACUUM')
    ds = None

    # Check that there is no more any reference to the layer
    f = gdal.VSIFOpenL(dbname, 'rb')
    content = gdal.VSIFReadL(1, 1000000, f).decode('latin1')
    gdal.VSIFCloseL(f)

    assert layer_name not in content

    gdaltest.gpkg_dr.DeleteDataSource(dbname)

    # Try again with DROP TABLE syntax
    ds = gdaltest.gpkg_dr.CreateDataSource(dbname)
    lyr = ds.CreateLayer(layer_name, geom_type=ogr.wkbCurvePolygon)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('CURVEPOLYGON ((0 0,0 1,1 1,0 0))'))
    lyr.CreateFeature(f)
    lyr.SetMetadataItem('FOO', 'BAR')
    lyr = ds.CreateLayer('another_layer_name')
    ds = None

    ds = ogr.Open(dbname, update=1)
    ds.ExecuteSQL('DROP TABLE "weird2\'layer""name"')
    assert gdal.GetLastErrorMsg() == ''
    ds.ExecuteSQL('DROP TABLE another_layer_name')
    assert gdal.GetLastErrorMsg() == ''
    with gdaltest.error_handler():
        ds.ExecuteSQL('DROP TABLE "foobar"')
    assert gdal.GetLastErrorMsg() != ''
    gdal.ErrorReset()
    ds.ExecuteSQL('VACUUM')
    ds = None

    # Check that there is no more any reference to the layer
    f = gdal.VSIFOpenL(dbname, 'rb')
    content = gdal.VSIFReadL(1, 1000000, f).decode('latin1')
    gdal.VSIFCloseL(f)

    assert layer_name not in content

    assert 'another_layer_name' not in content

    gdaltest.gpkg_dr.DeleteDataSource(dbname)

###############################################################################
# Test DeleteField()


def test_ogr_gpkg_35():

    dbname = '/vsimem/ogr_gpkg_35.gpkg'
    ds = gdaltest.gpkg_dr.CreateDataSource(dbname)
    lyr = ds.CreateLayer('test', geom_type=ogr.wkbPolygon)
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('bar_i_will_disappear', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('baz', ogr.OFTString))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(10)
    f.SetField('foo', 'fooval')
    f.SetField('bar_i_will_disappear', 'barval')
    f.SetField('baz', 'bazval')
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POLYGON ((0 0,0 1,1 1,0 0))'))
    lyr.CreateFeature(f)

    lyr_nonspatial = ds.CreateLayer('test_nonspatial', geom_type=ogr.wkbNone)
    lyr_nonspatial.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    lyr_nonspatial.CreateField(ogr.FieldDefn('bar_i_will_disappear', ogr.OFTString))
    lyr_nonspatial.CreateField(ogr.FieldDefn('baz', ogr.OFTString))

    # Metadata
    lyr_nonspatial.SetMetadataItem('FOO', 'BAR')
    ds.ExecuteSQL("INSERT INTO gpkg_metadata_reference VALUES ('column', 'test_nonspatial', 'bar_i_will_disappear', NULL, '2021-01-01T00:00:00.000Z', 1, NULL)")
    ds.ExecuteSQL("INSERT INTO gpkg_metadata VALUES (2, 'dataset','http://gdal.org','text/plain','bla')")
    ds.ExecuteSQL("INSERT INTO gpkg_metadata_reference VALUES ('column', 'test_nonspatial', 'bar_i_will_disappear', NULL, '2021-01-01T00:00:00.000Z', 2, NULL)")

    f = ogr.Feature(lyr_nonspatial.GetLayerDefn())
    f.SetFID(10)
    f.SetField('foo', 'fooval')
    f.SetField('bar_i_will_disappear', 'barval')
    f.SetField('baz', 'bazval')
    lyr_nonspatial.CreateFeature(f)

    ds.ExecuteSQL("""CREATE TABLE gpkg_data_columns (
  table_name TEXT NOT NULL,
  column_name TEXT NOT NULL,
  name TEXT UNIQUE,
  title TEXT,
  description TEXT,
  mime_type TEXT,
  constraint_name TEXT,
  CONSTRAINT pk_gdc PRIMARY KEY (table_name, column_name),
  CONSTRAINT fk_gdc_tn FOREIGN KEY (table_name) REFERENCES gpkg_contents(table_name)
)""")
    ds.ExecuteSQL("""CREATE TABLE gpkg_data_column_constraints (
            constraint_name TEXT NOT NULL,
            constraint_type TEXT NOT NULL,
            value TEXT,
            min NUMERIC,
            min_is_inclusive BOOLEAN,
            max NUMERIC,
            max_is_inclusive BOOLEAN,
            description TEXT,
            CONSTRAINT gdcc_ntv UNIQUE (constraint_name,
            constraint_type, value))""")
    ds.ExecuteSQL("INSERT INTO gpkg_data_columns VALUES('test', 'bar_i_will_disappear', 'bar_constraints', NULL, NULL, NULL, NULL)")
    ds.ExecuteSQL("INSERT INTO gpkg_extensions VALUES('test', 'bar_i_will_disappear', 'extension_name', 'definition', 'scope')")

    assert lyr.TestCapability(ogr.OLCDeleteField) == 1

    with gdaltest.error_handler():
        ret = lyr.DeleteField(-1)
    assert ret != 0

    with gdaltest.error_handler():
        ret = lyr.DeleteField(lyr.GetLayerDefn().GetFieldCount())
    assert ret != 0

    assert lyr.DeleteField(1) == 0
    assert lyr.GetLayerDefn().GetFieldCount() == 2

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f.GetFID() != 10 or f['foo'] != 'fooval' or f['baz'] != 'bazval' or \
       f.GetGeometryRef().ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,0 0))':
        f.DumpReadable()
        pytest.fail()

    lyr.StartTransaction()
    ret = lyr_nonspatial.DeleteField(1)
    lyr.CommitTransaction()
    assert ret == 0
    lyr_nonspatial.ResetReading()
    f = lyr_nonspatial.GetNextFeature()
    if f.GetFID() != 10 or f['foo'] != 'fooval' or f['baz'] != 'bazval':
        f.DumpReadable()
        pytest.fail()

    ds.ExecuteSQL('VACUUM')

    ds = None

    assert validate(dbname)

    # Try on read-only dataset
    ds = ogr.Open(dbname)

    sql_lyr = ds.ExecuteSQL('SELECT * FROM gpkg_metadata WHERE id = 1')
    assert sql_lyr.GetFeatureCount() == 1
    ds.ReleaseResultSet(sql_lyr)

    sql_lyr = ds.ExecuteSQL('SELECT * FROM gpkg_metadata WHERE id = 2')
    assert sql_lyr.GetFeatureCount() == 0
    ds.ReleaseResultSet(sql_lyr)

    lyr = ds.GetLayerByName('test_nonspatial')
    assert lyr.GetMetadataItem('FOO') == 'BAR'

    lyr = ds.GetLayer(0)
    with gdaltest.error_handler():
        ret = lyr.DeleteField(0)
    assert ret != 0
    ds = None

    # Check that there is no more any reference to the layer
    f = gdal.VSIFOpenL(dbname, 'rb')
    content = gdal.VSIFReadL(1, 1000000, f).decode('latin1')
    gdal.VSIFCloseL(f)

    assert 'bar_i_will_disappear' not in content

    gdaltest.gpkg_dr.DeleteDataSource(dbname)

###############################################################################
# Test AlterFieldDefn()


def test_ogr_gpkg_36():

    dbname = '/vsimem/ogr_gpkg_36.gpkg'
    ds = gdaltest.gpkg_dr.CreateDataSource(dbname)
    lyr = ds.CreateLayer('test', geom_type=ogr.wkbPolygon)
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('baz', ogr.OFTString))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(10)
    f.SetField('foo', '10.5')
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POLYGON ((0 0,0 1,1 1,0 0))'))
    lyr.CreateFeature(f)
    f = None

    ds.ExecuteSQL("""CREATE TABLE gpkg_data_columns (
  table_name TEXT NOT NULL,
  column_name TEXT NOT NULL,
  name TEXT UNIQUE,
  title TEXT,
  description TEXT,
  mime_type TEXT,
  constraint_name TEXT,
  CONSTRAINT pk_gdc PRIMARY KEY (table_name, column_name),
  CONSTRAINT fk_gdc_tn FOREIGN KEY (table_name) REFERENCES gpkg_contents(table_name)
)""")
    ds.ExecuteSQL("""CREATE TABLE gpkg_data_column_constraints (
            constraint_name TEXT NOT NULL,
            constraint_type TEXT NOT NULL,
            value TEXT,
            min NUMERIC,
            min_is_inclusive BOOLEAN,
            max NUMERIC,
            max_is_inclusive BOOLEAN,
            description TEXT,
            CONSTRAINT gdcc_ntv UNIQUE (constraint_name,
            constraint_type, value))""")
    ds.ExecuteSQL("INSERT INTO gpkg_data_columns VALUES('test', 'foo', 'constraint', NULL, NULL, NULL, NULL)")
    ds.ExecuteSQL("INSERT INTO gpkg_extensions VALUES('test', 'foo', 'extension_name', 'definition', 'read-write')")
    ds.ExecuteSQL("CREATE INDEX my_idx ON test(foo)")

    # Metadata
    lyr.SetMetadataItem('FOO', 'BAR')
    ds.ExecuteSQL("INSERT INTO gpkg_metadata_reference VALUES ('column', 'test', 'foo', NULL, '2021-01-01T00:00:00.000Z', 1, NULL)")

    assert lyr.TestCapability(ogr.OLCAlterFieldDefn) == 1

    with gdaltest.error_handler():
        ret = lyr.AlterFieldDefn(-1, ogr.FieldDefn('foo'), ogr.ALTER_ALL_FLAG)
    assert ret != 0

    with gdaltest.error_handler():
        ret = lyr.AlterFieldDefn(1, ogr.FieldDefn('foo'), ogr.ALTER_ALL_FLAG)
    assert ret != 0

    with gdaltest.error_handler():
        ret = lyr.AlterFieldDefn(0, ogr.FieldDefn(lyr.GetGeometryColumn()), ogr.ALTER_ALL_FLAG)
    assert ret != 0

    with gdaltest.error_handler():
        ret = lyr.AlterFieldDefn(0, ogr.FieldDefn(lyr.GetFIDColumn()), ogr.ALTER_ALL_FLAG)
    assert ret != 0

    with gdaltest.error_handler():
        ret = lyr.AlterFieldDefn(0, ogr.FieldDefn('baz'), ogr.ALTER_ALL_FLAG)
    assert ret != 0

    new_field_defn = ogr.FieldDefn('bar', ogr.OFTReal)
    new_field_defn.SetSubType(ogr.OFSTFloat32)
    assert lyr.AlterFieldDefn(0, new_field_defn, ogr.ALTER_ALL_FLAG) == 0

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f.GetFID() != 10 or f['bar'] != 10.5 or \
            f.GetGeometryRef().ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,0 0))':
        f.DumpReadable()
        pytest.fail()
    f = None

    lyr.StartTransaction()
    new_field_defn = ogr.FieldDefn('baw', ogr.OFTString)
    assert lyr.AlterFieldDefn(0, new_field_defn, ogr.ALTER_ALL_FLAG) == 0
    lyr.CommitTransaction()

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f.GetFID() != 10 or f['baw'] != '10.5' or \
       f.GetGeometryRef().ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,0 0))':
        f.DumpReadable()
        pytest.fail()
    f = None

    # Check that index has been recreated
    sql_lyr = ds.ExecuteSQL("SELECT * FROM sqlite_master WHERE name = 'my_idx'")
    f = sql_lyr.GetNextFeature()
    assert f is not None
    f = None
    ds.ReleaseResultSet(sql_lyr)

    ds.ExecuteSQL('VACUUM')

    ds = None

    assert validate(dbname)

    # Try on read-only dataset
    ds = ogr.Open(dbname)

    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_data_columns WHERE table_name = 'test' AND column_name = 'baw'")
    assert sql_lyr.GetFeatureCount() == 1
    ds.ReleaseResultSet(sql_lyr)

    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_metadata_reference WHERE table_name = 'test' AND column_name = 'baw'")
    assert sql_lyr.GetFeatureCount() == 1
    ds.ReleaseResultSet(sql_lyr)

    lyr = ds.GetLayer(0)
    with gdaltest.error_handler():
        ret = lyr.AlterFieldDefn(0, ogr.FieldDefn('foo'), ogr.ALTER_ALL_FLAG)
    assert ret != 0
    ds = None

    # Check that there is no more any reference to the layer
    f = gdal.VSIFOpenL(dbname, 'rb')
    content = gdal.VSIFReadL(1, 1000000, f).decode('latin1')
    gdal.VSIFCloseL(f)

    assert 'foo' not in content

    gdaltest.gpkg_dr.DeleteDataSource(dbname)

    # Test failed DB re-opening
    ds = gdaltest.gpkg_dr.CreateDataSource(dbname)
    lyr = ds.CreateLayer('test', geom_type=ogr.wkbPolygon)
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    # Unlink before AlterFieldDefn
    gdal.Unlink(dbname)
    with gdaltest.error_handler():
        ret = lyr.AlterFieldDefn(0, ogr.FieldDefn('bar'), ogr.ALTER_ALL_FLAG)
    assert ret != 0
    with gdaltest.error_handler():
        ds = None

    gdaltest.gpkg_dr.DeleteDataSource(dbname)

###############################################################################
# Test ReorderFields()


def test_ogr_gpkg_37():

    dbname = '/vsimem/ogr_gpkg_37.gpkg'
    ds = gdaltest.gpkg_dr.CreateDataSource(dbname)
    lyr = ds.CreateLayer('test', geom_type=ogr.wkbPolygon)
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('bar', ogr.OFTString))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(10)
    f.SetField('foo', 'fooval')
    f.SetField('bar', 'barval')
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POLYGON ((0 0,0 1,1 1,0 0))'))
    lyr.CreateFeature(f)

    ds.ExecuteSQL("""CREATE TABLE gpkg_data_columns (
  table_name TEXT NOT NULL,
  column_name TEXT NOT NULL,
  name TEXT UNIQUE,
  title TEXT,
  description TEXT,
  mime_type TEXT,
  constraint_name TEXT,
  CONSTRAINT pk_gdc PRIMARY KEY (table_name, column_name),
  CONSTRAINT fk_gdc_tn FOREIGN KEY (table_name) REFERENCES gpkg_contents(table_name)
)""")
    ds.ExecuteSQL("INSERT INTO gpkg_data_columns VALUES('test', 'foo', 'constraint', NULL, NULL, NULL, NULL)")
    ds.ExecuteSQL("INSERT INTO gpkg_extensions VALUES('test', 'foo', 'extension_name', 'definition', 'scope')")
    ds.ExecuteSQL("CREATE INDEX my_idx_foo ON test(foo)")
    ds.ExecuteSQL("CREATE INDEX my_idx_bar ON test(bar)")

    assert lyr.TestCapability(ogr.OLCReorderFields) == 1

    with gdaltest.error_handler():
        ret = lyr.ReorderFields([-1, -1])
    assert ret != 0

    assert lyr.ReorderFields([1, 0]) == 0

    lyr.ResetReading()
    assert lyr.GetLayerDefn().GetFieldIndex('foo') == 1 and lyr.GetLayerDefn().GetFieldIndex('bar') == 0
    f = lyr.GetNextFeature()
    if f.GetFID() != 10 or f['foo'] != 'fooval' or f['bar'] != 'barval' or \
            f.GetGeometryRef().ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,0 0))':
        f.DumpReadable()
        pytest.fail()

    # Check that index has been recreated
    sql_lyr = ds.ExecuteSQL("SELECT * FROM sqlite_master WHERE name = 'my_idx_foo' OR name = 'my_idx_bar'")
    assert sql_lyr.GetFeatureCount() == 2
    ds.ReleaseResultSet(sql_lyr)

    ds = None

    # Try on read-only dataset
    ds = ogr.Open(dbname)
    lyr = ds.GetLayer(0)
    with gdaltest.error_handler():
        ret = lyr.ReorderFields([1, 0])
    assert ret != 0
    ds = None

    gdaltest.gpkg_dr.DeleteDataSource(dbname)

###############################################################################
# Test GetExtent() and RECOMPUTE EXTENT ON


def test_ogr_gpkg_38(options=['SPATIAL_INDEX=YES']):

    dbname = '/vsimem/ogr_gpkg_38.gpkg'
    ds = gdaltest.gpkg_dr.CreateDataSource(dbname)
    lyr = ds.CreateLayer('test', geom_type=ogr.wkbLineString, options=options)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('LINESTRING (1 2,3 4)'))
    lyr.CreateFeature(f)
    ds = None

    # Simulate that extent is not recorded
    ds = ogr.Open(dbname, update=1)
    ds.ExecuteSQL('UPDATE gpkg_contents SET min_x = NULL, min_y = NULL, max_x = NULL, max_y = NULL')
    ds = None

    ds = ogr.Open(dbname, update=1)
    lyr = ds.GetLayer(0)
    extent = lyr.GetExtent(force=0, can_return_null=True)
    assert extent is None
    # Test that we can compute the extent of a layer that has none registered in gpkg_contents
    extent = lyr.GetExtent(force=1)
    assert extent == (1, 3, 2, 4)
    sql_lyr = ds.ExecuteSQL('SELECT min_x, min_y, max_x, max_y FROM gpkg_contents')
    f = sql_lyr.GetNextFeature()
    if f['min_x'] != 1 or f['min_y'] != 2 or f['max_x'] != 3 or f['max_y'] != 4:
        f.DumpReadable()
        pytest.fail()
    ds.ReleaseResultSet(sql_lyr)
    extent = lyr.GetExtent(force=0)
    assert extent == (1, 3, 2, 4)

    # Modify feature
    f = lyr.GetFeature(1)
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('LINESTRING (-1 -2,-3 -4)'))
    lyr.SetFeature(f)

    # The extent has grown
    extent = lyr.GetExtent(force=0)
    assert extent == (-3.0, 3.0, -4.0, 4.0)

    ds.ExecuteSQL('RECOMPUTE EXTENT ON test')
    extent = lyr.GetExtent(force=0)
    assert extent == (-3.0, -1.0, -4.0, -2.0)
    ds = None

    ds = ogr.Open(dbname)
    lyr = ds.GetLayer(0)
    extent = lyr.GetExtent(force=0)
    assert extent == (-3.0, -1.0, -4.0, -2.0)
    ds = None

    ds = ogr.Open(dbname, update=1)
    lyr = ds.GetLayer(0)
    # Delete last feature
    lyr.DeleteFeature(1)

    # This should cancel NULLify the extent in gpkg_contents
    ds.ExecuteSQL('RECOMPUTE EXTENT ON test')
    extent = lyr.GetExtent(force=0, can_return_null=True)
    assert extent is None
    ds = None

    ds = ogr.Open(dbname)
    lyr = ds.GetLayer(0)
    extent = lyr.GetExtent(force=0, can_return_null=True)
    assert extent is None
    ds = None

    gdaltest.gpkg_dr.DeleteDataSource(dbname)


def test_ogr_gpkg_38_nospi():
    return test_ogr_gpkg_38(options=['SPATIAL_INDEX=NO'])


###############################################################################
# Test checking of IDENTIFIER unicity


def test_ogr_gpkg_39():

    dbname = '/vsimem/ogr_gpkg_39.gpkg'
    ds = gdaltest.gpkg_dr.CreateDataSource(dbname)

    ds.CreateLayer('test')

    lyr = ds.CreateLayer('test_with_explicit_identifier', options=['IDENTIFIER=explicit_identifier'])
    assert lyr is not None

    # Allow overwriting
    lyr = ds.CreateLayer('test_with_explicit_identifier', options=['IDENTIFIER=explicit_identifier', 'OVERWRITE=YES'])
    assert lyr is not None

    with gdaltest.error_handler():
        lyr = ds.CreateLayer('test2', options=['IDENTIFIER=test'])
    assert lyr is None

    with gdaltest.error_handler():
        lyr = ds.CreateLayer('test2', options=['IDENTIFIER=explicit_identifier'])
    assert lyr is None

    ds.ExecuteSQL("INSERT INTO gpkg_contents ( table_name, identifier, data_type ) VALUES ( 'some_table', 'another_identifier', 'some_data_type' )")
    with gdaltest.error_handler():
        lyr = ds.CreateLayer('test2', options=['IDENTIFIER=another_identifier'])
    assert lyr is None
    ds = None

    gdaltest.gpkg_dr.DeleteDataSource(dbname)

###############################################################################
# Run creating a non-spatial layer that is registered as 'attributes' and
# read it back


def test_ogr_gpkg_40():

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_40.gpkg')
    ds.CreateLayer('aspatial', geom_type=ogr.wkbNone)
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_40.gpkg')
    assert ds.GetLayerCount() == 1
    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_contents")
    assert sql_lyr.GetFeatureCount() == 1
    ds.ReleaseResultSet(sql_lyr)
    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_geometry_columns")
    assert sql_lyr.GetFeatureCount() == 0
    ds.ReleaseResultSet(sql_lyr)
    sql_lyr = ds.ExecuteSQL("SELECT * FROM sqlite_master WHERE name = 'gpkg_extensions'")
    assert sql_lyr.GetFeatureCount() == 0
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    assert validate('/vsimem/ogr_gpkg_40.gpkg'), 'validation failed'

    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_40.gpkg')

###############################################################################
# Test tables without integer primary key (#6799), and unrecognized column type


def test_ogr_gpkg_41():

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_41.gpkg')
    ds.ExecuteSQL('CREATE TABLE foo (mycol VARCHAR_ILLEGAL)')
    ds.ExecuteSQL("INSERT INTO foo VALUES ('myval')")
    ds.ExecuteSQL("INSERT INTO gpkg_contents (table_name,data_type,identifier,description,last_change,srs_id) VALUES ('foo','attributes','foo','','',0)")
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_41.gpkg')
    lyr = ds.GetLayer(0)
    with gdaltest.error_handler():
        f = lyr.GetNextFeature()
    if f['mycol'] != 'myval' or f.GetFID() != 1:
        f.DumpReadable()
        pytest.fail()
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_41.gpkg')
    lyr = ds.GetLayer(0)
    with gdaltest.error_handler():
        f = lyr.GetFeature(1)
    if f['mycol'] != 'myval' or f.GetFID() != 1:
        f.DumpReadable()
        pytest.fail()
    ds = None

    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_41.gpkg')

###############################################################################
# Test feature_count


def foo_has_trigger(ds):
    sql_lyr = ds.ExecuteSQL(
        "SELECT COUNT(*) FROM sqlite_master WHERE name = 'trigger_insert_feature_count_foo'", dialect='DEBUG')
    f = sql_lyr.GetNextFeature()
    has_trigger = f.GetField(0) == 1
    f = None
    ds.ReleaseResultSet(sql_lyr)
    return has_trigger


def get_feature_count_from_gpkg_contents(ds):
    sql_lyr = ds.ExecuteSQL('SELECT feature_count FROM gpkg_ogr_contents', dialect='DEBUG')
    f = sql_lyr.GetNextFeature()
    val = f.GetField(0)
    f = None
    ds.ReleaseResultSet(sql_lyr)
    return val


def test_ogr_gpkg_42():

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_42.gpkg')
    lyr = ds.CreateLayer('foo', geom_type=ogr.wkbNone)
    lyr.CreateField(ogr.FieldDefn('i', ogr.OFTInteger))
    for i in range(5):
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField(0, i)
        lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_42.gpkg')
    lyr = ds.GetLayer(0)
    assert get_feature_count_from_gpkg_contents(ds) == 5
    assert foo_has_trigger(ds)
    assert lyr.TestCapability(ogr.OLCFastFeatureCount) != 0
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_42.gpkg', update=1)
    lyr = ds.GetLayer(0)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField(0, 10)
    lyr.CreateFeature(f)

    # Has been invalidated for now
    assert get_feature_count_from_gpkg_contents(ds) is None

    assert not foo_has_trigger(ds)

    fc = lyr.GetFeatureCount()
    assert fc == 6

    ds.ExecuteSQL('DELETE FROM foo WHERE i = 1')

    assert foo_has_trigger(ds)

    assert get_feature_count_from_gpkg_contents(ds) is None

    fc = lyr.GetFeatureCount()
    assert fc == 5

    assert get_feature_count_from_gpkg_contents(ds) == 5

    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_42.gpkg', update=1)
    lyr = ds.GetLayer(0)
    fc = lyr.GetFeatureCount()
    assert fc == 5
    ds.ExecuteSQL('UPDATE gpkg_ogr_contents SET feature_count = NULL')
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_42.gpkg', update=1)
    lyr = ds.GetLayer(0)
    assert get_feature_count_from_gpkg_contents(ds) is None
    fc = lyr.GetFeatureCount()
    assert fc == 5
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_42.gpkg', update=1)
    assert get_feature_count_from_gpkg_contents(ds) == 5

    # So as to test that we really read from gpkg_ogr_contents
    ds.ExecuteSQL('UPDATE gpkg_ogr_contents SET feature_count = 5000')

    ds = ogr.Open('/vsimem/ogr_gpkg_42.gpkg', update=1)
    lyr = ds.GetLayer(0)
    fc = lyr.GetFeatureCount()
    assert fc == 5000

    # Test renaming
    ds.ExecuteSQL('ALTER TABLE foo RENAME TO bar')
    ds = None
    ds = ogr.Open('/vsimem/ogr_gpkg_42.gpkg', update=1)
    sql_lyr = ds.ExecuteSQL("SELECT feature_count FROM gpkg_ogr_contents WHERE table_name = 'bar'", dialect='DEBUG')
    f = sql_lyr.GetNextFeature()
    val = f.GetField(0)
    f = None
    ds.ReleaseResultSet(sql_lyr)
    assert val == 5000

    # Test layer deletion
    ds.DeleteLayer(0)
    sql_lyr = ds.ExecuteSQL("SELECT feature_count FROM gpkg_ogr_contents", dialect='DEBUG')
    f = sql_lyr.GetNextFeature()
    assert f is None
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    # Test without feature_count column
    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_42.gpkg',
                                           options=['ADD_GPKG_OGR_CONTENTS=FALSE'])
    lyr = ds.CreateLayer('foo', geom_type=ogr.wkbNone)
    lyr.CreateField(ogr.FieldDefn('i', ogr.OFTInteger))
    for i in range(5):
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField(0, i)
        lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_42.gpkg', update=1)

    # Check that feature_count column is missing
    sql_lyr = ds.ExecuteSQL('PRAGMA table_info(gpkg_contents)')
    fc = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    assert fc == 10

    assert not foo_has_trigger(ds)

    lyr = ds.GetLayer(0)
    assert lyr.TestCapability(ogr.OLCFastFeatureCount) == 0
    fc = lyr.GetFeatureCount()
    assert fc == 5
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField(0, 10)
    lyr.CreateFeature(f)
    lyr = ds.GetLayer(0)
    fc = lyr.GetFeatureCount()
    assert fc == 6
    ds.ExecuteSQL('DELETE FROM foo WHERE i = 1')
    fc = lyr.GetFeatureCount()
    assert fc == 5
    ds = None

    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_42.gpkg')


###############################################################################
# Test limitations on number of tables

def test_ogr_gpkg_43():

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_43.gpkg')
    ds.StartTransaction()
    for i in range(1001):
        ds.ExecuteSQL("INSERT INTO gpkg_contents (table_name, data_type, identifier) " +
                      "VALUES ('tiles%d', 'tiles', 'tiles%d')" % (i + 1, i + 1))
        ds.ExecuteSQL("INSERT INTO gpkg_tile_matrix_set VALUES " +
                      "('tiles%d', 0, 440720, 3750120, 441920, 3751320)" % (i + 1))
    for i in range(1001):
        ds.ExecuteSQL("INSERT INTO gpkg_contents (table_name, data_type, identifier) " +
                      "VALUES ('attr%d', 'attributes', 'attr%d')" % (i + 1, i + 1))
        ds.ExecuteSQL("CREATE TABLE attr%d (id INTEGER PRIMARY KEY AUTOINCREMENT)" % (i + 1))
    ds.CommitTransaction()
    ds = None

    ds = gdal.OpenEx('/vsimem/ogr_gpkg_43.gpkg')
    assert len(ds.GetMetadata_List('SUBDATASETS')) == 2 * 1001
    assert ds.GetLayerCount() == 1001

    with gdaltest.config_option('OGR_TABLE_LIMIT', '1000'):
        with gdaltest.error_handler():
            ds = gdal.OpenEx('/vsimem/ogr_gpkg_43.gpkg')
            assert len(ds.GetMetadata_List('SUBDATASETS')) == 2 * 1000
            assert ds.GetLayerCount() == 1000
    ds = None

    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_43.gpkg')


###############################################################################
# Test GeoPackage without metadata table

def test_ogr_gpkg_44():

    gdal.SetConfigOption('CREATE_METADATA_TABLES', 'NO')
    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_44.gpkg')
    ds.CreateLayer('foo')
    ds = None
    gdal.SetConfigOption('CREATE_METADATA_TABLES', None)

    assert validate('/vsimem/ogr_gpkg_44.gpkg'), 'validation failed'

    ds = ogr.Open('/vsimem/ogr_gpkg_44.gpkg')
    md = ds.GetMetadata()
    assert md == {}
    md = ds.GetLayer(0).GetMetadata()
    assert md == {}
    sql_lyr = ds.ExecuteSQL("SELECT * FROM sqlite_master WHERE name = 'gpkg_metadata'")
    fc = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    assert fc == 0
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_44.gpkg', update=1)
    ds.SetMetadataItem('FOO', 'BAR')
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_44.gpkg')
    md = ds.GetMetadata()
    assert md == {'FOO': 'BAR'}
    ds = None

    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_44.gpkg')


###############################################################################
# Test non conformant GeoPackage: table with non INTEGER PRIMARY KEY

def test_ogr_gpkg_45():

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_45.gpkg')
    ds.ExecuteSQL('CREATE TABLE test (a INTEGER, b INTEGER, CONSTRAINT pkid_constraint PRIMARY KEY (a, b))')
    ds.ExecuteSQL("INSERT INTO gpkg_contents ( table_name, identifier, data_type ) VALUES ( 'test', 'test', 'attributes' )")
    ds = None
    ds = ogr.Open('/vsimem/ogr_gpkg_45.gpkg')
    lyr = ds.GetLayer(0)
    assert lyr.GetFIDColumn() == ''
    assert lyr.GetLayerDefn().GetFieldCount() == 2
    ds = None

    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_45.gpkg')

###############################################################################
# Test spatial view and spatial index


def test_ogr_gpkg_46():

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_46.gpkg')
    lyr = ds.CreateLayer('foo')
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(0 0)'))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1 1)'))
    lyr.CreateFeature(f)
    # Note: this definition of a view is non conformant with GPKG 1.3 clarifications on views
    ds.ExecuteSQL('CREATE VIEW my_view AS SELECT geom AS my_geom, fid AS my_fid FROM foo')
    ds.ExecuteSQL("INSERT INTO gpkg_contents (table_name, identifier, data_type, srs_id) VALUES ( 'my_view', 'my_view', 'features', 0 )")
    ds.ExecuteSQL("INSERT INTO gpkg_geometry_columns (table_name, column_name, geometry_type_name, srs_id, z, m) values ('my_view', 'my_geom', 'GEOMETRY', 0, 0, 0)")

    # Note: this definition of a view is non conformant with GPKG 1.3 clarifications on views
    ds.ExecuteSQL("CREATE VIEW my_view2 AS SELECT geom, fid AS OGC_FID, 'bla' as another_column FROM foo")
    ds.ExecuteSQL("INSERT INTO gpkg_contents (table_name, identifier, data_type, srs_id) VALUES ( 'my_view2', 'my_view2', 'features', 0 )")
    ds.ExecuteSQL("INSERT INTO gpkg_geometry_columns (table_name, column_name, geometry_type_name, srs_id, z, m) values ('my_view2', 'geom', 'GEOMETRY', 0, 0, 0)")

    ds.ExecuteSQL('CREATE VIEW my_view3 AS SELECT a.fid * 10000 + b.fid as my_fid, a.fid as fid1, a.geom, b.fid as fid2 FROM foo a, foo b')
    ds.ExecuteSQL("INSERT INTO gpkg_contents (table_name, identifier, data_type, srs_id) VALUES ( 'my_view3', 'my_view3', 'features', 0 )")
    ds.ExecuteSQL("INSERT INTO gpkg_geometry_columns (table_name, column_name, geometry_type_name, srs_id, z, m) values ('my_view3', 'geom', 'GEOMETRY', 0, 0, 0)")

    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_46.gpkg', update=1)
    lyr = ds.GetLayerByName('my_view')
    assert lyr.GetLayerDefn().GetFieldCount() == 1
    assert lyr.GetGeometryColumn() == 'my_geom'

    # Operations not valid on a view
    with gdaltest.error_handler():
        ds.ReleaseResultSet(ds.ExecuteSQL("SELECT CreateSpatialIndex('my_view', 'my_geom')"))
        ds.ReleaseResultSet(ds.ExecuteSQL("SELECT DisableSpatialIndex('my_view', 'my_geom')"))
        lyr.AlterFieldDefn(0, lyr.GetLayerDefn().GetFieldDefn(0), ogr.ALTER_ALL_FLAG)
        lyr.DeleteField(0)
        lyr.ReorderFields([0])
        lyr.CreateField(ogr.FieldDefn('bar'))

    # Check if spatial index is recognized
    sql_lyr = ds.ExecuteSQL("SELECT HasSpatialIndex('my_view', 'my_geom')")
    f = sql_lyr.GetNextFeature()
    has_spatial_index = f.GetField(0) == 1
    ds.ReleaseResultSet(sql_lyr)
    if not has_spatial_index:
        ds = None
        gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_46.gpkg')
        pytest.skip('SQLite likely built without SQLITE_HAS_COLUMN_METADATA')

    # Effectively test spatial index
    lyr.SetSpatialFilterRect(-0.5, -0.5, 0.5, 0.5)
    assert lyr.GetFeatureCount() == 1
    f = lyr.GetNextFeature()
    assert f is not None
    f = lyr.GetNextFeature()
    assert f is None

    # View with FID in non-first position
    lyr = ds.GetLayerByName('my_view2')
    assert lyr.GetLayerDefn().GetFieldCount() == 1
    assert lyr.GetFIDColumn() == 'OGC_FID'
    f = lyr.GetNextFeature()
    if f.GetFID() != 1 or f.GetField(0) != 'bla':
        f.DumpReadable()
        pytest.fail()

    # View with FID in first position
    lyr = ds.GetLayerByName('my_view3')
    assert lyr.GetLayerDefn().GetFieldCount() == 2
    f = lyr.GetNextFeature()
    assert f.GetFID() == 10001
    f = lyr.GetNextFeature()
    assert f.GetFID() == 10002
    f2 = lyr.GetFeature(10002)
    assert f.Equal(f2)
    ds = None

    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_46.gpkg')

###############################################################################
# Test corner case of Identify()


def test_ogr_gpkg_47():

    gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_47.gpkg')
    # Set wrong application_id
    fp = gdal.VSIFOpenL('/vsimem/ogr_gpkg_47.gpkg', 'rb+')
    gdal.VSIFSeekL(fp, 68, 0)
    gdal.VSIFWriteL(struct.pack('B' * 4, 0, 0, 0, 0), 4, 1, fp)
    gdal.VSIFCloseL(fp)

    with gdaltest.error_handler():
        ds = ogr.Open('/vsimem/ogr_gpkg_47.gpkg', update=1)
    assert ds is not None
    assert gdal.GetLastErrorMsg() != ''

    gdal.SetConfigOption('GPKG_WARN_UNRECOGNIZED_APPLICATION_ID', 'NO')
    ogr.Open('/vsimem/ogr_gpkg_47.gpkg')
    gdal.SetConfigOption('GPKG_WARN_UNRECOGNIZED_APPLICATION_ID', None)
    assert gdal.GetLastErrorMsg() == ''

    gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_47.gpkg', options=['VERSION=1.2'])
    # Set wrong user_version
    fp = gdal.VSIFOpenL('/vsimem/ogr_gpkg_47.gpkg', 'rb+')
    gdal.VSIFSeekL(fp, 60, 0)
    gdal.VSIFWriteL(struct.pack('B' * 4, 0, 0, 0, 0), 4, 1, fp)
    gdal.VSIFCloseL(fp)

    with gdaltest.error_handler():
        ds = ogr.Open('/vsimem/ogr_gpkg_47.gpkg', update=1)
    assert ds is not None
    assert gdal.GetLastErrorMsg() != ''
    ds = None

    gdal.SetConfigOption('GPKG_WARN_UNRECOGNIZED_APPLICATION_ID', 'NO')
    ogr.Open('/vsimem/ogr_gpkg_47.gpkg')
    gdal.SetConfigOption('GPKG_WARN_UNRECOGNIZED_APPLICATION_ID', None)
    assert gdal.GetLastErrorMsg() == ''

    # Set GPKG 1.2.1
    gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_47.gpkg', options=['VERSION=1.2'])
    # Set user_version
    fp = gdal.VSIFOpenL('/vsimem/ogr_gpkg_47.gpkg', 'rb+')
    gdal.VSIFSeekL(fp, 60, 0)
    assert struct.unpack('>I', gdal.VSIFReadL(4, 1, fp))[0] == 10200
    gdal.VSIFSeekL(fp, 60, 0)
    gdal.VSIFWriteL(struct.pack('>I', 10201), 4, 1, fp)
    gdal.VSIFCloseL(fp)

    ds = ogr.Open('/vsimem/ogr_gpkg_47.gpkg', update=1)
    assert ds is not None
    assert gdal.GetLastErrorMsg() == ''
    ds = None

    gdal.SetConfigOption('GPKG_WARN_UNRECOGNIZED_APPLICATION_ID', 'NO')
    ogr.Open('/vsimem/ogr_gpkg_47.gpkg')
    gdal.SetConfigOption('GPKG_WARN_UNRECOGNIZED_APPLICATION_ID', None)
    assert gdal.GetLastErrorMsg() == ''

    # Set GPKG 1.3.0
    gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_47.gpkg', options=['VERSION=1.3'])
    # Check user_version
    fp = gdal.VSIFOpenL('/vsimem/ogr_gpkg_47.gpkg', 'rb')
    gdal.VSIFSeekL(fp, 60, 0)
    assert struct.unpack('>I', gdal.VSIFReadL(4, 1, fp))[0] == 10300
    gdal.VSIFCloseL(fp)

    ds = ogr.Open('/vsimem/ogr_gpkg_47.gpkg', update=1)
    assert ds is not None
    assert gdal.GetLastErrorMsg() == ''
    ds = None

    gdal.SetConfigOption('GPKG_WARN_UNRECOGNIZED_APPLICATION_ID', 'NO')
    ogr.Open('/vsimem/ogr_gpkg_47.gpkg')
    gdal.SetConfigOption('GPKG_WARN_UNRECOGNIZED_APPLICATION_ID', None)
    assert gdal.GetLastErrorMsg() == ''

    # Set GPKG 1.99.0
    gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_47.gpkg', options=['VERSION=1.2'])
    # Set user_version
    fp = gdal.VSIFOpenL('/vsimem/ogr_gpkg_47.gpkg', 'rb+')
    gdal.VSIFSeekL(fp, 60, 0)
    gdal.VSIFWriteL(struct.pack('>I', 19900), 4, 1, fp)
    gdal.VSIFCloseL(fp)

    with gdaltest.error_handler():
        ds = ogr.Open('/vsimem/ogr_gpkg_47.gpkg', update=1)
    assert ds is not None
    assert gdal.GetLastErrorMsg() != ''

    gdal.SetConfigOption('GPKG_WARN_UNRECOGNIZED_APPLICATION_ID', 'NO')
    ogr.Open('/vsimem/ogr_gpkg_47.gpkg')
    gdal.SetConfigOption('GPKG_WARN_UNRECOGNIZED_APPLICATION_ID', None)
    assert gdal.GetLastErrorMsg() == ''

    # Just for the sake of coverage testing in DEBUG mode
    with gdaltest.error_handler():
        gdaltest.gpkg_dr.CreateDataSource('/vsimem/.cur_input')
    # Set wrong application_id
    fp = gdal.VSIFOpenL('/vsimem/.cur_input', 'rb+')
    gdal.VSIFSeekL(fp, 68, 0)
    gdal.VSIFWriteL(struct.pack('B' * 4, 0, 0, 0, 0), 4, 1, fp)
    gdal.VSIFCloseL(fp)
    ogr.Open('/vsimem/.cur_input')
    gdal.Unlink('/vsimem/.cur_input')

    with gdaltest.error_handler():
        gdaltest.gpkg_dr.CreateDataSource('/vsimem/.cur_input', options=['VERSION=1.2'])
    # Set wrong user_version
    fp = gdal.VSIFOpenL('/vsimem/.cur_input', 'rb+')
    gdal.VSIFSeekL(fp, 60, 0)
    gdal.VSIFWriteL(struct.pack('B' * 4, 0, 0, 0, 0), 4, 1, fp)
    gdal.VSIFCloseL(fp)
    ogr.Open('/vsimem/.cur_input')
    gdal.Unlink('/vsimem/.cur_input')

    # Test reading in a zip
    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_47.gpkg')
    ds.CreateLayer('foo')
    ds = None
    fp = gdal.VSIFOpenL('/vsimem/ogr_gpkg_47.gpkg', 'rb')
    content = gdal.VSIFReadL(1, 1000000, fp)
    gdal.VSIFCloseL(fp)
    fzip = gdal.VSIFOpenL('/vsizip//vsimem/ogr_gpkg_47.zip', 'wb')
    fp = gdal.VSIFOpenL('/vsizip//vsimem/ogr_gpkg_47.zip/my.gpkg', 'wb')
    gdal.VSIFWriteL(content, 1, len(content), fp)
    gdal.VSIFCloseL(fp)
    gdal.VSIFCloseL(fzip)
    ds = ogr.Open('/vsizip//vsimem/ogr_gpkg_47.zip')
    assert ds.GetDriver().GetName() == 'GPKG'
    ds = None

    gdal.Unlink('/vsimem/ogr_gpkg_47.zip')
    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_47.gpkg')

###############################################################################
# Test insertion of features with unset fields


def test_ogr_gpkg_48():

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_48.gpkg')
    lyr = ds.CreateLayer('foo')
    lyr.CreateField(ogr.FieldDefn('a'))
    lyr.CreateField(ogr.FieldDefn('b'))
    lyr.CreateField(ogr.FieldDefn('c'))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('a', 'a')
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('b', 'b')
    f.SetField('c', 'c')
    lyr.CreateFeature(f)
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f.GetField('a') != 'a' or f.GetField('b') is not None:
        f.DumpReadable()
        pytest.fail()
    f = lyr.GetNextFeature()
    if f.GetField('b') != 'b' or f.GetField('c') != 'c' or f.GetField('a') is not None:
        f.DumpReadable()
        pytest.fail()

    # No geom field, one single field with default value
    lyr = ds.CreateLayer('default_field_no_geom', geom_type=ogr.wkbNone)
    fld_defn = ogr.FieldDefn('foo')
    fld_defn.SetDefault('x')
    lyr.CreateField(fld_defn)
    f = ogr.Feature(lyr.GetLayerDefn())
    assert lyr.CreateFeature(f) == 0
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f.GetField('foo') != 'x':
        f.DumpReadable()
        pytest.fail()
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(1)
    assert lyr.SetFeature(f) == 0
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f.GetField('foo') != 'x':
        f.DumpReadable()
        pytest.fail()
    ds = None

    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_48.gpkg')

###############################################################################
# Test CreateGeomField() on a attributes layer


def test_ogr_gpkg_49():

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_49.gpkg')

    lyr = ds.CreateLayer('test', geom_type=ogr.wkbNone,
                         options=['ASPATIAL_VARIANT=GPKG_ATTRIBUTES'])

    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    f = None

    field_defn = ogr.GeomFieldDefn('', ogr.wkbPoint)
    assert lyr.CreateGeomField(field_defn) == 0
    ds = None

    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_49.gpkg')

###############################################################################
# Test minimalistic support of definition_12_063


def test_ogr_gpkg_50():

    gdal.SetConfigOption('GPKG_ADD_DEFINITION_12_063', 'YES')
    gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_50.gpkg')
    gdal.SetConfigOption('GPKG_ADD_DEFINITION_12_063', None)

    ds = ogr.Open('/vsimem/ogr_gpkg_50.gpkg', update=1)
    srs32631 = osr.SpatialReference()
    srs32631.ImportFromEPSG(32631)
    ds.CreateLayer('test', srs=srs32631)

    # No authority node
    srs_without_org = osr.SpatialReference()
    srs_without_org.SetFromUserInput("""GEOGCS["another geogcs",
    DATUM["another datum",
        SPHEROID["another spheroid",1000,0]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]]""")
    lyr = ds.CreateLayer('without_org', srs=srs_without_org)

    ds = None

    assert validate('/vsimem/ogr_gpkg_50.gpkg'), 'validation failed'

    ds = ogr.Open('/vsimem/ogr_gpkg_50.gpkg')
    lyr = ds.GetLayer('test')
    assert lyr.GetSpatialRef().IsSame(srs32631)
    lyr = ds.GetLayer('without_org')
    assert lyr.GetSpatialRef().IsSame(srs_without_org)
    sql_lyr = ds.ExecuteSQL('SELECT definition_12_063 FROM gpkg_spatial_ref_sys WHERE srs_id = 32631')
    f = sql_lyr.GetNextFeature()
    assert f.GetField(0).startswith('PROJCRS["WGS 84 / UTM zone 31N"')
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_50.gpkg')

###############################################################################
# Test opening a .gpkg.sql file


def test_ogr_gpkg_51():

    if gdaltest.gpkg_dr.GetMetadataItem("ENABLE_SQL_GPKG_FORMAT") != 'YES':
        pytest.skip()

    ds = ogr.Open('data/gpkg/poly.gpkg.sql')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f is not None

###############################################################################
# Test opening a .gpkg file


def test_ogr_gpkg_52():

    ds = ogr.Open('data/gpkg/poly_non_conformant.gpkg')
    lyr = ds.GetLayer(0)
    with gdaltest.error_handler():
        f = lyr.GetNextFeature()
    assert f is not None

###############################################################################
# Test opening a .gpkg file with inconsistency regarding table case (#6916)


def test_ogr_gpkg_53():

    if gdaltest.gpkg_dr.GetMetadataItem("ENABLE_SQL_GPKG_FORMAT") != 'YES':
        pytest.skip()

    ds = ogr.Open('data/gpkg/poly_inconsistent_case.gpkg.sql')
    assert ds.GetLayerCount() == 1
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f is not None

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is not None:
        ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' data/gpkg/poly_inconsistent_case.gpkg.sql')

        assert ret.find('INFO') != -1 and ret.find('ERROR') == -1


###############################################################################
# Test editing of a database with 2 layers (https://issues.qgis.org/issues/17034)


def test_ogr_gpkg_54():

    # Must be on a real file system to demonstrate potential locking
    # issue
    tmpfile = 'tmp/ogr_gpkg_54.gpkg'
    ds = ogr.GetDriverByName('GPKG').CreateDataSource(tmpfile)
    lyr = ds.CreateLayer('layer1', geom_type=ogr.wkbPoint)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(0 0)'))
    lyr.CreateFeature(f)
    f = None
    lyr = ds.CreateLayer('layer2', geom_type=ogr.wkbPoint)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1 1)'))
    lyr.CreateFeature(f)
    f = None
    ds = None

    ds1 = ogr.Open(tmpfile, update=1)
    ds2 = ogr.Open(tmpfile, update=1)

    lyr1 = ds1.GetLayer(0)
    lyr2 = ds2.GetLayer(1)

    f1 = lyr1.GetFeature(1)
    f1.SetGeometry(ogr.CreateGeometryFromWkt('POINT (1 2)'))
    lyr1.SetFeature(f1)

    f2 = lyr2.GetFeature(1)
    f2.SetGeometry(ogr.CreateGeometryFromWkt('POINT (3 4)'))
    lyr2.SetFeature(f2)

    f1 = lyr1.GetFeature(1)
    f1.SetGeometry(ogr.CreateGeometryFromWkt('POINT (5 6)'))
    lyr1.SetFeature(f1)

    f2 = lyr2.GetFeature(1)
    f2.SetGeometry(ogr.CreateGeometryFromWkt('POINT (7 8)'))
    lyr2.SetFeature(f2)

    ds1 = None
    ds2 = None

    ds = ogr.Open(tmpfile)
    lyr1 = ds.GetLayer(0)
    f = lyr1.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (5 6)':
        f.DumpReadable()
        pytest.fail()
    lyr2 = ds.GetLayer(1)
    f = lyr2.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (7 8)':
        f.DumpReadable()
        pytest.fail()
    ds = None

    gdal.Unlink(tmpfile)

###############################################################################
# Test inserting geometries incompatible with declared layer geometry type


def test_ogr_gpkg_55():

    tmpfile = '/vsimem/ogr_gpkg_55.gpkg'
    ds = ogr.GetDriverByName('GPKG').CreateDataSource(tmpfile)
    lyr = ds.CreateLayer('layer1', geom_type=ogr.wkbLineString)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(0 0)'))
    gdal.ErrorReset()
    with gdaltest.error_handler():
        lyr.CreateFeature(f)
    assert gdal.GetLastErrorMsg() != '', 'should have warned'
    f = None
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1 1)'))
    gdal.ErrorReset()
    lyr.CreateFeature(f)
    assert gdal.GetLastErrorMsg() == '', 'should NOT have warned'
    f = None
    ds = None

    gdal.Unlink(tmpfile)

###############################################################################
# Test FID identification on SQL result layer


def test_ogr_gpkg_56():

    ds = gdal.VectorTranslate('/vsimem/ogr_gpkg_56.gpkg', 'data/poly.shp', format='GPKG')
    lyr = ds.ExecuteSQL('select a.fid as fid1, b.fid as fid2 from poly a, poly b order by fid1, fid2')
    lyr.GetNextFeature()
    f = lyr.GetNextFeature()
    if f.GetField('fid1') != 1 or f.GetField('fid2') != 2:
        f.DumpReadable()
        pytest.fail()
    ds.ReleaseResultSet(lyr)
    ds = None
    gdal.Unlink('/vsimem/ogr_gpkg_56.gpkg')

###############################################################################
# Test creation of a field which is the same as the FID column


def test_ogr_gpkg_creation_fid():

    filename = '/vsimem/test_ogr_gpkg_creation_fid.gpkg'
    ds = ogr.GetDriverByName('GPKG').CreateDataSource(filename)

    lyr = ds.CreateLayer('fid_integer')
    assert lyr.CreateField(ogr.FieldDefn('fid', ogr.OFTInteger)) == ogr.OGRERR_NONE

    f = ogr.Feature(lyr.GetLayerDefn())
    f['fid'] = 12
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert f.GetFID() == 12
    f = lyr.GetFeature(f.GetFID())
    assert f is not None
    assert lyr.SetFeature(f) == ogr.OGRERR_NONE
    f = None

    f = ogr.Feature(lyr.GetLayerDefn())
    f['fid'] = 13
    f.SetFID(13)
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert f.GetFID() == 13
    f = lyr.GetFeature(f.GetFID())
    assert f is not None
    assert lyr.SetFeature(f) == ogr.OGRERR_NONE
    f = None

    lyr = ds.CreateLayer('fid_integer64')
    assert lyr.CreateField(ogr.FieldDefn('fid', ogr.OFTInteger64)) == ogr.OGRERR_NONE

    f = ogr.Feature(lyr.GetLayerDefn())
    f['fid'] = 1234567890123
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert f.GetFID() == 1234567890123
    f = lyr.GetFeature(f.GetFID())
    assert f is not None
    assert lyr.SetFeature(f) == ogr.OGRERR_NONE
    f = None

    f = ogr.Feature(lyr.GetLayerDefn())
    f['fid'] = 1234567890124
    f.SetFID(1234567890124)
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert f.GetFID() == 1234567890124
    f = lyr.GetFeature(f.GetFID())
    assert f is not None
    assert lyr.SetFeature(f) == ogr.OGRERR_NONE
    f = None

    f = ogr.Feature(lyr.GetLayerDefn())
    f['fid'] = 1234567890125
    f.SetFID(1)
    with gdaltest.error_handler():
        assert lyr.CreateFeature(f) == ogr.OGRERR_FAILURE

    # Simulates the situation of GeoPackage ---QGIS---> Shapefile --> GeoPackage
    # See https://github.com/qgis/QGIS/pull/43118
    lyr = ds.CreateLayer('fid_real')
    fld_defn = ogr.FieldDefn('fid', ogr.OFTReal)
    fld_defn.SetWidth(20)
    fld_defn.SetPrecision(0)
    assert lyr.CreateField(fld_defn) == ogr.OGRERR_NONE

    f = ogr.Feature(lyr.GetLayerDefn())
    f['fid'] = 1234567890123
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert f.GetFID() == 1234567890123
    f = lyr.GetFeature(f.GetFID())
    assert f is not None
    assert lyr.SetFeature(f) == ogr.OGRERR_NONE
    f = None

    f = ogr.Feature(lyr.GetLayerDefn())
    f['fid'] = 1234567890124
    f.SetFID(1234567890124)
    assert lyr.CreateFeature(f) == ogr.OGRERR_NONE
    assert f.GetFID() == 1234567890124
    f = lyr.GetFeature(f.GetFID())
    assert f is not None
    assert lyr.SetFeature(f) == ogr.OGRERR_NONE
    f = None

    f = ogr.Feature(lyr.GetLayerDefn())
    f['fid'] = 1234567890123.5
    with gdaltest.error_handler():
        assert lyr.CreateFeature(f) == ogr.OGRERR_FAILURE

    f = ogr.Feature(lyr.GetLayerDefn())
    f['fid'] = 1234567890125
    f.SetFID(1)
    with gdaltest.error_handler():
        assert lyr.CreateFeature(f) == ogr.OGRERR_FAILURE

    ds = None
    gdal.Unlink(filename)

###############################################################################
# Test opening a corrupted gpkg with duplicated layer names


def test_ogr_gpkg_57():

    if gdaltest.gpkg_dr.GetMetadataItem("ENABLE_SQL_GPKG_FORMAT") != 'YES':
        pytest.skip()

    tmpfile = '/vsimem/tmp.gpkg.txt'
    gdal.FileFromMemBuffer(tmpfile,
                           """-- SQL GPKG
CREATE TABLE gpkg_spatial_ref_sys (srs_name,srs_id,organization,organization_coordsys_id,definition,description);
INSERT INTO "gpkg_spatial_ref_sys" VALUES('',0,'NONE',0,'undefined','');
CREATE TABLE gpkg_contents (table_name,data_type,identifier,description,last_change,min_x, min_y,max_x, max_y,srs_id);
INSERT INTO "gpkg_contents" VALUES('poly','features','poly','','',NULL,NULL,NULL,NULL,0);
INSERT INTO "gpkg_contents" VALUES('poly','features','poly','','',NULL,NULL,NULL,NULL,0);
CREATE TABLE gpkg_geometry_columns (table_name,column_name,geometry_type_name,srs_id,z,m);
INSERT INTO "gpkg_geometry_columns" VALUES('poly','geom','POLYGON',0,0,0);
CREATE TABLE "poly"("fid" INTEGER PRIMARY KEY, "geom" POLYGON);
""")

    with gdaltest.error_handler():
        ds = ogr.Open(tmpfile)
    assert ds.GetLayerCount() == 1, 'bad layer count'
    assert gdal.GetLastErrorMsg().find('Table poly appearing several times') >= 0, \
        'should NOT have warned'
    ds = None

    gdal.Unlink(tmpfile)

###############################################################################
# Test overwriting a layer


def test_ogr_gpkg_58():

    out_filename = '/vsimem/ogr_gpkg_58.gpkg'
    gdal.VectorTranslate(out_filename, 'data/poly.shp', format='GPKG')
    gdal.VectorTranslate(out_filename, 'data/poly.shp', format='GPKG',
                         accessMode='overwrite')

    ds = ogr.Open(out_filename)
    sql_lyr = ds.ExecuteSQL("SELECT HasSpatialIndex('poly', 'geom')")
    f = sql_lyr.GetNextFeature()
    assert f.GetField(0) == 1
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    gdal.Unlink(out_filename)

###############################################################################
# Test CreateSpatialIndex()


def test_ogr_gpkg_59():

    out_filename = '/vsimem/ogr_gpkg_59.gpkg'
    gdal.VectorTranslate(out_filename, 'data/poly.shp', format='GPKG',
                         layerCreationOptions=['SPATIAL_INDEX=NO'])

    ds = ogr.Open(out_filename, update=1)
    sql_lyr = ds.ExecuteSQL("SELECT CreateSpatialIndex('poly', 'geom')")
    f = sql_lyr.GetNextFeature()
    assert f.GetField(0) == 1
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    gdal.Unlink(out_filename)

###############################################################################
# Test savepoints


def test_ogr_gpkg_savepoint():

    filename = '/vsimem/ogr_gpkg_savepoint.gpkg'
    ds = gdaltest.gpkg_dr.CreateDataSource(filename)
    lyr = ds.CreateLayer('foo')
    lyr.CreateField(ogr.FieldDefn('str', ogr.OFTString))
    f = ogr.Feature(lyr.GetLayerDefn())
    f['str'] = 'foo'
    lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open(filename, update=1)
    lyr = ds.GetLayer(0)
    ds.StartTransaction()
    ds.ExecuteSQL('SAVEPOINT pt')
    lyr.DeleteFeature(1)
    ds.ExecuteSQL('ROLLBACK TO SAVEPOINT pt')
    f = ogr.Feature(lyr.GetLayerDefn())
    f['str'] = 'bar'
    lyr.CreateFeature(f)
    ds.CommitTransaction()
    ds = None

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 2
    ds = None

    gdal.Unlink(filename)

###############################################################################
# Test that we don't open file handles behind the back of sqlite3


def test_ogr_gpkg_wal():

    import test_cli_utilities
    if test_cli_utilities.get_ogrinfo_path() is None:
        pytest.skip()

    # needs to be a real file
    filename = 'tmp/ogr_gpkg_wal.gpkg'

    with gdaltest.config_option('OGR_SQLITE_JOURNAL', 'WAL'):
        ds = gdaltest.gpkg_dr.CreateDataSource(filename)
    ds.CreateLayer('foo')
    ds = None

    ds = ogr.Open(filename, update=1)
    os.stat(filename + '-wal')

    # Re-open in read-only mode
    ds_ro = ogr.Open(filename)
    ds_ro.GetName()
    os.stat(filename + '-wal')

    # Test external process to read the file
    gdaltest.runexternal(test_cli_utilities.get_ogrinfo_path() + ' ' + filename)

    # The file must still exist
    os.stat(filename + '-wal')

    ds = None
    ds_ro = None

    gdal.Unlink(filename)
    gdal.Unlink(filename + '-wal')
    gdal.Unlink(filename + '-shm')


###############################################################################
# Run test_ogrsf


def test_ogr_gpkg_test_ogrsf():

    # Do integrity check first
    gpkg_ds = ogr.Open('tmp/gpkg_test.gpkg')
    sql_lyr = gpkg_ds.ExecuteSQL("PRAGMA integrity_check")
    feat = sql_lyr.GetNextFeature()
    assert feat.GetField(0) == 'ok', 'integrity check failed'
    gpkg_ds.ReleaseResultSet(sql_lyr)

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    gpkg_ds = None
    # sys.exit(0)
    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' tmp/gpkg_test.gpkg --config OGR_SQLITE_SYNCHRONOUS OFF')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' tmp/gpkg_test.gpkg -sql "select * from tbl_linestring_renamed" --config OGR_SQLITE_SYNCHRONOUS OFF')

    assert ret.find('INFO') != -1 and ret.find('ERROR') == -1


###############################################################################
# Test JSon subtype support


def test_ogr_gpkg_json():

    filename = '/vsimem/ogr_gpkg_json.gpkg'
    ds = gdaltest.gpkg_dr.CreateDataSource(filename)
    lyr = ds.CreateLayer('test')

    fld_defn = ogr.FieldDefn('test_json', ogr.OFTString)
    fld_defn.SetSubType(ogr.OFSTJSON)
    lyr.CreateField(fld_defn)
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetSubType() == ogr.OFSTJSON

    ds.ReleaseResultSet(ds.ExecuteSQL('SELECT 1 FROM test')) # will crystalize

    fld_defn = ogr.FieldDefn('test2_json', ogr.OFTString)
    fld_defn.SetSubType(ogr.OFSTJSON)
    lyr.CreateField(fld_defn)
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetSubType() == ogr.OFSTJSON

    fld_defn = ogr.FieldDefn('test_string', ogr.OFTString)
    lyr.CreateField(fld_defn)

    ds = None

    ds = ogr.Open(filename, update = 1)
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetSubType() == ogr.OFSTJSON
    assert lyr.GetLayerDefn().GetFieldDefn(1).GetSubType() == ogr.OFSTJSON

    # Demote field from JSON
    new_defn = ogr.FieldDefn('test_was_json_now_string', ogr.OFTString)
    assert lyr.AlterFieldDefn(lyr.GetLayerDefn().GetFieldIndex('test2_json'), new_defn, ogr.ALTER_ALL_FLAG) == 0

    # Alter field to JSON
    new_defn = ogr.FieldDefn('test_was_string_now_json', ogr.OFTString)
    new_defn.SetSubType(ogr.OFSTJSON)
    assert lyr.AlterFieldDefn(lyr.GetLayerDefn().GetFieldIndex('test_string'), new_defn, ogr.ALTER_ALL_FLAG) == 0

    # Delete JSON field
    assert lyr.DeleteField(lyr.GetLayerDefn().GetFieldIndex('test_json')) == 0

    ds = None

    assert validate(filename), 'validation failed'

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    assert (lyr.GetLayerDefn().GetFieldDefn(
        lyr.GetLayerDefn().GetFieldIndex('test_was_json_now_string')).GetSubType() == ogr.OFSTNone)
    assert (lyr.GetLayerDefn().GetFieldDefn(
        lyr.GetLayerDefn().GetFieldIndex('test_was_string_now_json')).GetSubType() == ogr.OFSTJSON)

    sql_lyr = ds.ExecuteSQL("SELECT 1 FROM gpkg_data_columns WHERE table_name = 'test'")
    fc = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    assert fc == 1

    ds = None

    gdal.Unlink(filename)

###############################################################################
# Test invalid/non-standard content in records


def test_ogr_gpkg_invalid_values_in_records():

    filename = '/vsimem/test_ogr_gpkg_invalid_date_content.gpkg'
    ds = gdaltest.gpkg_dr.CreateDataSource(filename)
    lyr = ds.CreateLayer('test')

    fld_defn = ogr.FieldDefn('dt', ogr.OFTDateTime)
    lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn('d', ogr.OFTDate)
    lyr.CreateField(fld_defn)

    for i in range(6):
        f = ogr.Feature(lyr.GetLayerDefn())
        lyr.CreateFeature(f)
    ds.ExecuteSQL("UPDATE test SET dt = 'foo' WHERE fid = 1")
    ds.ExecuteSQL("UPDATE test SET d = 'bar' WHERE fid = 2")
    ds.ExecuteSQL("UPDATE test SET dt = 3 WHERE fid = 3")
    ds.ExecuteSQL("UPDATE test SET d = 4 WHERE fid = 4")
    ds.ExecuteSQL("UPDATE test SET dt = '2020/01/21 12:34:56+01' WHERE fid = 5")
    ds.ExecuteSQL("UPDATE test SET d = '2020/01/21' WHERE fid = 6")

    lyr.ResetReading()

    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = lyr.GetNextFeature()
    assert gdal.GetLastErrorMsg() == 'Invalid content for record 1 in column dt: foo'
    assert not f.IsFieldSet('dt')

    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = lyr.GetNextFeature()
    assert gdal.GetLastErrorMsg() == 'Invalid content for record 2 in column d: bar'
    assert not f.IsFieldSet('d')

    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = lyr.GetNextFeature()
    assert gdal.GetLastErrorMsg() == 'Unexpected data type for record 3 in column dt'
    assert not f.IsFieldSet('dt')

    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = lyr.GetNextFeature()
    assert gdal.GetLastErrorMsg() == 'Unexpected data type for record 4 in column d'
    assert not f.IsFieldSet('d')

    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = lyr.GetNextFeature()
    assert gdal.GetLastErrorMsg() == 'Non-conformant content for record 5 in column dt, 2020/01/21 12:34:56+01, successfully parsed'
    assert f.IsFieldSet('dt')
    assert f['dt'] == '2020/01/21 12:34:56+01'

    gdal.ErrorReset()
    with gdaltest.error_handler():
        f = lyr.GetNextFeature()
    assert gdal.GetLastErrorMsg() == 'Non-conformant content for record 6 in column d, 2020/01/21, successfully parsed'
    assert f.IsFieldSet('d')
    assert f['d'] == '2020/01/21'

    ds = None

    gdal.Unlink(filename)

###############################################################################
# Test creating a table with layer geometry type unknown/GEOMETRY and
# geometries of mixed dimensionality


def test_ogr_gpkg_mixed_dimensionality_unknown_layer_geometry_type():

    filename = '/vsimem/test_ogr_gpkg_mixed_dimensionality_unknown_layer_geometry_type.gpkg'
    ds = gdaltest.gpkg_dr.CreateDataSource(filename)
    lyr = ds.CreateLayer('test')

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (1 2)'))
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (1 2 3)'))
    lyr.CreateFeature(f)

    ds = None

    assert validate(filename), 'validation failed'

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    assert lyr.GetGeomType() == ogr.wkbUnknown

    sql_lyr = ds.ExecuteSQL('SELECT z FROM gpkg_geometry_columns')
    f = sql_lyr.GetNextFeature()
    assert f.GetField(0) == 2
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    gdal.Unlink(filename)


###############################################################################
# Test fixing up wrong RTree update3 trigger from GeoPackage < 1.2.1

def test_ogr_gpkg_fixup_wrong_rtree_trigger():

    filename = '/vsimem/test_ogr_gpkg_fixup_wrong_rtree_trigger.gpkg'
    ds = ogr.GetDriverByName('GPKG').CreateDataSource(filename)
    ds.CreateLayer('test')
    ds.CreateLayer('test2')
    ds = None
    with gdaltest.error_handler():
        ds = ogr.Open(filename, update = 1)
        # inject wrong trigger on purpose with the wrong 'OF "geometry" ' part
        ds.ExecuteSQL('DROP TRIGGER rtree_test_geometry_update3')
        wrong_trigger = 'CREATE TRIGGER "rtree_test_geometry_update3" AFTER UPDATE OF "geometry" ON "test" WHEN OLD."fid" != NEW."fid" AND (NEW."geometry" NOTNULL AND NOT ST_IsEmpty(NEW."geometry")) BEGIN DELETE FROM "rtree_test_geometry" WHERE id = OLD."fid"; INSERT OR REPLACE INTO "rtree_test_geometry" VALUES (NEW."fid",ST_MinX(NEW."geometry"), ST_MaxX(NEW."geometry"),ST_MinY(NEW."geometry"), ST_MaxY(NEW."geometry")); END'
        ds.ExecuteSQL(wrong_trigger)

        ds.ExecuteSQL('DROP TRIGGER rtree_test2_geometry_update3')
        # Test another potential variant (although not generated by OGR)
        wrong_trigger2 = 'CREATE TRIGGER "rtree_test2_geometry_update3" AFTER UPDATE OF   geometry    ON test2 WHEN OLD."fid" != NEW."fid" AND (NEW."geometry" NOTNULL AND NOT ST_IsEmpty(NEW."geometry")) BEGIN DELETE FROM "rtree_test_geometry" WHERE id = OLD."fid"; INSERT OR REPLACE INTO "rtree_test_geometry" VALUES (NEW."fid",ST_MinX(NEW."geometry"), ST_MaxX(NEW."geometry"),ST_MinY(NEW."geometry"), ST_MaxY(NEW."geometry")); END'
        ds.ExecuteSQL(wrong_trigger2)

        ds = None

    # Open in read-only mode
    ds = ogr.Open(filename)
    sql_lyr = ds.ExecuteSQL("SELECT sql FROM sqlite_master WHERE type = 'trigger' AND name = 'rtree_test_geometry_update3'")
    f = sql_lyr.GetNextFeature()
    sql = f['sql']
    ds.ReleaseResultSet(sql_lyr)
    ds = None
    assert sql == wrong_trigger

    # Open in update mode
    ds = ogr.Open(filename, update = 1)
    sql_lyr = ds.ExecuteSQL("SELECT sql FROM sqlite_master WHERE type = 'trigger' AND name = 'rtree_test_geometry_update3'")
    f = sql_lyr.GetNextFeature()
    sql = f['sql']
    ds.ReleaseResultSet(sql_lyr)
    sql_lyr = ds.ExecuteSQL("SELECT sql FROM sqlite_master WHERE type = 'trigger' AND name = 'rtree_test2_geometry_update3'")
    f = sql_lyr.GetNextFeature()
    sql2 = f['sql']
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    gdal.Unlink(filename)
    assert sql == 'CREATE TRIGGER "rtree_test_geometry_update3" AFTER UPDATE ON "test" WHEN OLD."fid" != NEW."fid" AND (NEW."geometry" NOTNULL AND NOT ST_IsEmpty(NEW."geometry")) BEGIN DELETE FROM "rtree_test_geometry" WHERE id = OLD."fid"; INSERT OR REPLACE INTO "rtree_test_geometry" VALUES (NEW."fid",ST_MinX(NEW."geometry"), ST_MaxX(NEW."geometry"),ST_MinY(NEW."geometry"), ST_MaxY(NEW."geometry")); END'
    assert sql2 == 'CREATE TRIGGER "rtree_test2_geometry_update3" AFTER UPDATE    ON test2 WHEN OLD."fid" != NEW."fid" AND (NEW."geometry" NOTNULL AND NOT ST_IsEmpty(NEW."geometry")) BEGIN DELETE FROM "rtree_test_geometry" WHERE id = OLD."fid"; INSERT OR REPLACE INTO "rtree_test_geometry" VALUES (NEW."fid",ST_MinX(NEW."geometry"), ST_MaxX(NEW."geometry"),ST_MinY(NEW."geometry"), ST_MaxY(NEW."geometry")); END'


###############################################################################
# Test PRELUDE_STATEMENTS open option


def test_ogr_gpkg_prelude_statements():

    gdal.VectorTranslate('/vsimem/test.gpkg', 'data/poly.shp', format='GPKG')
    ds = gdal.OpenEx('/vsimem/test.gpkg',
                     open_options=["PRELUDE_STATEMENTS=ATTACH DATABASE '/vsimem/test.gpkg' AS other"])
    sql_lyr = ds.ExecuteSQL('SELECT * FROM poly JOIN other.poly USING (eas_id)')
    assert sql_lyr.GetFeatureCount() == 10
    ds.ReleaseResultSet(sql_lyr)
    gdal.Unlink('/vsimem/test.gpkg')

###############################################################################
# Test DATETIME_FORMAT


def test_ogr_gpkg_datetime_timezones():

    filename = '/vsimem/test_ogr_gpkg_datetime_timezones.gpkg'
    ds = gdaltest.gpkg_dr.CreateDataSource(filename, options = ['DATETIME_FORMAT=UTC'])
    lyr = ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn('dt', ogr.OFTDateTime))
    for val in ['2020/01/01 01:34:56', '2020/01/01 01:34:56+00', '2020/01/01 01:34:56.789+02']:
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField('dt', val)
        lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open(filename)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f.GetField('dt') == '2020/01/01 01:34:56+00'
    f = lyr.GetNextFeature()
    assert f.GetField('dt') == '2020/01/01 01:34:56+00'
    f = lyr.GetNextFeature()
    assert f.GetField('dt') == '2019/12/31 23:34:56.789+00'

    sql_lyr = ds.ExecuteSQL("SELECT dt || '' FROM test")
    f = sql_lyr.GetNextFeature()
    # check that milliseconds are written to be strictly compliant with the GPKG spec
    assert f.GetField(0) == '2020-01-01T01:34:56.000Z'
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    gdal.Unlink(filename)


###############################################################################
# Test AbortSQL

def test_abort_sql():

    filename = 'data/gpkg/poly_non_conformant.gpkg'
    ds = ogr.Open(filename)

    def abortAfterDelay():
        #print("Aborting SQL...")
        assert ds.AbortSQL() == ogr.OGRERR_NONE

    t = threading.Timer(0.5, abortAfterDelay)
    t.start()

    start = time.time()

    # Long running query
    sql = """
        WITH RECURSIVE r(i) AS (
            VALUES(0)
            UNION ALL
            SELECT i FROM r
            LIMIT 100000000
            )
        SELECT i FROM r WHERE i = 1;"""

    with gdaltest.error_handler():
        ds.ExecuteSQL(sql)

    end = time.time()
    assert int(end - start) < 1

    # Same test with a GDAL dataset
    ds2 = gdal.OpenEx(filename, gdal.OF_VECTOR)

    def abortAfterDelay2():
        #print("Aborting SQL...")
        assert ds2.AbortSQL() == ogr.OGRERR_NONE

    t = threading.Timer(0.5, abortAfterDelay2)
    t.start()

    start = time.time()

    # Long running query
    with gdaltest.error_handler():
        ds2.ExecuteSQL(sql)

    end = time.time()
    assert int(end - start) < 1


###############################################################################
# Test ST_Transform() with no record in gpkg_spatial_ref_sys and thus we
# fallback to EPSG

@pytest.mark.skipif(sys.platform == 'win32',
                    reason='f.GetGeometryRef() returns None on the current Windows CI')
def test_ogr_gpkg_st_transform_no_record_spatial_ref_sys():

    ds = ogr.GetDriverByName('GPKG').CreateDataSource('/vsimem/test.gpkg')
    lyr = ds.CreateLayer('test')
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (500000 0)'))
    lyr.CreateFeature(f)
    f = None

    if not _has_spatialite_4_3_or_later(ds):
        ds = None
        gdal.Unlink('/vsimem/test.gpkg')
        pytest.skip('Spatialite missing or too old')

    sql_lyr = ds.ExecuteSQL('SELECT ST_Transform(SetSRID(geom, 32631), 32731) FROM test')
    # Fails on a number of configs
    # assert sql_lyr.GetSpatialRef().GetAuthorityCode(None) == '32731'
    f = sql_lyr.GetNextFeature()
    assert f.GetGeometryRef().ExportToWkt() == 'POINT (500000 10000000)'
    f = None
    ds.ReleaseResultSet(sql_lyr)

    ds = None
    gdal.Unlink('/vsimem/test.gpkg')


###############################################################################
# Test deferred spatial index creation


def test_ogr_gpkg_deferred_spi_creation():

    def has_spi(ds):
        sql_lyr = ds.ExecuteSQL("SELECT 1 FROM sqlite_master WHERE name = 'rtree_test_geom'", dialect='DEBUG')
        res = sql_lyr.GetNextFeature() is not None
        ds.ReleaseResultSet(sql_lyr)
        return res

    ds = ogr.GetDriverByName('GPKG').CreateDataSource('/vsimem/test.gpkg')

    lyr = ds.CreateLayer('test')
    assert not has_spi(ds)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (0 0)'))
    lyr.CreateFeature(f)
    fid = f.GetFID()
    f = None
    assert not has_spi(ds)

    lyr.ResetReading()
    assert lyr.GetNextFeature() is not None
    assert not has_spi(ds)

    assert lyr.GetFeature(fid) is not None
    assert not has_spi(ds)

    assert lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString)) == ogr.OGRERR_NONE
    assert not has_spi(ds)

    assert lyr.DeleteField(0) == ogr.OGRERR_NONE
    assert not has_spi(ds)

    ds.ReleaseResultSet(ds.ExecuteSQL('SELECT 1'))
    assert has_spi(ds)

    # GetNextFeature() with spatial filter should cause SPI creation
    ds.ExecuteSQL('DELLAYER:test')
    lyr = ds.CreateLayer('test')
    assert not has_spi(ds)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (0 0)'))
    lyr.CreateFeature(f)
    fid = f.GetFID()
    f = None
    assert not has_spi(ds)

    lyr.SetSpatialFilterRect(-1,-1,1,1)

    lyr.ResetReading()
    assert lyr.GetNextFeature() is not None
    assert has_spi(ds)

    ds = None
    gdal.Unlink('/vsimem/test.gpkg')


###############################################################################
# Test deferred spatial index update


def test_ogr_gpkg_deferred_spi_update():

    def has_spi_triggers(ds):
        sql_lyr = ds.ExecuteSQL("SELECT * FROM sqlite_master WHERE type = 'trigger' AND name LIKE 'rtree_test_geom%'", dialect='DEBUG')
        res = sql_lyr.GetFeatureCount()
        ds.ReleaseResultSet(sql_lyr)
        return res == 6

    filename = '/vsimem/test.gpkg'

    # Basic test
    ds = ogr.GetDriverByName('GPKG').CreateDataSource(filename)
    ds.CreateLayer('test')
    ds = None
    with gdaltest.config_option('OGR_GPKG_DEFERRED_SPI_UPDATE_THRESHOLD', '2'):
        ds = ogr.Open(filename, update = 1)
        lyr = ds.GetLayer(0)

        ds.StartTransaction()

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (0 0)'))
        lyr.CreateFeature(f)
        assert has_spi_triggers(ds)

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (1 1)'))
        lyr.CreateFeature(f)
        assert not has_spi_triggers(ds)

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (2 2)'))
        lyr.CreateFeature(f)
        assert not has_spi_triggers(ds)

        sql_lyr = ds.ExecuteSQL("SELECT * FROM rtree_test_geom", dialect='DEBUG')
        res = sql_lyr.GetFeatureCount()
        ds.ReleaseResultSet(sql_lyr)
        assert res == 2

        ds.CommitTransaction()
        assert has_spi_triggers(ds)

        sql_lyr = ds.ExecuteSQL("SELECT * FROM rtree_test_geom", dialect='DEBUG')
        res = sql_lyr.GetFeatureCount()
        ds.ReleaseResultSet(sql_lyr)
        assert res == 3

        ds = None

    # Check effect of RollbackTransaction()
    ds = ogr.GetDriverByName('GPKG').CreateDataSource(filename)
    ds.CreateLayer('test')
    ds = None
    with gdaltest.config_option('OGR_GPKG_DEFERRED_SPI_UPDATE_THRESHOLD', '1'):
        ds = ogr.Open(filename, update = 1)
        lyr = ds.GetLayer(0)

        ds.StartTransaction()

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (0 0)'))
        lyr.CreateFeature(f)
        assert not has_spi_triggers(ds)

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (1 1)'))
        lyr.CreateFeature(f)
        assert not has_spi_triggers(ds)

        sql_lyr = ds.ExecuteSQL("SELECT * FROM rtree_test_geom", dialect='DEBUG')
        res = sql_lyr.GetFeatureCount()
        ds.ReleaseResultSet(sql_lyr)
        assert res == 1

        ds.RollbackTransaction()
        assert has_spi_triggers(ds)

        sql_lyr = ds.ExecuteSQL("SELECT * FROM rtree_test_geom", dialect='DEBUG')
        res = sql_lyr.GetFeatureCount()
        ds.ReleaseResultSet(sql_lyr)
        assert res == 0

        ds = None

    # Check that GetNextFeature() with a spatial filter causes flushing of
    # deferred SPI values
    ds = ogr.GetDriverByName('GPKG').CreateDataSource(filename)
    ds.CreateLayer('test')
    ds = None
    with gdaltest.config_option('OGR_GPKG_DEFERRED_SPI_UPDATE_THRESHOLD', '1'):
        ds = ogr.Open(filename, update = 1)
        lyr = ds.GetLayer(0)

        ds.StartTransaction()

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (0 0)'))
        lyr.CreateFeature(f)
        assert not has_spi_triggers(ds)

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (1 1)'))
        lyr.CreateFeature(f)

        lyr.SetSpatialFilterRect(0,0,1,1)
        lyr.ResetReading()
        assert lyr.GetNextFeature() is not None
        assert has_spi_triggers(ds)
        assert lyr.GetNextFeature() is not None
        assert lyr.GetNextFeature() is None
        ds = None

    gdal.Unlink('/vsimem/test.gpkg')


###############################################################################
# Test field domains


def test_ogr_gpkg_field_domains():

    filename = '/vsimem/test.gpkg'

    # Test write support
    ds = gdal.GetDriverByName('GPKG').Create(filename, 0, 0, 0, gdal.GDT_Unknown)

    assert ds.TestCapability(ogr.ODsCAddFieldDomain)

    assert ds.GetFieldDomain('does_not_exist') is None

    assert ds.AddFieldDomain(ogr.CreateRangeFieldDomain('range_domain_int', 'my desc', ogr.OFTInteger, ogr.OFSTNone, 1, True, 2, False))
    assert ds.GetFieldDomain('range_domain_int') is not None
    assert not ds.AddFieldDomain(ogr.CreateRangeFieldDomain('range_domain_int', 'my desc', ogr.OFTInteger, ogr.OFSTNone, 1, True, 2, True))

    assert ds.AddFieldDomain(ogr.CreateRangeFieldDomain('range_domain_int64', '', ogr.OFTInteger64, ogr.OFSTNone, -1234567890123, False, 1234567890123, True))
    assert ds.GetFieldDomain('range_domain_int64') is not None

    assert ds.AddFieldDomain(ogr.CreateRangeFieldDomain('range_domain_real', '', ogr.OFTReal, ogr.OFSTNone, 1.5, True, 2.5, True))
    assert ds.GetFieldDomain('range_domain_real') is not None

    assert ds.AddFieldDomain(ogr.CreateRangeFieldDomain('range_domain_real_inf', '', ogr.OFTReal, ogr.OFSTNone, -math.inf, True, math.inf, True))
    assert ds.GetFieldDomain('range_domain_real_inf') is not None

    assert ds.AddFieldDomain(ogr.CreateGlobFieldDomain('glob_domain', 'my desc', ogr.OFTString, ogr.OFSTNone, '*'))
    assert ds.GetFieldDomain('glob_domain') is not None

    assert ds.AddFieldDomain(ogr.CreateCodedFieldDomain('enum_domain', '', ogr.OFTInteger64, ogr.OFSTNone, {1: "one", "2": None}))
    assert ds.GetFieldDomain('enum_domain') is not None

    assert ds.AddFieldDomain(ogr.CreateCodedFieldDomain('enum_domain_guess_int_single', 'my desc', ogr.OFTInteger, ogr.OFSTNone, {1: "one"}))
    assert ds.AddFieldDomain(ogr.CreateCodedFieldDomain('enum_domain_guess_int', '', ogr.OFTInteger, ogr.OFSTNone, {1: "one", 2: "two"}))
    assert ds.AddFieldDomain(ogr.CreateCodedFieldDomain('enum_domain_guess_int64_single_1', '', ogr.OFTInteger64, ogr.OFSTNone, { 1234567890123: "1234567890123"}))
    assert ds.AddFieldDomain(ogr.CreateCodedFieldDomain('enum_domain_guess_int64_single_2', '', ogr.OFTInteger64, ogr.OFSTNone, { -1234567890123: "-1234567890123"}))
    assert ds.AddFieldDomain(ogr.CreateCodedFieldDomain('enum_domain_guess_int64', '', ogr.OFTInteger64, ogr.OFSTNone, {1: "one", 1234567890123: "1234567890123", 3: "three"}))
    assert ds.AddFieldDomain(ogr.CreateCodedFieldDomain('enum_domain_guess_real_single', '', ogr.OFTReal, ogr.OFSTNone, {1.5: "one dot five"}))
    assert ds.AddFieldDomain(ogr.CreateCodedFieldDomain('enum_domain_guess_real', '', ogr.OFTReal, ogr.OFSTNone, {1: "one", 1.5: "one dot five", 1234567890123: "1234567890123", 3: "three"}))
    assert ds.AddFieldDomain(ogr.CreateCodedFieldDomain('enum_domain_guess_string_single', '', ogr.OFTString, ogr.OFSTNone, {"three": "three"}))
    assert ds.AddFieldDomain(ogr.CreateCodedFieldDomain('enum_domain_guess_string', '', ogr.OFTString, ogr.OFSTNone, {1: "one", 1.5: "one dot five", "three": "three", 4: "four"}))

    lyr = ds.CreateLayer('test')

    fld_defn = ogr.FieldDefn('with_range_domain_int', ogr.OFTInteger)
    fld_defn.SetDomainName('range_domain_int')
    lyr.CreateField(fld_defn)

    fld_defn = ogr.FieldDefn('with_range_domain_int64', ogr.OFTInteger64)
    fld_defn.SetDomainName('range_domain_int64')
    lyr.CreateField(fld_defn)

    fld_defn = ogr.FieldDefn('with_range_domain_real', ogr.OFTReal)
    fld_defn.SetDomainName('range_domain_real')
    lyr.CreateField(fld_defn)

    fld_defn = ogr.FieldDefn('with_glob_domain', ogr.OFTString)
    fld_defn.SetDomainName('glob_domain')
    lyr.CreateField(fld_defn)

    fld_defn = ogr.FieldDefn('with_enum_domain', ogr.OFTInteger64)
    fld_defn.SetDomainName('enum_domain')
    lyr.CreateField(fld_defn)

    fld_defn = ogr.FieldDefn('without_domain_initially', ogr.OFTInteger)
    lyr.CreateField(fld_defn)

    ds = None

    assert validate(filename), 'validation failed'

    # Test read support
    ds = gdal.OpenEx(filename, gdal.OF_VECTOR)

    sql_lyr = ds.ExecuteSQL('SELECT * FROM gpkg_data_column_constraints')
    assert sql_lyr is not None
    ds.ReleaseResultSet(sql_lyr)

    domain = ds.GetFieldDomain('range_domain_int')
    assert domain is not None
    assert domain.GetName() == 'range_domain_int'
    assert domain.GetDescription() == 'my desc'
    assert domain.GetDomainType() == ogr.OFDT_RANGE
    assert domain.GetFieldType() == ogr.OFTInteger
    assert domain.GetMinAsDouble() == 1.0
    assert domain.IsMinInclusive()
    assert domain.GetMaxAsDouble() == 2.0
    assert not domain.IsMaxInclusive()

    domain = ds.GetFieldDomain('range_domain_int64')
    assert domain is not None
    assert domain.GetName() == 'range_domain_int64'
    assert domain.GetDescription() == ''
    assert domain.GetDomainType() == ogr.OFDT_RANGE
    assert domain.GetFieldType() == ogr.OFTInteger64
    assert domain.GetMinAsDouble() == -1234567890123
    assert not domain.IsMinInclusive()
    assert domain.GetMaxAsDouble() == 1234567890123
    assert domain.IsMaxInclusive()

    domain = ds.GetFieldDomain('range_domain_real')
    assert domain is not None
    assert domain.GetName() == 'range_domain_real'
    assert domain.GetDescription() == ''
    assert domain.GetDomainType() == ogr.OFDT_RANGE
    assert domain.GetFieldType() == ogr.OFTReal
    assert domain.GetMinAsDouble() == 1.5
    assert domain.IsMinInclusive()
    assert domain.GetMaxAsDouble() == 2.5
    assert domain.IsMaxInclusive()

    domain = ds.GetFieldDomain('range_domain_real_inf')
    assert domain is not None
    assert domain.GetName() == 'range_domain_real_inf'
    assert domain.GetDescription() == ''
    assert domain.GetDomainType() == ogr.OFDT_RANGE
    assert domain.GetFieldType() == ogr.OFTReal
    assert domain.GetMinAsDouble() == -math.inf
    assert domain.IsMinInclusive()
    assert domain.GetMaxAsDouble() == math.inf
    assert domain.IsMaxInclusive()

    domain = ds.GetFieldDomain('glob_domain')
    assert domain is not None
    assert domain.GetName() == 'glob_domain'
    assert domain.GetDescription() == 'my desc'
    assert domain.GetDomainType() == ogr.OFDT_GLOB
    assert domain.GetFieldType() == ogr.OFTString
    assert domain.GetGlob() == '*'

    domain = ds.GetFieldDomain('enum_domain')
    assert domain is not None
    assert domain.GetName() == 'enum_domain'
    assert domain.GetDescription() == ''
    assert domain.GetDomainType() == ogr.OFDT_CODED
    assert domain.GetFieldType() == ogr.OFTInteger64
    assert domain.GetEnumeration() == { "1": "one", "2": None }

    domain = ds.GetFieldDomain('enum_domain_guess_int_single')
    assert domain.GetDescription() == 'my desc'
    assert domain.GetFieldType() == ogr.OFTInteger

    domain = ds.GetFieldDomain('enum_domain_guess_int')
    assert domain.GetFieldType() == ogr.OFTInteger

    domain = ds.GetFieldDomain('enum_domain_guess_int64_single_1')
    assert domain.GetFieldType() == ogr.OFTInteger64

    domain = ds.GetFieldDomain('enum_domain_guess_int64_single_2')
    assert domain.GetFieldType() == ogr.OFTInteger64

    domain = ds.GetFieldDomain('enum_domain_guess_int64')
    assert domain.GetFieldType() == ogr.OFTInteger64

    domain = ds.GetFieldDomain('enum_domain_guess_real_single')
    assert domain.GetFieldType() == ogr.OFTReal

    domain = ds.GetFieldDomain('enum_domain_guess_real')
    assert domain.GetFieldType() == ogr.OFTReal

    domain = ds.GetFieldDomain('enum_domain_guess_string_single')
    assert domain.GetFieldType() == ogr.OFTString

    domain = ds.GetFieldDomain('enum_domain_guess_string')
    assert domain.GetFieldType() == ogr.OFTString

    lyr = ds.GetLayerByName('test')
    lyr_defn = lyr.GetLayerDefn()
    fld_defn = lyr_defn.GetFieldDefn(lyr_defn.GetFieldIndex('with_range_domain_int'))
    assert fld_defn.GetDomainName() == 'range_domain_int'

    ds = None

    # Test AlterFieldDefn() support
    ds = gdal.OpenEx(filename, gdal.OF_VECTOR | gdal.OF_UPDATE)
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()

    # Unset domain name
    idx = lyr_defn.GetFieldIndex('with_range_domain_int')
    fld_defn = lyr_defn.GetFieldDefn(idx)
    fld_defn = ogr.FieldDefn(fld_defn.GetName(), fld_defn.GetType())
    fld_defn.SetDomainName('')
    assert lyr.AlterFieldDefn(idx, fld_defn, ogr.ALTER_ALL_FLAG) == 0

    # Change domain name
    idx = lyr_defn.GetFieldIndex('with_range_domain_int64')
    fld_defn = lyr_defn.GetFieldDefn(idx)
    fld_defn = ogr.FieldDefn(fld_defn.GetName(), fld_defn.GetType())
    fld_defn.SetDomainName('with_enum_domain')
    assert lyr.AlterFieldDefn(idx, fld_defn, ogr.ALTER_ALL_FLAG) == 0

    # Set domain name
    idx = lyr_defn.GetFieldIndex('without_domain_initially')
    fld_defn = lyr_defn.GetFieldDefn(idx)
    fld_defn = ogr.FieldDefn(fld_defn.GetName(), fld_defn.GetType())
    fld_defn.SetDomainName('range_domain_int')
    assert lyr.AlterFieldDefn(idx, fld_defn, ogr.ALTER_ALL_FLAG) == 0

    # Don't change anything
    idx = lyr_defn.GetFieldIndex('with_glob_domain')
    fld_defn = lyr_defn.GetFieldDefn(idx)
    assert lyr.AlterFieldDefn(idx, fld_defn, ogr.ALTER_ALL_FLAG) == 0

    ds = None

    assert validate(filename), 'validation failed'

    # Test read support
    ds = gdal.OpenEx(filename, gdal.OF_VECTOR)
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()

    idx = lyr_defn.GetFieldIndex('with_range_domain_int')
    fld_defn = lyr_defn.GetFieldDefn(idx)
    assert fld_defn.GetDomainName() == ''

    idx = lyr_defn.GetFieldIndex('with_range_domain_int64')
    fld_defn = lyr_defn.GetFieldDefn(idx)
    assert fld_defn.GetDomainName() == 'with_enum_domain'

    idx = lyr_defn.GetFieldIndex('without_domain_initially')
    fld_defn = lyr_defn.GetFieldDefn(idx)
    assert fld_defn.GetDomainName() == 'range_domain_int'

    idx = lyr_defn.GetFieldIndex('with_glob_domain')
    fld_defn = lyr_defn.GetFieldDefn(idx)
    assert fld_defn.GetDomainName() == 'glob_domain'

    ds = None

    gdal.Unlink(filename)


###############################################################################
# Test error cases in field domains


def test_ogr_gpkg_field_domains_errors():

    filename = '/vsimem/test.gpkg'

    ds = gdal.GetDriverByName('GPKG').Create(filename, 0, 0, 0, gdal.GDT_Unknown)
    ds.CreateLayer('test')
    # The DDL lacks on purpose the NOT NULL constraints on constraint_name and constraint_type
    ds.ExecuteSQL("CREATE TABLE gpkg_data_column_constraints (" +
                  "constraint_name TEXT,constraint_type TEXT,value TEXT," +
                  "min NUMERIC,min_is_inclusive BOOLEAN," +
                  "max NUMERIC,max_is_inclusive BOOLEAN,description TEXT)")

    ds.ExecuteSQL("INSERT INTO gpkg_data_column_constraints VALUES "+
                  "('null_constraint_type', NULL, NULL, NULL, NULL, NULL, NULL, NULL)")

    ds.ExecuteSQL("INSERT INTO gpkg_data_column_constraints VALUES "+
                  "('invalid_constraint_type', 'invalid', NULL, NULL, NULL, NULL, NULL, NULL)")

    ds.ExecuteSQL("INSERT INTO gpkg_data_column_constraints VALUES "+
                  "('mix_glob_enum', 'glob', '*', NULL, NULL, NULL, NULL, NULL)")
    ds.ExecuteSQL("INSERT INTO gpkg_data_column_constraints VALUES "+
                  "('mix_glob_enum', 'enum', 'foo', NULL, NULL, NULL, NULL, 'bar')")

    ds.ExecuteSQL("INSERT INTO gpkg_data_column_constraints VALUES "+
                  "('null_in_enum_code', 'enum', NULL, NULL, NULL, NULL, NULL, 'bar')")

    ds.ExecuteSQL("INSERT INTO gpkg_data_column_constraints VALUES "+
                  "('null_in_glob_value', 'glob', NULL, NULL, NULL, NULL, NULL, NULL)")

    ds.ExecuteSQL("INSERT INTO gpkg_data_column_constraints VALUES "+
                  "('null_in_range', 'range', NULL, NULL, NULL, NULL, NULL, NULL)")
    ds = None

    ds = gdal.OpenEx(filename, gdal.OF_VECTOR)

    assert ds.GetFieldDomain('null_constraint_type') is None

    with gdaltest.error_handler():
        gdal.ErrorReset()
        assert ds.GetFieldDomain('invalid_constraint_type') is None
        assert gdal.GetLastErrorMsg() != ''

    with gdaltest.error_handler():
        gdal.ErrorReset()
        assert ds.GetFieldDomain('mix_glob_enum') is None
        assert gdal.GetLastErrorMsg() != ''

    with gdaltest.error_handler():
        gdal.ErrorReset()
        assert ds.GetFieldDomain('null_in_enum_code') is None
        assert gdal.GetLastErrorMsg() != ''

    with gdaltest.error_handler():
        gdal.ErrorReset()
        assert ds.GetFieldDomain('null_in_glob_value') is None
        assert gdal.GetLastErrorMsg() != ''

    # This is non conformant, but we accept it
    domain = ds.GetFieldDomain('null_in_range')
    assert domain is not None
    assert domain.GetMinAsDouble() == -math.inf
    assert domain.GetMaxAsDouble() == math.inf

    ds = None

    gdal.Unlink(filename)


###############################################################################
# Test attribute and spatial views


def test_ogr_gpkg_views():

    filename = '/vsimem/test_ogr_gpkg_views.gpkg'
    ds = gdaltest.gpkg_dr.CreateDataSource(filename)
    lyr = ds.CreateLayer('foo', geom_type = ogr.wkbPoint)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(0 0)'))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1 1)'))
    lyr.CreateFeature(f)

    ds.ExecuteSQL('CREATE VIEW geom_view AS SELECT fid AS my_fid, geom AS my_geom FROM foo')
    ds.ExecuteSQL("INSERT INTO gpkg_contents (table_name, identifier, data_type, srs_id) VALUES ( 'geom_view', 'geom_view', 'features', 0 )")
    ds.ExecuteSQL("INSERT INTO gpkg_geometry_columns (table_name, column_name, geometry_type_name, srs_id, z, m) values ('geom_view', 'my_geom', 'POINT', 0, 0, 0)")

    ds.ExecuteSQL('CREATE VIEW attr_view AS SELECT fid AS my_fid FROM foo')
    ds.ExecuteSQL("INSERT INTO gpkg_contents (table_name, identifier, data_type) VALUES ( 'attr_view', 'attr_view', 'attributes' )")

    ds = None

    assert validate(filename), 'validation failed'

    ds = ogr.Open(filename)
    assert ds.GetLayerCount() == 3

    lyr = ds.GetLayerByName('geom_view')
    assert lyr.GetGeomType() == ogr.wkbPoint

    lyr = ds.GetLayerByName('attr_view')
    assert lyr.GetGeomType() == ogr.wkbNone

    ds = None

    gdal.Unlink(filename)


###############################################################################
# Test read support for legacy gdal_aspatial extension


def test_ogr_gpkg_read_deprecated_gdal_aspatial():

    filename = '/vsimem/test_ogr_gpkg_aspatial.gpkg'
    ds = gdaltest.gpkg_dr.CreateDataSource(filename)
    ds.ExecuteSQL(
        "CREATE TABLE gpkg_extensions ("
        "table_name TEXT,"
        "column_name TEXT,"
        "extension_name TEXT NOT NULL,"
        "definition TEXT NOT NULL,"
        "scope TEXT NOT NULL,"
        "CONSTRAINT ge_tce UNIQUE (table_name, column_name, extension_name)"
        ")")
    ds.ExecuteSQL(
        "INSERT INTO gpkg_extensions "
        "(table_name, column_name, extension_name, definition, scope) "
        "VALUES "
        "(NULL, NULL, 'gdal_aspatial', 'http://gdal.org/geopackage_aspatial.html', 'read-write')")
    ds.ExecuteSQL('CREATE TABLE aspatial_layer(fid INTEGER PRIMARY KEY,bar TEXT)')
    ds.ExecuteSQL(
        "INSERT INTO gpkg_contents (table_name, data_type) VALUES ('aspatial_layer', 'aspatial')")
    ds.CreateLayer('spatial_layer', options=['SPATIAL_INDEX=NO'])
    ds = None

    ds = ogr.Open(filename)
    assert ds.GetLayerCount() == 2
    ds = None

    gdal.Unlink(filename)


###############################################################################
# Test fixing up wrong gpkg_metadata_reference_column_name_update trigger (GDAL < 2.4.0)

def test_ogr_gpkg_fixup_wrong_mr_column_name_update_trigger():

    filename = '/vsimem/test_ogr_gpkg_fixup_wrong_mr_column_name_update_trigger.gpkg'
    ds = ogr.GetDriverByName('GPKG').CreateDataSource(filename)
    ds.SetMetadata('FOO','BAR')
    ds = None

    ds = ogr.Open(filename, update = 1)
    # inject wrong trigger on purpose
    wrong_trigger = "CREATE TRIGGER 'gpkg_metadata_reference_column_name_update' " + \
                    "BEFORE UPDATE OF column_name ON 'gpkg_metadata_reference' " + \
                    "FOR EACH ROW BEGIN " + \
                    "SELECT RAISE(ABORT, 'update on table gpkg_metadata_reference " + \
                    "violates constraint: column name must be NULL when reference_scope " + \
                    "is \"geopackage\", \"table\" or \"row\"') " + \
                    "WHERE (NEW.reference_scope IN ('geopackage','table','row') " + \
                    "AND NEW.column_nameIS NOT NULL); END;"
    ds.ExecuteSQL(wrong_trigger)
    ds = None

    # Open in update mode
    ds = ogr.Open(filename, update = 1)
    sql_lyr = ds.ExecuteSQL(
        "SELECT sql FROM sqlite_master WHERE type = 'trigger' " + \
        "AND name = 'gpkg_metadata_reference_column_name_update'")
    f = sql_lyr.GetNextFeature()
    sql = f['sql']
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    gdal.Unlink(filename)
    assert 'column_nameIS' not in sql

###############################################################################
# Test support for CRS coordinate_epoch


def test_ogr_gpkg_crs_coordinate_epoch():

    filename = '/vsimem/test_ogr_gpkg_crs_coordinate_epoch.gpkg'
    ds = gdaltest.gpkg_dr.CreateDataSource(filename)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(7665) # WGS 84 (G1762) (3D)
    srs.SetCoordinateEpoch(2021.3)
    ds.CreateLayer('lyr_with_coordinate_epoch', srs=srs)

    srs.SetCoordinateEpoch(2021.3)
    ds.CreateLayer('lyr_with_same_coordinate_epoch', srs=srs)

    srs.SetCoordinateEpoch(2021.2)
    ds.CreateLayer('lyr_with_different_coordinate_epoch', srs=srs)

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4258) # ETRS89
    ds.CreateLayer('lyr_without_coordinate_epoch', srs=srs)

    ds = None

    assert validate(filename), 'validation failed'

    ds = ogr.Open(filename)

    sql_lyr = ds.ExecuteSQL('SELECT * FROM gpkg_spatial_ref_sys ORDER BY srs_id')
    assert sql_lyr.GetFeatureCount() == 6

    sql_lyr.GetNextFeature()
    sql_lyr.GetNextFeature()

    f = sql_lyr.GetNextFeature()
    assert f
    assert f['srs_id'] == 4258
    assert f['organization'] == 'EPSG'
    assert f['organization_coordsys_id'] == 4258
    assert f['epoch'] is None

    f = sql_lyr.GetNextFeature()
    assert f
    assert f['srs_id'] == 4326
    assert f['organization'] == 'EPSG'
    assert f['organization_coordsys_id'] == 4326
    assert f['epoch'] is None

    f = sql_lyr.GetNextFeature()
    assert f
    assert f['srs_id'] == 100000
    assert f['organization'] == 'EPSG'
    assert f['organization_coordsys_id'] == 7665
    assert f['epoch'] == 2021.3

    f = sql_lyr.GetNextFeature()
    assert f
    assert f['srs_id'] == 100001
    assert f['organization'] == 'EPSG'
    assert f['organization_coordsys_id'] == 7665
    assert f['epoch'] == 2021.2
    ds.ReleaseResultSet(sql_lyr)

    lyr = ds.GetLayerByName('lyr_with_coordinate_epoch')
    srs = lyr.GetSpatialRef()
    assert srs.GetCoordinateEpoch() == 2021.3

    lyr = ds.GetLayerByName('lyr_with_same_coordinate_epoch')
    srs = lyr.GetSpatialRef()
    assert srs.GetCoordinateEpoch() == 2021.3

    lyr = ds.GetLayerByName('lyr_with_different_coordinate_epoch')
    srs = lyr.GetSpatialRef()
    assert srs.GetCoordinateEpoch() == 2021.2

    lyr = ds.GetLayerByName('lyr_without_coordinate_epoch')
    srs = lyr.GetSpatialRef()
    assert srs.GetCoordinateEpoch() == 0

    ds = None

    gdal.Unlink(filename)

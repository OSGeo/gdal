#!/usr/bin/env python
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
# Copyright (c) 2014, Even Rouault <even dot rouault at mines-paris dot org>
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
import struct
import sys

# Make sure we run from the directory of the script
if os.path.basename(sys.argv[0]) == os.path.basename(__file__):
    if os.path.dirname(sys.argv[0]) != '':
        os.chdir(os.path.dirname(sys.argv[0]))

sys.path.append( '../pymod' )
sys.path.append('../../gdal/swig/python/samples')

from osgeo import gdal
from osgeo import ogr
from osgeo import osr
import gdaltest
sys.path.append( '../osr' )
import osr_proj4

###############################################################################
# Validate a geopackage

try:
    import validate_gpkg
    has_validate = True
except:
    has_validate = False

def validate(filename, quiet = False):
    if has_validate:
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
            validate_gpkg.check(my_filename)
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

def ogr_gpkg_1():

    gdaltest.gpkg_ds = None
    gdaltest.gpkg_dr = None

    try:
        gdaltest.gpkg_dr = ogr.GetDriverByName( 'GPKG' )
        if gdaltest.gpkg_dr is None:
            return 'skip'
    except:
        return 'skip'

    try:
        os.remove( 'tmp/gpkg_test.gpkg' )
    except:
        pass

    # This is to speed-up the runtime of tests on EXT4 filesystems
    # Do not use this for production environment if you care about data safety
    # w.r.t system/OS crashes, unless you know what you are doing.
    gdal.SetConfigOption('OGR_SQLITE_SYNCHRONOUS', 'OFF')

    gdaltest.gpkg_ds = gdaltest.gpkg_dr.CreateDataSource( 'tmp/gpkg_test.gpkg' )

    if gdaltest.gpkg_ds is None:
        return 'fail'

    gdaltest.gpkg_ds = None

    if not validate('tmp/gpkg_test.gpkg'):
        gdaltest.post_reason('validation failed')
        return 'fail'

    return 'success'

###############################################################################
# Re-open database to test validity

def ogr_gpkg_2():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    gdaltest.gpkg_ds = gdaltest.gpkg_dr.Open( 'tmp/gpkg_test.gpkg', update = 1 )

    # Check there a ogr_empty_table
    sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("SELECT COUNT(*) FROM sqlite_master WHERE name = 'ogr_empty_table'")
    f = sql_lyr.GetNextFeature()
    if f.GetField(0) != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    # Should default to GPKG 1.0
    sql_lyr = gdaltest.gpkg_ds.ExecuteSQL('PRAGMA application_id')
    f = sql_lyr.GetNextFeature()
    if f['application_id'] != 1196437808:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)
    sql_lyr = gdaltest.gpkg_ds.ExecuteSQL('PRAGMA user_version')
    f = sql_lyr.GetNextFeature()
    if f['user_version'] != 0:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    return 'success'


###############################################################################
# Create a layer

def ogr_gpkg_3():

    if gdaltest.gpkg_dr is None or gdaltest.gpkg_ds is None:
        return 'skip'

    # Test invalid FORMAT
    #gdal.PushErrorHandler('CPLQuietErrorHandler')
    srs4326 = osr.SpatialReference()
    srs4326.ImportFromEPSG( 4326 )
    lyr = gdaltest.gpkg_ds.CreateLayer( 'first_layer', geom_type = ogr.wkbPoint, srs = srs4326, options = ['GEOMETRY_NAME=gpkg_geometry', 'SPATIAL_INDEX=NO'])
    #gdal.PopErrorHandler()
    if lyr is None:
        return 'fail'

    # Test creating a layer with an existing name
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr = gdaltest.gpkg_ds.CreateLayer( 'a_layer', options = ['SPATIAL_INDEX=NO'])
    lyr = gdaltest.gpkg_ds.CreateLayer( 'a_layer', options = ['SPATIAL_INDEX=NO'])
    gdal.PopErrorHandler()
    if lyr is not None:
        gdaltest.post_reason('layer creation should have failed')
        return 'fail'

    return 'success'

###############################################################################
# Close and re-open to test the layer registration

def ogr_gpkg_4():

    if gdaltest.gpkg_dr is None or gdaltest.gpkg_ds is None:
        return 'skip'

    gdaltest.gpkg_ds = None

    if not validate('tmp/gpkg_test.gpkg'):
        gdaltest.post_reason('validation failed')
        return 'fail'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    gdaltest.gpkg_ds = gdaltest.gpkg_dr.Open( 'tmp/gpkg_test.gpkg', update = 1 )
    gdal.PopErrorHandler()

    if gdaltest.gpkg_ds is None:
        return 'fail'

    # Check there no ogr_empty_table
    sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("SELECT COUNT(*) FROM sqlite_master WHERE name = 'ogr_empty_table'")
    f = sql_lyr.GetNextFeature()
    if f.GetField(0) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    if gdaltest.gpkg_ds.GetLayerCount() != 2:
        gdaltest.post_reason( 'unexpected number of layers' )
        return 'fail'

    lyr0 = gdaltest.gpkg_ds.GetLayer(0)
    lyr1 = gdaltest.gpkg_ds.GetLayer(1)

    if lyr0.GetName() != 'first_layer':
        gdaltest.post_reason( 'unexpected layer name for layer 0' )
        return 'fail'
    if lyr0.GetLayerDefn().GetGeomFieldDefn(0).GetName() != 'gpkg_geometry':
        gdaltest.post_reason( 'unexpected geometry field name for layer 0' )
        return 'fail'

    if lyr1.GetName() != 'a_layer':
        gdaltest.post_reason( 'unexpected layer name for layer 1' )
        return 'fail'

    sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("SELECT * FROM sqlite_master WHERE name = 'gpkg_extensions'")
    if sql_lyr.GetFeatureCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    return 'success'


###############################################################################
# Delete a layer

def ogr_gpkg_5():

    if gdaltest.gpkg_dr is None or gdaltest.gpkg_ds is None:
        return 'skip'

    if gdaltest.gpkg_ds.GetLayerCount() != 2:
        gdaltest.post_reason( 'unexpected number of layers' )
        return 'fail'

    with gdaltest.error_handler():
        ret = gdaltest.gpkg_ds.DeleteLayer(-1)
    if ret == 0:
        gdaltest.post_reason( 'expected error' )
        return 'fail'

    with gdaltest.error_handler():
        ret = gdaltest.gpkg_ds.DeleteLayer(gdaltest.gpkg_ds.GetLayerCount())
    if ret == 0:
        gdaltest.post_reason( 'expected error' )
        return 'fail'

    if gdaltest.gpkg_ds.DeleteLayer(1) != 0:
        gdaltest.post_reason( 'got error code from DeleteLayer(1)' )
        return 'fail'

    if gdaltest.gpkg_ds.DeleteLayer(0) != 0:
        gdaltest.post_reason( 'got error code from DeleteLayer(0)' )
        return 'fail'

    if gdaltest.gpkg_ds.GetLayerCount() != 0:
        gdaltest.post_reason( 'unexpected number of layers (not 0)' )
        return 'fail'

    return 'success'


###############################################################################
# Add fields

def ogr_gpkg_6():

    if gdaltest.gpkg_dr is None or gdaltest.gpkg_ds is None:
        return 'skip'

    srs4326 = osr.SpatialReference()
    srs4326.ImportFromEPSG( 4326 )
    lyr = gdaltest.gpkg_ds.CreateLayer( 'field_test_layer', geom_type = ogr.wkbPoint, srs = srs4326)
    if lyr is None:
        return 'fail'

    field_defn = ogr.FieldDefn('dummy', ogr.OFTString)
    lyr.CreateField(field_defn)

    if lyr.GetLayerDefn().GetFieldDefn(0).GetType() != ogr.OFTString:
        gdaltest.post_reason( 'wrong field type' )
        return 'fail'

    gdaltest.gpkg_ds = None

    if not validate('tmp/gpkg_test.gpkg'):
        gdaltest.post_reason('validation failed')
        return 'fail'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    gdaltest.gpkg_ds = gdaltest.gpkg_dr.Open( 'tmp/gpkg_test.gpkg', update = 1 )
    gdal.PopErrorHandler()

    if gdaltest.gpkg_ds is None:
        return 'fail'

    if gdaltest.gpkg_ds.GetLayerCount() != 1:
        return 'fail'

    lyr = gdaltest.gpkg_ds.GetLayer(0)
    if lyr.GetName() != 'field_test_layer':
        return 'fail'

    field_defn_out = lyr.GetLayerDefn().GetFieldDefn(0)
    if field_defn_out.GetType() != ogr.OFTString:
        gdaltest.post_reason( 'wrong field type after reopen' )
        return 'fail'

    if field_defn_out.GetName() != 'dummy':
        gdaltest.post_reason( 'wrong field name after reopen' )
        return 'fail'

    return 'success'


###############################################################################
# Add a feature / read a feature / delete a feature

def ogr_gpkg_7():

    if gdaltest.gpkg_dr is None or gdaltest.gpkg_ds is None:
        return 'skip'

    lyr = gdaltest.gpkg_ds.GetLayerByName('field_test_layer')
    geom = ogr.CreateGeometryFromWkt('POINT(10 10)')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(geom)
    feat.SetField('dummy', 'a dummy value')

    if lyr.TestCapability(ogr.OLCSequentialWrite) != 1:
        gdaltest.post_reason('lyr.TestCapability(ogr.OLCSequentialWrite) != 1')
        return 'fail'

    if lyr.CreateFeature(feat) != 0:
        gdaltest.post_reason('cannot create feature')
        return 'fail'

    # Read back what we just inserted
    lyr.ResetReading()
    feat_read = lyr.GetNextFeature()
    if feat_read.GetField('dummy') != 'a dummy value':
        gdaltest.post_reason('output does not match input')
        return 'fail'

    # Only inserted one thing, so second feature should return NULL
    feat_read = lyr.GetNextFeature()
    if feat_read is not None:
        gdaltest.post_reason('last call should return NULL')
        return 'fail'

    # Add another feature
    geom = ogr.CreateGeometryFromWkt('POINT(100 100)')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(geom)
    feat.SetField('dummy', 'who you calling a dummy?')
    if lyr.CreateFeature(feat) != 0:
        gdaltest.post_reason('cannot create feature')
        return 'fail'

    if lyr.TestCapability(ogr.OLCRandomRead) != 1:
        gdaltest.post_reason('lyr.TestCapability(ogr.OLCRandomRead) != 1')
        return 'fail'

    # Random read a feature
    feat_read_random = lyr.GetFeature(feat.GetFID())
    if feat_read_random.GetField('dummy') != 'who you calling a dummy?':
        gdaltest.post_reason('random read output does not match input')
        return 'fail'

    if lyr.TestCapability(ogr.OLCRandomWrite) != 1:
        gdaltest.post_reason('lyr.TestCapability(ogr.OLCRandomWrite) != 1')
        return 'fail'

    # Random write a feature
    feat.SetField('dummy', 'i am no dummy')
    lyr.SetFeature(feat)
    feat_read_random = lyr.GetFeature(feat.GetFID())
    if feat_read_random.GetField('dummy') != 'i am no dummy':
        gdaltest.post_reason('random read output does not match random write input')
        return 'fail'

    if lyr.TestCapability(ogr.OLCDeleteFeature) != 1:
        gdaltest.post_reason('lyr.TestCapability(ogr.OLCDeleteFeature) != 1')
        return 'fail'

    # Delete a feature
    lyr.DeleteFeature(feat.GetFID())
    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('delete feature did not delete')
        return 'fail'

    # Test updating non-existing feature
    feat.SetFID(-10)
    if lyr.SetFeature( feat ) != ogr.OGRERR_NON_EXISTING_FEATURE:
        gdaltest.post_reason( 'Expected failure of SetFeature().' )
        return 'fail'

    # Test deleting non-existing feature
    if lyr.DeleteFeature( -10 ) != ogr.OGRERR_NON_EXISTING_FEATURE:
        gdaltest.post_reason( 'Expected failure of DeleteFeature().' )
        return 'fail'

    # Delete the layer
    if gdaltest.gpkg_ds.DeleteLayer('field_test_layer') != 0:
        gdaltest.post_reason( 'got error code from DeleteLayer(field_test_layer)' )

    return 'success'


###############################################################################
# Test a variety of geometry feature types and attribute types

def ogr_gpkg_8():

    # try:
    #     os.remove( 'tmp/gpkg_test.gpkg' )
    # except:
    #     pass
    # gdaltest.gpkg_dr = ogr.GetDriverByName( 'GPKG' )
    # gdaltest.gpkg_ds = gdaltest.gpkg_dr.CreateDataSource( 'tmp/gpkg_test.gpkg' )

    if gdaltest.gpkg_dr is None or gdaltest.gpkg_ds is None:
        return 'skip'

    srs = osr.SpatialReference()
    # Test a non-default SRS
    srs.ImportFromEPSG( 32631 )

    lyr = gdaltest.gpkg_ds.CreateLayer( 'tbl_linestring', geom_type = ogr.wkbLineString, srs = srs)
    if lyr is None:
        return 'fail'

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
        feat.SetField('fld_real', 3.14159/(i+1) )
        feat.SetField('fld_string', 'test string %d test' % i)
        feat.SetField('fld_date', '2014/05/17 ' )
        feat.SetField('fld_datetime', '2014/05/17  12:34:56' )
        feat.SetFieldBinaryFromHexString('fld_binary', 'fffe' )
        feat.SetField('fld_boolean', 1 )
        feat.SetField('fld_smallint', -32768 )
        feat.SetField('fld_float', 1.23 )
        feat.SetField('fld_integer64', 1000000000000 + i)

        if lyr.CreateFeature(feat) != 0:
            gdaltest.post_reason('cannot create feature %d' % i)
            return 'fail'
    lyr.CommitTransaction()

    feat = ogr.Feature(lyr.GetLayerDefn())
    if lyr.CreateFeature(feat) != 0:
        gdaltest.post_reason('cannot insert empty')
        return 'fail'

    feat.SetFID(6)
    if lyr.SetFeature(feat) != 0:
        gdaltest.post_reason('cannot update with empty')
        return 'fail'

    gdaltest.gpkg_ds = None
    gdaltest.gpkg_ds = gdaltest.gpkg_dr.Open( 'tmp/gpkg_test.gpkg', update = 1 )
    lyr = gdaltest.gpkg_ds.GetLayerByName('tbl_linestring')
    if lyr.GetLayerDefn().GetFieldDefn(6).GetSubType() != ogr.OFSTBoolean:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldDefn(7).GetSubType() != ogr.OFSTInt16:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldDefn(8).GetSubType() != ogr.OFSTFloat32:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = lyr.GetNextFeature()
    if feat.GetField(0) != 10 or feat.GetField(1) != 'test string 0 test' or \
       feat.GetField(2) != 3.14159  or feat.GetField(3) != '2014/05/17' or \
       feat.GetField(4) != '2014/05/17 12:34:56+00' or feat.GetField(5) != 'FFFE' or \
       feat.GetField(6) != 1 or feat.GetField(7) != -32768 or feat.GetField(8) != 1.23 or \
       feat.GetField(9) != 1000000000000:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    lyr = gdaltest.gpkg_ds.CreateLayer( 'tbl_polygon', geom_type = ogr.wkbPolygon, srs = srs)
    if lyr is None:
        return 'fail'

    lyr.StartTransaction()
    lyr.CreateField(ogr.FieldDefn('fld_datetime', ogr.OFTDateTime))
    lyr.CreateField(ogr.FieldDefn('fld_string', ogr.OFTString))

    geom = ogr.CreateGeometryFromWkt('POLYGON((5 5, 10 5, 10 10, 5 10, 5 5),(6 6, 6 7, 7 7, 7 6, 6 6))')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(geom)

    for i in range(10):
        feat.SetFID(-1)
        feat.SetField('fld_string', 'my super string %d' % i)
        feat.SetField('fld_datetime', '2010-01-01' )

        if lyr.CreateFeature(feat) != 0:
            gdaltest.post_reason('cannot create polygon feature %d' % i)
            return 'fail'
    lyr.CommitTransaction()

    feat = lyr.GetFeature(3)
    geom_read = feat.GetGeometryRef()
    if geom.ExportToWkt() != geom_read.ExportToWkt():
        gdaltest.post_reason('geom output not equal to geom input')
        return 'fail'

    # Test out the 3D support...
    lyr = gdaltest.gpkg_ds.CreateLayer( 'tbl_polygon25d', geom_type = ogr.wkbPolygon25D, srs = srs)
    if lyr is None:
        return 'fail'

    lyr.CreateField(ogr.FieldDefn('fld_string', ogr.OFTString))
    geom = ogr.CreateGeometryFromWkt('POLYGON((5 5 1, 10 5 2, 10 10 3, 5 104 , 5 5 1),(6 6 4, 6 7 5, 7 7 6, 7 6 7, 6 6 4))')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    geom_read = feat.GetGeometryRef()
    if geom.ExportToWkt() != geom_read.ExportToWkt():
        gdaltest.post_reason('3d geom output not equal to geom input')
        return 'fail'

    return 'success'

###############################################################################
# Test support for extents and counts

def ogr_gpkg_9():

    if gdaltest.gpkg_dr is None or gdaltest.gpkg_ds is None:
        return 'skip'

    lyr = gdaltest.gpkg_ds.GetLayerByName('tbl_linestring')
    extent = lyr.GetExtent()
    if extent != (5.0, 10.0, 5.0, 10.0):
        gdaltest.post_reason('got bad extent')
        print(extent)
        return 'fail'

    fcount = lyr.GetFeatureCount()
    if fcount != 11:
        gdaltest.post_reason('got bad featurecount')
        print(fcount)
        return 'fail'

    return 'success'

###############################################################################
# Test non-SELECT SQL commands

def ogr_gpkg_11():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    gdaltest.gpkg_ds = None
    gdaltest.gpkg_ds = ogr.Open('tmp/gpkg_test.gpkg', update = 1)
    gdaltest.gpkg_ds.ExecuteSQL('CREATE INDEX tbl_linestring_fld_integer_idx ON tbl_linestring(fld_integer)')
    gdaltest.gpkg_ds.ExecuteSQL('ALTER TABLE tbl_linestring RENAME TO tbl_linestring_renamed')
    gdaltest.gpkg_ds.ExecuteSQL('VACUUM')
    gdaltest.gpkg_ds = None

    gdaltest.gpkg_ds = ogr.Open('tmp/gpkg_test.gpkg', update = 1)
    lyr = gdaltest.gpkg_ds.GetLayerByName('tbl_linestring_renamed')
    if lyr is None:
        return 'fail'
    lyr.SetAttributeFilter('fld_integer = 10')
    if lyr.GetFeatureCount() != 1:
        return 'fail'

    return 'success'

###############################################################################
# Test SELECT SQL commands

def ogr_gpkg_12():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    sql_lyr = gdaltest.gpkg_ds.ExecuteSQL('SELECT * FROM tbl_linestring_renamed')
    if sql_lyr.GetFIDColumn() != 'fid':
        gdaltest.post_reason('fail')
        return 'fail'
    if sql_lyr.GetGeomType() != ogr.wkbLineString:
        gdaltest.post_reason('fail')
        return 'fail'
    if sql_lyr.GetGeometryColumn() != 'geom':
        gdaltest.post_reason('fail')
        return 'fail'
    if sql_lyr.GetSpatialRef().ExportToWkt().find('32631') < 0:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = sql_lyr.GetNextFeature()
    if feat.GetFID() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if sql_lyr.GetFeatureCount() != 11:
        gdaltest.post_reason('fail')
        return 'fail'
    if sql_lyr.GetLayerDefn().GetFieldCount() != 10:
        gdaltest.post_reason('fail')
        return 'fail'
    if sql_lyr.GetLayerDefn().GetFieldDefn(6).GetSubType() != ogr.OFSTBoolean:
        gdaltest.post_reason('fail')
        return 'fail'
    if sql_lyr.GetLayerDefn().GetFieldDefn(7).GetSubType() != ogr.OFSTInt16:
        gdaltest.post_reason('fail')
        return 'fail'
    if sql_lyr.GetLayerDefn().GetFieldDefn(8).GetSubType() != ogr.OFSTFloat32:
        gdaltest.post_reason('fail')
        return 'fail'
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    sql_lyr = gdaltest.gpkg_ds.ExecuteSQL(
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
    if sql_lyr.GetFIDColumn() != 'FID':
        gdaltest.post_reason('fail')
        print(sql_lyr.GetFIDColumn())
        return 'fail'
    if sql_lyr.GetGeometryColumn() != 'GEOM':
        gdaltest.post_reason('fail')
        print(sql_lyr.GetGeometryColumn())
        return 'fail'
    if sql_lyr.GetLayerDefn().GetFieldCount() != 5:
        gdaltest.post_reason('fail')
        print(sql_lyr.GetLayerDefn().GetFieldCount())
        return 'fail'
    if sql_lyr.GetLayerDefn().GetFieldDefn(0).GetName() != 'FLD_INTEGER':
        gdaltest.post_reason('fail')
        return 'fail'
    if sql_lyr.GetLayerDefn().GetFieldDefn(0).GetType() != ogr.OFTInteger:
        gdaltest.post_reason('fail')
        return 'fail'
    if sql_lyr.GetLayerDefn().GetFieldDefn(1).GetName() != 'FLD_STRING':
        gdaltest.post_reason('fail')
        return 'fail'
    if sql_lyr.GetLayerDefn().GetFieldDefn(1).GetType() != ogr.OFTString:
        gdaltest.post_reason('fail')
        return 'fail'
    if sql_lyr.GetLayerDefn().GetFieldDefn(2).GetName() != 'FLD_REAL':
        gdaltest.post_reason('fail')
        return 'fail'
    if sql_lyr.GetLayerDefn().GetFieldDefn(2).GetType() != ogr.OFTReal:
        gdaltest.post_reason('fail')
        return 'fail'
    if sql_lyr.GetLayerDefn().GetFieldDefn(3).GetName() != 'FLD_BINARY':
        gdaltest.post_reason('fail')
        return 'fail'
    if sql_lyr.GetLayerDefn().GetFieldDefn(3).GetType() != ogr.OFTBinary:
        gdaltest.post_reason('fail')
        return 'fail'
    if sql_lyr.GetLayerDefn().GetFieldDefn(4).GetName() != 'FLD_INTEGER64':
        gdaltest.post_reason('fail')
        return 'fail'
    if sql_lyr.GetLayerDefn().GetFieldDefn(4).GetType() != ogr.OFTInteger64:
        gdaltest.post_reason('fail')
        return 'fail'
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    sql_lyr = gdaltest.gpkg_ds.ExecuteSQL('SELECT * FROM tbl_linestring_renamed WHERE 0=1')
    feat = sql_lyr.GetNextFeature()
    if feat is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    for sql in [ 'SELECT * FROM tbl_linestring_renamed LIMIT 1',
                 'SELECT * FROM tbl_linestring_renamed ORDER BY fld_integer LIMIT 1',
                 'SELECT * FROM tbl_linestring_renamed UNION ALL SELECT * FROM tbl_linestring_renamed ORDER BY fld_integer LIMIT 1' ]:
        sql_lyr = gdaltest.gpkg_ds.ExecuteSQL(sql)
        feat = sql_lyr.GetNextFeature()
        if feat is None:
            gdaltest.post_reason('fail')
            return 'fail'
        feat = sql_lyr.GetNextFeature()
        if feat is not None:
            gdaltest.post_reason('fail')
            return 'fail'
        if sql_lyr.GetFeatureCount() != 1:
            gdaltest.post_reason('fail')
            return 'fail'
        gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    sql_lyr = gdaltest.gpkg_ds.ExecuteSQL('SELECT sqlite_version()')
    feat = sql_lyr.GetNextFeature()
    if feat is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if sql_lyr.GetLayerDefn().GetFieldCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if sql_lyr.GetLayerDefn().GetGeomFieldCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    return 'success'

###############################################################################
# Test non-spatial tables

def ogr_gpkg_13():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    lyr = gdaltest.gpkg_ds.CreateLayer('non_spatial', geom_type = ogr.wkbNone, options = [ 'ASPATIAL_VARIANT=OGR_ASPATIAL' ] )
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
        gdaltest.post_reason('fail')
        return 'fail'
    feat = lyr.GetNextFeature()
    if feat.GetField('fld_integer') != 1:
        feat.DumpReadable()
        gdaltest.post_reason('fail')
        return 'fail'

    # Test second aspatial layer
    lyr = gdaltest.gpkg_ds.CreateLayer('non_spatial2', geom_type = ogr.wkbNone, options = [ 'ASPATIAL_VARIANT=OGR_ASPATIAL' ] )

    gdaltest.gpkg_ds = None
    gdaltest.gpkg_ds = ogr.Open('tmp/gpkg_test.gpkg', update = 1)
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail : warning NOT expected')
        return 'fail'
    if gdaltest.gpkg_ds.GetLayerCount() != 5:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr = gdaltest.gpkg_ds.GetLayer('non_spatial')
    if lyr.GetGeomType() != ogr.wkbNone:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = lyr.GetNextFeature()
    if not feat.IsFieldNull('fld_integer'):
        gdaltest.post_reason('fail')
        return 'fail'
    feat = lyr.GetNextFeature()
    if feat.GetField('fld_integer') != 1:
        feat.DumpReadable()
        gdaltest.post_reason('fail')
        return 'fail'

    sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("SELECT * FROM gpkg_extensions WHERE table_name IS NULL AND extension_name = 'gdal_aspatial'")
    if sql_lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    return 'success'

###############################################################################
# Add various geometries to test spatial filtering

def ogr_gpkg_14():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    sr = osr.SpatialReference()
    sr.ImportFromEPSG(32631)

    lyr = gdaltest.gpkg_ds.CreateLayer('point_no_spi-but-with-dashes', geom_type = ogr.wkbPoint, options = ['SPATIAL_INDEX=NO'], srs = sr )
    if lyr.TestCapability(ogr.OLCFastSpatialFilter) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
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
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f = lyr.GetFeature(6)
    if f.GetGeometryRef().ExportToWkt() != 'POINT EMPTY':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f = None

    sql_lyr = gdaltest.gpkg_ds.ExecuteSQL('SELECT * FROM "point_no_spi-but-with-dashes"')
    res = sql_lyr.TestCapability(ogr.OLCFastSpatialFilter)
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)
    if res != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr = gdaltest.gpkg_ds.CreateLayer('point-with-spi-and-dashes', geom_type = ogr.wkbPoint )
    if lyr.TestCapability(ogr.OLCFastSpatialFilter) != 1:
        gdaltest.post_reason('fail')
        return 'fail'
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

    sql_lyr = gdaltest.gpkg_ds.ExecuteSQL('SELECT * FROM "point-with-spi-and-dashes"')
    res = sql_lyr.TestCapability(ogr.OLCFastSpatialFilter)
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)
    if res != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test spatial filer right away
    lyr.SetSpatialFilterRect(1000, 30000000,1000, 30000000)
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test SQL functions

def ogr_gpkg_15():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    sql_lyr = gdaltest.gpkg_ds.ExecuteSQL(
        'SELECT ST_IsEmpty(geom), ST_SRID(geom), ST_GeometryType(geom), ' + \
        'ST_MinX(geom), ST_MinY(geom), ST_MaxX(geom), ST_MaxY(geom) FROM \"point_no_spi-but-with-dashes\" WHERE fid = 1')
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != 0 or feat.GetField(1) != 32631 or \
       feat.GetField(2) != 'POINT' or \
       feat.GetField(3) != 1000 or feat.GetField(4) != 30000000 or \
       feat.GetField(5) != 1000 or feat.GetField(6) != 30000000:
        feat.DumpReadable()
        gdaltest.post_reason('fail')
        return 'fail'
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    sql_lyr = gdaltest.gpkg_ds.ExecuteSQL(
        'SELECT ST_IsEmpty(geom), ST_SRID(geom), ST_GeometryType(geom), ' + \
        'ST_MinX(geom), ST_MinY(geom), ST_MaxX(geom), ST_MaxY(geom) FROM tbl_linestring_renamed WHERE geom IS NULL')
    feat = sql_lyr.GetNextFeature()
    if not feat.IsFieldNull(0) or not feat.IsFieldNull(1) or not feat.IsFieldNull(2) or \
       not feat.IsFieldNull(3) or not feat.IsFieldNull(4) or not feat.IsFieldNull(5) or not feat.IsFieldNull(6):
        feat.DumpReadable()
        gdaltest.post_reason('fail')
        return 'fail'
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    for (expected_type, actual_type, expected_result) in [
                ('POINT', 'POINT', 1),
                ('LINESTRING', 'POINT', 0),
                ('GEOMETRY', 'POINT', 1),
                ('POINT', 'GEOMETRY', 0),
                ('GEOMETRYCOLLECTION', 'MULTIPOINT', 1),
                ('GEOMETRYCOLLECTION', 'POINT', 0) ]:
        sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("SELECT GPKG_IsAssignable('%s', '%s')" % (expected_type, actual_type))
        feat = sql_lyr.GetNextFeature()
        got_result = feat.GetField(0)
        gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)
        if got_result != expected_result:
            print("expected_type=%s actual_type=%s expected_result=%d got_result=%d" % (expected_type, actual_type, expected_result, got_result))
            gdaltest.post_reason('fail')
            return 'fail'


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
        sql_lyr = gdaltest.gpkg_ds.ExecuteSQL(sql)
        if expected_result == 0:
            gdal.PopErrorHandler()
        feat = sql_lyr.GetNextFeature()
        got_result = feat.GetField(0)
        gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)
        if got_result != expected_result:
            print(sql)
            gdaltest.post_reason('fail')
            return 'fail'

    # NULL argument
    sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("SELECT SridFromAuthCRS(NULL, 4326)")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != -1:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    # NULL argument
    sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("SELECT SridFromAuthCRS('epsg', NULL)")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != -1:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    # Existing entry
    sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("SELECT SridFromAuthCRS('epsg', 4326)")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != 4326:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    # Non existing entry
    sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("SELECT SridFromAuthCRS('epsg', 1234)")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != -1:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    # NULL argument
    sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("SELECT ImportFromEPSG(NULL)")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != -1:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    # Existing entry in gpkg_spatial_ref_sys
    sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("SELECT ImportFromEPSG(4326)")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != 4326:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    # New entry in gpkg_spatial_ref_sys
    sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("SELECT ImportFromEPSG(32633)")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != 32633:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    # Invalid code
    with gdaltest.error_handler():
        sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("SELECT ImportFromEPSG(0)")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != -1:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    # NULL argument
    sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("SELECT ST_Transform(NULL, 4326)")
    feat = sql_lyr.GetNextFeature()
    if feat.GetGeometryRef() is not None:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    # Invalid geometry
    with gdaltest.error_handler():
        sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("SELECT ST_Transform(x'00', 4326)")
    feat = sql_lyr.GetNextFeature()
    if feat.GetGeometryRef() is not None:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    # NULL argument
    sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("SELECT ST_Transform(geom, NULL) FROM tbl_linestring_renamed")
    feat = sql_lyr.GetNextFeature()
    if feat.GetGeometryRef() is not None:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    # Invalid target SRID
    with gdaltest.error_handler():
        sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("SELECT ST_Transform(geom, 0) FROM tbl_linestring_renamed")
    feat = sql_lyr.GetNextFeature()
    if feat.GetGeometryRef() is not None:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    # Invalid source SRID
    with gdaltest.error_handler():
        sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("SELECT ST_Transform(geom, 4326) FROM \"point-with-spi-and-dashes\"")
    feat = sql_lyr.GetNextFeature()
    if feat.GetGeometryRef() is not None:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    # Invalid spatialite geometry: SRID=4326,MULTIPOINT EMPTY truncated
    with gdaltest.error_handler():
        sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("SELECT ST_Transform(x'0001E610000000000000000000000000000000000000000000000000000000000000000000007C04000000000000FE', 4326) FROM tbl_linestring_renamed")
    feat = sql_lyr.GetNextFeature()
    if feat.GetGeometryRef() is not None:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)


    if osr_proj4.have_proj480():
        sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("SELECT ST_Transform(geom, ST_SRID(geom)) FROM tbl_linestring_renamed")
        feat = sql_lyr.GetNextFeature()
        if feat.GetGeometryRef().ExportToWkt() != 'LINESTRING (5 5,10 5,10 10,5 10)':
            gdaltest.post_reason('fail')
            feat.DumpReadable()
            return 'fail'
        gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

        sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("SELECT ST_SRID(ST_Transform(geom, 4326)) FROM tbl_linestring_renamed")
        feat = sql_lyr.GetNextFeature()
        if feat.GetField(0) != 4326:
            gdaltest.post_reason('fail')
            feat.DumpReadable()
            return 'fail'
        gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

        # Spatialite geometry: SRID=4326,MULTIPOINT EMPTY
        sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("SELECT ST_SRID(ST_Transform(x'0001E610000000000000000000000000000000000000000000000000000000000000000000007C0400000000000000FE', 4326)) FROM tbl_linestring_renamed")
        feat = sql_lyr.GetNextFeature()
        if feat.GetField(0) != 4326:
            gdaltest.post_reason('fail')
            feat.DumpReadable()
            return 'fail'
        gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)


    # Error case: less than 8 bytes
    sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("SELECT ST_MinX(x'00')")
    feat = sql_lyr.GetNextFeature()
    if feat.IsFieldSetAndNotNull(0):
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    feat = None
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    # Error case: 8 wrong bytes
    sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("SELECT ST_MinX(x'0001020304050607')")
    feat = sql_lyr.GetNextFeature()
    if feat.IsFieldSetAndNotNull(0):
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    feat = None
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    # Error case: too short blob
    sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("SELECT ST_GeometryType(x'4750001100000000')")
    feat = sql_lyr.GetNextFeature()
    if feat.IsFieldSetAndNotNull(0):
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    feat = None
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    # Error case: too short blob
    sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("SELECT ST_GeometryType(x'475000110000000001040000')")
    feat = sql_lyr.GetNextFeature()
    if feat.IsFieldSetAndNotNull(0):
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    feat = None
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    # Invalid geometry, but long enough for our purpose...
    sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("SELECT ST_GeometryType(x'47500011000000000104000000')")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != 'MULTIPOINT':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    feat = None
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    # Spatialite geometry (MULTIPOINT EMPTY)
    sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("SELECT ST_GeometryType(x'00010000000000000000000000000000000000000000000000000000000000000000000000007C0400000000000000FE')")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != 'MULTIPOINT':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    feat = None
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    # Spatialite geometry (MULTIPOINT EMPTY)
    sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("SELECT ST_IsEmpty(x'00010000000000000000000000000000000000000000000000000000000000000000000000007C0400000000000000FE')")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != 1:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    feat = None
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    # Error case: invalid geometry
    with gdaltest.error_handler():
        sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("SELECT ST_GeometryType(x'475000030000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000')")
    feat = sql_lyr.GetNextFeature()
    if feat.IsFieldSetAndNotNull(0):
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    feat = None
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    # Error case: invalid type
    sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("SELECT GPKG_IsAssignable('POINT', NULL)")
    feat = sql_lyr.GetNextFeature()
    feat = None
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    # Error case: invalid type
    sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("SELECT GPKG_IsAssignable(NULL, 'POINT')")
    feat = sql_lyr.GetNextFeature()
    feat = None
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    # Test hstore_get_value
    sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("SELECT hstore_get_value('a=>b', 'a')")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != 'b':
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    feat = None
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    # Test hstore_get_value
    sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("SELECT hstore_get_value('a=>b', 'x')")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) is not None:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    feat = None
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    # Error case: invalid type
    sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("SELECT hstore_get_value('a=>b', NULL)")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) is not None:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    feat = None
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    # Error case: invalid type
    sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("SELECT hstore_get_value(NULL, 'a')")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) is not None:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    feat = None
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    has_spatialite_4_3_or_later = False
    with gdaltest.error_handler():
        sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("SELECT spatialite_version()")
        if sql_lyr:
            f = sql_lyr.GetNextFeature()
            version = f.GetField(0)
            version = '.'.join(version.split('.')[0:2])
            version = float(version)
            if version >= 4.3:
                has_spatialite_4_3_or_later = True
                #print('Spatialite 4.3 or later found')
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)
    if has_spatialite_4_3_or_later:
        sql_lyr = gdaltest.gpkg_ds.ExecuteSQL(
            "SELECT ST_Buffer(geom, 0) FROM tbl_linestring_renamed")
        if sql_lyr.GetGeomType() != ogr.wkbPolygon:
            gdaltest.post_reason('fail')
            print(sql_lyr.GetGeomType())
            return 'fail'
        if sql_lyr.GetSpatialRef().ExportToWkt().find('32631') < 0:
            gdaltest.post_reason('fail')
            print(sql_lyr.GetSpatialRef())
            return 'fail'
        gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    gdaltest.gpkg_ds = None
    gdaltest.gpkg_ds = gdaltest.gpkg_dr.Open( 'tmp/gpkg_test.gpkg', update = 1 )

    return 'success'

###############################################################################
# Test unknown extensions

def ogr_gpkg_16():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpk_16.gpkg')
    ds.CreateLayer('foo')
    ds.ExecuteSQL("INSERT INTO gpkg_extensions ( table_name, column_name, " + \
        "extension_name, definition, scope ) VALUES ( 'foo', 'geom', 'myext', 'some ext', 'write-only' ) ")
    ds = None

    # No warning since we open as read-only
    ds = ogr.Open('/vsimem/ogr_gpk_16.gpkg')
    ds = None
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail : warning NOT expected')
        return 'fail'

    # Warning since we open as read-write
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogr.Open('/vsimem/ogr_gpk_16.gpkg', update = 1)
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail : warning expected')
        return 'fail'

    ds.ExecuteSQL("UPDATE gpkg_extensions SET scope = 'read-write' WHERE extension_name = 'myext'")
    ds = None

    # Warning since we open as read-only
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogr.Open('/vsimem/ogr_gpk_16.gpkg')
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail : warning expected')
        return 'fail'

    # and also as read-write
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogr.Open('/vsimem/ogr_gpk_16.gpkg', update = 1)
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail : warning expected')
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/ogr_gpk_16.gpkg')

    # Test with unsupported geometry type
    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpk_16.gpkg')
    ds.CreateLayer('foo')
    ds.ExecuteSQL("INSERT INTO gpkg_extensions ( table_name, column_name, " + \
        "extension_name, definition, scope ) VALUES ( 'foo', 'geom', 'gpkg_geom_XXXX', 'some ext', 'read-write' ) ")
    ds = None

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogr.Open('/vsimem/ogr_gpk_16.gpkg')
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail : warning expected')
        return 'fail'

    gdal.Unlink('/vsimem/ogr_gpk_16.gpkg')

    # Test with database wide unknown extension
    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpk_16.gpkg')
    ds.CreateLayer('foo')
    ds.ExecuteSQL("INSERT INTO gpkg_extensions ( "+ \
        "extension_name, definition, scope ) VALUES ( 'myext', 'some ext', 'write-only' ) ")
    ds = None

    # No warning since we open as read-only
    ds = ogr.Open('/vsimem/ogr_gpk_16.gpkg')
    ds = None
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail : warning NOT expected')
        return 'fail'

    # Warning since we open as read-write
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogr.Open('/vsimem/ogr_gpk_16.gpkg', update = 1)
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail : warning expected')
        return 'fail'

    ds.ExecuteSQL("UPDATE gpkg_extensions SET scope = 'read-write' WHERE extension_name = 'myext'")
    ds = None

    # Warning since we open as read-only
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogr.Open('/vsimem/ogr_gpk_16.gpkg')
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail : warning expected')
        return 'fail'

    # and also as read-write
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogr.Open('/vsimem/ogr_gpk_16.gpkg', update = 1)
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail : warning expected')
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/ogr_gpk_16.gpkg')

    return 'success'

###############################################################################
# Run INDIRECT_SQLITE dialect

def ogr_gpkg_17():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_17.gpkg')
    sql_lyr = ds.ExecuteSQL("SELECT ogr_version()", dialect = 'INDIRECT_SQLITE')
    f = sql_lyr.GetNextFeature()
    if f is None:
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    gdal.Unlink('/vsimem/ogr_gpkg_17.gpkg')
    return 'success'

###############################################################################
# Test geometry type extension

def ogr_gpkg_18():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_18.gpkg')
    lyr = ds.CreateLayer('wkbCircularString', geom_type = ogr.wkbCircularString)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('CIRCULARSTRING(0 0,1 0,0 0)'))
    lyr.CreateFeature(f)
    f = None
    ds = None

    if not validate('/vsimem/ogr_gpkg_18.gpkg'):
        gdaltest.post_reason('validation failed')
        return 'fail'

    ds = ogr.Open('/vsimem/ogr_gpkg_18.gpkg')
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail : warning NOT expected')
        return 'fail'

    lyr = ds.GetLayer(0)
    if lyr.GetGeomType() != ogr.wkbCircularString:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    g = f.GetGeometryRef()
    if g.GetGeometryType() != ogr.wkbCircularString:
        gdaltest.post_reason('fail')
        return 'fail'

    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_extensions WHERE table_name = 'wkbCircularString' AND extension_name = 'gpkg_geom_CIRCULARSTRING'")
    if sql_lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
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
    if sql_lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)

    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_18.gpkg')
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail : warning NOT expected')
        return 'fail'

    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    g = f.GetGeometryRef()
    if g.GetGeometryType() != ogr.wkbCircularString:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_18.gpkg', update = 1)
    lyr = ds.GetLayer(0)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('CIRCULARSTRING(0 0,1 0,0 0)'))
    ret = lyr.CreateFeature(f)
    if ret != 0 or gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'
    f = None
    ds = None

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_18.gpkg')
    lyr = ds.CreateLayer('test', geom_type = ogr.wkbTriangle)
    with gdaltest.error_handler():
        # Warning 1: Registering non-standard gpkg_geom_TRIANGLE extension
        ds.FlushCache()
    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_extensions WHERE table_name = 'test' AND extension_name = 'gpkg_geom_TRIANGLE'")
    if sql_lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    ret = validate('/vsimem/ogr_gpkg_18.gpkg', quiet = True)
    if ret:
        gdaltest.post_reason('validation unexpectedly succeeded')
        return 'fail'

    # Test non-linear geometry in GeometryCollection
    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_18.gpkg')
    lyr = ds.CreateLayer('test')
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION(CIRCULARSTRING(0 0,1 0,0 0))'))
    lyr.CreateFeature(f)
    f = None
    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_extensions WHERE table_name = 'test' AND extension_name LIKE 'gpkg_geom_%'")
    if sql_lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    gdal.Unlink('/vsimem/ogr_gpkg_18.gpkg')

    return 'success'

###############################################################################
# Test metadata

def ogr_gpkg_19():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_19.gpkg')
    if len(ds.GetMetadata()) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr = ds.CreateLayer('test_without_md')
    if len(lyr.GetMetadata()) != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    ds.SetMetadataItem('foo', 'bar')

    # GEOPACKAGE metadata domain is not allowed in a non-raster context
    gdal.PushErrorHandler()
    ds.SetMetadata(ds.GetMetadata('GEOPACKAGE'), 'GEOPACKAGE')
    ds.SetMetadataItem('foo', ds.GetMetadataItem('foo', 'GEOPACKAGE'), 'GEOPACKAGE')
    gdal.PopErrorHandler()

    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_19.gpkg')
    if ds.GetMetadataDomainList() != ['']:
        print(ds.GetMetadataDomainList())
        gdaltest.post_reason('fail')
        return 'fail'

    ds = ogr.Open('/vsimem/ogr_gpkg_19.gpkg')
    if len(ds.GetMetadata()) != 1:
        print(ds.GetMetadata())
        gdaltest.post_reason('fail')
        return 'fail'

    ds = ogr.Open('/vsimem/ogr_gpkg_19.gpkg')
    if ds.GetMetadataItem('foo') != 'bar':
        gdaltest.post_reason('fail')
        print(ds.GetMetadata())
        return 'fail'
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_19.gpkg', update = 1)
    lyr = ds.CreateLayer('test_with_md', options = ['IDENTIFIER=ident', 'DESCRIPTION=desc'])
    lyr.SetMetadataItem('IDENTIFIER', 'ignored_because_of_lco')
    lyr.SetMetadataItem('DESCRIPTION', 'ignored_because_of_lco')
    lyr.SetMetadata( { 'IDENTIFIER': 'ignored_because_of_lco', 'DESCRIPTION': 'ignored_because_of_lco'} )
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_19.gpkg')
    lyr = ds.GetLayer('test_with_md')
    if lyr.GetMetadataItem('IDENTIFIER') != 'ident':
        gdaltest.post_reason('fail')
        print(lyr.GetMetadataItem('IDENTIFIER'))
        return 'fail'
    if lyr.GetMetadataItem('DESCRIPTION') != 'desc':
        gdaltest.post_reason('fail')
        print(lyr.GetMetadataItem('DESCRIPTION'))
        return 'fail'

    ds = ogr.Open('/vsimem/ogr_gpkg_19.gpkg', update = 1)
    lyr = ds.GetLayer('test_with_md')
    if lyr.GetMetadata() != {'IDENTIFIER': 'ident', 'DESCRIPTION': 'desc'}:
        gdaltest.post_reason('fail')
        print(lyr.GetMetadata())
        return 'fail'
    lyr.SetMetadataItem('IDENTIFIER', 'another_ident')
    lyr.SetMetadataItem('DESCRIPTION', 'another_desc')
    ds = None

    # FIXME? Is it expected to have a .aux.xml here ?
    gdal.Unlink('/vsimem/ogr_gpkg_19.gpkg.aux.xml')

    ds = ogr.Open('/vsimem/ogr_gpkg_19.gpkg', update = 1)
    lyr = ds.GetLayer('test_with_md')
    if lyr.GetMetadata() != {'IDENTIFIER': 'another_ident', 'DESCRIPTION': 'another_desc'}:
        gdaltest.post_reason('fail')
        print(lyr.GetMetadata())
        return 'fail'
    lyr.SetMetadataItem('foo', 'bar')
    lyr.SetMetadataItem('bar', 'baz', 'another_domain')
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_19.gpkg', update = 1)
    lyr = ds.GetLayer('test_with_md')
    if lyr.GetMetadataDomainList() != ['', 'another_domain']:
        gdaltest.post_reason('fail')
        print(lyr.GetMetadataDomainList())
        return 'fail'
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_19.gpkg', update = 1)
    lyr = ds.GetLayer('test_with_md')
    if lyr.GetMetadata() != {'IDENTIFIER': 'another_ident', 'foo': 'bar', 'DESCRIPTION': 'another_desc'}:
        gdaltest.post_reason('fail')
        print(lyr.GetMetadata())
        return 'fail'
    if lyr.GetMetadata('another_domain') != {'bar': 'baz'}:
        gdaltest.post_reason('fail')
        print(lyr.GetMetadata('another_domain'))
        return 'fail'
    lyr.SetMetadata(None)
    lyr.SetMetadata(None, 'another_domain')
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_19.gpkg', update = 1)
    lyr = ds.GetLayer('test_with_md')
    if lyr.GetMetadata() != {'IDENTIFIER': 'another_ident', 'DESCRIPTION': 'another_desc'}:
        gdaltest.post_reason('fail')
        print(lyr.GetMetadata())
        return 'fail'
    if lyr.GetMetadataDomainList() != ['']:
        gdaltest.post_reason('fail')
        print(lyr.GetMetadataDomainList())
        return 'fail'
    ds = None

    if not validate('/vsimem/ogr_gpkg_19.gpkg'):
        gdaltest.post_reason('validation failed')
        return 'fail'

    gdal.Unlink('/vsimem/ogr_gpkg_19.gpkg')
    gdal.Unlink('/vsimem/ogr_gpkg_19.gpkg.aux.xml')

    return 'success'

###############################################################################
# Test spatial reference system

def ogr_gpkg_20():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_20.gpkg')

    # "Conflict" with EPSG:4326
    srs = osr.SpatialReference()
    srs.SetFromUserInput("""GEOGCS["my geogcs",
    DATUM["my datum",
        SPHEROID["my spheroid",1000,0]],
    AUTHORITY["my_org","4326"]]""")
    lyr = ds.CreateLayer('my_org_4326', srs = srs)

    # No authority node
    srs = osr.SpatialReference()
    srs.SetFromUserInput("""GEOGCS["another geogcs",
    DATUM["another datum",
        SPHEROID["another spheroid",1000,0]]]""")
    lyr = ds.CreateLayer('without_org', srs = srs)

    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_20.gpkg')

    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_spatial_ref_sys WHERE srs_name='my geogcs' AND srs_id = 4327 AND organization='MY_ORG' AND organization_coordsys_id=4326 AND description is NULL")
    fc = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    if fc != 1:
        gdaltest.post_reason('fail')
        print(fc)
        return 'fail'

    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_spatial_ref_sys WHERE srs_name='another geogcs' AND srs_id = 4328 AND organization='NONE' AND organization_coordsys_id=4328 AND description is NULL")
    fc = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    if fc != 1:
        gdaltest.post_reason('fail')
        print(fc)
        return 'fail'

    lyr = ds.GetLayer('my_org_4326')
    if lyr.GetSpatialRef().ExportToWkt().find('my geogcs') < 0:
        gdaltest.post_reason('fail')
        print(lyr.GetSpatialRef().ExportToWkt())
        return 'fail'
    lyr = ds.GetLayer('without_org')
    if lyr.GetSpatialRef().ExportToWkt().find('another geogcs') < 0:
        gdaltest.post_reason('fail')
        print(lyr.GetSpatialRef().ExportToWkt())
        return 'fail'
    ds = None

    if not validate('/vsimem/ogr_gpkg_20.gpkg'):
        gdaltest.post_reason('validation failed')
        return 'fail'

    gdal.Unlink('/vsimem/ogr_gpkg_20.gpkg')

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_20.gpkg')
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    lyr = ds.CreateLayer('foo4326', srs = srs)
    ds.ExecuteSQL("UPDATE gpkg_spatial_ref_sys SET definition='invalid', "
                  "organization='', organization_coordsys_id = 0 "
                  "WHERE srs_id = 4326")
    ds = None

    # Unable to parse srs_id '4326' well-known text 'invalid'
    with gdaltest.error_handler():
        ds = ogr.Open('/vsimem/ogr_gpkg_20.gpkg', update = 1 )

    ds.ExecuteSQL('DELETE FROM gpkg_spatial_ref_sys WHERE srs_id = 4326')
    ds = None
    gdal.SetConfigOption('OGR_GPKG_FOREIGN_KEY_CHECK', 'NO')
    # Warning 1: unable to read srs_id '4326' from gpkg_spatial_ref_sys
    with gdaltest.error_handler():
        ds = ogr.Open('/vsimem/ogr_gpkg_20.gpkg', update = 1 )
    gdal.SetConfigOption('OGR_GPKG_FOREIGN_KEY_CHECK', None)
    ds = None

    gdal.Unlink('/vsimem/ogr_gpkg_20.gpkg')

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_20.gpkg')
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    lyr = ds.CreateLayer('foo4326', srs = srs)

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
        ds = ogr.Open('/vsimem/ogr_gpkg_20.gpkg', update = 1 )
    ds = None

    gdal.Unlink('/vsimem/ogr_gpkg_20.gpkg')

    return 'success'

###############################################################################
# Test maximum width of text fields

def ogr_gpkg_21():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_21.gpkg')
    lyr = ds.CreateLayer('test')
    field_defn = ogr.FieldDefn('str', ogr.OFTString)
    field_defn.SetWidth(2)
    lyr.CreateField(field_defn)
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_21.gpkg', update = 1)
    lyr = ds.GetLayer(0)
    if lyr.GetLayerDefn().GetFieldDefn(0).GetWidth() != 2:
        gdaltest.post_reason('fail')
        return 'fail'
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField(0, 'ab')
    gdal.ErrorReset()
    lyr.CreateFeature(f)
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFieldBinaryFromHexString(0, '41E9')
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField(0, 'abc')
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'

    f = lyr.GetFeature(f.GetFID())
    if f.GetField(0) != 'abc':
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.Unlink('/vsimem/ogr_gpkg_21.gpkg')


    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_21.gpkg')
    lyr = ds.CreateLayer('test', options = ['TRUNCATE_FIELDS=YES'])
    field_defn = ogr.FieldDefn('str', ogr.OFTString)
    field_defn.SetWidth(2)
    lyr.CreateField(field_defn)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFieldBinaryFromHexString(0, '41E9')
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'

    f = lyr.GetFeature(f.GetFID())
    if f.GetField(0) != 'A_':
        gdaltest.post_reason('fail')
        return 'fail'

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField(0, 'abc')
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'

    f = lyr.GetFeature(f.GetFID())
    if f.GetField(0) != 'ab':
        gdaltest.post_reason('fail')

    gdal.Unlink('/vsimem/ogr_gpkg_21.gpkg')

    return 'success'

###############################################################################
# Test FID64 support

def ogr_gpkg_22():

    if gdaltest.gpkg_dr is None:
        return 'skip'

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
    if lyr.GetMetadataItem(ogr.OLMD_FID64) is None:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f.GetFID() != 1234567890123:
        gdaltest.post_reason('failure')
        return 'fail'

    gdal.Unlink('/vsimem/ogr_gpkg_22.gpkg')

    return 'success'

###############################################################################
# Test not nullable fields

def ogr_gpkg_23():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_23.gpkg')
    lyr = ds.CreateLayer('test', geom_type = ogr.wkbNone)
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
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    f = None

    # Error case: missing non-nullable field
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT(0 0)'))
    gdal.PushErrorHandler()
    ret = lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    f = None

    # Nullable geometry field
    lyr = ds.CreateLayer('test2', geom_type = ogr.wkbPoint)

    # Cannot add more than one geometry field
    gdal.PushErrorHandler()
    ret = lyr.CreateGeomField(ogr.GeomFieldDefn('foo', ogr.wkbPoint))
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    f = None

    # Not-nullable fields and geometry fields created after table creation
    lyr = ds.CreateLayer('test3', geom_type = ogr.wkbNone, options = [ 'ASPATIAL_VARIANT=OGR_ASPATIAL' ])

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
    if fc != 2:
        gdaltest.post_reason('fail')
        return 'fail'

    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_extensions")
    fc = sql_lyr.GetFeatureCount()
    f = sql_lyr.GetNextFeature()
    ds.ReleaseResultSet(sql_lyr)
    if fc != 2: # r-tree and aspatial
        print(fc)
        gdaltest.post_reason('fail')
        return 'fail'

    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_geometry_columns")
    fc = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    if fc != 2:
        gdaltest.post_reason('fail')
        return 'fail'

    field_defn = ogr.GeomFieldDefn('geomfield_not_nullable', ogr.wkbPoint)
    field_defn.SetNullable(0)
    lyr.CreateGeomField(field_defn)

    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_contents WHERE data_type = 'features'")
    fc = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    if fc != 3:
        gdaltest.post_reason('fail')
        return 'fail'

    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_extensions")
    fc = sql_lyr.GetFeatureCount()
    f = sql_lyr.GetNextFeature()
    ds.ReleaseResultSet(sql_lyr)
    if fc != 1: # r-tree
        print(fc)
        gdaltest.post_reason('fail')
        return 'fail'

    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_geometry_columns")
    fc = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    if fc != 3:
        gdaltest.post_reason('fail')
        return 'fail'

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('field_not_nullable', 'not_null')
    f.SetGeomFieldDirectly('geomfield_not_nullable', ogr.CreateGeometryFromWkt('POINT(0 0)'))
    lyr.CreateFeature(f)
    f = None

    # Not Nullable geometry field
    lyr = ds.CreateLayer('test4', geom_type = ogr.wkbPoint, options = ['GEOMETRY_NULLABLE=NO'] )
    if lyr.GetLayerDefn().GetGeomFieldDefn(0).IsNullable() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT(0 0)'))
    lyr.CreateFeature(f)
    f = None

    ds.CreateLayer('test5', geom_type = ogr.wkbNone)

    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_23.gpkg')

    lyr = ds.GetLayerByName('test5')
    field_defn = ogr.GeomFieldDefn('', ogr.wkbPoint)
    with gdaltest.error_handler():
        if lyr.CreateGeomField(field_defn) == 0:
            gdaltest.post_reason('fail')
            return 'fail'

    lyr = ds.GetLayerByName('test')
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_not_nullable')).IsNullable() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_nullable')).IsNullable() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetGeomFieldDefn(lyr.GetLayerDefn().GetGeomFieldIndex('geomfield_not_nullable')).IsNullable() != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr = ds.GetLayerByName('test2')
    if lyr.GetLayerDefn().GetGeomFieldDefn(0).IsNullable() != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr = ds.GetLayerByName('test3')
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_not_nullable')).IsNullable() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_nullable')).IsNullable() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetGeomFieldDefn(lyr.GetLayerDefn().GetGeomFieldIndex('geomfield_not_nullable')).IsNullable() != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr = ds.GetLayerByName('test4')
    if lyr.GetLayerDefn().GetGeomFieldDefn(0).IsNullable() != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = None

    gdal.Unlink('/vsimem/ogr_gpkg_23.gpkg')

    return 'success'

###############################################################################
# Test default values

def ogr_gpkg_24():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_24.gpkg')
    lyr = ds.CreateLayer('test', geom_type = ogr.wkbNone)

    field_defn = ogr.FieldDefn( 'field_string', ogr.OFTString )
    field_defn.SetDefault("'a''b'")
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn( 'field_int', ogr.OFTInteger )
    field_defn.SetDefault('123')
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn( 'field_real', ogr.OFTReal )
    field_defn.SetDefault('1.23')
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn( 'field_nodefault', ogr.OFTInteger )
    lyr.CreateField(field_defn)

    # This will be translated as "(strftime('%Y-%m-%dT%H:%M:%fZ','now'))"
    field_defn = ogr.FieldDefn( 'field_datetime', ogr.OFTDateTime )
    field_defn.SetDefault("CURRENT_TIMESTAMP")
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn( 'field_datetime2', ogr.OFTDateTime )
    field_defn.SetDefault("'2015/06/30 12:34:56'")
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn( 'field_datetime3', ogr.OFTDateTime )
    field_defn.SetDefault("(strftime('%Y-%m-%dT%H:%M:%fZ','now'))")
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn( 'field_datetime4', ogr.OFTDateTime )
    field_defn.SetDefault("'2015/06/30 12:34:56.123'")
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn( 'field_date', ogr.OFTDate )
    field_defn.SetDefault("CURRENT_DATE")
    lyr.CreateField(field_defn)

    #field_defn = ogr.FieldDefn( 'field_time', ogr.OFTTime )
    #field_defn.SetDefault("CURRENT_TIME")
    #lyr.CreateField(field_defn)

    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    f = None

    # Test adding columns after "crystallization"
    field_defn = ogr.FieldDefn( 'field_datetime5', ogr.OFTDateTime )
    field_defn.SetDefault("'2016/06/30 12:34:56.123'")
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn( 'field_datetime6', ogr.OFTDateTime )
    field_defn.SetDefault("'2016/06/30 12:34:56'")
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn( 'field_string2', ogr.OFTString )
    field_defn.SetDefault("'X'")
    lyr.CreateField(field_defn)

    # Doesn't work currently. Would require rewriting the whole table
    #field_defn = ogr.FieldDefn( 'field_datetimeX', ogr.OFTDateTime )
    #field_defn.SetDefault("CURRENT_TIMESTAMP")
    #lyr.CreateField(field_defn)

    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_24.gpkg', update = 1)
    lyr = ds.GetLayerByName('test')
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_string')).GetDefault() != "'a''b'":
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_int')).GetDefault() != '123':
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_real')).GetDefault() != '1.23':
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_nodefault')).GetDefault() is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    # Translated from "(strftime('%Y-%m-%dT%H:%M:%fZ','now'))" to CURRENT_TIMESTAMP
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_datetime')).GetDefault() != 'CURRENT_TIMESTAMP':
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_datetime2')).GetDefault() != "'2015/06/30 12:34:56'":
        gdaltest.post_reason('fail')
        print(lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_datetime2')).GetDefault())
        return 'fail'
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_datetime3')).GetDefault() != "CURRENT_TIMESTAMP":
        gdaltest.post_reason('fail')
        print(lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_datetime3')).GetDefault())
        return 'fail'
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_datetime4')).GetDefault() != "'2015/06/30 12:34:56.123'":
        gdaltest.post_reason('fail')
        print(lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_datetime4')).GetDefault())
        return 'fail'
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_date')).GetDefault() != "CURRENT_DATE":
        gdaltest.post_reason('fail')
        return 'fail'
    #if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_time')).GetDefault() != "CURRENT_TIME":
    #    gdaltest.post_reason('fail')
    #    return 'fail'
    f = lyr.GetNextFeature()
    if f.GetField('field_string') != 'a\'b' or f.GetField('field_int') != 123 or \
       f.GetField('field_real') != 1.23 or \
       not f.IsFieldNull('field_nodefault') or not f.IsFieldSet('field_datetime')  or \
       f.GetField('field_datetime2') != '2015/06/30 12:34:56+00' or \
       f.GetField('field_datetime4') != '2015/06/30 12:34:56.123+00' or \
       not f.IsFieldSet('field_datetime3') or \
       not f.IsFieldSet('field_date') or \
       f.GetField('field_datetime5') != '2016/06/30 12:34:56.123+00' or \
       f.GetField('field_datetime6') != '2016/06/30 12:34:56+00' or \
       f.GetField('field_string2') != 'X' :
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    ds = None

    gdal.Unlink('/vsimem/ogr_gpkg_24.gpkg')

    return 'success'

###############################################################################
# Test creating a field with the fid name

def ogr_gpkg_25():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_25.gpkg')
    lyr = ds.CreateLayer('test', geom_type = ogr.wkbNone, options = ['FID=myfid'])

    lyr.CreateField(ogr.FieldDefn('str', ogr.OFTString))
    gdal.PushErrorHandler()
    ret = lyr.CreateField(ogr.FieldDefn('myfid', ogr.OFTString))
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    ret = lyr.CreateField(ogr.FieldDefn('myfid', ogr.OFTInteger))
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr.CreateField(ogr.FieldDefn('str2', ogr.OFTString))

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('str', 'first string')
    feat.SetField('myfid', 10)
    feat.SetField('str2', 'second string')
    ret = lyr.CreateFeature(feat)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if feat.GetFID() != 10:
        gdaltest.post_reason('fail')
        return 'fail'

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('str2', 'second string')
    ret = lyr.CreateFeature(feat)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if feat.GetFID() < 0:
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    if feat.GetField('myfid') != feat.GetFID():
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    feat.SetField('str', 'foo')
    ret = lyr.SetFeature(feat)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetFID(1)
    feat.SetField('myfid', 10)
    gdal.PushErrorHandler()
    ret = lyr.CreateFeature(feat)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.PushErrorHandler()
    ret = lyr.SetFeature(feat)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    feat.UnsetField('myfid')
    gdal.PushErrorHandler()
    ret = lyr.SetFeature(feat)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f.GetFID() != 10 or f.GetField('str') != 'first string' or f.GetField('str2') != 'second string' or f.GetField('myfid') != 10:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f = lyr.GetFeature(f.GetFID())
    if f.GetFID() != 10 or f.GetField('str') != 'first string' or f.GetField('str2') != 'second string' or f.GetField('myfid') != 10:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f = None

    ds = None

    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_25.gpkg')

    return 'success'

###############################################################################
# Test dataset transactions

def ogr_gpkg_26():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_26.gpkg')

    if ds.TestCapability(ogr.ODsCTransactions) != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    ret = ds.StartTransaction()
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.PushErrorHandler()
    ret = ds.StartTransaction()
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr = ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    ret = ds.RollbackTransaction()
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.PushErrorHandler()
    ret = ds.RollbackTransaction()
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_26.gpkg', update = 1)
    if ds.GetLayerCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ret = ds.StartTransaction()
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.PushErrorHandler()
    ret = ds.StartTransaction()
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr = ds.CreateLayer('test')
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    ret = ds.CommitTransaction()
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.PushErrorHandler()
    ret = ds.CommitTransaction()
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_26.gpkg', update = 1)
    if ds.GetLayerCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr = ds.GetLayerByName('test')

    ds.StartTransaction()
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.RollbackTransaction()
    if lyr.GetFeatureCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    ds.StartTransaction()
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f is None or f.GetFID() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.CommitTransaction()
    # the cursor is still valid after CommitTransaction(), which isn't the case for other backends such as PG !
    f = lyr.GetNextFeature()
    if f is None or f.GetFID() != 2:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetFeatureCount() != 2:
        gdaltest.post_reason('fail')
        return 'fail'

    ds.StartTransaction()
    lyr = ds.CreateLayer('test2', geom_type = ogr.wkbPoint)
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT(0 0)'))
    ret = lyr.CreateFeature(f)
    ds.CommitTransaction()
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    ds.StartTransaction()
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT(0 0)'))
    ret = lyr.CreateFeature(f)
    ds.CommitTransaction()
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    if False:
        ds.StartTransaction()
        lyr = ds.CreateLayer('test3', geom_type = ogr.wkbPoint)
        lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT(0 0)'))
        ret = lyr.CreateFeature(f)

        #ds.CommitTransaction()
        ds.ReleaseResultSet(ds.ExecuteSQL('SELECT 1'))
        #ds = None
        #ds = ogr.Open('/vsimem/ogr_gpkg_26.gpkg', update = 1)
        #lyr = ds.GetLayerByName('test3')
        #ds.StartTransaction()

        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT(0 0)'))
        ret = lyr.CreateFeature(f)
        ds.CommitTransaction()
        # For some reason fails with SQLite 3.6.X with 'failed to execute insert : callback requested query abort'
        # but not with later versions...
        if ret != 0:
            gdaltest.post_reason('fail')
            return 'fail'

    ds = None

    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_26.gpkg')

    return 'success'

###############################################################################
# Test interface with Spatialite

def ogr_gpkg_27():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_27.gpkg')
    gdal.PushErrorHandler()
    sql_lyr = ds.ExecuteSQL("SELECT GeomFromGPB(null)")
    gdal.PopErrorHandler()
    if sql_lyr is None:
        ds = None
        gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_27.gpkg')
        return 'skip'
    ds.ReleaseResultSet(sql_lyr)

    lyr = ds.CreateLayer('test')
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT (2 49)'))
    lyr.CreateFeature(f)
    sql_lyr = ds.ExecuteSQL('SELECT GeomFromGPB(geom) FROM test')
    f = sql_lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (2 49)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)

    ds = None
    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_27.gpkg')

    return 'success'

###############################################################################
# Test ogr2ogr -a_srs (as the geopackage driver doesn't clone the passed SRS
# but inc/dec its ref count, which can exhibit issues in GDALVectorTanslate())

def ogr_gpkg_28():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    srcDS = gdal.OpenEx('../ogr/data/poly.shp')
    ds = gdal.VectorTranslate('/vsimem/ogr_gpkg_28.gpkg', srcDS, format = 'GPKG', dstSRS='EPSG:4326')
    if str(ds.GetLayer(0).GetSpatialRef()).find('1984') == -1:
        return 'fail'

    ds = None
    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_28.gpkg')

    return 'success'

###############################################################################
# Test XYM / XYZM support

def ogr_gpkg_29():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_29.gpkg')
    if ds.TestCapability(ogr.ODsCMeasuredGeometries) != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr = ds.CreateLayer('pointm', geom_type = ogr.wkbPointM)
    if lyr.TestCapability(ogr.OLCMeasuredGeometries) != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT M (1 2 3)'))
    lyr.CreateFeature(f)
    lyr = ds.CreateLayer('pointzm', geom_type = ogr.wkbPointZM)
    if lyr.TestCapability(ogr.OLCMeasuredGeometries) != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT ZM (1 2 3 4)'))
    lyr.CreateFeature(f)
    ds = None

    if not validate('/vsimem/ogr_gpkg_29.gpkg'):
        gdaltest.post_reason('validation failed')
        return 'fail'

    ds = ogr.Open('/vsimem/ogr_gpkg_29.gpkg', update = 1)
    lyr = ds.GetLayerByName('pointm')
    if lyr.GetGeomType() != ogr.wkbPointM:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToIsoWkt() != 'POINT M (1 2 3)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    # Generate a XYM envelope
    ds.ExecuteSQL("UPDATE pointm SET geom = x'4750000700000000000000000000F03F000000000000F03F000000000000004000000000000000400000000000000840000000000000084001D1070000000000000000F03F00000000000000400000000000000840'")

    lyr = ds.GetLayerByName('pointzm')
    if lyr.GetGeomType() != ogr.wkbPointZM:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToIsoWkt() != 'POINT ZM (1 2 3 4)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    # Generate a XYZM envelope
    ds.ExecuteSQL("UPDATE pointzm SET geom = x'4750000900000000000000000000F03F000000000000F03F00000000000000400000000000000040000000000000084000000000000008400000000000001040000000000000104001B90B0000000000000000F03F000000000000004000000000000008400000000000001040'")

    ds = None

    # Check again
    ds = ogr.Open('/vsimem/ogr_gpkg_29.gpkg')
    lyr = ds.GetLayerByName('pointm')
    if lyr.GetGeomType() != ogr.wkbPointM:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToIsoWkt() != 'POINT M (1 2 3)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    lyr = ds.GetLayerByName('pointzm')
    if lyr.GetGeomType() != ogr.wkbPointZM:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToIsoWkt() != 'POINT ZM (1 2 3 4)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds = None

    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_29.gpkg')

    return 'success'

###############################################################################
# Test non standard file extension (#6396)

def ogr_gpkg_30():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    with gdaltest.error_handler():
        ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_30.geopkg')
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    with gdaltest.error_handler():
        ds = ogr.Open('/vsimem/ogr_gpkg_30.geopkg', update = 1)
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    with gdaltest.error_handler():
        gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_30.geopkg')

    return 'success'

###############################################################################
# Test CURVE and SURFACE types

def ogr_gpkg_31():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_31.gpkg')
    lyr = ds.CreateLayer('curve', geom_type = ogr.wkbCurve)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('LINESTRING (1 2,3 4)'))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('COMPOUNDCURVE ((1 2,3 4))'))
    lyr.CreateFeature(f)
    lyr = ds.CreateLayer('surface', geom_type = ogr.wkbSurface)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POLYGON ((0 0,0 1,1 1,0 0))'))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('CURVEPOLYGON ((0 0,0 1,1 1,0 0))'))
    lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_31.gpkg')
    lyr = ds.GetLayerByName('curve')
    if lyr.GetGeomType() != ogr.wkbCurve:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr = ds.GetLayerByName('surface')
    if lyr.GetGeomType() != ogr.wkbSurface:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    if not validate('/vsimem/ogr_gpkg_31.gpkg'):
        gdaltest.post_reason('validation failed')
        return 'fail'

    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_31.gpkg')

    return 'success'

###############################################################################
# Run creating a non-spatial layer that isn't registered as 'aspatial' and
# read it back

def ogr_gpkg_32():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_32.gpkg')
    ds.CreateLayer('aspatial', geom_type = ogr.wkbNone, options = ['ASPATIAL_VARIANT=NOT_REGISTERED'] )
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_32.gpkg')
    if ds.GetLayerCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_contents WHERE table_name != 'ogr_empty_table'")
    if sql_lyr.GetFeatureCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)
    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_geometry_columns WHERE table_name != 'ogr_empty_table'")
    if sql_lyr.GetFeatureCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)
    sql_lyr = ds.ExecuteSQL("SELECT * FROM sqlite_master WHERE name = 'gpkg_extensions'")
    if sql_lyr.GetFeatureCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    if not validate('/vsimem/ogr_gpkg_32.gpkg'):
        gdaltest.post_reason('validation failed')
        return 'fail'

    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_32.gpkg')

    return 'success'

###############################################################################
# Test OGR_CURRENT_DATE

def ogr_gpkg_33():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    gdal.SetConfigOption('OGR_CURRENT_DATE', '2000-01-01T:00:00:00.000Z')
    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_33.gpkg')
    ds.CreateLayer('test', geom_type = ogr.wkbNone )
    ds = None
    gdal.SetConfigOption('OGR_CURRENT_DATE', None)

    ds = ogr.Open('/vsimem/ogr_gpkg_33.gpkg')
    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_contents WHERE last_change = '2000-01-01T:00:00:00.000Z' AND table_name != 'ogr_empty_table'")
    if sql_lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_33.gpkg')

    return 'success'

###############################################################################
# Run test_ogrsf

def ogr_gpkg_test_ogrsf():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    # Do integrity check first
    sql_lyr = gdaltest.gpkg_ds.ExecuteSQL("PRAGMA integrity_check")
    feat = sql_lyr.GetNextFeature()
    if feat.GetField(0) != 'ok':
        gdaltest.post_reason('integrity check failed')
        return 'fail'
    gdaltest.gpkg_ds.ReleaseResultSet(sql_lyr)

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    gdaltest.gpkg_ds = None
    #sys.exit(0)
    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' tmp/gpkg_test.gpkg --config OGR_SQLITE_SYNCHRONOUS OFF')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        gdaltest.post_reason('fail')
        print(ret)
        return 'fail'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' tmp/gpkg_test.gpkg -sql "select * from tbl_linestring_renamed" --config OGR_SQLITE_SYNCHRONOUS OFF')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        gdaltest.post_reason('fail')
        print(ret)
        return 'fail'

    return 'success'


###############################################################################
# Test rename and delete a layer registered in extensions, metadata, spatial index etc

def ogr_gpkg_34():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    layer_name = """weird'layer"name"""

    dbname = '/vsimem/ogr_gpkg_34.gpkg'
    ds = gdaltest.gpkg_dr.CreateDataSource(dbname)
    lyr = ds.CreateLayer(layer_name, geom_type = ogr.wkbCurvePolygon )
    lyr.CreateField( ogr.FieldDefn('foo', ogr.OFTString) )
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

    if content.find(layer_name) < 0:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = ogr.Open(dbname, update = 1)
    new_layer_name = """weird2'layer"name"""
    with gdaltest.error_handler():
        ds.ExecuteSQL('ALTER TABLE "weird\'layer""name" RENAME TO gpkg_contents')
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.ErrorReset()
    ds.ExecuteSQL('ALTER TABLE "weird\'layer""name" RENAME TO "weird2\'layer""name"')
    ds.ExecuteSQL('VACUUM')
    ds = None

    # Check that there is no more any reference to the layer
    f = gdal.VSIFOpenL(dbname, 'rb')
    content = gdal.VSIFReadL(1, 1000000, f).decode('latin1')
    gdal.VSIFCloseL(f)

    if content.find(layer_name) >= 0:
        gdaltest.post_reason('fail')
        return 'fail'
    layer_name = new_layer_name

    ds = ogr.Open(dbname, update = 1)
    with gdaltest.error_handler():
        ds.ExecuteSQL('DELLAYER:does_not_exist')
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.ErrorReset()
    ds.ExecuteSQL('DELLAYER:' + layer_name)
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'
    ds.ExecuteSQL('VACUUM')
    ds = None

    # Check that there is no more any reference to the layer
    f = gdal.VSIFOpenL(dbname, 'rb')
    content = gdal.VSIFReadL(1, 1000000, f).decode('latin1')
    gdal.VSIFCloseL(f)

    if content.find(layer_name) >= 0:
        gdaltest.post_reason('fail')
        return 'fail'

    gdaltest.gpkg_dr.DeleteDataSource(dbname)


    # Try again with DROP TABLE syntax
    ds = gdaltest.gpkg_dr.CreateDataSource(dbname)
    lyr = ds.CreateLayer(layer_name, geom_type = ogr.wkbCurvePolygon )
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('CURVEPOLYGON ((0 0,0 1,1 1,0 0))'))
    lyr.CreateFeature(f)
    lyr.SetMetadataItem('FOO', 'BAR')
    lyr = ds.CreateLayer('another_layer_name' )
    ds = None

    ds = ogr.Open(dbname, update = 1)
    ds.ExecuteSQL('DROP TABLE "weird2\'layer""name"')
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'
    ds.ExecuteSQL('DROP TABLE another_layer_name')
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'
    with gdaltest.error_handler():
        ds.ExecuteSQL('DROP TABLE "foobar"')
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.ErrorReset()
    ds.ExecuteSQL('VACUUM')
    ds = None

    # Check that there is no more any reference to the layer
    f = gdal.VSIFOpenL(dbname, 'rb')
    content = gdal.VSIFReadL(1, 1000000, f).decode('latin1')
    gdal.VSIFCloseL(f)

    if content.find(layer_name) >= 0:
        gdaltest.post_reason('fail')
        return 'fail'

    if content.find('another_layer_name') >= 0:
        gdaltest.post_reason('fail')
        return 'fail'

    gdaltest.gpkg_dr.DeleteDataSource(dbname)

    return 'success'

###############################################################################
# Test DeleteField()

def ogr_gpkg_35():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    dbname = '/vsimem/ogr_gpkg_35.gpkg'
    ds = gdaltest.gpkg_dr.CreateDataSource(dbname)
    lyr = ds.CreateLayer('test', geom_type = ogr.wkbPolygon )
    lyr.CreateField( ogr.FieldDefn('foo', ogr.OFTString) )
    lyr.CreateField( ogr.FieldDefn('bar_i_will_disappear', ogr.OFTString) )
    lyr.CreateField( ogr.FieldDefn('baz', ogr.OFTString) )
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(10)
    f.SetField('foo', 'fooval')
    f.SetField('bar_i_will_disappear', 'barval')
    f.SetField('baz', 'bazval')
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POLYGON ((0 0,0 1,1 1,0 0))'))
    lyr.CreateFeature(f)

    lyr_nonspatial = ds.CreateLayer('test_nonspatial', geom_type = ogr.wkbNone )
    lyr_nonspatial.CreateField( ogr.FieldDefn('foo', ogr.OFTString) )
    lyr_nonspatial.CreateField( ogr.FieldDefn('bar_i_will_disappear', ogr.OFTString) )
    lyr_nonspatial.CreateField( ogr.FieldDefn('baz', ogr.OFTString) )
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
    ds.ExecuteSQL("INSERT INTO gpkg_data_columns VALUES('test', 'bar_i_will_disappear', 'bar_constraints', NULL, NULL, NULL, NULL)")
    ds.ExecuteSQL("INSERT INTO gpkg_extensions VALUES('test', 'bar_i_will_disappear', 'extension_name', 'definition', 'scope')")

    if lyr.TestCapability(ogr.OLCDeleteField) != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    with gdaltest.error_handler():
        ret = lyr.DeleteField(-1)
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    with gdaltest.error_handler():
        ret = lyr.DeleteField(lyr.GetLayerDefn().GetFieldCount())
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    if lyr.DeleteField(1) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldCount() != 2:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f.GetFID() != 10 or f['foo'] != 'fooval' or f['baz'] != 'bazval' or \
       f.GetGeometryRef().ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,0 0))':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    lyr.StartTransaction()
    ret = lyr_nonspatial.DeleteField(1)
    lyr.CommitTransaction()
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr_nonspatial.ResetReading()
    f = lyr_nonspatial.GetNextFeature()
    if f.GetFID() != 10 or f['foo'] != 'fooval' or f['baz'] != 'bazval':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    ds.ExecuteSQL('VACUUM')

    ds = None

    # Try on read-only dataset
    ds = ogr.Open(dbname)
    lyr = ds.GetLayer(0)
    with gdaltest.error_handler():
        ret = lyr.DeleteField(0)
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    # Check that there is no more any reference to the layer
    f = gdal.VSIFOpenL(dbname, 'rb')
    content = gdal.VSIFReadL(1, 1000000, f).decode('latin1')
    gdal.VSIFCloseL(f)

    if content.find('bar_i_will_disappear') >= 0:
        gdaltest.post_reason('fail')
        return 'fail'

    gdaltest.gpkg_dr.DeleteDataSource(dbname)

    return 'success'

###############################################################################
# Test AlterFieldDefn()

def ogr_gpkg_36():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    dbname = '/vsimem/ogr_gpkg_36.gpkg'
    ds = gdaltest.gpkg_dr.CreateDataSource(dbname)
    lyr = ds.CreateLayer('test', geom_type = ogr.wkbPolygon )
    lyr.CreateField( ogr.FieldDefn('foo', ogr.OFTString) )
    lyr.CreateField( ogr.FieldDefn('baz', ogr.OFTString) )
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
    ds.ExecuteSQL("INSERT INTO gpkg_data_columns VALUES('test', 'foo', 'constraint', NULL, NULL, NULL, NULL)")
    ds.ExecuteSQL("INSERT INTO gpkg_extensions VALUES('test', 'foo', 'extension_name', 'definition', 'scope')")
    ds.ExecuteSQL("CREATE INDEX my_idx ON test(foo)")

    if lyr.TestCapability(ogr.OLCAlterFieldDefn) != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    with gdaltest.error_handler():
        ret = lyr.AlterFieldDefn(-1, ogr.FieldDefn('foo'), ogr.ALTER_ALL_FLAG)
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    with gdaltest.error_handler():
        ret = lyr.AlterFieldDefn(1, ogr.FieldDefn('foo'), ogr.ALTER_ALL_FLAG)
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    with gdaltest.error_handler():
        ret = lyr.AlterFieldDefn(0, ogr.FieldDefn(lyr.GetGeometryColumn()), ogr.ALTER_ALL_FLAG)
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    with gdaltest.error_handler():
        ret = lyr.AlterFieldDefn(0, ogr.FieldDefn(lyr.GetFIDColumn()), ogr.ALTER_ALL_FLAG)
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    with gdaltest.error_handler():
        ret = lyr.AlterFieldDefn(0, ogr.FieldDefn('baz'), ogr.ALTER_ALL_FLAG)
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    new_field_defn = ogr.FieldDefn('bar', ogr.OFTReal)
    new_field_defn.SetSubType(ogr.OFSTFloat32)
    if lyr.AlterFieldDefn(0, new_field_defn, ogr.ALTER_ALL_FLAG) != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f.GetFID() != 10 or f['bar'] != 10.5 or \
    f.GetGeometryRef().ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,0 0))':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f = None

    lyr.StartTransaction()
    new_field_defn = ogr.FieldDefn('baw', ogr.OFTString)
    if lyr.AlterFieldDefn(0, new_field_defn, ogr.ALTER_ALL_FLAG) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr.CommitTransaction()

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f.GetFID() != 10 or f['baw'] != '10.5' or \
       f.GetGeometryRef().ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,0 0))':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f = None

    # Check that index has been recreated
    sql_lyr = ds.ExecuteSQL("SELECT * FROM sqlite_master WHERE name = 'my_idx'")
    f = sql_lyr.GetNextFeature()
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    f = None
    ds.ReleaseResultSet(sql_lyr)

    ds.ExecuteSQL('VACUUM')

    ds = None

    # Try on read-only dataset
    ds = ogr.Open(dbname)
    lyr = ds.GetLayer(0)
    with gdaltest.error_handler():
        ret = lyr.AlterFieldDefn(0, ogr.FieldDefn('foo'), ogr.ALTER_ALL_FLAG)
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    # Check that there is no more any reference to the layer
    f = gdal.VSIFOpenL(dbname, 'rb')
    content = gdal.VSIFReadL(1, 1000000, f).decode('latin1')
    gdal.VSIFCloseL(f)

    if content.find('foo') >= 0:
        gdaltest.post_reason('fail')
        return 'fail'

    gdaltest.gpkg_dr.DeleteDataSource(dbname)

    # Test failed DB re-opening
    ds = gdaltest.gpkg_dr.CreateDataSource(dbname)
    lyr = ds.CreateLayer('test', geom_type = ogr.wkbPolygon )
    lyr.CreateField( ogr.FieldDefn('foo', ogr.OFTString) )
    lyr.CreateFeature( ogr.Feature(lyr.GetLayerDefn()) )
    # Unlink before AlterFieldDefn
    gdal.Unlink(dbname)
    with gdaltest.error_handler():
        ret = lyr.AlterFieldDefn(0, ogr.FieldDefn('bar'), ogr.ALTER_ALL_FLAG)
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    with gdaltest.error_handler():
        ds = None

    gdaltest.gpkg_dr.DeleteDataSource(dbname)

    return 'success'

###############################################################################
# Test ReorderFields()

def ogr_gpkg_37():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    dbname = '/vsimem/ogr_gpkg_37.gpkg'
    ds = gdaltest.gpkg_dr.CreateDataSource(dbname)
    lyr = ds.CreateLayer('test', geom_type = ogr.wkbPolygon )
    lyr.CreateField( ogr.FieldDefn('foo', ogr.OFTString) )
    lyr.CreateField( ogr.FieldDefn('bar', ogr.OFTString) )
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

    if lyr.TestCapability(ogr.OLCReorderFields) != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    with gdaltest.error_handler():
        ret = lyr.ReorderFields([-1, -1])
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'


    if lyr.ReorderFields([1,0]) != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr.ResetReading()
    if lyr.GetLayerDefn().GetFieldIndex('foo') != 1 or lyr.GetLayerDefn().GetFieldIndex('bar') != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f.GetFID() != 10 or f['foo'] != 'fooval' or f['bar'] != 'barval' or \
    f.GetGeometryRef().ExportToWkt() != 'POLYGON ((0 0,0 1,1 1,0 0))':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    # Check that index has been recreated
    sql_lyr = ds.ExecuteSQL("SELECT * FROM sqlite_master WHERE name = 'my_idx_foo' OR name = 'my_idx_bar'")
    if sql_lyr.GetFeatureCount() != 2:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)

    ds = None

    # Try on read-only dataset
    ds = ogr.Open(dbname)
    lyr = ds.GetLayer(0)
    with gdaltest.error_handler():
        ret = lyr.ReorderFields([1,0])
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdaltest.gpkg_dr.DeleteDataSource(dbname)

    return 'success'

###############################################################################
# Test GetExtent() and RECOMPUTE EXTENT ON

def ogr_gpkg_38():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    dbname = '/vsimem/ogr_gpkg_38.gpkg'
    ds = gdaltest.gpkg_dr.CreateDataSource(dbname)
    lyr = ds.CreateLayer('test', geom_type = ogr.wkbLineString )
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('LINESTRING (1 2,3 4)'))
    lyr.CreateFeature(f)
    ds = None

    # Simulate that extent is not recorded
    ds = ogr.Open(dbname, update = 1)
    ds.ExecuteSQL('UPDATE gpkg_contents SET min_x = NULL, min_y = NULL, max_x = NULL, max_y = NULL')
    ds = None

    ds = ogr.Open(dbname, update = 1)
    lyr = ds.GetLayer(0)
    extent = lyr.GetExtent(force = 0, can_return_null = True)
    if extent is not None:
        gdaltest.post_reason('fail')
        print(extent)
        return 'fail'
    # Test that we can compute the extent of a layer that has none registered in gpkg_contents
    extent = lyr.GetExtent(force = 1)
    if extent != (1,3,2,4):
        gdaltest.post_reason('fail')
        print(extent)
        return 'fail'
    sql_lyr = ds.ExecuteSQL('SELECT min_x, min_y, max_x, max_y FROM gpkg_contents')
    f = sql_lyr.GetNextFeature()
    if f['min_x'] != 1 or f['min_y'] != 2 or f['max_x'] != 3 or f['max_y'] != 4:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)
    extent = lyr.GetExtent(force = 0)
    if extent != (1,3,2,4):
        gdaltest.post_reason('fail')
        print(extent)
        return 'fail'

    # Modify feature
    f = lyr.GetFeature(1)
    f.SetGeometryDirectly(ogr.CreateGeometryFromWkt('LINESTRING (-1 -2,-3 -4)'))
    lyr.SetFeature(f)

    # The extent has grown
    extent = lyr.GetExtent(force = 0)
    if extent != (-3.0, 3.0, -4.0, 4.0):
        gdaltest.post_reason('fail')
        print(extent)
        return 'fail'

    ds.ExecuteSQL('RECOMPUTE EXTENT ON test')
    extent = lyr.GetExtent(force = 0)
    if extent != (-3.0, -1.0, -4.0, -2.0):
        gdaltest.post_reason('fail')
        print(extent)
        return 'fail'
    ds = None

    ds = ogr.Open(dbname)
    lyr = ds.GetLayer(0)
    extent = lyr.GetExtent(force = 0)
    if extent != (-3.0, -1.0, -4.0, -2.0):
        gdaltest.post_reason('fail')
        print(extent)
        return 'fail'
    ds = None

    ds = ogr.Open(dbname, update = 1)
    lyr = ds.GetLayer(0)
    # Delete last feature
    lyr.DeleteFeature(1)

    # This should cancel NULLify the extent in gpkg_contents
    ds.ExecuteSQL('RECOMPUTE EXTENT ON test')
    extent = lyr.GetExtent(force = 0, can_return_null = True)
    if extent is not None:
        gdaltest.post_reason('fail')
        print(extent)
        return 'fail'
    ds = None

    ds = ogr.Open(dbname)
    lyr = ds.GetLayer(0)
    extent = lyr.GetExtent(force = 0, can_return_null = True)
    if extent is not None:
        gdaltest.post_reason('fail')
        print(extent)
        return 'fail'
    ds = None

    gdaltest.gpkg_dr.DeleteDataSource(dbname)

    return 'success'

###############################################################################
# Test checking of IDENTIFIER unicity

def ogr_gpkg_39():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    dbname = '/vsimem/ogr_gpkg_39.gpkg'
    ds = gdaltest.gpkg_dr.CreateDataSource(dbname)

    ds.CreateLayer('test' )

    lyr = ds.CreateLayer('test_with_explicit_identifier', options = ['IDENTIFIER=explicit_identifier'] )
    if lyr is None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Allow overwriting
    lyr = ds.CreateLayer('test_with_explicit_identifier', options = ['IDENTIFIER=explicit_identifier', 'OVERWRITE=YES'] )
    if lyr is None:
        gdaltest.post_reason('fail')
        return 'fail'

    with gdaltest.error_handler():
        lyr = ds.CreateLayer('test2', options = ['IDENTIFIER=test'] )
    if lyr is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    with gdaltest.error_handler():
        lyr = ds.CreateLayer('test2', options = ['IDENTIFIER=explicit_identifier'] )
    if lyr is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds.ExecuteSQL("INSERT INTO gpkg_contents ( table_name, identifier, data_type ) VALUES ( 'some_table', 'another_identifier', 'some_data_type' )")
    with gdaltest.error_handler():
        lyr = ds.CreateLayer('test2', options = ['IDENTIFIER=another_identifier'] )
    if lyr is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdaltest.gpkg_dr.DeleteDataSource(dbname)

    return 'success'

###############################################################################
# Run creating a non-spatial layer that is registered as 'attributes' and
# read it back

def ogr_gpkg_40():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_40.gpkg')
    ds.CreateLayer('aspatial', geom_type = ogr.wkbNone, options = ['ASPATIAL_VARIANT=GPKG_ATTRIBUTES'] )
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_40.gpkg')
    if ds.GetLayerCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_contents WHERE table_name != 'ogr_empty_table'")
    if sql_lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)
    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_geometry_columns WHERE table_name != 'ogr_empty_table'")
    if sql_lyr.GetFeatureCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)
    sql_lyr = ds.ExecuteSQL("SELECT * FROM sqlite_master WHERE name = 'gpkg_extensions'")
    if sql_lyr.GetFeatureCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    if not validate('/vsimem/ogr_gpkg_40.gpkg'):
        gdaltest.post_reason('validation failed')
        return 'fail'

    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_40.gpkg')

    return 'success'

###############################################################################
# Test tables without integer primary key (#6799), and unrecognized column type

def ogr_gpkg_41():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_41.gpkg')
    ds.ExecuteSQL('CREATE TABLE foo (mycol VARCHAR_ILLEGAL)')
    ds.ExecuteSQL("INSERT INTO foo VALUES ('myval')")
    ds.ExecuteSQL("INSERT INTO gpkg_contents (table_name,data_type,identifier,description,last_change,srs_id) VALUES ('foo','attributes','foo','','',0)")
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_41.gpkg')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['mycol'] != 'myval' or f.GetFID() != 1:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_41.gpkg')
    lyr = ds.GetLayer(0)
    f = lyr.GetFeature(1)
    if f['mycol'] != 'myval' or f.GetFID() != 1:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds = None

    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_41.gpkg')

    return 'success'

###############################################################################
# Test feature_count

def foo_has_trigger(ds):
    sql_lyr = ds.ExecuteSQL(
        "SELECT COUNT(*) FROM sqlite_master WHERE name = 'trigger_insert_feature_count_foo'", dialect = 'DEBUG')
    f = sql_lyr.GetNextFeature()
    has_trigger = f.GetField(0) == 1
    f = None
    ds.ReleaseResultSet(sql_lyr)
    return has_trigger

def get_feature_count_from_gpkg_contents(ds):
    sql_lyr = ds.ExecuteSQL('SELECT feature_count FROM gpkg_ogr_contents', dialect = 'DEBUG')
    f = sql_lyr.GetNextFeature()
    val = f.GetField(0)
    f = None
    ds.ReleaseResultSet(sql_lyr)
    return val

def ogr_gpkg_42():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_42.gpkg')
    lyr = ds.CreateLayer('foo', geom_type = ogr.wkbNone)
    lyr.CreateField( ogr.FieldDefn('i', ogr.OFTInteger) )
    for i in range(5):
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField(0, i)
        lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_42.gpkg')
    lyr = ds.GetLayer(0)
    if get_feature_count_from_gpkg_contents(ds) != 5:
        gdaltest.post_reason('fail')
        return 'fail'
    if not foo_has_trigger(ds):
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.TestCapability(ogr.OLCFastFeatureCount) == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_42.gpkg', update = 1)
    lyr = ds.GetLayer(0)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField(0, 10)
    lyr.CreateFeature(f)

    # Has been invalidated for now
    if get_feature_count_from_gpkg_contents(ds) is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    if foo_has_trigger(ds):
        gdaltest.post_reason('fail')
        return 'fail'

    fc = lyr.GetFeatureCount()
    if fc != 6:
        gdaltest.post_reason('fail')
        print(fc)
        return 'fail'

    ds.ExecuteSQL('DELETE FROM foo WHERE i = 1')

    if not foo_has_trigger(ds):
        gdaltest.post_reason('fail')
        return 'fail'

    if get_feature_count_from_gpkg_contents(ds) is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    fc = lyr.GetFeatureCount()
    if fc != 5:
        gdaltest.post_reason('fail')
        print(fc)
        return 'fail'

    if get_feature_count_from_gpkg_contents(ds) != 5:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_42.gpkg', update = 1)
    lyr = ds.GetLayer(0)
    fc = lyr.GetFeatureCount()
    if fc != 5:
        gdaltest.post_reason('fail')
        print(fc)
        return 'fail'
    ds.ExecuteSQL('UPDATE gpkg_ogr_contents SET feature_count = NULL')
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_42.gpkg', update = 1)
    lyr = ds.GetLayer(0)
    if get_feature_count_from_gpkg_contents(ds) is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    fc = lyr.GetFeatureCount()
    if fc != 5:
        gdaltest.post_reason('fail')
        print(fc)
        return 'fail'
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_42.gpkg', update = 1)
    if get_feature_count_from_gpkg_contents(ds) != 5:
        gdaltest.post_reason('fail')
        return 'fail'

    # So as to test that we really read from gpkg_ogr_contents
    ds.ExecuteSQL('UPDATE gpkg_ogr_contents SET feature_count = 5000')

    ds = ogr.Open('/vsimem/ogr_gpkg_42.gpkg', update = 1)
    lyr = ds.GetLayer(0)
    fc = lyr.GetFeatureCount()
    if fc != 5000:
        gdaltest.post_reason('fail')
        print(fc)
        return 'fail'

    # Test renaming
    ds.ExecuteSQL('ALTER TABLE foo RENAME TO bar')
    ds = None
    ds = ogr.Open('/vsimem/ogr_gpkg_42.gpkg', update = 1)
    sql_lyr = ds.ExecuteSQL("SELECT feature_count FROM gpkg_ogr_contents WHERE table_name = 'bar'", dialect = 'DEBUG')
    f = sql_lyr.GetNextFeature()
    val = f.GetField(0)
    f = None
    ds.ReleaseResultSet(sql_lyr)
    if val != 5000:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test layer deletion
    ds.DeleteLayer(0)
    sql_lyr = ds.ExecuteSQL("SELECT feature_count FROM gpkg_ogr_contents WHERE table_name != 'ogr_empty_table'", dialect = 'DEBUG')
    f = sql_lyr.GetNextFeature()
    if f is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    # Test without feature_count column
    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_42.gpkg',
                            options = ['ADD_GPKG_OGR_CONTENTS=FALSE'])
    lyr = ds.CreateLayer('foo', geom_type = ogr.wkbNone)
    lyr.CreateField( ogr.FieldDefn('i', ogr.OFTInteger) )
    for i in range(5):
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetField(0, i)
        lyr.CreateFeature(f)
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_42.gpkg', update = 1)

    # Check that feature_count column is missing
    sql_lyr = ds.ExecuteSQL('PRAGMA table_info(gpkg_contents)')
    fc = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    if fc != 10:
        gdaltest.post_reason('fail')
        return 'fail'

    if foo_has_trigger(ds):
        gdaltest.post_reason('fail')
        return 'fail'

    lyr = ds.GetLayer(0)
    if lyr.TestCapability(ogr.OLCFastFeatureCount) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    fc = lyr.GetFeatureCount()
    if fc != 5:
        gdaltest.post_reason('fail')
        print(fc)
        return 'fail'
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField(0, 10)
    lyr.CreateFeature(f)
    lyr = ds.GetLayer(0)
    fc = lyr.GetFeatureCount()
    if fc != 6:
        gdaltest.post_reason('fail')
        print(fc)
        return 'fail'
    ds.ExecuteSQL('DELETE FROM foo WHERE i = 1')
    fc = lyr.GetFeatureCount()
    if fc != 5:
        gdaltest.post_reason('fail')
        print(fc)
        return 'fail'
    ds = None


    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_42.gpkg')

    return 'success'


###############################################################################
# Test limitations on number of tables

def ogr_gpkg_43():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_43.gpkg')
    ds.StartTransaction()
    for i in range(1001):
        ds.ExecuteSQL("INSERT INTO gpkg_contents (table_name, data_type, identifier) " +
                      "VALUES ('tiles%d', 'tiles', 'tiles%d')" % (i+1,i+1))
        ds.ExecuteSQL("INSERT INTO gpkg_tile_matrix_set VALUES " +
                      "('tiles%d', 0, 440720, 3750120, 441920, 3751320)" % (i+1))
    for i in range(1001):
        ds.ExecuteSQL("INSERT INTO gpkg_contents (table_name, data_type, identifier) " +
                      "VALUES ('attr%d', 'attributes', 'attr%d')" % (i+1,i+1))
        ds.ExecuteSQL("CREATE TABLE attr%d (id INTEGER PRIMARY KEY AUTOINCREMENT)" % (i+1))
    ds.CommitTransaction()
    ds = None

    with gdaltest.error_handler():
        ds = gdal.Open('/vsimem/ogr_gpkg_43.gpkg')
    if len(ds.GetMetadata_List('SUBDATASETS')) != 2 * 1000:
        gdaltest.post_reason('fail')
        print(len(ds.GetMetadata_List('SUBDATASETS')))
        return 'fail'
    ds = None
    gdal.SetConfigOption('OGR_TABLE_LIMIT', '1000')
    with gdaltest.error_handler():
        ds = ogr.Open('/vsimem/ogr_gpkg_43.gpkg')
    gdal.SetConfigOption('OGR_TABLE_LIMIT', None)
    if ds.GetLayerCount() != 1000:
        gdaltest.post_reason('fail')
        print(ds.GetLayerCount())
        return 'fail'
    ds = None

    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_43.gpkg')

    return 'success'


###############################################################################
# Test GeoPackage without metadata table

def ogr_gpkg_44():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    gdal.SetConfigOption('CREATE_METADATA_TABLES', 'NO')
    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_44.gpkg')
    ds.CreateLayer('foo')
    ds = None
    gdal.SetConfigOption('CREATE_METADATA_TABLES', None)

    if not validate('/vsimem/ogr_gpkg_44.gpkg'):
        gdaltest.post_reason('validation failed')
        return 'fail'

    ds = ogr.Open('/vsimem/ogr_gpkg_44.gpkg')
    md = ds.GetMetadata()
    if md != {}:
        gdaltest.post_reason('fail')
        print(md)
        return 'fail'
    md = ds.GetLayer(0).GetMetadata()
    if md != {}:
        gdaltest.post_reason('fail')
        print(md)
        return 'fail'
    sql_lyr = ds.ExecuteSQL("SELECT * FROM sqlite_master WHERE name = 'gpkg_metadata'")
    fc = sql_lyr.GetFeatureCount()
    ds.ReleaseResultSet(sql_lyr)
    if fc != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_44.gpkg', update = 1)
    ds.SetMetadataItem('FOO', 'BAR')
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_44.gpkg')
    md = ds.GetMetadata()
    if md != { 'FOO': 'BAR' }:
        gdaltest.post_reason('fail')
        print(md)
        return 'fail'
    ds = None

    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_44.gpkg')

    return 'success'


###############################################################################
# Test non conformant GeoPackage: table with non INTEGER PRIMARY KEY

def ogr_gpkg_45():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_45.gpkg')
    ds.ExecuteSQL('CREATE TABLE test (a INTEGER, b INTEGER, CONSTRAINT pkid_constraint PRIMARY KEY (a, b))')
    ds.ExecuteSQL("INSERT INTO gpkg_contents ( table_name, identifier, data_type ) VALUES ( 'test', 'test', 'attributes' )")
    ds = None
    ds = ogr.Open('/vsimem/ogr_gpkg_45.gpkg')
    lyr = ds.GetLayer(0)
    if lyr.GetFIDColumn() != '':
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldCount() != 2:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_45.gpkg')

    return 'success'

###############################################################################
# Test spatial view and spatial index

def ogr_gpkg_46():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_46.gpkg')
    lyr = ds.CreateLayer('foo')
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(0 0)'))
    lyr.CreateFeature(f)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1 1)'))
    lyr.CreateFeature(f)
    ds.ExecuteSQL('CREATE VIEW my_view AS SELECT geom AS my_geom, fid AS my_fid FROM foo')
    ds.ExecuteSQL("INSERT INTO gpkg_contents (table_name, identifier, data_type, srs_id) VALUES ( 'my_view', 'my_view', 'features', 0 )")
    ds.ExecuteSQL("INSERT INTO gpkg_geometry_columns (table_name, column_name, geometry_type_name, srs_id, z, m) values ('my_view', 'my_geom', 'GEOMETRY', 0, 0, 0)")

    ds.ExecuteSQL("CREATE VIEW my_view2 AS SELECT geom, fid AS OGC_FID, 'bla' as another_column FROM foo")
    ds.ExecuteSQL("INSERT INTO gpkg_contents (table_name, identifier, data_type, srs_id) VALUES ( 'my_view2', 'my_view2', 'features', 0 )")
    ds.ExecuteSQL("INSERT INTO gpkg_geometry_columns (table_name, column_name, geometry_type_name, srs_id, z, m) values ('my_view2', 'my_geom', 'GEOMETRY', 0, 0, 0)")

    ds.ExecuteSQL('CREATE VIEW my_view3 AS SELECT a.fid, a.geom, b.fid as fid2 FROM foo a, foo b')
    ds.ExecuteSQL("INSERT INTO gpkg_contents (table_name, identifier, data_type, srs_id) VALUES ( 'my_view3', 'my_view3', 'features', 0 )")
    ds.ExecuteSQL("INSERT INTO gpkg_geometry_columns (table_name, column_name, geometry_type_name, srs_id, z, m) values ('my_view3', 'my_geom', 'GEOMETRY', 0, 0, 0)")

    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_46.gpkg', update = 1)
    lyr = ds.GetLayerByName('my_view')
    if lyr.GetLayerDefn().GetFieldCount() != 1:
        gdaltest.post_reason('fail')
        print(lyr.GetLayerDefn().GetFieldCount())
        return 'fail'
    if lyr.GetGeometryColumn() != 'my_geom':
        gdaltest.post_reason('fail')
        print(lyr.GetGeometryColumn())
        return 'fail'

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
        print('SQLite likely built without SQLITE_HAS_COLUMN_METADATA')
        return 'skip'

    # Effectively test spatial index
    lyr.SetSpatialFilterRect(-0.5,-0.5,0.5,0.5)
    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # View with FID
    lyr = ds.GetLayerByName('my_view2')
    if lyr.GetLayerDefn().GetFieldCount() != 1:
        gdaltest.post_reason('fail')
        print(lyr.GetLayerDefn().GetFieldCount())
        return 'fail'
    if lyr.GetFIDColumn() != 'OGC_FID':
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f.GetFID() != 1 or f.GetField(0) != 'bla':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    # View without valid rowid
    lyr = ds.GetLayerByName('my_view3')
    if lyr.GetLayerDefn().GetFieldCount() != 2:
        gdaltest.post_reason('fail')
        print(lyr.GetLayerDefn().GetFieldCount())
        return 'fail'
    f = lyr.GetNextFeature()
    if f.GetFID() != 0:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f = lyr.GetNextFeature()
    if f.GetFID() != 1:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f2 = lyr.GetFeature(1)
    if not f.Equal(f2):
        gdaltest.post_reason('fail')
        f.DumpReadable()
        f2.DumpReadable()
        return 'fail'

    ds = None

    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_46.gpkg')

    return 'success'

###############################################################################
# Test corner case of Identify()

def ogr_gpkg_47():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_47.gpkg')
    # Set wrong application_id
    fp = gdal.VSIFOpenL('/vsimem/ogr_gpkg_47.gpkg', 'rb+')
    gdal.VSIFSeekL(fp, 68, 0)
    gdal.VSIFWriteL(struct.pack('B'*4,0,0,0,0), 4, 1, fp)
    gdal.VSIFCloseL(fp)

    with gdaltest.error_handler():
        ds = ogr.Open('/vsimem/ogr_gpkg_47.gpkg', update = 1)
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.SetConfigOption('GPKG_WARN_UNRECOGNIZED_APPLICATION_ID', 'NO')
    ogr.Open('/vsimem/ogr_gpkg_47.gpkg')
    gdal.SetConfigOption('GPKG_WARN_UNRECOGNIZED_APPLICATION_ID', None)
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'

    gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_47.gpkg', options = ['VERSION=1.2'])
    # Set wrong user_version
    fp = gdal.VSIFOpenL('/vsimem/ogr_gpkg_47.gpkg', 'rb+')
    gdal.VSIFSeekL(fp, 60, 0)
    gdal.VSIFWriteL(struct.pack('B'*4,0,0,0,0), 4, 1, fp)
    gdal.VSIFCloseL(fp)

    with gdaltest.error_handler():
        ds = ogr.Open('/vsimem/ogr_gpkg_47.gpkg', update = 1)
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.SetConfigOption('GPKG_WARN_UNRECOGNIZED_APPLICATION_ID', 'NO')
    ogr.Open('/vsimem/ogr_gpkg_47.gpkg')
    gdal.SetConfigOption('GPKG_WARN_UNRECOGNIZED_APPLICATION_ID', None)
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'

    # Set GPKG 1.2.1
    gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_47.gpkg', options = ['VERSION=1.2'])
    # Set user_version
    fp = gdal.VSIFOpenL('/vsimem/ogr_gpkg_47.gpkg', 'rb+')
    gdal.VSIFSeekL(fp, 60, 0)
    gdal.VSIFWriteL(struct.pack('B'*4,0,0,0x27,0xD9), 4, 1, fp)
    gdal.VSIFCloseL(fp)

    ds = ogr.Open('/vsimem/ogr_gpkg_47.gpkg', update = 1)
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.SetConfigOption('GPKG_WARN_UNRECOGNIZED_APPLICATION_ID', 'NO')
    ogr.Open('/vsimem/ogr_gpkg_47.gpkg')
    gdal.SetConfigOption('GPKG_WARN_UNRECOGNIZED_APPLICATION_ID', None)
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'

    # Set GPKG 1.3.0
    gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_47.gpkg', options = ['VERSION=1.2'])
    # Set user_version
    fp = gdal.VSIFOpenL('/vsimem/ogr_gpkg_47.gpkg', 'rb+')
    gdal.VSIFSeekL(fp, 60, 0)
    gdal.VSIFWriteL(struct.pack('B'*4,0,0,0x28,0x3C), 4, 1, fp)
    gdal.VSIFCloseL(fp)

    with gdaltest.error_handler():
        ds = ogr.Open('/vsimem/ogr_gpkg_47.gpkg', update = 1)
    if ds is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'

    gdal.SetConfigOption('GPKG_WARN_UNRECOGNIZED_APPLICATION_ID', 'NO')
    ogr.Open('/vsimem/ogr_gpkg_47.gpkg')
    gdal.SetConfigOption('GPKG_WARN_UNRECOGNIZED_APPLICATION_ID', None)
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'

    # Just for the sake of coverage testing in DEBUG mode
    with gdaltest.error_handler():
        gdaltest.gpkg_dr.CreateDataSource('/vsimem/.cur_input')
    # Set wrong application_id
    fp = gdal.VSIFOpenL('/vsimem/.cur_input', 'rb+')
    gdal.VSIFSeekL(fp, 68, 0)
    gdal.VSIFWriteL(struct.pack('B'*4,0,0,0,0), 4, 1, fp)
    gdal.VSIFCloseL(fp)
    ogr.Open('/vsimem/.cur_input')
    gdal.Unlink('/vsimem/.cur_input')

    with gdaltest.error_handler():
        gdaltest.gpkg_dr.CreateDataSource('/vsimem/.cur_input', options = ['VERSION=1.2'])
    # Set wrong user_version
    fp = gdal.VSIFOpenL('/vsimem/.cur_input', 'rb+')
    gdal.VSIFSeekL(fp, 60, 0)
    gdal.VSIFWriteL(struct.pack('B'*4,0,0,0,0), 4, 1, fp)
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
    if ds.GetDriver().GetName() != 'GPKG':
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/ogr_gpkg_47.zip')
    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_47.gpkg')

    return 'success'

###############################################################################
# Test insertion of features with unset fields

def ogr_gpkg_48():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_48.gpkg')
    lyr = ds.CreateLayer('foo')
    lyr.CreateField( ogr.FieldDefn('a') )
    lyr.CreateField( ogr.FieldDefn('b') )
    lyr.CreateField( ogr.FieldDefn('c') )
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
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f = lyr.GetNextFeature()
    if f.GetField('b') != 'b' or f.GetField('c') != 'c' or f.GetField('a') is not None:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    # No geom field, one single field with default value
    lyr = ds.CreateLayer('default_field_no_geom', geom_type = ogr.wkbNone)
    fld_defn = ogr.FieldDefn('foo')
    fld_defn.SetDefault('x')
    lyr.CreateField(fld_defn)
    f = ogr.Feature(lyr.GetLayerDefn())
    if lyr.CreateFeature(f) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f.GetField('foo') != 'x':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(1)
    if lyr.SetFeature(f) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f.GetField('foo') != 'x':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds = None

    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_48.gpkg')

    return 'success'

###############################################################################
# Test CreateGeomField() on a attributes layer

def ogr_gpkg_49():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_49.gpkg')

    lyr = ds.CreateLayer('test', geom_type = ogr.wkbNone,
                         options = [ 'ASPATIAL_VARIANT=GPKG_ATTRIBUTES' ])

    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    f = None

    field_defn = ogr.GeomFieldDefn('', ogr.wkbPoint)
    if lyr.CreateGeomField(field_defn) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_49.gpkg')

    return 'success'

###############################################################################
# Test minimalistic support of definition_12_063

def ogr_gpkg_50():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    gdal.SetConfigOption('GPKG_ADD_DEFINITION_12_063', 'YES')
    gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_50.gpkg')
    gdal.SetConfigOption('GPKG_ADD_DEFINITION_12_063', None)

    ds = ogr.Open('/vsimem/ogr_gpkg_50.gpkg', update = 1)
    srs32631 = osr.SpatialReference()
    srs32631.ImportFromEPSG( 32631 )
    ds.CreateLayer('test', srs = srs32631)

    # No authority node
    srs_without_org = osr.SpatialReference()
    srs_without_org.SetFromUserInput("""GEOGCS["another geogcs",
    DATUM["another datum",
        SPHEROID["another spheroid",1000,0]]]""")
    lyr = ds.CreateLayer('without_org', srs = srs_without_org)

    ds = None

    if not validate('/vsimem/ogr_gpkg_50.gpkg'):
        gdaltest.post_reason('validation failed')
        return 'fail'

    ds = ogr.Open('/vsimem/ogr_gpkg_50.gpkg')
    lyr = ds.GetLayer('test')
    if not lyr.GetSpatialRef().IsSame(srs32631):
        gdaltest.post_reason('fail')
        return 'fail'
    lyr = ds.GetLayer('without_org')
    if not lyr.GetSpatialRef().IsSame(srs_without_org):
        gdaltest.post_reason('fail')
        return 'fail'
    sql_lyr = ds.ExecuteSQL('SELECT definition_12_063 FROM gpkg_spatial_ref_sys WHERE srs_id = 32631')
    f = sql_lyr.GetNextFeature()
    if f.GetField(0) != 'undefined':
        gdaltest.post_reason('fail')
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_50.gpkg')

    return 'success'

###############################################################################
# Test opening a .gpkg.sql file

def ogr_gpkg_51():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    if gdaltest.gpkg_dr.GetMetadataItem("ENABLE_SQL_GPKG_FORMAT") != 'YES':
        return 'skip'

    ds = ogr.Open('data/poly.gpkg.sql')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f is None:
        return 'fail'

    return 'success'

###############################################################################
# Test opening a .gpkg file

def ogr_gpkg_52():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    ds = ogr.Open('data/poly.gpkg')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f is None:
        return 'fail'

    return 'success'

###############################################################################
# Test opening a .gpkg file with inconsistency regarding table case (#6916)

def ogr_gpkg_53():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    if gdaltest.gpkg_dr.GetMetadataItem("ENABLE_SQL_GPKG_FORMAT") != 'YES':
        return 'skip'

    ds = ogr.Open('data/poly_inconsistent_case.gpkg.sql')
    if ds.GetLayerCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f is None:
        return 'fail'

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is not None:
        ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' data/poly_inconsistent_case.gpkg.sql')

        if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
            gdaltest.post_reason('fail')
            print(ret)
            return 'fail'

    return 'success'

###############################################################################
# Test editing of a database with 2 layers (https://issues.qgis.org/issues/17034)

def ogr_gpkg_54():

    if gdaltest.gpkg_dr is None:
        return 'skip'

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

    ds1 = ogr.Open(tmpfile, update = 1)
    ds2 = ogr.Open(tmpfile, update = 1)

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
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    lyr2 = ds.GetLayer(1)
    f = lyr2.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (7 8)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds = None

    gdal.Unlink(tmpfile)

    return 'success'

###############################################################################
# Test inserting geometries incompatible with declared layer geometry type

def ogr_gpkg_55():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    tmpfile = '/vsimem/ogr_gpkg_55.gpkg'
    ds = ogr.GetDriverByName('GPKG').CreateDataSource(tmpfile)
    lyr = ds.CreateLayer('layer1', geom_type=ogr.wkbLineString)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(0 0)'))
    gdal.ErrorReset()
    with gdaltest.error_handler():
        lyr.CreateFeature(f)
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('should have warned')
        return 'fail'
    f = None
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1 1)'))
    gdal.ErrorReset()
    lyr.CreateFeature(f)
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('should NOT have warned')
        return 'fail'
    f = None
    ds = None

    gdal.Unlink(tmpfile)

    return 'success'

###############################################################################
# Test FID identification on SQL result layer

def ogr_gpkg_56():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    ds = gdal.VectorTranslate('/vsimem/ogr_gpkg_56.gpkg', 'data/poly.shp', format = 'GPKG')
    lyr = ds.ExecuteSQL('select a.fid as fid1, b.fid as fid2 from poly a, poly b order by fid1, fid2')
    lyr.GetNextFeature()
    f = lyr.GetNextFeature()
    if f.GetField('fid1') != 1 or f.GetField('fid2') != 2:
        f.DumpReadable()
        return 'fail'
    ds.ReleaseResultSet(lyr)
    ds = None
    gdal.Unlink('/vsimem/ogr_gpkg_56.gpkg')

    return 'success'

###############################################################################
# Remove the test db from the tmp directory

def ogr_gpkg_cleanup():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    gdaltest.gpkg_ds = None

    if gdal.ReadDir('/vsimem') is not None:
        print(gdal.ReadDir('/vsimem'))
        for f in gdal.ReadDir('/vsimem'):
            gdal.Unlink('/vsimem/' + f)

    try:
        os.remove( 'tmp/gpkg_test.gpkg' )
    except:
        pass

    return 'success'

###############################################################################


gdaltest_list = [
    ogr_gpkg_1,
    ogr_gpkg_2,
    ogr_gpkg_3,
    ogr_gpkg_4,
    ogr_gpkg_5,
    ogr_gpkg_6,
    ogr_gpkg_7,
    ogr_gpkg_8,
    ogr_gpkg_9,
    ogr_gpkg_11,
    ogr_gpkg_12,
    ogr_gpkg_13,
    ogr_gpkg_14,
    ogr_gpkg_15,
    ogr_gpkg_16,
    ogr_gpkg_17,
    ogr_gpkg_18,
    ogr_gpkg_19,
    ogr_gpkg_20,
    ogr_gpkg_21,
    ogr_gpkg_22,
    ogr_gpkg_23,
    ogr_gpkg_24,
    ogr_gpkg_25,
    ogr_gpkg_26,
    ogr_gpkg_27,
    ogr_gpkg_28,
    ogr_gpkg_29,
    ogr_gpkg_30,
    ogr_gpkg_31,
    ogr_gpkg_32,
    ogr_gpkg_33,
    ogr_gpkg_34,
    ogr_gpkg_35,
    ogr_gpkg_36,
    ogr_gpkg_37,
    ogr_gpkg_38,
    ogr_gpkg_39,
    ogr_gpkg_40,
    ogr_gpkg_41,
    ogr_gpkg_42,
    ogr_gpkg_43,
    ogr_gpkg_44,
    ogr_gpkg_45,
    ogr_gpkg_46,
    ogr_gpkg_47,
    ogr_gpkg_48,
    ogr_gpkg_49,
    ogr_gpkg_50,
    ogr_gpkg_51,
    ogr_gpkg_52,
    ogr_gpkg_53,
    ogr_gpkg_54,
    ogr_gpkg_55,
    ogr_gpkg_56,
    ogr_gpkg_test_ogrsf,
    ogr_gpkg_cleanup,
]

# gdaltest_list = [ ogr_gpkg_1, ogr_gpkg_38, ogr_gpkg_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_gpkg' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

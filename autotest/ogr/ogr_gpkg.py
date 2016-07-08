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
import sys

# Make sure we run from the directory of the script
if os.path.basename(sys.argv[0]) == os.path.basename(__file__):
    if os.path.dirname(sys.argv[0]) != '':
        os.chdir(os.path.dirname(sys.argv[0]))

sys.path.append( '../pymod' )

from osgeo import gdal
from osgeo import ogr
from osgeo import osr
import gdaltest

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

    if gdaltest.gpkg_ds is not None:
        return 'success'
    else:
        return 'fail'

    gdaltest.gpkg_ds = None


###############################################################################
# Re-open database to test validity

def ogr_gpkg_2():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    gdaltest.gpkg_ds = gdaltest.gpkg_dr.Open( 'tmp/gpkg_test.gpkg', update = 1 )

    if gdaltest.gpkg_ds is not None:
        return 'success'
    else:
        return 'fail'


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

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    gdaltest.gpkg_ds = gdaltest.gpkg_dr.Open( 'tmp/gpkg_test.gpkg', update = 1 )
    gdal.PopErrorHandler()

    if gdaltest.gpkg_ds is None:
        return 'fail'

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

    # Random read a feature
    feat_read_random = lyr.GetFeature(feat.GetFID())
    if feat_read_random.GetField('dummy') != 'who you calling a dummy?':
        gdaltest.post_reason('random read output does not match input')
        return 'fail'

    # Random write a feature
    feat.SetField('dummy', 'i am no dummy')
    lyr.SetFeature(feat)
    feat_read_random = lyr.GetFeature(feat.GetFID())
    if feat_read_random.GetField('dummy') != 'i am no dummy':
        gdaltest.post_reason('random read output does not match random write input')
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

    lyr = gdaltest.gpkg_ds.CreateLayer('non_spatial', geom_type = ogr.wkbNone )
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
    if feat.IsFieldSet('fld_integer'):
        feat.DumpReadable()
        gdaltest.post_reason('fail')
        return 'fail'
    feat = lyr.GetNextFeature()
    if feat.GetField('fld_integer') != 1:
        feat.DumpReadable()
        gdaltest.post_reason('fail')
        return 'fail'

    # Test second aspatial layer
    lyr = gdaltest.gpkg_ds.CreateLayer('non_spatial2', geom_type = ogr.wkbNone )

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
    if feat.IsFieldSet('fld_integer'):
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

    lyr = gdaltest.gpkg_ds.CreateLayer('point-with-spi-and-dashes', geom_type = ogr.wkbPoint )
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
    if feat.IsFieldSet(0) or feat.IsFieldSet(1) or feat.IsFieldSet(2) or \
       feat.IsFieldSet(3) or feat.IsFieldSet(4) or feat.IsFieldSet(5) or feat.IsFieldSet(6):
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
            ("SELECT DisableSpatialIndex('point-with-spi-and-dashes', 'geom')", 1),
            ("SELECT DisableSpatialIndex('point-with-spi-and-dashes', 'geom')", 0),
            ("SELECT CreateSpatialIndex('point-with-spi-and-dashes', 'geom')", 1),
            ("SELECT CreateSpatialIndex('point-with-spi-and-dashes', 'geom')", 0),
            ("SELECT CreateSpatialIndex('point-with-spi-and-dashes', NULL)", 0),
            ("SELECT CreateSpatialIndex(NULL, 'geom')", 0),
            ("SELECT CreateSpatialIndex('bla', 'geom')", 0),
            ("SELECT CreateSpatialIndex('point-with-spi-and-dashes', 'bla')", 0),
            ("SELECT DisableSpatialIndex('point-with-spi-and-dashes', NULL)", 0),
            ("SELECT DisableSpatialIndex(NULL, 'geom')", 0),
            ("SELECT DisableSpatialIndex('bla', 'geom')", 0),
            ("SELECT DisableSpatialIndex('point-with-spi-and-dashes', 'bla')", 0),
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

    gdal.Unlink('/vsimem/ogr_gpkg_19.gpkg')

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
    lyr = ds.CreateLayer('test3', geom_type = ogr.wkbNone)

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

    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_23.gpkg')

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

    # Doesn't work currently. Would require rewriting the whole table
    #field_defn = ogr.FieldDefn( 'field_datetime4', ogr.OFTDateTime )
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
       f.IsFieldSet('field_nodefault') or not f.IsFieldSet('field_datetime')  or \
       f.GetField('field_datetime2') != '2015/06/30 12:34:56+00' or \
       f.GetField('field_datetime4') != '2015/06/30 12:34:56.123+00' or \
       not f.IsFieldSet('field_datetime3') or \
       not f.IsFieldSet('field_date'):
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

    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_25.sqlite')

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
        ds = ogr.Open('/vsimem/ogr_gpkg_30.geopkg')
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

    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_31.gpkg')

    return 'success'

###############################################################################
# Run creating a non-spatial layer that isn't registered as 'aspatial' and
# read it back

def ogr_gpkg_32():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    ds = gdaltest.gpkg_dr.CreateDataSource('/vsimem/ogr_gpkg_32.gpkg')
    ds.CreateLayer('aspatial', geom_type = ogr.wkbNone, options = ['REGISTER_AS_ASPATIAL=NO'] )
    ds = None

    ds = ogr.Open('/vsimem/ogr_gpkg_32.gpkg')
    if ds.GetLayerCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    sql_lyr = ds.ExecuteSQL('SELECT * FROM gpkg_contents')
    if sql_lyr.GetFeatureCount() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)
    sql_lyr = ds.ExecuteSQL('SELECT * FROM gpkg_geometry_columns')
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
    sql_lyr = ds.ExecuteSQL("SELECT * FROM gpkg_contents WHERE last_change = '2000-01-01T:00:00:00.000Z'")
    if sql_lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)
    ds = None

    gdaltest.gpkg_dr.DeleteDataSource('/vsimem/ogr_gpkg_32.gpkg')

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
# Remove the test db from the tmp directory

def ogr_gpkg_cleanup():

    if gdaltest.gpkg_dr is None:
        return 'skip'

    gdaltest.gpkg_ds = None

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
    ogr_gpkg_test_ogrsf,
    ogr_gpkg_cleanup,
]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_gpkg' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

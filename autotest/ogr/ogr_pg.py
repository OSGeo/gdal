#!/usr/bin/env python
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
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import ogr
from osgeo import osr
from osgeo import gdal

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
# Open Database.

def ogr_pg_1():

    gdaltest.pg_ds = None
    gdaltest.pg_use_copy = gdal.GetConfigOption('PG_USE_COPY', None)
    val = gdal.GetConfigOption('OGR_PG_CONNECTION_STRING', None)
    if val is not None:
        gdaltest.pg_connection_string=val
    else:
        gdaltest.pg_connection_string='dbname=autotest'
    #gdaltest.pg_connection_string='dbname=autotest-postgis1.4'
    #gdaltest.pg_connection_string='dbname=autotest port=5432'
    #gdaltest.pg_connection_string='dbname=autotest-postgis2.0'
    #gdaltest.pg_connection_string='dbname=autotest host=127.0.0.1 port=5433 user=postgres'
    #gdaltest.pg_connection_string='dbname=autotest host=127.0.0.1 port=5434 user=postgres'
    #gdaltest.pg_connection_string='dbname=autotest port=5435 host=127.0.0.1'
    #7.4
    #gdaltest.pg_connection_string='dbname=autotest port=5436 user=postgres'

    try:
        gdaltest.pg_dr = ogr.GetDriverByName( 'PostgreSQL' )
    except:
        return 'skip'

    if gdaltest.pg_dr is None:
        return 'skip'

    try:
        gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
        gdaltest.pg_ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string, update = 1 )
        gdal.PopErrorHandler()
    except:
        gdaltest.pg_ds = None
        gdal.PopErrorHandler()

    if gdaltest.pg_ds is None:
        return 'skip'

    sql_lyr = gdaltest.pg_ds.ExecuteSQL('SELECT version()')
    feat = sql_lyr.GetNextFeature()
    version_str = feat.GetFieldAsString('version')
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

    gdaltest.pg_retrieve_fid = False
    if version_str[0:11] == "PostgreSQL ":
        if float(version_str[11:14]) >= 8.2:
            gdaltest.pg_retrieve_fid = True

    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    sql_lyr = gdaltest.pg_ds.ExecuteSQL('SHOW standard_conforming_strings')
    gdal.PopErrorHandler()
    gdaltest.pg_quote_with_E = sql_lyr is not None
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
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

    return 'success'

###############################################################################
# Create table from data/poly.shp

def ogr_pg_2():

    if gdaltest.pg_ds is None:
        return 'skip'

    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:tpoly' )
    gdal.PopErrorHandler()

    ######################################################
    # Create Layer
    gdaltest.pg_lyr = gdaltest.pg_ds.CreateLayer( 'tpoly',
                                                  options = [ 'DIM=3' ] )

    ######################################################
    # Check capabilities

    if gdaltest.pg_has_postgis:
        if not gdaltest.pg_lyr.TestCapability(ogr.OLCFastSpatialFilter):
            return 'fail'
        if not gdaltest.pg_lyr.TestCapability(ogr.OLCFastGetExtent):
            return 'fail'
    else:
        if gdaltest.pg_lyr.TestCapability(ogr.OLCFastSpatialFilter):
            return 'fail'
        if gdaltest.pg_lyr.TestCapability(ogr.OLCFastGetExtent):
            return 'fail'

    if not gdaltest.pg_lyr.TestCapability(ogr.OLCRandomRead):
        return 'fail'
    if not gdaltest.pg_lyr.TestCapability(ogr.OLCFastFeatureCount):
        return 'fail'
    if not gdaltest.pg_lyr.TestCapability(ogr.OLCFastSetNextByIndex):
        return 'fail'
    try:
        ogr.OLCStringsAsUTF8
        if not gdaltest.pg_lyr.TestCapability(ogr.OLCStringsAsUTF8):
            return 'fail'
    except:
        pass
    if not gdaltest.pg_lyr.TestCapability(ogr.OLCSequentialWrite):
        return 'fail'
    if not gdaltest.pg_lyr.TestCapability(ogr.OLCCreateField):
        return 'fail'
    if not gdaltest.pg_lyr.TestCapability(ogr.OLCRandomWrite):
        return 'fail'
    if not gdaltest.pg_lyr.TestCapability(ogr.OLCTransactions):
        return 'fail'

    ######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def( gdaltest.pg_lyr,
                                    [ ('AREA', ogr.OFTReal),
                                      ('EAS_ID', ogr.OFTInteger),
                                      ('PRFEDEA', ogr.OFTString),
                                      ('SHORTNAME', ogr.OFTString, 8),
                                      ('REALLIST', ogr.OFTRealList) ] )

    ######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature( feature_def = gdaltest.pg_lyr.GetLayerDefn() )

    shp_ds = ogr.Open( 'data/poly.shp' )
    shp_lyr = shp_ds.GetLayer(0)

    feat = shp_lyr.GetNextFeature()
    gdaltest.poly_feat = []

    expected_fid = 1

    while feat is not None:

        gdaltest.poly_feat.append( feat )

        dst_feat.SetFrom( feat )
        gdaltest.pg_lyr.CreateFeature( dst_feat )
        if gdaltest.pg_retrieve_fid:
            got_fid = dst_feat.GetFID()
            if got_fid != expected_fid:
                gdaltest.post_reason("didn't get expected fid : %d instead of %d" % (got_fid, expected_fid))
                return 'fail'

        expected_fid = expected_fid + 1

        feat = shp_lyr.GetNextFeature()

    dst_feat.Destroy()

    return 'success'

###############################################################################
# Verify that stuff we just wrote is still OK.

def ogr_pg_3():
    if gdaltest.pg_ds is None:
        return 'skip'

    if gdaltest.pg_lyr.GetFeatureCount() != 10:
        gdaltest.post_reason( 'GetFeatureCount() returned %d instead of 10' % gdaltest.pg_lyr.GetFeatureCount() )
        return 'fail'

    expect = [168, 169, 166, 158, 165]

    gdaltest.pg_lyr.SetAttributeFilter( 'eas_id < 170' )
    tr = ogrtest.check_features_against_list( gdaltest.pg_lyr,
                                              'eas_id', expect )

    if gdaltest.pg_lyr.GetFeatureCount() != 5:
        gdaltest.post_reason( 'GetFeatureCount() returned %d instead of 5' % gdaltest.pg_lyr.GetFeatureCount() )
        return 'fail'

    gdaltest.pg_lyr.SetAttributeFilter( None )

    for i in range(len(gdaltest.poly_feat)):
        orig_feat = gdaltest.poly_feat[i]
        read_feat = gdaltest.pg_lyr.GetNextFeature()

        if ogrtest.check_feature_geometry(read_feat,orig_feat.GetGeometryRef(),
                                          max_error = 0.001 ) != 0:
            return 'fail'

        for fld in range(3):
            if orig_feat.GetField(fld) != read_feat.GetField(fld):
                gdaltest.post_reason( 'Attribute %d does not match' % fld )
                return 'fail'

        read_feat.Destroy()
        orig_feat.Destroy()

    gdaltest.poly_feat = None

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Write more features with a bunch of different geometries, and verify the
# geometries are still OK.

def ogr_pg_4():

    if gdaltest.pg_ds is None:
        return 'skip'

    dst_feat = ogr.Feature( feature_def = gdaltest.pg_lyr.GetLayerDefn() )
    wkt_list = [ '10', '2', '1', '3d_1', '4', '5', '6' ]

    for item in wkt_list:

        wkt = open( 'data/wkb_wkt/'+item+'.wkt' ).read()
        geom = ogr.CreateGeometryFromWkt( wkt )

        ######################################################################
        # Write geometry as a new Oracle feature.

        dst_feat.SetGeometryDirectly( geom )
        dst_feat.SetField( 'PRFEDEA', item )
        dst_feat.SetFID(-1)
        gdaltest.pg_lyr.CreateFeature( dst_feat )

        ######################################################################
        # Read back the feature and get the geometry.

        gdaltest.pg_lyr.SetAttributeFilter( "PRFEDEA = '%s'" % item )
        feat_read = gdaltest.pg_lyr.GetNextFeature()
        geom_read = feat_read.GetGeometryRef()

        if ogrtest.check_feature_geometry( feat_read, geom ) != 0:
            print(item)
            print(wkt)
            print(geom_read)
            return 'fail'

        feat_read.Destroy()

    dst_feat.Destroy()
    gdaltest.pg_lyr.ResetReading() # to close implicit transaction

    return 'success'

###############################################################################
# Test ExecuteSQL() results layers without geometry.

def ogr_pg_5():

    if gdaltest.pg_ds is None:
        return 'skip'

    expect = [ None, 179, 173, 172, 171, 170, 169, 168, 166, 165, 158 ]

    sql_lyr = gdaltest.pg_ds.ExecuteSQL( 'select distinct eas_id from tpoly order by eas_id desc' )

    if sql_lyr.GetFeatureCount() != 11:
        gdaltest.post_reason( 'GetFeatureCount() returned %d instead of 11' % sql_lyr.GetFeatureCount() )
        return 'fail'

    tr = ogrtest.check_features_against_list( sql_lyr, 'eas_id', expect )

    gdaltest.pg_ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test ExecuteSQL() results layers with geometry.

def ogr_pg_6():

    if gdaltest.pg_ds is None:
        return 'skip'

    sql_lyr = gdaltest.pg_ds.ExecuteSQL( "select * from tpoly where prfedea = '2'" )

    tr = ogrtest.check_features_against_list( sql_lyr, 'prfedea', [ '2' ] )
    if tr:
        sql_lyr.ResetReading()
        feat_read = sql_lyr.GetNextFeature()
        if ogrtest.check_feature_geometry( feat_read, 'MULTILINESTRING ((5.00121349 2.99853132,5.00121349 1.99853133),(5.00121349 1.99853133,5.00121349 0.99853133),(3.00121351 1.99853127,5.00121349 1.99853133),(5.00121349 1.99853133,6.00121348 1.99853135))' ) != 0:
            tr = 0
        feat_read.Destroy()


    sql_lyr.ResetReading()

    geom = ogr.CreateGeometryFromWkt( \
        'LINESTRING(-10 -10,0 0)' )
    sql_lyr.SetSpatialFilter( geom )
    geom.Destroy()

    if sql_lyr.GetFeatureCount() != 0:
        gdaltest.post_reason( 'GetFeatureCount() returned %d instead of 0' % sql_lyr.GetFeatureCount() )
        return 'fail'

    if sql_lyr.GetNextFeature() != None:
        gdaltest.post_reason( 'GetNextFeature() did not return None' )
        return 'fail'

    gdaltest.pg_ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test spatial filtering.

def ogr_pg_7():

    if gdaltest.pg_ds is None:
        return 'skip'

    gdaltest.pg_lyr.SetAttributeFilter( None )

    geom = ogr.CreateGeometryFromWkt( \
        'LINESTRING(479505 4763195,480526 4762819)' )
    gdaltest.pg_lyr.SetSpatialFilter( geom )
    geom.Destroy()

    if gdaltest.pg_lyr.GetFeatureCount() != 1:
        gdaltest.post_reason( 'GetFeatureCount() returned %d instead of 1' % gdaltest.pg_lyr.GetFeatureCount() )
        return 'fail'

    tr = ogrtest.check_features_against_list( gdaltest.pg_lyr, 'eas_id',
                                              [ 158 ] )

    gdaltest.pg_lyr.SetAttributeFilter( 'eas_id = 158' )

    if gdaltest.pg_lyr.GetFeatureCount() != 1:
        gdaltest.post_reason( 'GetFeatureCount() returned %d instead of 1' % gdaltest.pg_lyr.GetFeatureCount() )
        return 'fail'

    gdaltest.pg_lyr.SetAttributeFilter( None )

    gdaltest.pg_lyr.SetSpatialFilter( None )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Write a feature with too long a text value for a fixed length text field.
# The driver should now truncate this (but with a debug message).  Also,
# put some crazy stuff in the value to verify that quoting and escaping
# is working smoothly.
#
# No geometry in this test.

def ogr_pg_8():

    if gdaltest.pg_ds is None:
        return 'skip'

    dst_feat = ogr.Feature( feature_def = gdaltest.pg_lyr.GetLayerDefn() )

    dst_feat.SetField( 'PRFEDEA', 'CrazyKey' )
    dst_feat.SetField( 'SHORTNAME', 'Crazy"\'Long' )
    gdaltest.pg_lyr.CreateFeature( dst_feat )
    dst_feat.Destroy()

    gdaltest.pg_lyr.SetAttributeFilter( "PRFEDEA = 'CrazyKey'" )
    feat_read = gdaltest.pg_lyr.GetNextFeature()

    if feat_read is None:
        gdaltest.post_reason( 'creating crazy feature failed!' )
        return 'fail'

    if feat_read.GetField( 'shortname' ) != 'Crazy"\'L':
        gdaltest.post_reason( 'Vvalue not properly escaped or truncated:' \
                              + feat_read.GetField( 'shortname' ) )
        return 'fail'

    feat_read.Destroy()

    return 'success'

###############################################################################
# Verify inplace update of a feature with SetFeature().

def ogr_pg_9():

    if gdaltest.pg_ds is None:
        return 'skip'

    gdaltest.pg_lyr.SetAttributeFilter( "PRFEDEA = 'CrazyKey'" )
    feat = gdaltest.pg_lyr.GetNextFeature()
    gdaltest.pg_lyr.SetAttributeFilter( None )

    feat.SetField( 'SHORTNAME', 'Reset' )

    point = ogr.Geometry( ogr.wkbPoint25D )
    point.SetPoint( 0, 5, 6, 7 )
    feat.SetGeometryDirectly( point )

    if gdaltest.pg_lyr.SetFeature( feat ) != 0:
        feat.Destroy()
        gdaltest.post_reason( 'SetFeature() method failed.' )
        return 'fail'

    fid = feat.GetFID()
    feat.Destroy()

    feat = gdaltest.pg_lyr.GetFeature( fid )
    if feat is None:
        gdaltest.post_reason( 'GetFeature(%d) failed.' % fid )
        return 'fail'

    shortname = feat.GetField( 'SHORTNAME' )
    if shortname[:5] != 'Reset':
        gdaltest.post_reason( 'SetFeature() did not update SHORTNAME, got %s.'\
                              % shortname )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'POINT(5 6 7)' ) != 0:
        print(feat.GetGeometryRef())
        gdaltest.post_reason( 'Geometry update failed' )
        return 'fail'

    try:
        ogr.OLCStringsAsUTF8
    except:
        # With OG-python bindings SetGeometryDirectly(None) doesn't work
        return 'success'

    feat.SetGeometryDirectly( None )

    if gdaltest.pg_lyr.SetFeature( feat ) != 0:
        feat.Destroy()
        gdaltest.post_reason( 'SetFeature() method failed.' )
        return 'fail'

    feat.Destroy()

    feat = gdaltest.pg_lyr.GetFeature( fid )
    if feat.GetGeometryRef() != None:
        print(feat.GetGeometryRef())
        gdaltest.post_reason( 'Geometry update failed. null geometry expected' )
        return 'fail'

    # Test updating non-existing feature
    feat.SetFID(-10)
    if gdaltest.pg_lyr.SetFeature( feat ) != ogr.OGRERR_NON_EXISTING_FEATURE:
        feat.Destroy()
        gdaltest.post_reason( 'Expected failure of SetFeature().' )
        return 'fail'

    feat.Destroy()

    return 'success'

###############################################################################
# Verify that DeleteFeature() works properly.

def ogr_pg_10():

    if gdaltest.pg_ds is None:
        return 'skip'

    gdaltest.pg_lyr.SetAttributeFilter( "PRFEDEA = 'CrazyKey'" )
    feat = gdaltest.pg_lyr.GetNextFeature()
    gdaltest.pg_lyr.SetAttributeFilter( None )

    fid = feat.GetFID()
    feat.Destroy()

    if gdaltest.pg_lyr.DeleteFeature( fid ) != 0:
        gdaltest.post_reason( 'DeleteFeature() method failed.' )
        return 'fail'

    gdaltest.pg_lyr.SetAttributeFilter( "PRFEDEA = 'CrazyKey'" )
    feat = gdaltest.pg_lyr.GetNextFeature()
    gdaltest.pg_lyr.SetAttributeFilter( None )

    if feat is not None:
        feat.Destroy()
        gdaltest.post_reason( 'DeleteFeature() seems to have had no effect.' )
        return 'fail'

    # Test deleting non-existing feature
    if gdaltest.pg_lyr.DeleteFeature( -10 ) != ogr.OGRERR_NON_EXISTING_FEATURE:
        gdaltest.post_reason( 'Expected failure of DeleteFeature().' )
        return 'fail'

    return 'success'

###############################################################################
# Create table from data/poly.shp in INSERT mode.

def ogr_pg_11():

    if gdaltest.pg_ds is None:
        return 'skip'

    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:tpolycopy' )
    gdal.PopErrorHandler()

    gdal.SetConfigOption( 'PG_USE_COPY', 'NO' )

    ######################################################
    # Create Layer
    gdaltest.pgc_lyr = gdaltest.pg_ds.CreateLayer( 'tpolycopy',
                                                  options = [ 'DIM=3' ] )

    ######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def( gdaltest.pgc_lyr,
                                    [ ('AREA', ogr.OFTReal),
                                      ('EAS_ID', ogr.OFTInteger),
                                      ('PRFEDEA', ogr.OFTString),
                                      ('SHORTNAME', ogr.OFTString, 8) ] )

    ######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature( feature_def = gdaltest.pgc_lyr.GetLayerDefn() )

    shp_ds = ogr.Open( 'data/poly.shp' )
    shp_lyr = shp_ds.GetLayer(0)

    feat = shp_lyr.GetNextFeature()
    gdaltest.poly_feat = []

    while feat is not None:

        gdaltest.poly_feat.append( feat )

        dst_feat.SetFrom( feat )
        gdaltest.pgc_lyr.CreateFeature( dst_feat )

        feat = shp_lyr.GetNextFeature()

    dst_feat.Destroy()

    gdal.SetConfigOption( 'PG_USE_COPY', gdaltest.pg_use_copy )

    return 'success'

###############################################################################
# Verify that stuff we just wrote is still OK.

def ogr_pg_12():
    if gdaltest.pg_ds is None:
        return 'skip'

    gdaltest.pgc_lyr.ResetReading()
    gdaltest.pgc_lyr.SetAttributeFilter( None )

    for i in range(len(gdaltest.poly_feat)):
        orig_feat = gdaltest.poly_feat[i]
        read_feat = gdaltest.pgc_lyr.GetNextFeature()

        if ogrtest.check_feature_geometry(read_feat,orig_feat.GetGeometryRef(),
                                          max_error = 0.001 ) != 0:
            return 'fail'

        for fld in range(3):
            if orig_feat.GetField(fld) != read_feat.GetField(fld):
                gdaltest.post_reason( 'Attribute %d does not match' % fld )
                return 'fail'

        read_feat.Destroy()
        orig_feat.Destroy()

    gdaltest.poly_feat = None
    gdaltest.pgc_lyr.ResetReading() # to close implicit transaction

    return 'success'

###############################################################################
# Create a table with some date fields.

def ogr_pg_13():

    if gdaltest.pg_ds is None:
        return 'skip'

    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:datetest' )
    gdal.PopErrorHandler()

    ######################################################
    # Create Table
    lyr = gdaltest.pg_ds.CreateLayer( 'datetest' )

    ######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def( lyr, [ ('ogrdate', ogr.OFTDate),
                                           ('ogrtime', ogr.OFTTime),
                                           ('ogrdatetime', ogr.OFTDateTime)] )

    ######################################################
    # add some custom date fields.
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE datetest ADD COLUMN tsz timestamp with time zone' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE datetest ADD COLUMN ts timestamp without time zone' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE datetest ADD COLUMN dt date' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE datetest ADD COLUMN tm time' )

    ######################################################
    # Create a populated records.
    gdaltest.pg_ds.ExecuteSQL( "INSERT INTO datetest ( ogrdate, ogrtime, ogrdatetime, tsz, ts, dt, tm) VALUES ( '2005-10-12 10:41:33-05', '2005-10-12 10:41:33-05', '2005-10-12 10:41:33-05', '2005-10-12 10:41:33-05','2005-10-12 10:41:33-05','2005-10-12 10:41:33-05','2005-10-12 10:41:33-05' )" )

    return 'success'

###############################################################################
# Verify that stuff we just wrote is still OK.
# Fetch in several timezones to test our timezone processing.

def ogr_pg_14():
    if gdaltest.pg_ds is None:
        return 'skip'

    ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string, update = 1 )

    ds.ExecuteSQL( 'set timezone to "UTC"' )

    lyr = ds.GetLayerByName( 'datetest' )

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('ogrdatetime') != '2005/10/12 15:41:33+00' \
       or feat.GetFieldAsString('ogrdate') != '2005/10/12' \
       or feat.GetFieldAsString('ogrtime') != '10:41:33' \
       or feat.GetFieldAsString('tsz') != '2005/10/12 15:41:33+00' \
       or feat.GetFieldAsString('ts') != '2005/10/12 10:41:33' \
       or feat.GetFieldAsString('dt') != '2005/10/12' \
       or feat.GetFieldAsString('tm') != '10:41:33':
        gdaltest.post_reason( 'UTC value wrong' )
        feat.DumpReadable()
        return 'fail'


    sql_lyr = ds.ExecuteSQL( "select * from pg_timezone_names where name = 'Canada/Newfoundland'" )
    if sql_lyr is None:
        has_tz = True
    else:
        has_tz = sql_lyr.GetFeatureCount() != 0
        ds.ReleaseResultSet(sql_lyr)

    if has_tz:
        ds.ExecuteSQL( 'set timezone to "Canada/Newfoundland"' )

        lyr.ResetReading()

        feat = lyr.GetNextFeature()

        if feat.GetFieldAsString('ogrdatetime') != '2005/10/12 13:11:33-0230' \
        or feat.GetFieldAsString('tsz') != '2005/10/12 13:11:33-0230' \
        or feat.GetFieldAsString('ts') != '2005/10/12 10:41:33' \
        or feat.GetFieldAsString('dt') != '2005/10/12' \
        or feat.GetFieldAsString('tm') != '10:41:33':
            gdaltest.post_reason( 'Newfoundland value wrong' )
            feat.DumpReadable()
            return 'fail'

    ds.ExecuteSQL( 'set timezone to "+5"' )

    lyr.ResetReading()

    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('ogrdatetime') != '2005/10/12 20:41:33+05' \
       or feat.GetFieldAsString('tsz') != '2005/10/12 20:41:33+05':
        gdaltest.post_reason( '+5 value wrong' )
        feat.DumpReadable()
        return 'fail'

    feat = None
    ds.Destroy()

    return 'success'

###############################################################################
# Test very large query.

def ogr_pg_15():

    if gdaltest.pg_ds is None:
        return 'skip'

    expect = [169]

    query = 'eas_id = 169'

    for id in range(1000):
        query = query + (' or eas_id = %d' % (id+1000))

    gdaltest.pg_lyr.SetAttributeFilter( query )
    tr = ogrtest.check_features_against_list( gdaltest.pg_lyr,
                                              'eas_id', expect )
    gdaltest.pg_lyr.SetAttributeFilter( None )

    if tr:
        return 'success'
    else:
        return 'fail'


###############################################################################
# Test very large statement.

def ogr_pg_16():

    if gdaltest.pg_ds is None:
        return 'skip'

    expect = [169]

    query = 'eas_id = 169'

    for id in range(1000):
        query = query + (' or eas_id = %d' % (id+1000))

    statement = 'select eas_id from tpoly where ' + query

    lyr = gdaltest.pg_ds.ExecuteSQL( statement )

    tr = ogrtest.check_features_against_list( lyr, 'eas_id', expect )

    gdaltest.pg_ds.ReleaseResultSet( lyr )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test requesting a non-existent table by name (bug 1480).

def ogr_pg_17():

    if gdaltest.pg_ds is None:
        return 'skip'

    count = gdaltest.pg_ds.GetLayerCount()
    try:
        layer = gdaltest.pg_ds.GetLayerByName( 'JunkTableName' )
    except:
        layer = None

    if layer is not None:
        gdaltest.post_reason( 'got layer for non-existent table!' )
        return 'fail'

    if count != gdaltest.pg_ds.GetLayerCount():
        gdaltest.post_reason( 'layer count changed unexpectedly.' )
        return 'fail'

    return 'success'

###############################################################################
# Test getting a layer by name that was not previously a layer.

def ogr_pg_18():

    if gdaltest.pg_ds is None or not gdaltest.pg_has_postgis:
        return 'skip'

    count = gdaltest.pg_ds.GetLayerCount()
    layer = gdaltest.pg_ds.GetLayerByName( 'geometry_columns' )
    if layer is None:
        gdaltest.post_reason( 'did not get geometry_columns layer' )
        return 'fail'

    if count+1 != gdaltest.pg_ds.GetLayerCount():
        gdaltest.post_reason( 'layer count unexpectedly unchanged.' )
        return 'fail'

    return 'success'

###############################################################################
# Test reading a layer extent

def ogr_pg_19():

    if gdaltest.pg_ds is None:
        return 'skip'

    layer = gdaltest.pg_ds.GetLayerByName( 'tpoly' )
    if layer is None:
        gdaltest.post_reason( 'did not get tpoly layer' )
        return 'fail'

    extent = layer.GetExtent()
    expect = ( 478315.53125, 481645.3125, 4762880.5, 4765610.5 )

    minx = abs(extent[0] - expect[0])
    maxx = abs(extent[1] - expect[1])
    miny = abs(extent[2] - expect[2])
    maxy = abs(extent[3] - expect[3])

    if max(minx, maxx, miny, maxy) > 0.0001:
        gdaltest.post_reason( 'Extents do not match' )
        print(extent)
        return 'fail'

    estimated_extent = layer.GetExtent(force = 0)
    if not gdaltest.pg_has_postgis:
        # The OGRLayer default implementation in force = 0 returns error
        if estimated_extent != (0, 0, 0, 0):
            gdaltest.post_reason( 'Wrong estimated extent' )
            print(extent)
            return 'fail'
    else:
        # Better testing needed ?
        if estimated_extent == (0, 0, 0, 0):
            gdaltest.post_reason( 'Wrong estimated extent' )
            print(extent)
            return 'fail'

    return 'success'

###############################################################################
# Test reading a SQL result layer extent

def ogr_pg_19_2():

    if gdaltest.pg_ds is None:
        return 'skip'

    sql_lyr = gdaltest.pg_ds.ExecuteSQL( 'select * from tpoly' )

    extent = sql_lyr.GetExtent()
    expect = ( 478315.53125, 481645.3125, 4762880.5, 4765610.5 )

    minx = abs(extent[0] - expect[0])
    maxx = abs(extent[1] - expect[1])
    miny = abs(extent[2] - expect[2])
    maxy = abs(extent[3] - expect[3])

    if max(minx, maxx, miny, maxy) > 0.0001:
        gdaltest.post_reason( 'Extents do not match' )
        return 'fail'

    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

    return 'success'

###############################################################################
# Test reading 4-dim geometry in EWKT format

def ogr_pg_20():

    if gdaltest.pg_ds is None or not gdaltest.pg_has_postgis:
        return 'skip'

    #
    # Prepare test layer with 4-dim geometries.
    #


    # Collection of test geometry pairs:
    # ( <EWKT>, <WKT> ) <=> ( <tested>, <expected> )
    geometries = (
        ( 'POINT (10 20 5 5)',
          'POINT ZM (10 20 5 5)' ),
        ( 'LINESTRING (10 10 1 2,20 20 3 4,30 30 5 6,40 40 7 8)',
          'LINESTRING ZM (10 10 1 2,20 20 3 4,30 30 5 6,40 40 7 8)' ),
        ( 'POLYGON ((0 0 1 2,4 0 3 4,4 4 5 6,0 4 7 8,0 0 1 2))',
          'POLYGON ZM ((0 0 1 2,4 0 3 4,4 4 5 6,0 4 7 8,0 0 1 2))' ),
        ( 'MULTIPOINT (10 20 5 5,30 30 7 7)',
          'MULTIPOINT ZM ((10 20 5 5),(30 30 7 7))' ),
        ( 'MULTILINESTRING ((10 10 1 2,20 20 3 4),(30 30 5 6,40 40 7 8))',
          'MULTILINESTRING ZM ((10 10 1 2,20 20 3 4),(30 30 5 6,40 40 7 8))' ),
        ( 'MULTIPOLYGON(((0 0 0 1,4 0 0 1,4 4 0 1,0 4 0 1,0 0 0 1),(1 1 0 5,2 1 0 5,2 2 0 5,1 2 0 5,1 1 0 5)),((-1 -1 0 10,-1 -2 0 10,-2 -2 0 10,-2 -1 0 10,-1 -1 0 10)))',
          'MULTIPOLYGON ZM (((0 0 0 1,4 0 0 1,4 4 0 1,0 4 0 1,0 0 0 1),(1 1 0 5,2 1 0 5,2 2 0 5,1 2 0 5,1 1 0 5)),((-1 -1 0 10,-1 -2 0 10,-2 -2 0 10,-2 -1 0 10,-1 -1 0 10)))' ),
        ( 'GEOMETRYCOLLECTION(POINT(2 3 11 101),LINESTRING(2 3 12 102,3 4 13 103))',
          'GEOMETRYCOLLECTION ZM (POINT ZM (2 3 11 101),LINESTRING ZM (2 3 12 102,3 4 13 103))'),
        ( 'TRIANGLE ((0 0 0 0,100 0 100 1,0 100 100 0,0 0 0 0))',
          'TRIANGLE ZM ((0 0 0 0,100 0 100 1,0 100 100 0,0 0 0 0))' ),
        ( 'TIN (((0 0 0 0,0 0 1 0,0 1 0 0,0 0 0 0)),((0 0 0 0,0 1 0 0,1 1 0 0,0 0 0 0)))',
          'TIN ZM (((0 0 0 0,0 0 1 0,0 1 0 0,0 0 0 0)),((0 0 0 0,0 1 0 0,1 1 0 0,0 0 0 0)))' ),
        ( 'POLYHEDRALSURFACE (((0 0 0 0,0 0 1 0,0 1 1 0,0 1 0 0,0 0 0 0)),((0 0 0 0,0 1 0 0,1 1 0 0,1 0 0 0,0 0 0 0)),((0 0 0 0,1 0 0 0,1 0 1 0,0 0 1 0,0 0 0 0)),((1 1 0 0,1 1 1 0,1 0 1 0,1 0 0 0,1 1 0 0)),((0 1 0 0,0 1 1 0,1 1 1 0,1 1 0 0,0 1 0 0)),((0 0 1 0,1 0 1 0,1 1 1 0,0 1 1 0,0 0 1 0)))',
          'POLYHEDRALSURFACE ZM (((0 0 0 0,0 0 1 0,0 1 1 0,0 1 0 0,0 0 0 0)),((0 0 0 0,0 1 0 0,1 1 0 0,1 0 0 0,0 0 0 0)),((0 0 0 0,1 0 0 0,1 0 1 0,0 0 1 0,0 0 0 0)),((1 1 0 0,1 1 1 0,1 0 1 0,1 0 0 0,1 1 0 0)),((0 1 0 0,0 1 1 0,1 1 1 0,1 1 0 0,0 1 0 0)),((0 0 1 0,1 0 1 0,1 1 1 0,0 1 1 0,0 0 1 0)))')
    )

    # This layer is also used in ogr_pg_21() test.
    gdaltest.pg_ds.ExecuteSQL( "CREATE TABLE testgeom (ogc_fid integer)" )

    # XXX - mloskot - if 'public' is omitted, then OGRPGDataSource::DeleteLayer fails, line 438
    sql_lyr = gdaltest.pg_ds.ExecuteSQL( "SELECT AddGeometryColumn('public','testgeom','wkb_geometry',-1,'GEOMETRY',4)" )
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)
    for i in range(len(geometries)):
        gdaltest.pg_ds.ExecuteSQL( "INSERT INTO testgeom (ogc_fid,wkb_geometry) \
                                    VALUES (%d,GeomFromEWKT('%s'))" % ( i, geometries[i][0])  )

    # We need to re-read layers
    gdaltest.pg_ds.Destroy()
    gdaltest.pg_ds = None
    try:
        gdaltest.pg_ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string, update = 1 )
    except:
        gdaltest.pg_ds = None
        gdaltest.post_reason( 'can not re-open datasource' )
        return 'fail'

    #
    # Test reading 4-dim geometries normalized to OGC WKT form.
    #

    layer = gdaltest.pg_ds.GetLayerByName( 'testgeom' )
    if layer is None:
        gdaltest.post_reason( 'did not get testgeom layer' )
        return 'fail'

    # Test updating the geometries
    for i in range(len(geometries)):
        feat = layer.GetFeature(i)
        layer.SetFeature(feat)

    # Test we get them back as expected
    for i in range(len(geometries)):
        feat = layer.GetFeature(i)
        geom = feat.GetGeometryRef()
        if geom is None:
            gdaltest.post_reason( 'did not get geometry, expected %s' % geometries[i][1] )
            return 'fail'
        wkt = geom.ExportToIsoWkt()
        feat.Destroy()
        feat = None

        if wkt != geometries[i][1]:
            gdaltest.post_reason( 'WKT do not match: expected %s, got %s' % (geometries[i][1], wkt) )
            return 'fail'

    layer = None

    return 'success'

###############################################################################
# Test reading 4-dimension geometries in EWKB format

def ogr_pg_21():

    if gdaltest.pg_ds is None or not gdaltest.pg_has_postgis:
        return 'skip'

    layer = gdaltest.pg_ds.ExecuteSQL( "SELECT wkb_geometry FROM testgeom" )
    if layer is None:
        gdaltest.post_reason( 'did not get testgeom layer' )
        return 'fail'

    feat = layer.GetNextFeature()
    while feat is not None:
        geom = feat.GetGeometryRef()
        if ogr.GT_HasZ(geom.GetGeometryType()) == 0 or ogr.GT_HasM(geom.GetGeometryType()) == 0:
            gdaltest.post_reason( 'expected feature with type >3000' )
            feat.Destroy()
            feat = None
            gdaltest.pg_ds.ReleaseResultSet(layer)
            layer = None
            return 'fail'

        feat.Destroy()
        feat = layer.GetNextFeature()

    feat = None
    gdaltest.pg_ds.ReleaseResultSet(layer)
    layer = None

    return 'success'

###############################################################################
# Check if the sub geometries of TIN and POLYHEDRALSURFACE are valid

def ogr_pg_21_subgeoms():

    if gdaltest.pg_ds is None or not gdaltest.pg_has_postgis:
        return 'skip'

    subgeom_PS = [  'POLYGON ZM ((0 0 0 0,0 0 1 0,0 1 1 0,0 1 0 0,0 0 0 0))',
                    'POLYGON ZM ((0 0 0 0,0 1 0 0,1 1 0 0,1 0 0 0,0 0 0 0))',
                    'POLYGON ZM ((0 0 0 0,1 0 0 0,1 0 1 0,0 0 1 0,0 0 0 0))',
                    'POLYGON ZM ((1 1 0 0,1 1 1 0,1 0 1 0,1 0 0 0,1 1 0 0))',
                    'POLYGON ZM ((0 1 0 0,0 1 1 0,1 1 1 0,1 1 0 0,0 1 0 0))',
                    'POLYGON ZM ((0 0 1 0,1 0 1 0,1 1 1 0,0 1 1 0,0 0 1 0))' ]

    subgeom_TIN = [ 'TRIANGLE ZM ((0 0 0 0,0 0 1 0,0 1 0 0,0 0 0 0))',
                    'TRIANGLE ZM ((0 0 0 0,0 1 0 0,1 1 0 0,0 0 0 0))' ]

    layer = gdaltest.pg_ds.GetLayerByName( 'testgeom' )
    for i in range(8,10):
        feat = layer.GetFeature(i)
        geom = feat.GetGeometryRef()
        if geom is None:
            gdaltest.post_reason( 'did not get the expected geometry')
            return 'fail'
        if geom.GetGeometryName() == "POLYHEDRALSURFACE":
            for j in range(0, geom.GetGeometryCount()):
                sub_geom = geom.GetGeometryRef(j)
                subgeom_wkt = sub_geom.ExportToIsoWkt()
                if subgeom_wkt != subgeom_PS[j]:
                    gdaltest.post_reason( 'did not get the expected subgeometry, expected %s' % (subgeom_PS[j]))
                    return 'fail'
        if geom.GetGeometryName() == "TIN":
            for j in range(0, geom.GetGeometryCount()):
                sub_geom = geom.GetGeometryRef(j)
                subgeom_wkt = sub_geom.ExportToIsoWkt()
                if subgeom_wkt != subgeom_TIN[j]:
                    gdaltest.post_reason( 'did not get the expected subgeometry, expected %s' % (subgeom_TIN[j]))
                    return 'fail'
        feat.Destroy()
        feat = None

    return 'success'

###############################################################################
# Check if the 3d geometries of TIN, Triangle and POLYHEDRALSURFACE are valid

def ogr_pg_21_3d_geometries():

    if gdaltest.pg_ds is None or gdaltest.ogr_pg_second_run:
        return 'skip'

    connection_string = "dbname=autotest"

    gdaltest.pg_ds = ogr.Open( 'PG:' + connection_string, update = 1 )

    gdaltest.pg_ds.ExecuteSQL( "CREATE TABLE zgeoms (field_no integer)" )
    sql_lyr = gdaltest.pg_ds.ExecuteSQL( "SELECT AddGeometryColumn('public','zgeoms','wkb_geometry',-1,'GEOMETRY',3)" )
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)
    wkt_list = ['POLYHEDRALSURFACE (((0 0 0,0 0 1,0 1 1,0 1 0,0 0 0)),((0 0 0,0 1 0,1 1 0,1 0 0,0 0 0)),((0 0 0,1 0 0,1 0 1,0 0 1,0 0 0)),((1 1 0,1 1 1,1 0 1,1 0 0,1 1 0)),((0 1 0,0 1 1,1 1 1,1 1 0,0 1 0)),((0 0 1,1 0 1,1 1 1,0 1 1,0 0 1)))',
                'TIN (((0 0 0,0 0 1,0 1 0,0 0 0)),((0 0 0,0 1 0,1 1 0,0 0 0)))',
                'TRIANGLE ((48 36 84,32 54 64,86 11 54,48 36 84))' ]

    wkt_expected = ['POLYHEDRALSURFACE Z (((0 0 0,0 0 1,0 1 1,0 1 0,0 0 0)),((0 0 0,0 1 0,1 1 0,1 0 0,0 0 0)),((0 0 0,1 0 0,1 0 1,0 0 1,0 0 0)),((1 1 0,1 1 1,1 0 1,1 0 0,1 1 0)),((0 1 0,0 1 1,1 1 1,1 1 0,0 1 0)),((0 0 1,1 0 1,1 1 1,0 1 1,0 0 1)))',
                    'TIN Z (((0 0 0,0 0 1,0 1 0,0 0 0)),((0 0 0,0 1 0,1 1 0,0 0 0)))',
                    'TRIANGLE Z ((48 36 84,32 54 64,86 11 54,48 36 84))' ]

    for i in range(0,3):
        gdaltest.pg_ds.ExecuteSQL( "INSERT INTO zgeoms (field_no, wkb_geometry) VALUES (%d,GeomFromEWKT('%s'))" % ( i, wkt_list[i] ) )

    gdaltest.pg_ds.Destroy()
    gdaltest.pg_ds = None

    try:
        gdaltest.pg_ds = ogr.Open( 'PG:' + connection_string, update = 1 )
    except:
        gdaltest.pg_ds = None
        gdaltest.post_reason( 'Cannot open the dataset' )
        return 'fail'

    layer = gdaltest.pg_ds.GetLayerByName( 'zgeoms' )
    if layer is None:
        gdaltest.post_reason( 'No layer received' )
        return 'fail'

    for i in range (0, 3):
        feat = layer.GetFeature(i)
        geom = feat.GetGeometryRef()

        wkt = geom.ExportToIsoWkt()

        if (wkt != wkt_expected[i]):
            gdaltest.post_reason( 'Unexpected WKT, expected %s and got %s' % ( wkt_expected[i], wkt) )
            return 'fail'

    gdaltest.pg_ds.ExecuteSQL( "DROP TABLE zgeoms" )
    return 'success'

###############################################################################
# Create table from data/poly.shp under specified SCHEMA
# This test checks if schema support and schema name quoting works well.

def ogr_pg_22():

    if gdaltest.pg_ds is None:
        return 'skip'

    ######################################################
    # Create Schema

    schema_name = 'AutoTest-schema'
    layer_name = schema_name + '.tpoly'

    gdaltest.pg_ds.ExecuteSQL( 'CREATE SCHEMA \"' + schema_name + '\"')

    ######################################################
    # Create Layer
    gdaltest.pg_lyr = gdaltest.pg_ds.CreateLayer( layer_name,
                                                  options = [
                                                      'DIM=3',
                                                      'SCHEMA=' + schema_name ]
                                                )

    ######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def( gdaltest.pg_lyr,
                                    [ ('AREA', ogr.OFTReal),
                                      ('EAS_ID', ogr.OFTInteger),
                                      ('PRFEDEA', ogr.OFTString),
                                      ('SHORTNAME', ogr.OFTString, 8) ] )

    ######################################################
    # Copy 3 features from the poly.shp

    dst_feat = ogr.Feature( feature_def = gdaltest.pg_lyr.GetLayerDefn() )

    shp_ds = ogr.Open( 'data/poly.shp' )
    shp_lyr = shp_ds.GetLayer(0)

    # Insert 3 features only
    for id in range(0, 3):
        feat = shp_lyr.GetFeature(id)
        dst_feat.SetFrom( feat )
        gdaltest.pg_lyr.CreateFeature( dst_feat )

    dst_feat.Destroy()

    # Test if test layer under custom schema is listed

    found = ogr_pg_check_layer_in_list(gdaltest.pg_ds, layer_name)

    if found is False:
        gdaltest.post_reason( 'layer from schema \''+schema_name+'\' not listed' )
        return 'fail'

    return 'success'

###############################################################################
# Create table with all data types

def ogr_pg_23(layer_name = 'datatypetest', include_timestamptz = True):

    if gdaltest.pg_ds is None:
        return 'skip'

    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:' + layer_name )
    gdal.PopErrorHandler()

    ######################################################
    # Create Table
    lyr = gdaltest.pg_ds.CreateLayer( layer_name )

    ######################################################
    # Setup Schema
    # ogrtest.quick_create_layer_def( lyr, None )

    ######################################################
    # add some custom date fields.
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE ' + layer_name + ' ADD COLUMN my_numeric numeric' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE ' + layer_name + ' ADD COLUMN my_numeric5 numeric(5)' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE ' + layer_name + ' ADD COLUMN my_numeric5_3 numeric(5,3)' )
    #gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE ' + layer_name + ' ADD COLUMN my_bool bool' )
    fld = ogr.FieldDefn('my_bool', ogr.OFTInteger)
    fld.SetSubType(ogr.OFSTBoolean)
    lyr.CreateField(fld)
    #gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE ' + layer_name + ' ADD COLUMN my_int2 int2' )
    fld = ogr.FieldDefn('my_int2', ogr.OFTInteger)
    fld.SetSubType(ogr.OFSTInt16)
    lyr.CreateField(fld)
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE ' + layer_name + ' ADD COLUMN my_int4 int4' )
    lyr.CreateField(ogr.FieldDefn('my_int8', ogr.OFTInteger64))
    #gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE ' + layer_name + ' ADD COLUMN my_float4 float4' )
    fld = ogr.FieldDefn('my_float4', ogr.OFTReal)
    fld.SetSubType(ogr.OFSTFloat32)
    lyr.CreateField(fld)
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE ' + layer_name + ' ADD COLUMN my_float8 float8' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE ' + layer_name + ' ADD COLUMN my_real real' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE ' + layer_name + ' ADD COLUMN my_char char' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE ' + layer_name + ' ADD COLUMN my_varchar character varying' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE ' + layer_name + ' ADD COLUMN my_varchar10 character varying(10)' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE ' + layer_name + ' ADD COLUMN my_text text' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE ' + layer_name + ' ADD COLUMN my_bytea bytea' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE ' + layer_name + ' ADD COLUMN my_time time' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE ' + layer_name + ' ADD COLUMN my_date date' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE ' + layer_name + ' ADD COLUMN my_timestamp timestamp without time zone' )
    if include_timestamptz:
        gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE ' + layer_name + ' ADD COLUMN my_timestamptz timestamp with time zone' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE ' + layer_name + ' ADD COLUMN my_chararray char(1)[]' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE ' + layer_name + ' ADD COLUMN my_textarray text[]' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE ' + layer_name + ' ADD COLUMN my_varchararray character varying[]' )
    fld = ogr.FieldDefn('my_int2array', ogr.OFTIntegerList)
    fld.SetSubType(ogr.OFSTInt16)
    lyr.CreateField(fld)
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE ' + layer_name + ' ADD COLUMN my_int4array int4[]' )
    lyr.CreateField(ogr.FieldDefn('my_int8array', ogr.OFTInteger64List))
    fld = ogr.FieldDefn('my_float4array', ogr.OFTRealList)
    fld.SetSubType(ogr.OFSTFloat32)
    lyr.CreateField(fld)
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE ' + layer_name + ' ADD COLUMN my_float8array float8[]' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE ' + layer_name + ' ADD COLUMN my_numericarray numeric[]' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE ' + layer_name + ' ADD COLUMN my_numeric5array numeric(5)[]' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE ' + layer_name + ' ADD COLUMN my_numeric5_3array numeric(5,3)[]' )
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
    sql = "INSERT INTO " + layer_name + " ( my_numeric, my_numeric5, my_numeric5_3, my_bool, my_int2, "
    sql += "my_int4, my_int8, my_float4, my_float8, my_real, my_char, my_varchar, "
    sql += "my_varchar10, my_text, my_bytea, my_time, my_date, my_timestamp, "
    if include_timestamptz:
        sql += "my_timestamptz, "
    sql += "my_chararray, my_textarray, my_varchararray, my_int2array, my_int4array, "
    sql += "my_int8array, my_float4array, my_float8array, my_numericarray, my_numeric5array, my_numeric5_3array, my_boolarray, wkb_geometry) "
    sql += "VALUES ( 1.2, 12345, 0.123, 'T', 12345, 12345678, 1234567901234, 0.123, "
    sql += "0.12345678, 0.876, 'a', 'ab', 'varchar10 ', 'abc', 'xyz', '12:34:56', "
    sql += "'2000-01-01', '2000-01-01 00:00:00', "
    if include_timestamptz:
        sql += "'2000-01-01 00:00:00+00', "
    sql += "'{a,b}', "
    sql += "'{aa,bb}', '{cc,dd}', '{100,200}', '{100,200}', '{1234567901234}', "
    sql += "'{100.1,200.1}', '{100.12,200.12}', ARRAY[100.12,200.12], ARRAY[10,20], ARRAY[10.12,20.12], '{1,0}', " + geom_str + " )"
    gdaltest.pg_ds.ExecuteSQL( sql )

    return 'success'

###############################################################################

def test_val_test_23(layer_defn, feat):

    field_defn = layer_defn.GetFieldDefn(layer_defn.GetFieldIndex("my_numeric5"))
    if field_defn.GetWidth() != 5 or field_defn.GetPrecision() != 0 or field_defn.GetType() != ogr.OFTInteger:
        gdaltest.post_reason('Wrong field defn for my_numeric5 : %d, %d, %d' % (field_defn.GetWidth(), field_defn.GetPrecision(), field_defn.GetType()))
        return 'fail'

    field_defn = layer_defn.GetFieldDefn(layer_defn.GetFieldIndex("my_numeric5_3"))
    if field_defn.GetWidth() != 5 or field_defn.GetPrecision() != 3 or field_defn.GetType() != ogr.OFTReal:
        gdaltest.post_reason('Wrong field defn for my_numeric5_3 : %d, %d, %d' % (field_defn.GetWidth(), field_defn.GetPrecision(), field_defn.GetType()))
        return 'fail'

    field_defn = layer_defn.GetFieldDefn(layer_defn.GetFieldIndex("my_varchar10"))
    if field_defn.GetWidth() != 10 or field_defn.GetPrecision() != 0:
        gdaltest.post_reason('Wrong field defn for my_varchar10 : %d, %d, %d' % (field_defn.GetWidth(), field_defn.GetPrecision(), field_defn.GetType()))
        return 'fail'

    field_defn = layer_defn.GetFieldDefn(layer_defn.GetFieldIndex("my_bool"))
    if field_defn.GetWidth() != 1 or field_defn.GetType() != ogr.OFTInteger or field_defn.GetSubType() != ogr.OFSTBoolean:
        gdaltest.post_reason('Wrong field defn for my_bool : %d, %d, %d, %d' % (field_defn.GetWidth(), field_defn.GetPrecision(), field_defn.GetType(), field_defn.GetSubType()))
        return 'fail'

    field_defn = layer_defn.GetFieldDefn(layer_defn.GetFieldIndex("my_boolarray"))
    if field_defn.GetType() != ogr.OFTIntegerList or field_defn.GetSubType() != ogr.OFSTBoolean:
        gdaltest.post_reason('Wrong field defn for my_boolarray : %d, %d, %d, %d' % (field_defn.GetWidth(), field_defn.GetPrecision(), field_defn.GetType(), field_defn.GetSubType()))
        return 'fail'

    field_defn = layer_defn.GetFieldDefn(layer_defn.GetFieldIndex("my_int2"))
    if field_defn.GetType() != ogr.OFTInteger or field_defn.GetSubType() != ogr.OFSTInt16:
        gdaltest.post_reason('Wrong field defn for my_int2 : %d, %d, %d, %d' % (field_defn.GetWidth(), field_defn.GetPrecision(), field_defn.GetType(), field_defn.GetSubType()))
        return 'fail'

    field_defn = layer_defn.GetFieldDefn(layer_defn.GetFieldIndex("my_float4"))
    if field_defn.GetType() != ogr.OFTReal or field_defn.GetSubType() != ogr.OFSTFloat32:
        gdaltest.post_reason('Wrong field defn for my_float4 : %d, %d, %d, %d' % (field_defn.GetWidth(), field_defn.GetPrecision(), field_defn.GetType(), field_defn.GetSubType()))
        return 'fail'

    field_defn = layer_defn.GetFieldDefn(layer_defn.GetFieldIndex("my_int2array"))
    if field_defn.GetType() != ogr.OFTIntegerList or field_defn.GetSubType() != ogr.OFSTInt16:
        gdaltest.post_reason('Wrong field defn for my_int2array : %d, %d, %d, %d' % (field_defn.GetWidth(), field_defn.GetPrecision(), field_defn.GetType(), field_defn.GetSubType()))
        return 'fail'

    field_defn = layer_defn.GetFieldDefn(layer_defn.GetFieldIndex("my_float4array"))
    if field_defn.GetType() != ogr.OFTRealList or field_defn.GetSubType() != ogr.OFSTFloat32:
        gdaltest.post_reason('Wrong field defn for my_float4array : %d, %d, %d, %d' % (field_defn.GetWidth(), field_defn.GetPrecision(), field_defn.GetType(), field_defn.GetSubType()))
        return 'fail'

    if abs(feat.my_numeric - 1.2) > 1e-8 or \
    feat.my_numeric5 != 12345 or \
    feat.my_numeric5_3 != 0.123 or \
    feat.my_bool != 1 or \
    feat.my_int2 != 12345 or \
    feat.my_int4 != 12345678 or \
    feat.my_int8 != 1234567901234 or \
    abs(feat.my_float4 - 0.123) > 1e-8 or \
    feat.my_float8 != 0.12345678 or \
    abs(feat.my_real - 0.876) > 1e-6 or \
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
    feat.my_int8array != [ 1234567901234 ] or \
    feat.GetFieldAsString('my_boolarray') != '(2:1,0)' or \
    abs(feat.my_float4array[0] - 100.1) > 1e-6 or \
    abs(feat.my_float8array[0] - 100.12) > 1e-8 or \
    abs(feat.my_numericarray[0] - 100.12) > 1e-8 or \
    abs(feat.my_numeric5array[0] - 10) > 1e-8 or \
    abs(feat.my_numeric5_3array[0] - 10.12) > 1e-8 :
        gdaltest.post_reason( 'Wrong values' )
        feat.DumpReadable()
        return 'fail'

    geom = feat.GetGeometryRef()
    if geom is None:
        gdaltest.post_reason( 'geom is none' )
        return 'fail'

    wkt = geom.ExportToWkt()
    if wkt != 'POINT (10 20)':
        gdaltest.post_reason( 'Wrong WKT :' + wkt )
        return 'fail'

    return 'success'

###############################################################################
# Test with PG: connection

def ogr_pg_24():

    if gdaltest.pg_ds is None:
        return 'skip'

    ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string, update = 1 )

    ds.ExecuteSQL( 'set timezone to "UTC"' )

    lyr = ds.GetLayerByName( 'datatypetest' )

    feat = lyr.GetNextFeature()
    if test_val_test_23(lyr.GetLayerDefn(), feat) != 'success':
        return 'fail'

    feat = None

    ds.Destroy()

    return 'success'

###############################################################################
# Test with PG: connection and SELECT query

def ogr_pg_25():

    if gdaltest.pg_ds is None:
        return 'skip'

    ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string, update = 1 )

    ds.ExecuteSQL( 'set timezone to "UTC"' )

    sql_lyr = ds.ExecuteSQL( 'select * from datatypetest' )

    feat = sql_lyr.GetNextFeature()
    if test_val_test_23(sql_lyr.GetLayerDefn(), feat) != 'success':
        return 'fail'

    ds.ReleaseResultSet( sql_lyr )

    feat = None

    ds.Destroy()

    return 'success'

###############################################################################
# Test with PGB: connection

def ogr_pg_26_DISABLED_AS_BINARY_CURSOR_NO_LONGER_SUPPORTED():

    if gdaltest.pg_ds is None:
        return 'skip'

    # The presence of timestamptz field currently disables binary cursor
    if ogr_pg_23(layer_name = 'datatypetest_withouttimestamptz', include_timestamptz = False) != 'success':
        return 'fail'

    ds = ogr.Open( 'PGB:' + gdaltest.pg_connection_string, update = 1 )

    ds.ExecuteSQL( 'set timezone to "UTC"' )

    lyr = ds.GetLayerByName( 'datatypetest' )

    feat = lyr.GetNextFeature()
    if test_val_test_23(lyr.GetLayerDefn(), feat) != 'success':
        return 'fail'

    feat = None

    lyr = ds.GetLayerByName( 'datatypetest_withouttimestamptz' )

    feat = lyr.GetNextFeature()
    if test_val_test_23(lyr.GetLayerDefn(), feat) != 'success':
        return 'fail'

    feat = None

    ds = None

    return 'success'

###############################################################################
# Test with PGB: connection and SELECT query

def ogr_pg_27_DISABLED_AS_BINARY_CURSOR_NO_LONGER_SUPPORTED():

    if gdaltest.pg_ds is None:
        return 'skip'

    ds = ogr.Open( 'PGB:' + gdaltest.pg_connection_string, update = 1 )

    ds.ExecuteSQL( 'set timezone to "UTC"')

    sql_lyr = ds.ExecuteSQL( 'select * from datatypetest' )

    feat = sql_lyr.GetNextFeature()
    if test_val_test_23(sql_lyr.GetLayerDefn(), feat) != 'success':
        return 'fail'

    ds.ReleaseResultSet( sql_lyr )

    feat = None

    sql_lyr = ds.ExecuteSQL( 'select * from datatypetest_withouttimestamptz' )

    feat = sql_lyr.GetNextFeature()
    if test_val_test_23(sql_lyr.GetLayerDefn(), feat) != 'success':
        return 'fail'

    ds.ReleaseResultSet( sql_lyr )

    feat = None

    ds = None

    return 'success'

###############################################################################
# Duplicate all data types in INSERT mode

def ogr_pg_28():

    if gdaltest.pg_ds is None:
        return 'skip'

    gdal.SetConfigOption( 'PG_USE_COPY', "NO" )

    ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string, update = 1 )

    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    ds.ExecuteSQL( 'DELLAYER:datatypetest2' )
    gdal.PopErrorHandler()

    ds.ExecuteSQL( 'set timezone to "UTC"' )

    src_lyr = ds.GetLayerByName( 'datatypetest' )

    dst_lyr = ds.CreateLayer( 'datatypetest2' )

    src_lyr.ResetReading()

    for i in range(src_lyr.GetLayerDefn().GetFieldCount()):
        field_defn = src_lyr.GetLayerDefn().GetFieldDefn(i)
        dst_lyr.CreateField( field_defn )

    dst_feat = ogr.Feature( feature_def = dst_lyr.GetLayerDefn() )

    feat = src_lyr.GetNextFeature()
    if feat is None:
        return 'fail'

    dst_feat.SetFrom( feat )
    if dst_lyr.CreateFeature( dst_feat ) != 0:
        gdaltest.post_reason('CreateFeature failed.')
        return 'fail'

    dst_feat.Destroy()

    src_lyr = None
    dst_lyr = None

    ds.Destroy()

    gdal.SetConfigOption( 'PG_USE_COPY', gdaltest.pg_use_copy )

    return 'success'

###############################################################################
# Test with PG: connection

def ogr_pg_29():

    if gdaltest.pg_ds is None:
        return 'skip'

    ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string, update = 1 )

    ds.ExecuteSQL( 'set timezone to "UTC"' )

    lyr = ds.GetLayerByName( 'datatypetest2' )

    # my_timestamp has now a time zone...
    feat = lyr.GetNextFeature()
    if test_val_test_23(lyr.GetLayerDefn(), feat) != 'success':
        return 'fail'

    geom = feat.GetGeometryRef()
    wkt = geom.ExportToWkt()
    if wkt != 'POINT (10 20)':
        gdaltest.post_reason( 'Wrong WKT :' + wkt )
        return 'fail'

    feat = None

    ds.Destroy()

    return 'success'

###############################################################################
# Duplicate all data types in PG_USE_COPY mode

def ogr_pg_30():

    if gdaltest.pg_ds is None:
        return 'skip'

    gdal.SetConfigOption( 'PG_USE_COPY', 'YES' )

    ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string, update = 1 )

    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    ds.ExecuteSQL( 'DELLAYER:datatypetest2' )
    gdal.PopErrorHandler()

    ds.ExecuteSQL( 'set timezone to "UTC"' )

    src_lyr = ds.GetLayerByName( 'datatypetest' )

    dst_lyr = ds.CreateLayer( 'datatypetest2' )

    src_lyr.ResetReading()

    for i in range(src_lyr.GetLayerDefn().GetFieldCount()):
        field_defn = src_lyr.GetLayerDefn().GetFieldDefn(i)
        dst_lyr.CreateField( field_defn )

    dst_feat = ogr.Feature( feature_def = dst_lyr.GetLayerDefn() )

    feat = src_lyr.GetNextFeature()
    if feat is None:
        return 'fail'

    dst_feat.SetFrom( feat )
    if dst_lyr.CreateFeature( dst_feat ) != 0:
        gdaltest.post_reason('CreateFeature failed.')
        return 'fail'

    dst_feat.Destroy()

    ds.Destroy()

    gdal.SetConfigOption( 'PG_USE_COPY', gdaltest.pg_use_copy )

    return 'success'


###############################################################################
# Test the tables= connection string option

def ogr_pg_31():

    if gdaltest.pg_ds is None:
        return 'skip'

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    lyr = gdaltest.pg_ds.CreateLayer('test_for_tables_equal_param', geom_type = ogr.wkbPoint, srs = srs, options = ['OVERWRITE=YES'])
    lyr.StartTransaction()
    for i in range(501):
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(0 0)'))
        lyr.CreateFeature(f)
    lyr.CommitTransaction()

    ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string + ' tables=tpoly,tpolycopy', update = 1 )

    if ds is None or ds.GetLayerCount() != 2:
        gdaltest.post_reason('fail')
        return 'fail'

    sql_lyr = ds.ExecuteSQL('SELECT * FROM test_for_tables_equal_param')
    i = 0
    while True:
        f = sql_lyr.GetNextFeature()
        if f is None:
            break
        i = i + 1
    ds.ReleaseResultSet(sql_lyr)
    if i != 501:
        gdaltest.post_reason('fail')
        return 'fail'

    ds.Destroy()

    return 'success'

###############################################################################
# Test approximate srtext (#2123, #3508)

def ogr_pg_32():

    if gdaltest.pg_ds is None or not gdaltest.pg_has_postgis:
        return 'skip'

    gdaltest.pg_ds.ExecuteSQL("DELETE FROM spatial_ref_sys")

    ######################################################
    # Create Layer with EPSG:4326
    srs = osr.SpatialReference()
    srs.ImportFromEPSG( 4326 )

    gdaltest.pg_lyr = gdaltest.pg_ds.CreateLayer( 'testsrtext', srs = srs)

    sql_lyr = gdaltest.pg_ds.ExecuteSQL("SELECT COUNT(*) FROM spatial_ref_sys")
    feat = sql_lyr.GetNextFeature()
    if  feat.count != 1:
        gdaltest.post_reason('did not get expected count after step (1)')
        feat.DumpReadable()
        return 'fail'
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

    ######################################################
    # Create second layer with very approximative EPSG:4326

    srs = osr.SpatialReference()
    srs.SetFromUserInput('GEOGCS["WGS 84",AUTHORITY["EPSG","4326"]]')
    gdaltest.pg_lyr = gdaltest.pg_ds.CreateLayer( 'testsrtext2', srs = srs)

    # Must still be 1
    sql_lyr = gdaltest.pg_ds.ExecuteSQL("SELECT COUNT(*) FROM spatial_ref_sys")
    feat = sql_lyr.GetNextFeature()
    if  feat.count != 1:
        gdaltest.post_reason('did not get expected count after step (2)')
        feat.DumpReadable()
        return 'fail'
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

    ######################################################
    # Create third layer with very approximative EPSG:4326 but without authority

    srs = osr.SpatialReference()
    srs.SetFromUserInput("""GEOGCS["GCS_WGS_1984",DATUM["WGS_1984",SPHEROID["WGS_1984",6378137,298.257223563]],PRIMEM["Greenwich",0],UNIT["Degree",0.017453292519943295]]""")
    gdaltest.pg_lyr = gdaltest.pg_ds.CreateLayer( 'testsrtext3', srs = srs)

    # Must still be 1
    sql_lyr = gdaltest.pg_ds.ExecuteSQL("SELECT COUNT(*) FROM spatial_ref_sys")
    feat = sql_lyr.GetNextFeature()
    if  feat.count != 1:
        gdaltest.post_reason('did not get expected count after step (3)')
        feat.DumpReadable()
        return 'fail'
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

    ######################################################
    # Create Layer with EPSG:26632

    srs = osr.SpatialReference()
    srs.ImportFromEPSG( 26632 )

    gdaltest.pg_lyr = gdaltest.pg_ds.CreateLayer( 'testsrtext4', geom_type = ogr.wkbPoint, srs = srs)
    feat = ogr.Feature(gdaltest.pg_lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(0 0)'))
    gdaltest.pg_lyr.CreateFeature(feat)
    feat = None
    sr = gdaltest.pg_lyr.GetSpatialRef()
    if sr.ExportToWkt().find('26632') == -1:
        gdaltest.post_reason('did not get expected SRS')
        return 'fail'

    sql_lyr = gdaltest.pg_ds.ExecuteSQL("SELECT COUNT(*) FROM spatial_ref_sys")
    feat = sql_lyr.GetNextFeature()
    # Must be 2 now
    if  feat.count != 2:
        gdaltest.post_reason('did not get expected count after step (4)')
        feat.DumpReadable()
        return 'fail'
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)


    ######################################################
    # Test GetSpatialRef() on SQL layer (#4644)

    sql_lyr = gdaltest.pg_ds.ExecuteSQL('SELECT * FROM testsrtext4')
    sr = sql_lyr.GetSpatialRef()
    if sr.ExportToWkt().find('26632') == -1:
        gdaltest.post_reason('did not get expected SRS')
        return 'fail'
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

    ######################################################
    # Test getting SRS and geom type without requiring to fetch the layer defn

    for i in range(2):
        #sys.stderr.write('BEFORE OPEN\n')
        ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string, update = 1 )
        #sys.stderr.write('AFTER Open\n')
        lyr = ds.GetLayerByName('testsrtext4')
        #sys.stderr.write('AFTER GetLayerByName\n')
        if i == 0:
            sr = lyr.GetSpatialRef()
            #sys.stderr.write('AFTER GetSpatialRef\n')
            geom_type = lyr.GetGeomType()
            #sys.stderr.write('AFTER GetGeomType\n')
        else:
            geom_type = lyr.GetGeomType()
            #sys.stderr.write('AFTER GetGeomType\n')
            sr = lyr.GetSpatialRef()
            #sys.stderr.write('AFTER GetSpatialRef\n')

        if sr.ExportToWkt().find('26632') == -1:
            gdaltest.post_reason('did not get expected SRS')
            return 'fail'
        if geom_type != ogr.wkbPoint:
            gdaltest.post_reason('did not get expected geom type')
            return 'fail'

        ds = None

    ######################################################
    # Create Layer with non EPSG SRS

    srs = osr.SpatialReference()
    srs.SetFromUserInput('+proj=vandg')

    gdaltest.pg_lyr = gdaltest.pg_ds.CreateLayer( 'testsrtext5', srs = srs)

    sql_lyr = gdaltest.pg_ds.ExecuteSQL("SELECT COUNT(*) FROM spatial_ref_sys")
    feat = sql_lyr.GetNextFeature()
    # Must be 3 now
    if  feat.count != 3:
        gdaltest.post_reason('did not get expected count after step (5)')
        feat.DumpReadable()
        return 'fail'
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

    return 'success'

###############################################################################
# Test encoding as UTF8

def ogr_pg_33():

    if gdaltest.pg_ds is None:
        return 'skip'

    gdaltest.pg_lyr = gdaltest.pg_ds.GetLayerByName( 'tpoly' )
    if gdaltest.pg_lyr is None:
        gdaltest.post_reason( 'did not get tpoly layer' )
        return 'fail'

    dst_feat = ogr.Feature( feature_def = gdaltest.pg_lyr.GetLayerDefn() )
    # eacute in UTF8 : 0xc3 0xa9
    dst_feat.SetField( 'SHORTNAME', '\xc3\xa9' )
    gdaltest.pg_lyr.CreateFeature( dst_feat )
    dst_feat.Destroy()

    return 'success'

###############################################################################
# Test encoding as Latin1

def ogr_pg_34():

    if gdaltest.pg_ds is None:
        return 'skip'

    # We only test that on Linux since setting os.environ['XXX']
    # is not guaranteed to have effects on system not supporting putenv
    if sys.platform.startswith('linux'):
        os.environ['PGCLIENTENCODING'] = 'LATIN1'
        ogr_pg_1()
        del os.environ['PGCLIENTENCODING']

        # For some unknown reasons, some servers don't like forcing LATIN1
        # but prefer LATIN9 instead...
        if gdaltest.pg_ds is None:
            os.environ['PGCLIENTENCODING'] = 'LATIN9'
            ogr_pg_1()
            del os.environ['PGCLIENTENCODING']
    else:
        gdaltest.pg_ds.ExecuteSQL('SET client_encoding = LATIN1')

    gdaltest.pg_lyr = gdaltest.pg_ds.GetLayerByName( 'tpoly' )
    if gdaltest.pg_lyr is None:
        gdaltest.post_reason( 'did not get tpoly layer' )
        return 'fail'

    dst_feat = ogr.Feature( feature_def = gdaltest.pg_lyr.GetLayerDefn() )
    # eacute in Latin1 : 0xe9
    dst_feat.SetField( 'SHORTNAME', '\xe9' )
    gdaltest.pg_lyr.CreateFeature( dst_feat )
    dst_feat.Destroy()

    return 'success'


###############################################################################
# Test for buffer overflows

def ogr_pg_35():

    if gdaltest.pg_ds is None:
        return 'skip'

    gdal.PushErrorHandler()
    try:
        gdaltest.pg_lyr = gdaltest.pg_ds.CreateLayer( 'testoverflows' )
        ogrtest.quick_create_layer_def( gdaltest.pg_lyr, [ ('0123456789' * 1000, ogr.OFTReal)] )
        # To trigger actual layer creation
        gdaltest.pg_lyr.ResetReading()
    except:
        pass
    finally:
        gdal.PopErrorHandler()

    gdal.PushErrorHandler()
    try:
        gdaltest.pg_lyr = gdaltest.pg_ds.CreateLayer( 'testoverflows', options = [ 'OVERWRITE=YES', 'GEOMETRY_NAME=' + ('0123456789' * 1000) ] )
        # To trigger actual layer creation
        gdaltest.pg_lyr.ResetReading()
    except:
        pass
    finally:
        gdal.PopErrorHandler()

    return 'success'

###############################################################################
# Test support for inherited tables : tables inherited from a Postgis Table

def ogr_pg_36():

    if gdaltest.pg_ds is None:
        return 'skip'

    if gdaltest.pg_has_postgis:
        lyr = gdaltest.pg_ds.CreateLayer( 'table36_base', geom_type = ogr.wkbPoint )
    else:
        lyr = gdaltest.pg_ds.CreateLayer( 'table36_base' )

    gdaltest.pg_ds.ExecuteSQL('CREATE TABLE table36_inherited ( col1 CHAR(1) ) INHERITS ( table36_base )')
    gdaltest.pg_ds.ExecuteSQL('CREATE TABLE table36_inherited2 ( col2 CHAR(1) ) INHERITS ( table36_inherited )')

    # Test fix for #3636 when 2 inherited tables with same name exist in 2 different schemas
    if gdaltest.pg_has_postgis:
        #lyr = gdaltest.pg_ds.CreateLayer( 'table36_base', geom_type = ogr.wkbLineString, options = ['SCHEMA=AutoTest-schema'] )
        lyr = gdaltest.pg_ds.CreateLayer( 'AutoTest-schema.table36_base', geom_type = ogr.wkbLineString )
    else:
        lyr = gdaltest.pg_ds.CreateLayer( 'table36_base', options = ['SCHEMA=AutoTest-schema'] )

    gdaltest.pg_ds.ExecuteSQL('CREATE TABLE "AutoTest-schema"."table36_inherited" ( col3 CHAR(1) ) INHERITS ( "AutoTest-schema".table36_base )')
    gdaltest.pg_ds.ExecuteSQL('CREATE TABLE "AutoTest-schema"."table36_inherited2" ( col4 CHAR(1) ) INHERITS ( "AutoTest-schema".table36_inherited )')


    ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string, update = 1 )

    found = ogr_pg_check_layer_in_list(ds, 'table36_inherited')
    if found is False:
        gdaltest.post_reason( 'layer table36_inherited not listed' )
        return 'fail'

    found = ogr_pg_check_layer_in_list(ds, 'table36_inherited2')
    if found is False:
        gdaltest.post_reason( 'layer table36_inherited2 not listed' )
        return 'fail'

    lyr = ds.GetLayerByName( 'table36_inherited2' )
    if lyr is None:
        return 'fail'
    if gdaltest.pg_has_postgis and lyr.GetGeomType() != ogr.wkbPoint:
        gdaltest.post_reason( 'wrong geometry type for layer table36_inherited2' )
        print(lyr.GetGeomType())
        return 'fail'

    lyr = ds.GetLayerByName( 'AutoTest-schema.table36_inherited2' )
    if lyr is None:
        return 'fail'
    if gdaltest.pg_has_postgis and lyr.GetGeomType() != ogr.wkbLineString:
        gdaltest.post_reason( 'wrong geometry type for layer AutoTest-schema.table36_inherited2' )
        return 'fail'

    ds.Destroy()

    return 'success'

def ogr_pg_36_bis():

    if gdaltest.pg_ds is None:
        return 'skip'

    ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string + ' TABLES=table36_base', update = 1 )

    found = ogr_pg_check_layer_in_list(ds, 'table36_inherited')
    if found is True:
        gdaltest.post_reason( 'layer table36_inherited is listed but it should not' )
        return 'fail'

    lyr = ds.GetLayerByName( 'table36_inherited' )
    if lyr is None:
        return 'fail'
    if gdaltest.pg_has_postgis and lyr.GetGeomType() != ogr.wkbPoint:
        return 'fail'

    ds.Destroy()

    return 'success'

###############################################################################
# Test support for inherited tables : Postgis table inherited from a non-Postgis table

def ogr_pg_37():

    if gdaltest.pg_ds is None or not gdaltest.pg_has_postgis:
        return 'skip'

    gdaltest.pg_ds.ExecuteSQL('CREATE TABLE table37_base ( col1 CHAR(1) )')
    gdaltest.pg_ds.ExecuteSQL('CREATE TABLE table37_inherited ( col2 CHAR(1) ) INHERITS ( table37_base )')
    sql_lyr = gdaltest.pg_ds.ExecuteSQL( "SELECT AddGeometryColumn('public','table37_inherited','wkb_geometry',-1,'POINT',2)" )
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

    ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string, update = 1 )

    found = ogr_pg_check_layer_in_list(ds, 'table37_inherited')
    if found is False:
        gdaltest.post_reason( 'layer table37_inherited not listed' )
        return 'fail'

    lyr = ds.GetLayerByName( 'table37_inherited' )
    if lyr is None:
        return 'fail'
    if gdaltest.pg_has_postgis and lyr.GetGeomType() != ogr.wkbPoint:
        return 'fail'

    ds.Destroy()

    return 'success'

###############################################################################
# Test support for multiple geometry columns (RFC 41)

def ogr_pg_38():
    if gdaltest.pg_ds is None or not gdaltest.pg_has_postgis:
        return 'skip'

    sql_lyr = gdaltest.pg_ds.ExecuteSQL( "SELECT AddGeometryColumn('public','table37_base','pointBase',-1,'POINT',2)" )
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

    sql_lyr = gdaltest.pg_ds.ExecuteSQL( "SELECT AddGeometryColumn('public','table37_inherited','point25D',-1,'POINT',3)" )
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

    ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string, update = 1 )

    # Check for the layer with the wkb_geometry column
    found = ogr_pg_check_layer_in_list(ds, 'table37_inherited')
    if found is False:
        gdaltest.post_reason( 'layer table37_inherited not listed' )
        return 'fail'

    lyr = ds.GetLayerByName( 'table37_inherited' )
    if lyr is None:
        gdaltest.post_reason( 'fail' )
        return 'fail'
    gfld_defn = lyr.GetLayerDefn().GetGeomFieldDefn(lyr.GetLayerDefn().GetGeomFieldIndex("wkb_geometry"))
    if gfld_defn is None:
        gdaltest.post_reason( 'fail' )
        return 'fail'
    if gfld_defn.GetType() != ogr.wkbPoint:
        gdaltest.post_reason( 'fail' )
        return 'fail'
    if lyr.GetLayerDefn().GetGeomFieldCount() != 3:
        gdaltest.post_reason( 'fail' )
        print(lyr.GetLayerDefn().GetGeomFieldCount())
        for i in range(lyr.GetLayerDefn().GetGeomFieldCount()):
            print(lyr.GetLayerDefn().GetGeomFieldDefn(i).GetName())
        return 'fail'

    # Explicit query to 'table37_inherited(wkb_geometry)' should also work
    lyr = ds.GetLayerByName( 'table37_inherited(wkb_geometry)' )
    if lyr is None:
        gdaltest.post_reason( 'fail' )
        return 'fail'

    lyr = ds.GetLayerByName( 'table37_inherited(pointBase)' )
    if lyr is None:
        gdaltest.post_reason( 'fail' )
        return 'fail'
    if lyr.GetGeomType() != ogr.wkbPoint:
        gdaltest.post_reason( 'fail' )
        return 'fail'
    if lyr.GetGeometryColumn() != 'pointBase':
        gdaltest.post_reason( 'wrong geometry column name' )
        return 'fail'

    lyr = ds.GetLayerByName( 'table37_inherited(point25D)' )
    if lyr is None:
        gdaltest.post_reason( 'fail' )
        return 'fail'
    if lyr.GetGeomType() != ogr.wkbPoint25D:
        gdaltest.post_reason( 'fail' )
        return 'fail'
    if lyr.GetGeometryColumn() != 'point25D':
        gdaltest.post_reason( 'wrong geometry column name' )
        return 'fail'

    ds.Destroy()

    # Check for the layer with the new 'point25D' geometry column when tables= is specified
    ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string + ' tables=table37_inherited(point25D)', update = 1 )

    lyr = ds.GetLayerByName( 'table37_inherited(point25D)' )
    if lyr is None:
        gdaltest.post_reason( 'fail' )
        return 'fail'
    if lyr.GetGeomType() != ogr.wkbPoint25D:
        gdaltest.post_reason( 'fail' )
        return 'fail'
    if lyr.GetGeometryColumn() != 'point25D':
        gdaltest.post_reason( 'wrong geometry column name' )
        return 'fail'

    ds.Destroy()

    return 'success'

###############################################################################
# Test support for named views

def ogr_pg_39():
    if gdaltest.pg_ds is None:
        return 'skip'

    if not gdaltest.pg_has_postgis:
        gdaltest.pg_ds.ExecuteSQL( "CREATE VIEW testview AS SELECT * FROM table36_base" )
        ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string, update = 1 )
        found = ogr_pg_check_layer_in_list(ds, 'testview')
        if found is False:
            gdaltest.post_reason( 'layer testview not listed' )
            return 'fail'
        ds.Destroy()
        return 'success'

    gdaltest.pg_ds.ExecuteSQL( "CREATE VIEW testview AS SELECT * FROM table37_inherited" )
    if not gdaltest.pg_has_postgis_2:
        gdaltest.pg_ds.ExecuteSQL( "INSERT INTO geometry_columns VALUES ( '', 'public', 'testview', 'wkb_geometry', 2, -1, 'POINT') ")
    gdaltest.pg_ds.ExecuteSQL( "INSERT INTO table37_inherited (col1, col2, wkb_geometry) VALUES ( 'a', 'b', GeomFromEWKT('POINT (0 1)') )" )

    # Check for the layer
    ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string, update = 1 )
    found = ogr_pg_check_layer_in_list(ds, 'testview')
    if found is False:
        gdaltest.post_reason( 'layer testview not listed' )
        return 'fail'

    lyr = ds.GetLayerByName( 'testview' )
    if lyr is None:
        return 'fail'
    if gdaltest.pg_has_postgis:
        gfld_defn = lyr.GetLayerDefn().GetGeomFieldDefn(lyr.GetLayerDefn().GetGeomFieldIndex("wkb_geometry"))
        if gfld_defn is None:
            gdaltest.post_reason( 'fail' )
            return 'fail'
        if gfld_defn.GetType() != ogr.wkbPoint:
            gdaltest.post_reason( 'fail' )
            return 'fail'

    feat = lyr.GetNextFeature()
    if feat is None:
        gdaltest.post_reason( 'no feature')
        return 'fail'

    if feat.GetGeomFieldRef("wkb_geometry") is None or feat.GetGeomFieldRef("wkb_geometry").ExportToWkt() != 'POINT (0 1)':
        gdaltest.post_reason( 'bad geometry %s' % feat.GetGeometryRef().ExportToWkt())
        return 'fail'

    ds.Destroy()

    # Test another geometry column
    if not gdaltest.pg_has_postgis_2:
        gdaltest.pg_ds.ExecuteSQL( "INSERT INTO geometry_columns VALUES ( '', 'public', 'testview', 'point25D', 3, -1, 'POINT') ")
    gdaltest.pg_ds.ExecuteSQL( "UPDATE table37_inherited SET \"point25D\" = GeomFromEWKT('POINT (0 1 2)') " )

    # Check for the layer
    ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string, update = 1 )
    found = ogr_pg_check_layer_in_list(ds, 'testview')
    if found is False:
        gdaltest.post_reason( 'layer testview not listed' )
        return 'fail'

    lyr = ds.GetLayerByName( 'testview(point25D)' )
    if lyr is None:
        return 'fail'
    if gdaltest.pg_has_postgis and lyr.GetGeomType() != ogr.wkbPoint25D:
        return 'fail'

    try:
        if lyr.GetGeometryColumn() != 'point25D':
            gdaltest.post_reason( 'wrong geometry column name' )
            return 'fail'
    except:
        pass

    feat = lyr.GetNextFeature()
    if feat is None:
        gdaltest.post_reason( 'no feature')
        return 'fail'

    if feat.GetGeometryRef() is None or feat.GetGeometryRef().ExportToWkt() != 'POINT (0 1 2)':
        gdaltest.post_reason( 'bad geometry %s' % feat.GetGeometryRef().ExportToWkt())
        return 'fail'

    ds.Destroy()

    return 'success'

###############################################################################
# Test GetFeature() with an invalid id

def ogr_pg_40():
    if gdaltest.pg_ds is None:
        return 'skip'

    layer = gdaltest.pg_ds.GetLayerByName('tpoly')
    if layer.GetFeature(0) != None:
        return 'fail'

    return 'success'

###############################################################################
# Test active_schema= option

def ogr_pg_41():
    if gdaltest.pg_ds is None:
        return 'skip'

    ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string + ' active_schema=AutoTest-schema', update = 1 )

    # tpoly without schema refers to the active schema, that is to say AutoTest-schema
    found = ogr_pg_check_layer_in_list(ds, 'tpoly')
    if found is False:
        gdaltest.post_reason( 'layer tpoly not listed' )
        return 'fail'

    layer = ds.GetLayerByName('tpoly')
    if layer.GetFeatureCount() != 3:
        gdaltest.post_reason( 'wrong feature count' )
        print(layer.GetFeatureCount())
        return 'fail'

    found = ogr_pg_check_layer_in_list(ds, 'AutoTest-schema.tpoly')
    if found is True:
        gdaltest.post_reason( 'layer AutoTest-schema.tpoly is listed, but should not' )
        return 'fail'

    layer = ds.GetLayerByName('AutoTest-schema.tpoly')
    if layer.GetFeatureCount() != 3:
        gdaltest.post_reason( 'wrong feature count' )
        print(layer.GetFeatureCount())
        return 'fail'

    found = ogr_pg_check_layer_in_list(ds, 'public.tpoly')
    if found is False:
        gdaltest.post_reason( 'layer tpoly not listed' )
        return 'fail'

    layer = ds.GetLayerByName('public.tpoly')
    if layer.GetFeatureCount() != 19:
        gdaltest.post_reason( 'wrong feature count' )
        print(layer.GetFeatureCount())
        return 'fail'

    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:test41' )
    gdal.PopErrorHandler()

    ds.CreateLayer( 'test41')

    found = ogr_pg_check_layer_in_list(ds, 'test41')
    if found is False:
        gdaltest.post_reason( 'layer test41 not listed' )
        return 'fail'

    layer = ds.GetLayerByName('test41')
    if layer.GetFeatureCount() != 0:
        gdaltest.post_reason( 'wrong feature count' )
        print(layer.GetFeatureCount())
        return 'fail'

    layer = ds.GetLayerByName('AutoTest-schema.test41')
    if layer.GetFeatureCount() != 0:
        gdaltest.post_reason( 'wrong feature count' )
        print(layer.GetFeatureCount())
        return 'fail'

    ds.Destroy()

    return 'success'

###############################################################################
# Test schemas= option

def ogr_pg_42():
    if gdaltest.pg_ds is None:
        return 'skip'

    ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string + ' schemas=AutoTest-schema', update = 1 )

    # tpoly without schema refers to the active schema, that is to say AutoTest-schema
    found = ogr_pg_check_layer_in_list(ds, 'tpoly')
    if found is False:
        gdaltest.post_reason( 'layer tpoly not listed' )
        return 'fail'

    layer = ds.GetLayerByName('tpoly')
    if layer.GetFeatureCount() != 3:
        gdaltest.post_reason( 'wrong feature count' )
        print(layer.GetFeatureCount())
        return 'fail'

    found = ogr_pg_check_layer_in_list(ds, 'AutoTest-schema.tpoly')
    if found is True:
        gdaltest.post_reason( 'layer AutoTest-schema.tpoly is listed, but should not' )
        return 'fail'

    layer = ds.GetLayerByName('AutoTest-schema.tpoly')
    if layer.GetFeatureCount() != 3:
        gdaltest.post_reason( 'wrong feature count' )
        print(layer.GetFeatureCount())
        return 'fail'

    found = ogr_pg_check_layer_in_list(ds, 'public.tpoly')
    if found is True:
        gdaltest.post_reason( 'layer public.tpoly is listed, but should not' )
        return 'fail'

    layer = ds.GetLayerByName('public.tpoly')
    if layer.GetFeatureCount() != 19:
        gdaltest.post_reason( 'wrong feature count' )
        print(layer.GetFeatureCount())
        return 'fail'

    found = ogr_pg_check_layer_in_list(ds, 'test41')
    if found is False:
        gdaltest.post_reason( 'layer test41 not listed' )
        return 'fail'

    layer = ds.GetLayerByName('test41')
    if layer.GetFeatureCount() != 0:
        gdaltest.post_reason( 'wrong feature count' )
        print(layer.GetFeatureCount())
        return 'fail'

    layer = ds.GetLayerByName('AutoTest-schema.test41')
    if layer.GetFeatureCount() != 0:
        gdaltest.post_reason( 'wrong feature count' )
        print(layer.GetFeatureCount())
        return 'fail'

    ds.Destroy()

    return 'success'


###############################################################################
# Test schemas= option

def ogr_pg_43():
    if gdaltest.pg_ds is None:
        return 'skip'

    ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string + ' schemas=public,AutoTest-schema', update = 1 )

    # tpoly without schema refers to the active schema, that is to say public
    found = ogr_pg_check_layer_in_list(ds, 'tpoly')
    if found is False:
        gdaltest.post_reason( 'layer tpoly not listed' )
        return 'fail'

    layer = ds.GetLayerByName('tpoly')
    if layer.GetFeatureCount() != 19:
        gdaltest.post_reason( 'wrong feature count' )
        print(layer.GetFeatureCount())
        return 'fail'

    found = ogr_pg_check_layer_in_list(ds, 'AutoTest-schema.tpoly')
    if found is False:
        gdaltest.post_reason( 'layer AutoTest-schema.tpoly not listed' )
        return 'fail'

    layer = ds.GetLayerByName('AutoTest-schema.tpoly')
    if layer.GetFeatureCount() != 3:
        gdaltest.post_reason( 'wrong feature count' )
        print(layer.GetFeatureCount())
        return 'fail'

    ds.Destroy()

    return 'success'

###############################################################################
# Test for table and column names that need quoting (#2945)

def ogr_pg_44():

    if gdaltest.pg_ds is None:
        return 'skip'

    gdaltest.pg_lyr = gdaltest.pg_ds.CreateLayer( 'select', options = [ 'OVERWRITE=YES', 'GEOMETRY_NAME=where', 'DIM=3' ]  )
    ogrtest.quick_create_layer_def( gdaltest.pg_lyr, [ ('from', ogr.OFTReal)] )
    feat = ogr.Feature(gdaltest.pg_lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (0.5 0.5 1)'))
    gdaltest.pg_lyr.CreateFeature(feat)
    feat.Destroy()

    gdaltest.pg_ds.ExecuteSQL('ALTER TABLE "select" RENAME COLUMN "ogc_fid" to "AND"')

    ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string, update = 1 )
    layer = ds.GetLayerByName('select')
    geom = ogr.CreateGeometryFromWkt( 'POLYGON((0 0,0 1,1 1,1 0,0 0))' )
    layer.SetSpatialFilter( geom )
    geom.Destroy()
    if layer.GetFeatureCount() != 1:
        return 'fail'
    feat = layer.GetNextFeature()
    if feat.GetGeometryRef().ExportToWkt() != 'POINT (0.5 0.5 1)':
        return 'fail'

    feat = layer.GetFeature(1)
    if feat.GetGeometryRef().ExportToWkt() != 'POINT (0.5 0.5 1)':
        return 'fail'

    sql_lyr = ds.ExecuteSQL('SELECT * FROM "select"')
    geom = ogr.CreateGeometryFromWkt( 'POLYGON((0 0,0 1,1 1,1 0,0 0))' )
    sql_lyr.SetSpatialFilter( geom )
    geom.Destroy()
    if sql_lyr.GetFeatureCount() != 1:
        return 'fail'
    feat = sql_lyr.GetNextFeature()
    if feat.GetGeometryRef().ExportToWkt() != 'POINT (0.5 0.5 1)':
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)

    ds.Destroy()

    return 'success'

###############################################################################
# Test SetNextByIndex (#3117)

def ogr_pg_45():

    if gdaltest.pg_ds is None:
        return 'skip'

    lyr = gdaltest.pg_ds.GetLayerByName('tpoly')

    if not lyr.TestCapability(ogr.OLCFastSetNextByIndex):
        gdaltest.post_reason('OLCFastSetNextByIndex returned false')
        return 'fail'

    nb_feat = lyr.GetFeatureCount()
    tab_feat = [ None for i in range(nb_feat) ]
    for i in range(nb_feat):
        tab_feat[i] = lyr.GetNextFeature()

    lyr.SetNextByIndex(2)
    feat = lyr.GetNextFeature()
    if feat.GetFID() != tab_feat[2].GetFID():
        gdaltest.post_reason('SetNextByIndex(2) did not return expected feature')
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetFID() != tab_feat[3].GetFID():
        gdaltest.post_reason('did not get expected feature')
        return 'fail'

    return 'success'

###############################################################################
# Test that we can read more than 500 features and update each one
# with SetFeature()

def ogr_pg_46():

    if gdaltest.pg_ds is None:
        return 'skip'

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
        if feat.GetFieldAsInteger(0) != expected_val:
            gdaltest.post_reason('expected value was %d. Got %d' % (expected_val, feat.GetFieldAsInteger(0)))
            return 'fail'
        feat.SetField(0, -count)
        lyr.SetFeature(feat)
        feat.Destroy()

        count = count + 1

        feat = sqllyr.GetNextFeature()

    if count != nFeatures:
        gdaltest.post_reason('did not get expected %d features' % nFeatures)
        return 'fail'

    # Check that 1 feature has been correctly updated
    sqllyr.SetNextByIndex(1)
    feat = sqllyr.GetNextFeature()
    expected_val = -1
    if feat.GetFieldAsInteger(0) != expected_val:
        gdaltest.post_reason('expected value was %d. Got %d' % (expected_val, feat.GetFieldAsInteger(0)))
        return 'fail'
    feat.Destroy()

    gdaltest.pg_ds.ReleaseResultSet(sqllyr)

    return 'success'

###############################################################################
# Test that we can handle 'geography' column type introduced in PostGIS 1.5

def ogr_pg_47():

    if gdaltest.pg_ds is None:
        return 'skip'

    if not gdaltest.pg_has_postgis:
        return 'skip'

    # Create table with geography column
    gdaltest.pg_ds.ExecuteSQL("DELETE FROM spatial_ref_sys")
    gdaltest.pg_ds.ExecuteSQL("""INSERT INTO "spatial_ref_sys" ("srid","auth_name","auth_srid","srtext","proj4text") VALUES (4326,'EPSG',4326,'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.01745329251994328,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4326"]]','+proj=longlat +ellps=WGS84 +datum=WGS84 +no_defs ')""")

    if gdaltest.pg_ds.GetLayerByName('geography_columns') is None:
        gdaltest.post_reason('autotest database must be created with PostGIS >= 1.5')
        return 'skip'

    gdaltest.pg_ds = None
    gdaltest.pg_ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string, update = 1 )

    srs = osr.SpatialReference()
    srs.ImportFromEPSG( 4326 )
    lyr = gdaltest.pg_ds.CreateLayer('test_geog', srs = srs, options = [ 'GEOM_TYPE=geography', 'GEOMETRY_NAME=my_geog' ] )
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
    gdaltest.pg_ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string, update = 1 )

    # Check if the layer is listed
    found = ogr_pg_check_layer_in_list(gdaltest.pg_ds, 'test_geog')
    if found is False:
        gdaltest.post_reason( 'layer test_geog not listed' )
        return 'fail'

    # Check that the layer is recorder in geometry_columns table
    geography_columns_lyr = gdaltest.pg_ds.ExecuteSQL("SELECT * FROM geography_columns WHERE f_table_name = 'test_geog'")
    feat = geography_columns_lyr.GetNextFeature()
    if feat.GetFieldAsString('f_geography_column') != 'my_geog':
        feat.DumpReadable()
        return 'fail'
    gdaltest.pg_ds.ReleaseResultSet(geography_columns_lyr)

    # Get the layer by name
    lyr = gdaltest.pg_ds.GetLayerByName('test_geog')
    if lyr.GetExtent() != (2.0, 2.0, 49.0, 49.0):
        gdaltest.post_reason( 'bad extent for test_geog' )
        return 'fail'

    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    if geom.ExportToWkt() != 'POINT (2 49)':
        gdaltest.post_reason( 'bad geometry for test_geog' )
        return 'fail'
    feat.Destroy()

    # Check with result set
    sql_lyr = gdaltest.pg_ds.ExecuteSQL('SELECT * FROM test_geog')
    if sql_lyr.GetExtent() != (2.0, 2.0, 49.0, 49.0):
        gdaltest.post_reason( 'bad extent for SELECT * FROM test_geog' )
        return 'fail'

    feat = sql_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    if geom.ExportToWkt() != 'POINT (2 49)':
        gdaltest.post_reason( 'bad geometry for SELECT * FROM test_geog' )
        return 'fail'
    feat.Destroy()
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

    # Check ST_AsText
    sql_lyr = gdaltest.pg_ds.ExecuteSQL('SELECT ST_AsText(my_geog) FROM test_geog')
    feat = sql_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    if geom.ExportToWkt() != 'POINT (2 49)':
        gdaltest.post_reason( 'bad geometry for SELECT ST_AsText(my_geog) FROM test_geog' )
        return 'fail'
    feat.Destroy()
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

    # Check ST_AsBinary
    sql_lyr = gdaltest.pg_ds.ExecuteSQL('SELECT ST_AsBinary(my_geog) FROM test_geog')
    feat = sql_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    if geom.ExportToWkt() != 'POINT (2 49)':
        gdaltest.post_reason( 'bad geometry for SELECT ST_AsBinary(my_geog) FROM test_geog' )
        return 'fail'
    feat.Destroy()
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

    return 'success'

###############################################################################
# Test that we can read a table without any primary key (#2082)
# Test also the effect of PG_LIST_ALL_TABLES with a non spatial table in a
# PostGIS DB.

def ogr_pg_48():

    if gdaltest.pg_ds is None:
        return 'skip'

    gdaltest.pg_ds.ExecuteSQL("CREATE TABLE no_pk_table (gid serial NOT NULL, other_id INTEGER)")
    gdaltest.pg_ds.ExecuteSQL("INSERT INTO no_pk_table (gid, other_id) VALUES (1, 10)")

    gdaltest.pg_ds.Destroy()
    gdaltest.pg_ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string, update = 1 )

    found = ogr_pg_check_layer_in_list(gdaltest.pg_ds, 'no_pk_table')
    if gdaltest.pg_has_postgis:
        # Non spatial table in a PostGIS db -> not listed ...
        if found:
            gdaltest.post_reason( 'layer no_pk_table unexpectedly listed' )
            return 'fail'

        # ... but should be found on explicit request
        lyr = gdaltest.pg_ds.GetLayer('no_pk_table')
        if lyr is None:
            gdaltest.post_reason( 'could not get no_pk_table' )
            return 'fail'

        # Try again by setting PG_LIST_ALL_TABLES=YES
        gdal.SetConfigOption('PG_LIST_ALL_TABLES', 'YES')
        gdaltest.pg_ds.Destroy()
        gdaltest.pg_ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string, update = 1 )
        gdal.SetConfigOption('PG_LIST_ALL_TABLES', None)
        found = ogr_pg_check_layer_in_list(gdaltest.pg_ds, 'no_pk_table')

        if found is False:
            gdaltest.post_reason( 'layer no_pk_table not listed' )
            return 'fail'

        # Test LIST_ALL_TABLES=YES open option
        gdaltest.pg_ds.Destroy()
        gdaltest.pg_ds = gdal.OpenEx( 'PG:' + gdaltest.pg_connection_string, gdal.OF_VECTOR | gdal.OF_UPDATE, open_options = ['LIST_ALL_TABLES=YES'] )
        found = ogr_pg_check_layer_in_list(gdaltest.pg_ds, 'no_pk_table')

    if found is False:
        gdaltest.post_reason( 'layer no_pk_table not listed' )
        return 'fail'

    lyr = gdaltest.pg_ds.GetLayer('no_pk_table')
    if lyr is None:
        gdaltest.post_reason( 'could not get no_pk_table' )
        return 'fail'

    sr = lyr.GetSpatialRef()
    if sr is not None:
        gdaltest.post_reason( 'did not get expected SRS' )
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat is None:
        gdaltest.post_reason( 'did not get feature' )
        return 'fail'

    if lyr.GetFIDColumn() != '':
        gdaltest.post_reason( 'got a non NULL FID column' )
        print(lyr.GetFIDColumn())
        return 'fail'

    if feat.GetFID() != 0:
        gdaltest.post_reason( 'did not get expected FID' )
        feat.DumpReadable()
        return 'fail'

    if feat.GetFieldAsInteger('gid') != 1:
        gdaltest.post_reason( 'did not get expected gid' )
        feat.DumpReadable()
        return 'fail'

    if feat.GetFieldAsInteger('other_id') != 10:
        gdaltest.post_reason( 'did not get expected other_id' )
        feat.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################
# Go on with previous test but set PGSQL_OGR_FID this time

def ogr_pg_49():

    if gdaltest.pg_ds is None:
        return 'skip'

    gdal.SetConfigOption('PGSQL_OGR_FID', 'other_id')
    gdaltest.pg_ds = None
    gdaltest.pg_ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string, update = 1 )
    lyr = gdaltest.pg_ds.GetLayer('no_pk_table')
    gdal.SetConfigOption('PGSQL_OGR_FID', None)

    feat = lyr.GetNextFeature()
    lyr.ResetReading() # to close implicit transaction

    if lyr.GetFIDColumn() != 'other_id':
        print(lyr.GetFIDColumn())
        gdaltest.post_reason( 'did not get expected FID column' )
        return 'fail'

    if feat.GetFID() != 10:
        gdaltest.post_reason( 'did not get expected FID' )
        feat.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################
# Write and read NaN values (#3667)
# This tests writing using COPY and INSERT

def ogr_pg_50():

    if gdaltest.pg_ds is None:
        return 'skip'

    gdaltest.pg_lyr = gdaltest.pg_ds.GetLayerByName( 'tpoly' )
    if gdaltest.pg_lyr is None:
        gdaltest.post_reason( 'did not get tpoly layer' )
        return 'fail'

    feature_def = gdaltest.pg_lyr.GetLayerDefn()
    dst_feat = ogr.Feature( feature_def )

    try:
        dst_feat.SetFieldDoubleList
        bHasSetFieldDoubleList = True
    except:
        bHasSetFieldDoubleList = False

    for option in [ 'NO', 'YES' ]:
        gdal.SetConfigOption( 'PG_USE_COPY', option )
        gdaltest.pg_lyr.ResetReading()
        for value in [ 'NaN', 'Inf', '-Inf' ]:
            dst_feat.SetField( 'AREA', float(value) )
            dst_feat.SetField( 'PRFEDEA', value )
            dst_feat.SetField( 'SHORTNAME', option )
            if bHasSetFieldDoubleList:
                dst_feat.SetFieldDoubleList( feature_def.GetFieldIndex('REALLIST'), [float(value), float(value)])
            dst_feat.SetFID(-1)
            gdaltest.pg_lyr.CreateFeature( dst_feat )

    gdal.SetConfigOption( 'PG_USE_COPY', gdaltest.pg_use_copy )
    dst_feat.Destroy()

    for option in [ 'NO', 'YES' ]:
        for value in [ 'NaN', 'Inf', '-Inf' ]:
            gdaltest.pg_lyr.SetAttributeFilter( 'PRFEDEA = \''+value+'\' AND SHORTNAME = \''+option+'\'' )
            feat = gdaltest.pg_lyr.GetNextFeature()
            got_val = feat.GetField( 'AREA' )
            if value == 'NaN':
                if not gdaltest.isnan(got_val):
                    print(feat.GetFieldAsString( 'AREA' )+' returned for AREA instead of '+value)
                    gdaltest.pg_lyr.ResetReading() # to close implicit transaction
                    return 'fail'
            elif got_val != float(value):
                print(feat.GetFieldAsString( 'AREA' )+' returned for AREA instead of '+value)
                gdaltest.pg_lyr.ResetReading() # to close implicit transaction
                return 'fail'

            if bHasSetFieldDoubleList:
                got_val = feat.GetFieldAsDoubleList( feature_def.GetFieldIndex('REALLIST') )
                if value == 'NaN':
                    if not gdaltest.isnan(got_val[0]) or not gdaltest.isnan(got_val[1]):
                        print(feat.GetFieldAsString( 'REALLIST' )+' returned for REALLIST instead of '+value)
                        gdaltest.pg_lyr.ResetReading() # to close implicit transaction
                        return 'fail'
                elif got_val[0] != float(value) or got_val[1] != float(value):
                    print(feat.GetFieldAsString( 'REALLIST' )+' returned for REALLIST instead of '+value)
                    gdaltest.pg_lyr.ResetReading() # to close implicit transaction
                    return 'fail'

    gdaltest.pg_lyr.ResetReading() # to close implicit transaction

    return 'success'

###############################################################################
# Run test_ogrsf

def ogr_pg_51():

    if gdaltest.pg_ds is None:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' "' + 'PG:' + gdaltest.pg_connection_string + '" tpoly testview')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Run test_ogrsf with -sql

def ogr_pg_52():

    if gdaltest.pg_ds is None:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' "' + 'PG:' + gdaltest.pg_connection_string + '" -sql "SELECT * FROM tpoly"')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test creating a layer with explicitly wkbNone geometry type.

def ogr_pg_53():

    if gdaltest.pg_ds is None:
        return 'skip'

    lyr = gdaltest.pg_ds.CreateLayer( 'no_geometry_table', geom_type = ogr.wkbNone )
    field_defn = ogr.FieldDefn('foo')
    lyr.CreateField(field_defn)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 'bar')
    lyr.CreateFeature(feat)

    lyr.ResetReading() # force above feature to be committed

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string)

    if gdaltest.pg_has_postgis is True and ogr_pg_check_layer_in_list(ds, 'no_geometry_table') is True:
        gdaltest.post_reason('did not expected no_geometry_table to be listed at that point')
        return 'fail'

    lyr = ds.GetLayerByName('no_geometry_table')
    feat = lyr.GetNextFeature()
    if feat.GetField(0) != 'bar':
        return 'fail'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr = ds.CreateLayer( 'no_geometry_table', geom_type = ogr.wkbNone)
    gdal.PopErrorHandler()
    if lyr is not None:
        gdaltest.post_reason('layer creation should have failed')
        return 'fail'

    lyr = ds.CreateLayer( 'no_geometry_table', geom_type = ogr.wkbNone, options = ['OVERWRITE=YES'] )
    field_defn = ogr.FieldDefn('baz')
    lyr.CreateField(field_defn)

    ds = None
    ds = ogr.Open('PG:' + gdaltest.pg_connection_string)

    lyr = ds.CreateLayer( 'no_geometry_table', geom_type = ogr.wkbNone, options = ['OVERWRITE=YES'] )
    field_defn = ogr.FieldDefn('bar')
    lyr.CreateField(field_defn)
    field_defn = ogr.FieldDefn('baz')
    lyr.CreateField(field_defn)
    if lyr is None:
        return 'fail'

    ds = None

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
    lyr = ds.GetLayerByName('no_geometry_table')
    if lyr.GetLayerDefn().GetFieldCount() != 2:
        return 'fail'

    return 'success'

###############################################################################
# Check that we can overwrite a non-spatial geometry table (#4012)

def ogr_pg_53_bis():
    import test_cli_utilities
    if test_cli_utilities.get_ogr2ogr_path() is None:
        return 'skip'

    f = open('tmp/no_geometry_table.csv', 'wt')
    f.write('foo,bar\n')
    f.write('"baz","foo"\n')
    f.close()
    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f PostgreSQL "' + 'PG:' + gdaltest.pg_connection_string + '" tmp/no_geometry_table.csv -overwrite')

    os.unlink('tmp/no_geometry_table.csv')

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
    lyr = ds.GetLayerByName('no_geometry_table')
    feat = lyr.GetNextFeature()
    if feat.GetField(0) != 'baz':
        return 'fail'

    return 'success'

###############################################################################
# Test reading AsEWKB()

def ogr_pg_54():

    if gdaltest.pg_ds is None:
        return 'skip'

    if not gdaltest.pg_has_postgis:
        return 'skip'

    if gdaltest.pg_has_postgis_2:
        sql_lyr = gdaltest.pg_ds.ExecuteSQL("SELECT ST_AsEWKB(GeomFromEWKT('POINT (0 1 2)'))")
    else:
        sql_lyr = gdaltest.pg_ds.ExecuteSQL("SELECT AsEWKB(GeomFromEWKT('POINT (0 1 2)'))")
    feat = sql_lyr.GetNextFeature()
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

    geom = feat.GetGeometryRef()
    if geom.ExportToWkt() != 'POINT (0 1 2)':
        return 'fail'

    return 'success'

###############################################################################
# Test reading geoms as Base64 encoded strings

def ogr_pg_55():

    if gdaltest.pg_ds is None:
        return 'skip'

    if not gdaltest.pg_has_postgis:
        return 'skip'

    layer = gdaltest.pg_ds.CreateLayer( 'ogr_pg_55', options = [ 'DIM=3' ]  )
    feat = ogr.Feature(layer.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT (1 2 3)'))
    layer.CreateFeature(feat)
    feat = None

    layer.ResetReading() # force above feature to be committed

    old_val = gdal.GetConfigOption('PG_USE_BASE64')
    gdal.SetConfigOption('PG_USE_BASE64', 'YES')
    ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string, update = 1 )
    layer = ds.GetLayerByName('ogr_pg_55')
    feat = layer.GetNextFeature()
    gdal.SetConfigOption('PG_USE_BASE64', old_val)
    if feat.GetGeometryRef().ExportToWkt() != 'POINT (1 2 3)':
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test insertion of features with first field being a 0-character string in a
# non-spatial table and without FID in COPY mode (#4040)

def ogr_pg_56():

    if gdaltest.pg_ds is None:
        return 'skip'

    gdaltest.pg_ds.ExecuteSQL('CREATE TABLE ogr_pg_56 ( bar varchar, baz varchar ) WITH (OIDS=FALSE)')

    gdal.SetConfigOption( 'PG_USE_COPY', 'YES' )

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update = 1)
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

    gdal.SetConfigOption( 'PG_USE_COPY', gdaltest.pg_use_copy )

    ds = None

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
    lyr = ds.GetLayerByName('ogr_pg_56')

    feat = lyr.GetNextFeature()
    if feat.GetField(0) != None or feat.GetField(1) != None:
        gdaltest.post_reason('did not get expected value for feat %d' % feat.GetFID())
        feat.DumpReadable()
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetField(0) != '' or feat.GetField(1) != None:
        gdaltest.post_reason('did not get expected value for feat %d' % feat.GetFID())
        feat.DumpReadable()
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetField(0) != None or feat.GetField(1) != '':
        gdaltest.post_reason('did not get expected value for feat %d' % feat.GetFID())
        feat.DumpReadable()
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetField(0) != '' or feat.GetField(1) != '':
        gdaltest.post_reason('did not get expected value for feat %d' % feat.GetFID())
        feat.DumpReadable()
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetField(0) != 'bar' or feat.GetField(1) != '':
        gdaltest.post_reason('did not get expected value for feat %d' % feat.GetFID())
        feat.DumpReadable()
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetField(0) != '' or feat.GetField(1) != 'baz':
        gdaltest.post_reason('did not get expected value for feat %d' % feat.GetFID())
        feat.DumpReadable()
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test inserting an empty feature

def ogr_pg_57():

    if gdaltest.pg_ds is None:
        return 'skip'

    lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_57')
    lyr.CreateField(ogr.FieldDefn('acolumn', ogr.OFTString))
    feat = ogr.Feature(lyr.GetLayerDefn())
    ret = lyr.CreateFeature(feat)
    feat = None

    if ret != 0:
        return 'fail'

    return 'success'

###############################################################################
# Test RFC35

def ogr_pg_58():

    if gdaltest.pg_ds is None:
        return 'skip'

    lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_58')
    lyr.CreateField(ogr.FieldDefn('strcolumn', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('aintcolumn', ogr.OFTInteger))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('aintcolumn', 12345)
    lyr.CreateFeature(feat)
    feat = None

    if lyr.TestCapability(ogr.OLCReorderFields) != 0:
        return 'fail'
    if lyr.TestCapability(ogr.OLCAlterFieldDefn) != 1:
        return 'fail'
    if lyr.TestCapability(ogr.OLCDeleteField) != 1:
        return 'fail'

    fd = ogr.FieldDefn('anotherstrcolumn', ogr.OFTString)
    fd.SetWidth(5)
    lyr.AlterFieldDefn(lyr.GetLayerDefn().GetFieldIndex('aintcolumn'), fd, ogr.ALTER_ALL_FLAG)

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat.GetField('anotherstrcolumn') != '12345':
        gdaltest.post_reason('failed (1)')
        return 'fail'

    gdaltest.pg_ds = None
    gdaltest.pg_ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string, update = 1 )
    lyr = gdaltest.pg_ds.GetLayer('ogr_pg_58')

    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat.GetField('anotherstrcolumn') != '12345':
        gdaltest.post_reason('failed (2)')
        return 'fail'
    feat = None
    lyr.ResetReading() # to close implicit transaction

    if lyr.DeleteField(lyr.GetLayerDefn().GetFieldIndex('anotherstrcolumn')) != 0:
        gdaltest.post_reason('failed (3)')
        return 'fail'

    gdaltest.pg_ds = None
    gdaltest.pg_ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string, update = 1 )
    lyr = gdaltest.pg_ds.GetLayer('ogr_pg_58')

    if lyr.GetLayerDefn().GetFieldCount() != 1:
        gdaltest.post_reason('failed (4)')
        return 'fail'

    return 'success'

###############################################################################
# Check that we can use -nln with a layer name that is recognized by GetLayerByName()
# but which is not the layer name.

def ogr_pg_59():

    if gdaltest.pg_ds is None:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_ogr2ogr_path() is None:
        return 'skip'

    gdaltest.runexternal(
        test_cli_utilities.get_ogr2ogr_path()
        + ' -append -f PostgreSQL "' + 'PG:' + gdaltest.pg_connection_string
        + '" data/poly.shp -nln public.tpoly')

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
    lyr = ds.GetLayerByName('tpoly')
    fc = lyr.GetFeatureCount()
    ds = None

    if fc != 35:
        gdaltest.post_reason('did not get expected feature count')
        return 'fail'

    return 'success'

###############################################################################
# Test that we can insert a feature that has a FID on a table with a
# non-incrementing PK.

def ogr_pg_60():

    if gdaltest.pg_ds is None:
        return 'skip'

    sql_lyr = gdaltest.pg_ds.ExecuteSQL("CREATE TABLE ogr_pg_60(id integer,"
                                        "name varchar(50),primary key (id)) "
                                        "without oids")
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

    gdaltest.pg_ds = None
    gdaltest.pg_ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string,
                               update = 1 )
    lyr = gdaltest.pg_ds.GetLayerByName('ogr_pg_60')
    if lyr.GetFIDColumn() != 'id':
        gdaltest.post_reason('did not get expected name for FID column')
        print(lyr.GetFIDColumn())
        return 'fail'

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetFID(100)
    feat.SetField('name', 'a_name')
    lyr.CreateFeature(feat)
    if feat.GetFID() != 100:
        gdaltest.post_reason('bad FID value')
        return 'fail'

    feat = lyr.GetFeature(100)
    if feat is None:
        gdaltest.post_reason('did not get feature')
        return 'fail'

    return 'success'

###############################################################################
# Test insertion of features with FID set in COPY mode (#4495)

def ogr_pg_61():

    if gdaltest.pg_ds is None:
        return 'skip'

    gdaltest.pg_ds.ExecuteSQL('CREATE TABLE ogr_pg_61 ( id integer NOT NULL PRIMARY KEY, bar varchar )')

    gdal.SetConfigOption( 'PG_USE_COPY', 'YES' )

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update = 1)
    lyr = ds.GetLayerByName('ogr_pg_61')

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetFID(10)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetFID(20)
    feat.SetField(0, 'baz')
    lyr.CreateFeature(feat)

    gdal.SetConfigOption( 'PG_USE_COPY', gdaltest.pg_use_copy )

    ds = None

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
    lyr = ds.GetLayerByName('ogr_pg_61')

    feat = lyr.GetFeature(10)
    if feat.IsFieldSet(0):
        gdaltest.post_reason('did not get expected value for feat %d' % feat.GetFID())
        feat.DumpReadable()
        return 'fail'

    feat = lyr.GetFeature(20)
    if feat.GetField(0) != 'baz':
        gdaltest.post_reason('did not get expected value for feat %d' % feat.GetFID())
        feat.DumpReadable()
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test ExecuteSQL() and getting SRID of the request (#4699)

def ogr_pg_62():

    if gdaltest.pg_ds is None:
        return 'skip'

    if not gdaltest.pg_has_postgis:
        return 'skip'

    # Test on a regular request in a table
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    gdaltest.pg_ds.ExecuteSQL('DELLAYER:testsrtext2')
    gdaltest.pg_ds.CreateLayer( 'testsrtext2', srs = srs)

    sql_lyr = gdaltest.pg_ds.ExecuteSQL('SELECT * FROM testsrtext2')
    got_srs = sql_lyr.GetSpatialRef()
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)
    if got_srs is None or got_srs.ExportToWkt().find('32631') == -1:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test a request on a table, but with a geometry column not in the table
    sql_lyr = gdaltest.pg_ds.ExecuteSQL("SELECT eas_id, GeomFromEWKT('SRID=4326;POINT(0 1)') FROM tpoly")
    got_srs = sql_lyr.GetSpatialRef()
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)
    if got_srs is None or got_srs.ExportToWkt().find('4326') == -1:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test COLUMN_TYPES layer creation option (#4788)

def ogr_pg_63():

    if gdaltest.pg_ds is None:
        return 'skip'

    # No need to test it in the non PostGIS case
    if not gdaltest.pg_has_postgis:
        return 'skip'

    lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_63', options = ['COLUMN_TYPES=foo=int8,bar=numeric(10,5),baz=hstore,baw=numeric(20,0)'])
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
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('foo')).GetType() != ogr.OFTInteger64:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('bar')).GetType() != ogr.OFTReal:
        gdaltest.post_reason('fail')
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetField('foo') != 123:
        gdaltest.post_reason('fail')
        return 'fail'
    if feat.GetField('baw') != 123456789012345:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test OGR_TRUNCATE config. option (#5091)

def ogr_pg_64():

    if gdaltest.pg_ds is None:
        return 'skip'

    # No need to test it in the non PostGIS case
    if not gdaltest.pg_has_postgis:
        return 'skip'

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update = 1)
    lyr = ds.GetLayerByName('ogr_pg_63')

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('foo', '124')
    lyr.CreateFeature(feat)

    if lyr.GetFeatureCount() != 2:
        gdaltest.post_reason('fail')
        print(lyr.GetFeatureCount())
        return 'fail'

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update = 1)
    lyr = ds.GetLayerByName('ogr_pg_63')

    gdal.SetConfigOption('OGR_TRUNCATE', 'YES')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('foo', '125')
    lyr.CreateFeature(feat)

    gdal.SetConfigOption('OGR_TRUNCATE', None)

    # Just one feature because of truncation
    if lyr.GetFeatureCount() != 1:
        gdaltest.post_reason('fail')
        print(lyr.GetFeatureCount())
        return 'fail'

    return 'success'

###############################################################################
# Test RFC 41

def ogr_pg_65():

    if gdaltest.pg_ds is None:
        return 'skip'

    if not gdaltest.pg_has_postgis:
        return 'skip'

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update = 1)
    if ds.TestCapability(ogr.ODsCCreateGeomFieldAfterCreateLayer) == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr = ds.CreateLayer('ogr_pg_65', geom_type = ogr.wkbNone)
    if lyr.TestCapability(ogr.OLCCreateGeomField) == 0:
        gdaltest.post_reason('fail')
        return 'fail'

    gfld_defn = ogr.GeomFieldDefn('geom1', ogr.wkbPoint)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    gfld_defn.SetSpatialRef(srs)
    if lyr.CreateGeomField(gfld_defn) != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    gfld_defn = ogr.GeomFieldDefn('geom2', ogr.wkbLineString25D)
    if lyr.CreateGeomField(gfld_defn) != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    gfld_defn = ogr.GeomFieldDefn('geom3', ogr.wkbPoint)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(32631)
    gfld_defn.SetSpatialRef(srs)
    if lyr.CreateGeomField(gfld_defn) != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr.CreateField(ogr.FieldDefn('intfield', ogr.OFTInteger))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('intfield', 123)
    feat.SetGeomFieldDirectly('geom1', ogr.CreateGeometryFromWkt('POINT (1 2)'))
    feat.SetGeomFieldDirectly('geom2', ogr.CreateGeometryFromWkt('LINESTRING (3 4 5,6 7 8)'))
    if lyr.CreateFeature(feat) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = ogr.Feature(lyr.GetLayerDefn())
    if lyr.CreateFeature(feat) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat.GetField('intfield') != 123 or \
       feat.GetGeomFieldRef('geom1').ExportToWkt() != 'POINT (1 2)' or \
       feat.GetGeomFieldRef('geom2').ExportToWkt() != 'LINESTRING (3 4 5,6 7 8)':
        feat.DumpReadable()
        gdaltest.post_reason('fail')
        return 'fail'
    feat = lyr.GetNextFeature()
    if feat.GetGeomFieldRef('geom1') is not None or \
       feat.GetGeomFieldRef('geom2') is not None:
        feat.DumpReadable()
        gdaltest.post_reason('fail')
        return 'fail'

    ds = None
    for i in range(2):
        ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
        if i == 0:
            lyr = ds.GetLayerByName('ogr_pg_65')
        else:
            lyr = ds.ExecuteSQL('SELECT * FROM ogr_pg_65')
        if lyr.GetLayerDefn().GetGeomFieldDefn(0).GetName() != 'geom1':
            gdaltest.post_reason('fail')
            return 'fail'
        if i == 0 and lyr.GetLayerDefn().GetGeomFieldDefn(0).GetType() != ogr.wkbPoint:
            gdaltest.post_reason('fail')
            return 'fail'
        if lyr.GetLayerDefn().GetGeomFieldDefn(0).GetSpatialRef().ExportToWkt().find('4326') < 0:
            gdaltest.post_reason('fail')
            return 'fail'
        if lyr.GetLayerDefn().GetGeomFieldDefn(1).GetName() != 'geom2':
            gdaltest.post_reason('fail')
            return 'fail'
        if i == 0 and lyr.GetLayerDefn().GetGeomFieldDefn(1).GetType() != ogr.wkbLineString25D:
            gdaltest.post_reason('fail')
            return 'fail'
        if lyr.GetLayerDefn().GetGeomFieldDefn(1).GetSpatialRef() is not None:
            gdaltest.post_reason('fail')
            return 'fail'
        if i == 0 and lyr.GetLayerDefn().GetGeomFieldDefn(2).GetSpatialRef().ExportToWkt().find('32631') < 0:
            gdaltest.post_reason('fail')
            return 'fail'
        feat = lyr.GetNextFeature()
        if feat.GetField('intfield') != 123 or \
        feat.GetGeomFieldRef('geom1').ExportToWkt() != 'POINT (1 2)' or \
        feat.GetGeomFieldRef('geom2').ExportToWkt() != 'LINESTRING (3 4 5,6 7 8)':
            feat.DumpReadable()
            gdaltest.post_reason('fail')
            return 'fail'
        feat = lyr.GetNextFeature()
        if feat.GetGeomFieldRef('geom1') is not None or \
           feat.GetGeomFieldRef('geom2') is not None:
            feat.DumpReadable()
            gdaltest.post_reason('fail')
            return 'fail'
        if i == 1:
            ds.ReleaseResultSet(lyr)

    gdal.SetConfigOption('PG_USE_COPY', 'YES')
    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update = 1)
    lyr = ds.GetLayerByName('ogr_pg_65')
    lyr.DeleteFeature(1)
    lyr.DeleteFeature(2)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeomFieldDirectly('geom1', ogr.CreateGeometryFromWkt('POINT (3 4)'))
    feat.SetGeomFieldDirectly('geom2', ogr.CreateGeometryFromWkt('LINESTRING (4 5 6,7 8 9)'))
    if lyr.CreateFeature(feat) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = ogr.Feature(lyr.GetLayerDefn())
    if lyr.CreateFeature(feat) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.SetConfigOption('PG_USE_COPY', gdaltest.pg_use_copy)

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
    lyr = ds.GetLayerByName('ogr_pg_65')
    feat = lyr.GetNextFeature()
    if feat.GetGeomFieldRef('geom1').ExportToWkt() != 'POINT (3 4)' or \
       feat.GetGeomFieldRef('geom2').ExportToWkt() != 'LINESTRING (4 5 6,7 8 9)':
        feat.DumpReadable()
        gdaltest.post_reason('fail')
        return 'fail'
    feat = lyr.GetNextFeature()
    if feat.GetGeomFieldRef('geom1') is not None or \
        feat.GetGeomFieldRef('geom2') is not None:
        feat.DumpReadable()
        gdaltest.post_reason('fail')
        return 'fail'

    import test_cli_utilities
    if test_cli_utilities.get_ogr2ogr_path() is not None:
        gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -update "' + 'PG:' + gdaltest.pg_connection_string + '" "' + 'PG:' + gdaltest.pg_connection_string + '" ogr_pg_65 -nln ogr_pg_65_copied -overwrite')
        ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
        lyr = ds.GetLayerByName('ogr_pg_65_copied')
        if lyr.GetLayerDefn().GetGeomFieldDefn(0).GetSpatialRef().ExportToWkt().find('4326') < 0:
            gdaltest.post_reason('fail')
            return 'fail'
        if lyr.GetLayerDefn().GetGeomFieldDefn(1).GetSpatialRef() is not None:
            gdaltest.post_reason('fail')
            return 'fail'
        if lyr.GetLayerDefn().GetGeomFieldDefn(2).GetSpatialRef().ExportToWkt().find('32631') < 0:
            gdaltest.post_reason('fail')
            return 'fail'

    return 'success'

###############################################################################
# Run test_ogrsf

def ogr_pg_66():

    if gdaltest.pg_ds is None:
        return 'skip'

    if not gdaltest.pg_has_postgis:
        return 'skip'

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' "' + 'PG:' + gdaltest.pg_connection_string + '" ogr_pg_65')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test retrieving SRID from values (#5131)

def ogr_pg_67():

    if gdaltest.pg_ds is None:
        return 'skip'

    if not gdaltest.pg_has_postgis:
        return 'skip'

    lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_67')
    lyr.ResetReading() # to trigger layer creation
    lyr = None

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update = 1)
    lyr = ds.GetLayerByName('ogr_pg_67')
    if lyr.GetSpatialRef() is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)
    feat = None
    ds = None

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update = 1)
    lyr = ds.GetLayerByName('ogr_pg_67')
    if lyr.GetSpatialRef() is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.ExecuteSQL("ALTER TABLE ogr_pg_67 DROP CONSTRAINT enforce_srid_wkb_geometry")
    ds.ExecuteSQL("UPDATE ogr_pg_67 SET wkb_geometry = ST_GeomFromEWKT('SRID=4326;POINT(0 1)')")
    ds = None

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update = 1)
    lyr = ds.GetLayerByName('ogr_pg_67')
    if lyr.GetSpatialRef() is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string + ' tables=fake', update = 1)
    lyr = ds.GetLayerByName('ogr_pg_67')
    if lyr.GetSpatialRef() is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test retrieving SRID from values even if we don't have select rights on geometry_columns (#5131)

def ogr_pg_68():

    if gdaltest.pg_ds is None:
        return 'skip'

    if not gdaltest.pg_has_postgis:
        return 'skip'

    srs = osr.SpatialReference()
    srs.ImportFromEPSG( 4326 )

    lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_68', srs = srs)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt('POINT(0 0)'))
    lyr.CreateFeature(feat)
    lyr = None

    sql_lyr = gdaltest.pg_ds.ExecuteSQL('SELECT current_user')
    feat = sql_lyr.GetNextFeature()
    current_user = feat.GetField(0)
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

    gdaltest.pg_ds.ExecuteSQL("REVOKE SELECT ON geometry_columns FROM %s" % current_user)

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string + ' tables=fake', update = 1)
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

    if got_srs is None:
        gdaltest.post_reason('fail')
        return 'fail'

    if got_srs2 is None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test deferred loading of tables (#5450)

def has_run_load_tables(ds):
    return int(ds.GetMetadataItem("bHasLoadTables", "_DEBUG_"))

def ogr_pg_69():

    if gdaltest.pg_ds is None:
        return 'skip'

    gdaltest.pg_ds = None
    gdaltest.pg_ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
    if has_run_load_tables(gdaltest.pg_ds):
        gdaltest.post_reason('fail')
        return 'fail'
    gdaltest.pg_ds.GetLayerByName('tpoly')
    if has_run_load_tables(gdaltest.pg_ds):
        gdaltest.post_reason('fail')
        return 'fail'
    sql_lyr = gdaltest.pg_ds.ExecuteSQL('SELECT * FROM tpoly')
    if has_run_load_tables(gdaltest.pg_ds):
        gdaltest.post_reason('fail')
        return 'fail'
    feat = sql_lyr.GetNextFeature()
    if has_run_load_tables(gdaltest.pg_ds):
        gdaltest.post_reason('fail')
        return 'fail'
    del feat
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

    gdaltest.pg_ds.GetLayer(0)
    if not has_run_load_tables(gdaltest.pg_ds):
        gdaltest.post_reason('fail')
        return 'fail'

    # Test that we can find a layer with non lowercase
    gdaltest.pg_ds = None
    gdaltest.pg_ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
    if gdaltest.pg_ds.GetLayerByName('TPOLY') is None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test historical non-differed creation of tables (#5547)

def ogr_pg_70():

    if gdaltest.pg_ds is None:
        return 'skip'

    gdal.SetConfigOption('OGR_PG_DEFERRED_CREATION', 'NO')
    lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_70')
    gdal.SetConfigOption('OGR_PG_DEFERRED_CREATION', None)

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
    lyr2 = ds.GetLayerByName('ogr_pg_70')
    if lyr2 is None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
    lyr2 = ds.GetLayerByName('ogr_pg_70')
    if lyr2.GetLayerDefn().GetFieldCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    gfld_defn = ogr.GeomFieldDefn('geom', ogr.wkbPoint)
    lyr.CreateGeomField(gfld_defn)

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
    lyr2 = ds.GetLayerByName('ogr_pg_70')
    if lyr2.GetLayerDefn().GetGeomFieldCount() != 2:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    if  gdaltest.pg_has_postgis and gdaltest.pg_ds.GetLayerByName('geography_columns') is not None:
        print('Trying geography')

        gdaltest.pg_ds.ExecuteSQL('DELLAYER:ogr_pg_70')

        gdal.SetConfigOption('OGR_PG_DEFERRED_CREATION', 'NO')
        lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_70', options = [ 'GEOM_TYPE=geography', 'GEOMETRY_NAME=my_geog' ] )
        gdal.SetConfigOption('OGR_PG_DEFERRED_CREATION', None)

        ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
        lyr2 = ds.GetLayerByName('ogr_pg_70')
        if lyr2.GetLayerDefn().GetGeomFieldCount() != 1:
            gdaltest.post_reason('fail')
            return 'fail'
        geography_columns_lyr = ds.ExecuteSQL("SELECT * FROM geography_columns WHERE f_table_name = 'ogr_pg_70' AND f_geography_column = 'my_geog'")
        if geography_columns_lyr.GetFeatureCount() != 1:
            gdaltest.post_reason('fail')
            return 'fail'
        ds.ReleaseResultSet(geography_columns_lyr)
        ds = None

    return 'success'

###############################################################################
# Test interoperability of WKT/WKB with PostGIS.

def ogr_pg_71():

    if gdaltest.pg_ds is None:
        return 'skip'

    if not gdaltest.pg_has_postgis:
        return 'skip'

    curve_lyr = gdaltest.pg_ds.CreateLayer('test_curve')
    curve_lyr2 = gdaltest.pg_ds.CreateLayer('test_curve_3d', geom_type = ogr.wkbUnknown | ogr.wkb25DBit)
    # FIXME: the ResetReading() should not be necessary
    curve_lyr.ResetReading()
    curve_lyr2.ResetReading()

    for wkt in [ 'CIRCULARSTRING EMPTY',
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
        if not gdaltest.pg_has_postgis_2 and wkt.find('MULTICURVE') >= 0 and wkt.find('CIRCULARSTRING') >= 0:
            continue

        postgis_in_wkt = wkt
        while True:
            z_pos = postgis_in_wkt.find('Z ')
            # PostGIS 1.X doesn't like Z in WKT
            if not gdaltest.pg_has_postgis_2 and z_pos >= 0:
                postgis_in_wkt = postgis_in_wkt[0:z_pos] + postgis_in_wkt[z_pos+2:]
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
        if not gdaltest.pg_has_postgis_2 and wkt.find('EMPTY') >= 0:
            expected_wkt = 'GEOMETRYCOLLECTION EMPTY'
        if out_wkt != expected_wkt:
            gdaltest.post_reason('fail')
            print(expected_wkt)
            print(out_wkt)
            return 'fail'

        # Test parsing PostGIS WKT
        if gdaltest.pg_has_postgis_2:
            fct = 'ST_AsText'
        else:
            fct = 'AsEWKT'

        lyr = gdaltest.pg_ds.ExecuteSQL("SELECT %s(ST_GeomFromText('%s'))" % (fct,postgis_in_wkt))
        f = lyr.GetNextFeature()
        g = f.GetGeometryRef()
        out_wkt = g.ExportToWkt()
        g = None
        f = None
        gdaltest.pg_ds.ReleaseResultSet(lyr)

        expected_wkt = wkt
        if not gdaltest.pg_has_postgis_2 and wkt.find('EMPTY') >= 0:
            expected_wkt = 'GEOMETRYCOLLECTION EMPTY'
        if out_wkt != expected_wkt:
            gdaltest.post_reason('fail')
            print(expected_wkt)
            print(out_wkt)
            return 'fail'

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
        if ret != 0:
            gdaltest.post_reason('fail')
            print(wkt)
            return 'fail'
        fid = f.GetFID()

        # AsEWKT() in PostGIS 1.X does not like CIRCULARSTRING EMPTY
        if not gdaltest.pg_has_postgis_2 and wkt.find('CIRCULARSTRING') >= 0 and wkt.find('EMPTY') >= 0:
            continue

        lyr = gdaltest.pg_ds.ExecuteSQL("SELECT %s(wkb_geometry) FROM %s WHERE ogc_fid = %d" % (fct, active_lyr.GetName(), fid))
        f = lyr.GetNextFeature()
        g = f.GetGeometryRef()
        out_wkt = g.ExportToWkt()
        gdaltest.pg_ds.ReleaseResultSet(lyr)
        g = None
        f = None

        if out_wkt != wkt:
            gdaltest.post_reason('fail')
            print(wkt)
            print(out_wkt)
            return 'fail'

    return 'success'

###############################################################################
# Test 64 bit FID

def ogr_pg_72():

    if gdaltest.pg_ds is None:
        return 'skip'

    # Regular layer with 32 bit IDs
    lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_72')
    if lyr.GetMetadataItem(ogr.OLMD_FID64) is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr.CreateField(ogr.FieldDefn('foo'))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(123456789012345)
    f.SetField(0, 'bar')
    if lyr.CreateFeature(f) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetFeature(123456789012345)
    if f is None:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_72', options = ['FID64=YES', 'OVERWRITE=YES'])
    if lyr.GetMetadataItem(ogr.OLMD_FID64) is None:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr.CreateField(ogr.FieldDefn('foo'))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFID(123456789012345)
    f.SetField(0, 'bar')
    if lyr.CreateFeature(f) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.SetFeature(f) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    gdaltest.pg_ds = None
    # Test with binary protocol
    #gdaltest.pg_ds = ogr.Open( 'PGB:' + gdaltest.pg_connection_string, update = 1 )
    #lyr = gdaltest.pg_ds.GetLayerByName('ogr_pg_72')
    #if lyr.GetMetadataItem(ogr.OLMD_FID64) is None:
    #    gdaltest.post_reason('fail')
    #    return 'fail'
    #f = lyr.GetNextFeature()
    #if f.GetFID() != 123456789012345:
    #    gdaltest.post_reason('fail')
    #    f.DumpReadable()
    #    return 'fail'
    #gdaltest.pg_ds = None
    # Test with normal protocol
    gdaltest.pg_ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string, update = 1 )
    lyr = gdaltest.pg_ds.GetLayerByName('ogr_pg_72')
    if lyr.GetMetadataItem(ogr.OLMD_FID64) is None:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    if f.GetFID() != 123456789012345:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    lyr.ResetReading() # to close implicit transaction

    return 'success'

###############################################################################
# Test not nullable fields

def ogr_pg_73():

    if gdaltest.pg_ds is None:
        return 'skip'

    if not gdaltest.pg_has_postgis:
        return 'skip'

    gdal.SetConfigOption('PG_USE_COPY', 'NO')

    lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_73', geom_type = ogr.wkbNone)
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

    gdal.SetConfigOption('PG_USE_COPY', gdaltest.pg_use_copy)

    lyr.ResetReading() # force above feature to be committed

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update = 1)
    lyr = ds.GetLayerByName('ogr_pg_73')
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_not_nullable')).IsNullable() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_nullable')).IsNullable() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetGeomFieldDefn(lyr.GetLayerDefn().GetGeomFieldIndex('geomfield_not_nullable')).IsNullable() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetGeomFieldDefn(lyr.GetLayerDefn().GetGeomFieldIndex('geomfield_nullable')).IsNullable() != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    # Turn not null into nullable
    src_fd = lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_not_nullable'))
    fd = ogr.FieldDefn('now_nullable', src_fd.GetType())
    fd.SetNullable(1)
    lyr.AlterFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_not_nullable'), fd, ogr.ALTER_ALL_FLAG)
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('now_nullable')).IsNullable() != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    # Turn nullable into not null, but remove NULL values first
    ds.ExecuteSQL("UPDATE ogr_pg_73 SET field_nullable = '' WHERE field_nullable IS NULL")
    src_fd = lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_nullable'))
    fd = ogr.FieldDefn('now_nullable', src_fd.GetType())
    fd.SetName('now_not_nullable')
    fd.SetNullable(0)
    lyr.AlterFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_nullable'), fd, ogr.ALTER_ALL_FLAG)
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('now_not_nullable')).IsNullable() != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    sql_lyr = ds.ExecuteSQL('SELECT * FROM ogr_pg_73')
    if sql_lyr.GetLayerDefn().GetFieldDefn(sql_lyr.GetLayerDefn().GetFieldIndex('now_not_nullable')).IsNullable() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if sql_lyr.GetLayerDefn().GetFieldDefn(sql_lyr.GetLayerDefn().GetFieldIndex('now_nullable')).IsNullable() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    if sql_lyr.GetLayerDefn().GetGeomFieldDefn(sql_lyr.GetLayerDefn().GetGeomFieldIndex('geomfield_not_nullable')).IsNullable() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if sql_lyr.GetLayerDefn().GetGeomFieldDefn(sql_lyr.GetLayerDefn().GetGeomFieldIndex('geomfield_nullable')).IsNullable() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)

    return 'success'

###############################################################################
# Test default values

def ogr_pg_74():

    if gdaltest.pg_ds is None:
        return 'skip'

    if not gdaltest.pg_has_postgis:
        return 'skip'

    lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_74', geom_type = ogr.wkbNone)

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

    field_defn = ogr.FieldDefn( 'field_datetime', ogr.OFTDateTime )
    field_defn.SetDefault("CURRENT_TIMESTAMP")
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn( 'field_datetime2', ogr.OFTDateTime )
    field_defn.SetDefault("'2015/06/30 12:34:56'")
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn( 'field_datetime3', ogr.OFTDateTime )
    field_defn.SetDefault("'2015/06/30 12:34:56.123'")
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn( 'field_date', ogr.OFTDate )
    field_defn.SetDefault("CURRENT_DATE")
    lyr.CreateField(field_defn)

    field_defn = ogr.FieldDefn( 'field_time', ogr.OFTTime )
    field_defn.SetDefault("CURRENT_TIME")
    lyr.CreateField(field_defn)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('field_string', '')
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

    lyr.ResetReading() # force above feature to be committed

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update = 1)
    ds.ExecuteSQL( 'set timezone to "UTC"' )
    lyr = ds.GetLayerByName('ogr_pg_74')
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
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_datetime')).GetDefault() != 'CURRENT_TIMESTAMP':
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_datetime2')).GetDefault() != "'2015/06/30 12:34:56'":
        gdaltest.post_reason('fail')
        print(lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_datetime2')).GetDefault())
        return 'fail'
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_datetime3')).GetDefault() != "'2015/06/30 12:34:56.123'":
        gdaltest.post_reason('fail')
        print(lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_datetime3')).GetDefault())
        return 'fail'
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_date')).GetDefault() != "CURRENT_DATE":
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_time')).GetDefault() != "CURRENT_TIME":
        gdaltest.post_reason('fail')
        return 'fail'

    f = lyr.GetNextFeature()
    if f.GetField('field_string') != '':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    f = lyr.GetNextFeature()
    if f.GetField('field_string') != 'a\'b' or f.GetField('field_int') != 123 or \
       f.GetField('field_real') != 1.23 or \
       f.IsFieldSet('field_nodefault') or not f.IsFieldSet('field_datetime')  or \
       f.GetField('field_datetime2') != '2015/06/30 12:34:56+00' or \
       f.GetField('field_datetime3') != '2015/06/30 12:34:56.123+00' or \
       not f.IsFieldSet('field_date') or not f.IsFieldSet('field_time'):
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    f = lyr.GetNextFeature()
    if f.GetField('field_string') != 'b':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    lyr.ResetReading() # to close implicit transaction

    # Change DEFAULT value
    src_fd = lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_string'))
    fd = ogr.FieldDefn('field_string', src_fd.GetType())
    fd.SetDefault("'c'")
    lyr.AlterFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_string'), fd, ogr.ALTER_DEFAULT_FLAG)
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_string')).GetDefault() != "'c'":
        gdaltest.post_reason('fail')
        print(lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_string')).GetDefault())
        return 'fail'

    # Drop DEFAULT value
    src_fd = lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_int'))
    fd = ogr.FieldDefn('field_int', src_fd.GetType())
    fd.SetDefault(None)
    lyr.AlterFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_int'), fd, ogr.ALTER_DEFAULT_FLAG)
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_int')).GetDefault() is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    ds = None
    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update = 1)
    ds.ExecuteSQL( 'set timezone to "UTC"' )
    lyr = ds.GetLayerByName('ogr_pg_74')
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_string')).GetDefault() != "'c'":
        gdaltest.post_reason('fail')
        print(lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_string')).GetDefault())
        return 'fail'
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('field_int')).GetDefault() is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test creating a field with the fid name

def ogr_pg_75():

    if gdaltest.pg_ds is None:
        return 'skip'

    if not gdaltest.pg_has_postgis:
        return 'skip'

    lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_75', geom_type = ogr.wkbNone, options = ['FID=myfid'])

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

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('str', 'first string')
    feat.SetField('myfid', 12)
    feat.SetField('str2', 'second string')
    ret = lyr.CreateFeature(feat)
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if feat.GetFID() != 12:
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
    lyr.ResetReading() # to close implicit transaction

    return 'success'

###############################################################################
# Test transactions RFC 54

def ogr_pg_76_get_transaction_state(ds):
    return ( ds.GetMetadataItem("osDebugLastTransactionCommand", "_DEBUG_"),
             int(ds.GetMetadataItem("nSoftTransactionLevel", "_DEBUG_")),
             int(ds.GetMetadataItem("bSavePointActive", "_DEBUG_")),
             int(ds.GetMetadataItem("bUserTransactionActive", "_DEBUG_")) )

def ogr_pg_76():

    if gdaltest.pg_ds is None:
        return 'skip'

    if gdaltest.pg_ds.TestCapability(ogr.ODsCTransactions) != 1:
        gdaltest.post_reason('fail')
        return 'fail'

    level = int(gdaltest.pg_ds.GetMetadataItem("nSoftTransactionLevel", "_DEBUG_"))
    if level != 0:
        gdaltest.post_reason('fail')
        print(level)
        return 'fail'

    if gdaltest.pg_has_postgis_2:
        gdaltest.pg_ds.StartTransaction()
        lyr = gdaltest.pg_ds.CreateLayer('will_not_be_created', options = ['OVERWRITE=YES'])
        lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))

        sql_lyr = gdaltest.pg_ds.ExecuteSQL("SELECT COUNT(*) FROM geometry_columns WHERE f_table_name = 'will_not_be_created'")
        f = sql_lyr.GetNextFeature()
        res = f.GetField(0)
        gdaltest.pg_ds.ReleaseResultSet(sql_lyr)
        if res != 1:
            gdaltest.post_reason('fail')
            return 'fail'

        gdaltest.pg_ds.RollbackTransaction()

        # Rollback doesn't rollback the insertion in geometry_columns if done through the AddGeometryColumn()
        sql_lyr = gdaltest.pg_ds.ExecuteSQL("SELECT COUNT(*) FROM geometry_columns WHERE f_table_name = 'will_not_be_created'")
        f = sql_lyr.GetNextFeature()
        res = f.GetField(0)
        gdaltest.pg_ds.ReleaseResultSet(sql_lyr)
        if res != 0:
            gdaltest.post_reason('fail')
            print(res)
            return 'fail'

    gdal.SetConfigOption('OGR_PG_CURSOR_PAGE', '1')
    lyr1 = gdaltest.pg_ds.CreateLayer('ogr_pg_76_lyr1', geom_type = ogr.wkbNone, options = ['OVERWRITE=YES'])
    lyr2 = gdaltest.pg_ds.CreateLayer('ogr_pg_76_lyr2', geom_type = ogr.wkbNone, options = ['OVERWRITE=YES'])
    gdal.SetConfigOption('OGR_PG_CURSOR_PAGE', None)
    lyr1.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    #lyr2.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    lyr1.CreateFeature(ogr.Feature(lyr1.GetLayerDefn()))
    lyr2.CreateFeature(ogr.Feature(lyr2.GetLayerDefn()))
    lyr1.CreateFeature(ogr.Feature(lyr1.GetLayerDefn()))
    lyr1.CreateFeature(ogr.Feature(lyr1.GetLayerDefn()))
    lyr2.CreateFeature(ogr.Feature(lyr2.GetLayerDefn()))

    level = int(gdaltest.pg_ds.GetMetadataItem("nSoftTransactionLevel", "_DEBUG_"))
    if level != 0:
        gdaltest.post_reason('fail')
        print(level)
        return 'fail'

    ret = ogr_pg_76_scenario1(lyr1, lyr2)
    if ret != 'success':
        return ret
    ret = ogr_pg_76_scenario2(lyr1, lyr2)
    if ret != 'success':
        return ret
    ret = ogr_pg_76_scenario3(lyr1, lyr2)
    if ret != 'success':
        return ret
    ret = ogr_pg_76_scenario4(lyr1, lyr2)
    if ret != 'success':
        return ret

    return ret

# Scenario 1 : a CreateFeature done in the middle of GetNextFeature()
def ogr_pg_76_scenario1(lyr1, lyr2):

    (_, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    if (level, savepoint, usertransac) != (0, 0, 0):
        gdaltest.post_reason('fail')
        print(level, savepoint, usertransac)
        return 'fail'

    f = lyr1.GetNextFeature()
    if f is None or f.GetFID() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    if (lastcmd, level, savepoint, usertransac) != ('BEGIN', 1, 0, 0):
        gdaltest.post_reason('fail')
        print(lastcmd, level, savepoint, usertransac)
        return 'fail'

    lyr1.SetAttributeFilter("foo is NULL")
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    if (lastcmd, level, savepoint, usertransac) != ('COMMIT', 0, 0, 0):
        gdaltest.post_reason('fail')
        print(lastcmd, level, savepoint, usertransac)
        return 'fail'

    f = lyr1.GetNextFeature()
    if f is None or f.GetFID() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    if (lastcmd, level, savepoint, usertransac) != ('BEGIN', 1, 0, 0):
        gdaltest.post_reason('fail')
        print(lastcmd, level, savepoint, usertransac)
        return 'fail'

    f = lyr2.GetNextFeature()
    if f is None or f.GetFID() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    if (lastcmd, level, savepoint, usertransac) != ('', 2, 0, 0):
        gdaltest.post_reason('fail')
        print(lastcmd, level, savepoint, usertransac)
        return 'fail'

    f = lyr1.GetNextFeature()
    if f is None or f.GetFID() != 2:
        gdaltest.post_reason('fail')
        return 'fail'

    # Check that GetFeature() doesn't reset the cursor
    f = lyr1.GetFeature(f.GetFID())
    if f is None or f.GetFID() != 2:
        gdaltest.post_reason('fail')
        return 'fail'

    f = lyr1.GetNextFeature()
    if f is None or f.GetFID() != 3:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr2.GetNextFeature()
    if f is None or f.GetFID() != 2:
        gdaltest.post_reason('fail')
        return 'fail'
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    if (lastcmd, level, savepoint, usertransac) != ('', 2, 0, 0):
        gdaltest.post_reason('fail')
        print(lastcmd, level, savepoint, usertransac)
        return 'fail'

    lyr1.CreateFeature(ogr.Feature(lyr1.GetLayerDefn()))

    lyr1.ResetReading()
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    if (lastcmd, level, savepoint, usertransac) != ('', 1, 0, 0):
        gdaltest.post_reason('fail')
        print(lastcmd, level, savepoint, usertransac)
        return 'fail'
    lyr2.ResetReading()
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    if (lastcmd, level, savepoint, usertransac) != ('COMMIT', 0, 0, 0):
        gdaltest.post_reason('fail')
        print(lastcmd, level, savepoint, usertransac)
        return 'fail'
    if lyr1.GetFeatureCount() != 4:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'


# Scenario 2 : a CreateFeature done in the middle of GetNextFeature(), themselves between a user transaction
def ogr_pg_76_scenario2(lyr1, lyr2):

    if gdaltest.pg_ds.StartTransaction() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    if (lastcmd, level, savepoint, usertransac) != ('BEGIN', 1, 0, 1):
        gdaltest.post_reason('fail')
        print(lastcmd, level, savepoint, usertransac)
        return 'fail'

    # Try to re-enter a transaction
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    ret = gdaltest.pg_ds.StartTransaction()
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '' or ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    if (lastcmd, level, savepoint, usertransac) != ('', 1, 0, 1):
        gdaltest.post_reason('fail')
        print(lastcmd, level, savepoint, usertransac)
        return 'fail'

    f = lyr1.GetNextFeature()
    if f is None or f.GetFID() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    if (lastcmd, level, savepoint, usertransac) != ('', 2, 0, 1):
        gdaltest.post_reason('fail')
        print(lastcmd, level, savepoint, usertransac)
        return 'fail'

    f = lyr2.GetNextFeature()
    if f is None or f.GetFID() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    if (lastcmd, level, savepoint, usertransac) != ('', 3, 0, 1):
        gdaltest.post_reason('fail')
        print(lastcmd, level, savepoint, usertransac)
        return 'fail'

    f = lyr1.GetNextFeature()
    if f is None or f.GetFID() != 2:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr1.GetNextFeature()
    if f is None or f.GetFID() != 3:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr2.GetNextFeature()
    if f is None or f.GetFID() != 2:
        gdaltest.post_reason('fail')
        return 'fail'
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    if (lastcmd, level, savepoint, usertransac) != ('', 3, 0, 1):
        gdaltest.post_reason('fail')
        print(lastcmd, level, savepoint, usertransac)
        return 'fail'

    lyr1.CreateFeature(ogr.Feature(lyr1.GetLayerDefn()))

    lyr1.ResetReading()
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    if (lastcmd, level, savepoint, usertransac) != ('', 2, 0, 1):
        gdaltest.post_reason('fail')
        print(lastcmd, level, savepoint, usertransac)
        return 'fail'

    lyr2.ResetReading()
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    if (lastcmd, level, savepoint, usertransac) != ('', 1, 0, 1):
        gdaltest.post_reason('fail')
        print(lastcmd, level, savepoint, usertransac)
        return 'fail'


    if gdaltest.pg_ds.CommitTransaction() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    if (lastcmd, level, savepoint, usertransac) != ('COMMIT', 0, 0, 0):
        gdaltest.post_reason('fail')
        print(lastcmd, level, savepoint, usertransac)
        return 'fail'

    if gdaltest.pg_ds.StartTransaction() != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    if gdaltest.pg_ds.RollbackTransaction() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    if (lastcmd, level, savepoint, usertransac) != ('ROLLBACK', 0, 0, 0):
        gdaltest.post_reason('fail')
        print(lastcmd, level, savepoint, usertransac)
        return 'fail'

    # Try to re-commit a transaction
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    ret = gdaltest.pg_ds.CommitTransaction()
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '' or ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    if (lastcmd, level, savepoint, usertransac) != ('', 0, 0, 0):
        gdaltest.post_reason('fail')
        print(lastcmd, level, savepoint, usertransac)
        return 'fail'

    # Try to rollback a non-transaction
    gdal.ErrorReset()
    gdal.PushErrorHandler()
    ret = gdaltest.pg_ds.RollbackTransaction()
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '' or ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    if (lastcmd, level, savepoint, usertransac) != ('', 0, 0, 0):
        gdaltest.post_reason('fail')
        print(lastcmd, level, savepoint, usertransac)
        return 'fail'

    return 'success'

# Scenario 3 : StartTransaction(), GetNextFeature(), CommitTransaction(), GetNextFeature()
def ogr_pg_76_scenario3(lyr1, lyr2):

    if gdaltest.pg_ds.StartTransaction() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    if (lastcmd, level, savepoint, usertransac) != ('BEGIN', 1, 0, 1):
        gdaltest.post_reason('fail')
        print(lastcmd, level, savepoint, usertransac)
        return 'fail'

    f = lyr1.GetNextFeature()
    if f is None or f.GetFID() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    if (lastcmd, level, savepoint, usertransac) != ('', 2, 0, 1):
        gdaltest.post_reason('fail')
        print(lastcmd, level, savepoint, usertransac)
        return 'fail'

    if gdaltest.pg_ds.CommitTransaction() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    if (lastcmd, level, savepoint, usertransac) != ('COMMIT', 0, 0, 0):
        gdaltest.post_reason('fail')
        print(lastcmd, level, savepoint, usertransac)
        return 'fail'

    gdal.ErrorReset()
    gdal.PushErrorHandler()
    f = lyr1.GetNextFeature()
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '' or f is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Must re-issue an explicit ResetReading()
    lyr1.ResetReading()

    f = lyr1.GetNextFeature()
    if f is None or f.GetFID() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    if (lastcmd, level, savepoint, usertransac) != ('BEGIN', 1, 0, 0):
        gdaltest.post_reason('fail')
        print(lastcmd, level, savepoint, usertransac)
        return 'fail'


    lyr1.ResetReading()
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    if (lastcmd, level, savepoint, usertransac) != ('COMMIT', 0, 0, 0):
        gdaltest.post_reason('fail')
        print(lastcmd, level, savepoint, usertransac)
        return 'fail'

    lyr2.ResetReading()

    return 'success'

# Scenario 4 : GetNextFeature(), StartTransaction(), CreateFeature(), CommitTransaction(), GetNextFeature(), ResetReading()
def ogr_pg_76_scenario4(lyr1, lyr2):

    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    if (lastcmd, level, savepoint, usertransac) != ('', 0, 0, 0):
        gdaltest.post_reason('fail')
        print(lastcmd, level, savepoint, usertransac)
        return 'fail'

    f = lyr1.GetNextFeature()
    if f is None or f.GetFID() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    if (lastcmd, level, savepoint, usertransac) != ('BEGIN', 1, 0, 0):
        gdaltest.post_reason('fail')
        print(lastcmd, level, savepoint, usertransac)
        return 'fail'

    if gdaltest.pg_ds.StartTransaction() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    if (lastcmd, level, savepoint, usertransac) != ('SAVEPOINT ogr_savepoint', 2, 1, 1):
        gdaltest.post_reason('fail')
        print(lastcmd, level, savepoint, usertransac)
        return 'fail'

    lyr1.CreateFeature(ogr.Feature(lyr1.GetLayerDefn()))

    f = lyr1.GetNextFeature()
    if f is None or f.GetFID() != 2:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr2.GetNextFeature()
    if f is None or f.GetFID() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    if (lastcmd, level, savepoint, usertransac) != ('', 3, 1, 1):
        gdaltest.post_reason('fail')
        print(lastcmd, level, savepoint, usertransac)
        return 'fail'

    # Check that it doesn't commit the transaction
    lyr1.SetAttributeFilter("foo is NULL")
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    if (lastcmd, level, savepoint, usertransac) != ('', 2, 1, 1):
        gdaltest.post_reason('fail')
        print(lastcmd, level, savepoint, usertransac)
        return 'fail'

    f = lyr1.GetNextFeature()
    if f is None or f.GetFID() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    if (lastcmd, level, savepoint, usertransac) != ('', 3, 1, 1):
        gdaltest.post_reason('fail')
        print(lastcmd, level, savepoint, usertransac)
        return 'fail'

    f = lyr2.GetNextFeature()
    if f is None or f.GetFID() != 2:
        gdaltest.post_reason('fail')
        return 'fail'

    if gdaltest.pg_ds.CommitTransaction() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    if (lastcmd, level, savepoint, usertransac) != ('RELEASE SAVEPOINT ogr_savepoint', 2, 0, 0):
        gdaltest.post_reason('fail')
        print(lastcmd, level, savepoint, usertransac)
        return 'fail'

    lyr2.ResetReading()

    if gdaltest.pg_ds.StartTransaction() != 0:
        gdaltest.post_reason('fail')
        return 'fail'

    if gdaltest.pg_ds.RollbackTransaction() != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    if (lastcmd, level, savepoint, usertransac) != ('ROLLBACK TO SAVEPOINT ogr_savepoint', 1, 0, 0):
        gdaltest.post_reason('fail')
        print(lastcmd, level, savepoint, usertransac)
        return 'fail'

    f = lyr1.GetNextFeature()
    if f is None or f.GetFID() != 2:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr1.ResetReading()
    (lastcmd, level, savepoint, usertransac) = ogr_pg_76_get_transaction_state(gdaltest.pg_ds)
    if (lastcmd, level, savepoint, usertransac) != ('COMMIT', 0, 0, 0):
        gdaltest.post_reason('fail')
        print(lastcmd, level, savepoint, usertransac)
        return 'fail'

    return 'success'

###############################################################################
# Test ogr2ogr can insert multiple layers at once

def ogr_pg_77():
    import test_cli_utilities
    if test_cli_utilities.get_ogr2ogr_path() is None:
        return 'skip'

    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:ogr_pg_77_1' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:ogr_pg_77_2' )

    try:
        shutil.rmtree('tmp/ogr_pg_77')
    except:
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

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string, update = 1)
    lyr = ds.GetLayerByName('ogr_pg_77_1')
    feat = lyr.GetNextFeature()
    if feat.GetField(0) != '1':
        return 'fail'
    feat.SetField(0, 10)
    lyr.SetFeature(feat)
    lyr = ds.GetLayerByName('ogr_pg_77_2')
    feat = lyr.GetNextFeature()
    if feat.GetField(0) != '2':
        return 'fail'
    ds = None

    # Test fix for #6018
    gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f PostgreSQL "' + 'PG:' + gdaltest.pg_connection_string + '" tmp/ogr_pg_77 -overwrite')

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
    lyr = ds.GetLayerByName('ogr_pg_77_1')
    feat = lyr.GetNextFeature()
    if feat.GetField(0) != '1':
        return 'fail'
    ds = None

    try:
        shutil.rmtree('tmp/ogr_pg_77')
    except:
        pass

    return 'success'

###############################################################################
# Test manually added geometry constraints

def ogr_pg_78():

    if gdaltest.pg_ds is None or not gdaltest.pg_has_postgis_2:
        return 'skip'

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
    gdaltest.pg_ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string, update = 1 )
    lc = gdaltest.pg_ds.GetLayerCount() # force discovery of all tables
    ogr_pg_78_found = False
    ogr_pg_78_2_found = False
    for i in range(lc):
        lyr = gdaltest.pg_ds.GetLayer(i)
        if lyr.GetName() == 'ogr_pg_78':
            ogr_pg_78_found = True
            if lyr.GetGeomType() != ogr.wkbPoint25D:
                gdaltest.post_reason('fail')
                return 'fail'
            if lyr.GetSpatialRef().ExportToWkt().find('4326') < 0:
                gdaltest.post_reason('fail')
                return 'fail'
        if lyr.GetName() == 'ogr_pg_78_2':
            ogr_pg_78_2_found = True
            if lyr.GetGeomType() != ogr.wkbPoint25D:
                gdaltest.post_reason('fail')
                return 'fail'
            if lyr.GetSpatialRef().ExportToWkt().find('4326') < 0:
                gdaltest.post_reason('fail')
                return 'fail'
    if not ogr_pg_78_found:
        gdaltest.post_reason('fail')
        return 'fail'
    if not ogr_pg_78_2_found:
        gdaltest.post_reason('fail')
        return 'fail'

    gdaltest.pg_ds = None
    # Test with slow method
    gdal.SetConfigOption('PG_USE_POSTGIS2_OPTIM', 'NO')
    gdaltest.pg_ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string, update = 1 )
    lc = gdaltest.pg_ds.GetLayerCount() # force discovery of all tables
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
                    gdaltest.post_reason('fail')
                    return 'fail'
            if lyr.GetSpatialRef() is None or lyr.GetSpatialRef().ExportToWkt().find('4326') < 0:
                val = gdal.GetConfigOption('TRAVIS', None)
                if val is not None:
                    print('Fails on Travis. GetSpatialRef() = %s' % str(lyr.GetSpatialRef()))
                else:
                    gdaltest.post_reason('fail')
                    return 'fail'
        if lyr.GetName() == 'ogr_pg_78_2':
            ogr_pg_78_2_found = True
            # No logic in geography_columns to get type/coordim/srid from constraints
            #if lyr.GetGeomType() != ogr.wkbPoint25D:
            #    gdaltest.post_reason('fail')
            #    return 'fail'
            #if lyr.GetSpatialRef().ExportToWkt().find('4326') < 0:
            #    gdaltest.post_reason('fail')
            #    return 'fail'
    if not ogr_pg_78_found:
        gdaltest.post_reason('fail')
        return 'fail'
    if not ogr_pg_78_2_found:
        gdaltest.post_reason('fail')
        return 'fail'


    return 'success'

###############################################################################
# Test PRELUDE_STATEMENTS and CLOSING_STATEMENTS open options

def ogr_pg_79():

    if gdaltest.pg_ds is None:
        return 'skip'

    # PRELUDE_STATEMENTS starting with BEGIN (use case: pg_bouncer in transaction pooling)
    ds = gdal.OpenEx( 'PG:' + gdaltest.pg_connection_string, \
        gdal.OF_VECTOR | gdal.OF_UPDATE, \
        open_options = ['PRELUDE_STATEMENTS=BEGIN; SET LOCAL statement_timeout TO "1h";',
                        'CLOSING_STATEMENTS=COMMIT;'] )
    sql_lyr = ds.ExecuteSQL('SHOW statement_timeout')
    f = sql_lyr.GetNextFeature()
    if f.GetField(0) != '1h':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)
    ret = ds.StartTransaction()
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ret = ds.CommitTransaction()
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.ErrorReset()
    ds = None
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'

    # random PRELUDE_STATEMENTS
    ds = gdal.OpenEx( 'PG:' + gdaltest.pg_connection_string, \
        gdal.OF_VECTOR | gdal.OF_UPDATE, \
        open_options = ['PRELUDE_STATEMENTS=SET statement_timeout TO "1h"' ] )
    sql_lyr = ds.ExecuteSQL('SHOW statement_timeout')
    f = sql_lyr.GetNextFeature()
    if f.GetField(0) != '1h':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    ds.ReleaseResultSet(sql_lyr)
    ret = ds.StartTransaction()
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ret = ds.CommitTransaction()
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    gdal.ErrorReset()
    ds = None
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('fail')
        return 'fail'

    # Test wrong PRELUDE_STATEMENTS
    with gdaltest.error_handler():
        ds = gdal.OpenEx( 'PG:' + gdaltest.pg_connection_string, \
            gdal.OF_VECTOR | gdal.OF_UPDATE, \
            open_options = ['PRELUDE_STATEMENTS=BEGIN;error SET LOCAL statement_timeout TO "1h";',
                            'CLOSING_STATEMENTS=COMMIT;'] )
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test wrong CLOSING_STATEMENTS
    ds = gdal.OpenEx( 'PG:' + gdaltest.pg_connection_string, \
        gdal.OF_VECTOR | gdal.OF_UPDATE, \
        open_options = ['PRELUDE_STATEMENTS=BEGIN; SET LOCAL statement_timeout TO "1h";',
                        'CLOSING_STATEMENTS=COMMIT;error'] )
    gdal.ErrorReset()
    with gdaltest.error_handler():
        ds = None
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test retrieving an error from ExecuteSQL() (#6194)

def ogr_pg_80():

    if gdaltest.pg_ds is None or gdaltest.ogr_pg_second_run:
        return 'skip'

    gdal.ErrorReset()
    with gdaltest.error_handler():
        sql_lyr = gdaltest.pg_ds.ExecuteSQL('SELECT FROM')
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('fail')
        return 'fail'
    if sql_lyr is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    return 'success'

###############################################################################
# Test that ogr2ogr -skip properly rollbacks transactions (#6328)

def ogr_pg_81():

    if gdaltest.pg_ds is None or gdaltest.ogr_pg_second_run:
        return 'skip'

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

    gdal.VectorTranslate('PG:' + gdaltest.pg_connection_string, '/vsimem/ogr_pg_81', accessMode = 'append')

    gdal.FileFromMemBuffer('/vsimem/ogr_pg_81/ogr_pg_81_2.csv',
"""id,foo
2,2""")

    with gdaltest.error_handler():
        gdal.VectorTranslate('PG:' + gdaltest.pg_connection_string, '/vsimem/ogr_pg_81', accessMode = 'append', skipFailures = True)

    gdal.Unlink('/vsimem/ogr_pg_81/ogr_pg_81_1.csv')
    gdal.Unlink('/vsimem/ogr_pg_81/ogr_pg_81_2.csv')
    gdal.Unlink('/vsimem/ogr_pg_81')

    lyr = gdaltest.pg_ds.GetLayer('ogr_pg_81_2')
    f = lyr.GetNextFeature()
    f = lyr.GetNextFeature()
    if f['id'] != '2':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'
    lyr.ResetReading() # flushes implicit transaction

    return 'success'

###############################################################################
# Test that GEOMETRY_NAME works even when the geometry column creation is
# done through CreateGeomField (#6366)
# This is important for the ogr2ogr use case when the source geometry column
# is not-nullable, and hence the CreateGeomField() interface is used.

def ogr_pg_82():

    if gdaltest.pg_ds is None or not gdaltest.pg_has_postgis or gdaltest.ogr_pg_second_run :
        return 'skip'

    lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_82', geom_type = ogr.wkbNone, options = ['GEOMETRY_NAME=another_name'])
    lyr.CreateGeomField(ogr.GeomFieldDefn('my_geom', ogr.wkbPoint))
    if lyr.GetLayerDefn().GetGeomFieldDefn(0).GetName() != 'another_name':
        gdaltest.post_reason('fail')
        print(lyr.GetLayerDefn().GetGeomFieldDefn(0).GetName())
        return 'fail'

    return 'success'

###############################################################################
# Test ZM support

def ogr_pg_83():

    if gdaltest.pg_ds is None or not gdaltest.pg_has_postgis or gdaltest.ogr_pg_second_run :
        return 'skip'

    tests = [ [ ogr.wkbUnknown, [], 'POINT ZM (1 2 3 4)', 'POINT (1 2)' ],
              [ ogr.wkbUnknown, [ 'DIM=XYZM' ], 'POINT ZM (1 2 3 4)', 'POINT ZM (1 2 3 4)' ],
              [ ogr.wkbUnknown, [ 'DIM=XYZ' ], 'POINT ZM (1 2 3 4)', 'POINT Z (1 2 3)' ],
              [ ogr.wkbUnknown, [ 'DIM=XYM' ], 'POINT M (1 2 4)', 'POINT M (1 2 4)' ],
              [ ogr.wkbPointZM, [], 'POINT ZM (1 2 3 4)', 'POINT ZM (1 2 3 4)' ],
              [ ogr.wkbPoint25D, [], 'POINT ZM (1 2 3 4)', 'POINT Z (1 2 3)' ],
              [ ogr.wkbPointM, [], 'POINT ZM (1 2 3 4)', 'POINT M (1 2 4)' ],
              [ ogr.wkbUnknown, [ 'GEOM_TYPE=geography', 'DIM=XYM' ], 'POINT ZM (1 2 3 4)', 'POINT M (1 2 4)' ],
            ]

    for (geom_type, options, wkt, expected_wkt) in tests:
        lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_83', geom_type = geom_type, options = options + [ 'OVERWRITE=YES' ] )
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
        if got_wkt != expected_wkt:
            gdaltest.post_reason('fail')
            print(geom_type, options, wkt, expected_wkt, got_wkt)
            return 'fail'
        lyr.ResetReading() # flushes implicit transaction

        if 'GEOM_TYPE=geography' in options:
            continue
        # Cannot do AddGeometryColumn( 'GEOMETRYM', 3 ) with PostGIS 2, and doesn't accept inserting a XYM geometry
        if gdaltest.pg_has_postgis_2 and geom_type == ogr.wkbUnknown and options == [ 'DIM=XYM' ]:
            continue

        lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_83', geom_type = ogr.wkbNone, options = options + [ 'OVERWRITE=YES' ] )
        # To force table creation to happen now so that following
        # CreateGeomField() is done through a AddGeometryColumn() call
        lyr.ResetReading()
        lyr.GetNextFeature()
        lyr.CreateGeomField( ogr.GeomFieldDefn("my_geom", geom_type) )
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
        if got_wkt != expected_wkt:
            gdaltest.post_reason('fail')
            print(geom_type, options, wkt, expected_wkt, got_wkt)
            return 'fail'
        lyr.ResetReading() # flushes implicit transaction

    return 'success'

###############################################################################
# Test description

def ogr_pg_84():

    if gdaltest.pg_ds is None or gdaltest.ogr_pg_second_run :
        return 'skip'

    ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string, update = 1 )
    lyr = ds.CreateLayer('ogr_pg_84', geom_type = ogr.wkbPoint, options = [ 'OVERWRITE=YES', 'DESCRIPTION=foo' ] )
    # Test that SetMetadata() and SetMetadataItem() are without effect
    lyr.SetMetadata( {'DESCRIPTION': 'bar' } )
    lyr.SetMetadataItem( 'DESCRIPTION', 'baz' )
    if lyr.GetMetadataItem('DESCRIPTION') != 'foo':
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetMetadata_List() != [ 'DESCRIPTION=foo' ]:
        gdaltest.post_reason('fail')
        print(lyr.GetMetadata())
        return 'fail'
    ds = None

    ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string, update = 1 )
    ds.GetLayerCount() # load all layers
    lyr = ds.GetLayerByName('ogr_pg_84')
    if lyr.GetMetadataItem('DESCRIPTION') != 'foo':
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetMetadata_List() != [ 'DESCRIPTION=foo' ]:
        gdaltest.post_reason('fail')
        print(lyr.GetMetadata())
        return 'fail'
    # Set with SetMetadata()
    lyr.SetMetadata( [ 'DESCRIPTION=bar' ] )
    ds = None

    ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string, update = 1 )
    lyr = ds.GetLayerByName('ogr_pg_84') # load just this layer
    if lyr.GetMetadataItem('DESCRIPTION') != 'bar':
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetMetadataDomainList() is None:
        gdaltest.post_reason('fail')
        return 'fail'
    # Set with SetMetadataItem()
    lyr.SetMetadataItem( 'DESCRIPTION', 'baz' )
    ds = None

    ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string, update = 1 )
    lyr = ds.GetLayerByName('ogr_pg_84')
    if lyr.GetMetadataDomainList() is None:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetMetadataItem('DESCRIPTION') != 'baz':
        gdaltest.post_reason('fail')
        return 'fail'
    # Unset with SetMetadataItem()
    lyr.SetMetadataItem( 'DESCRIPTION', None )
    ds = None

    ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string )
    lyr = ds.GetLayerByName('ogr_pg_84') # load just this layer
    if lyr.GetMetadataDomainList() is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetMetadataItem('DESCRIPTION') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string )
    ds.GetLayerCount() # load all layers
    lyr = ds.GetLayerByName('ogr_pg_84') # load just this layer
    if lyr.GetMetadataItem('DESCRIPTION') is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test append of several layers in PG_USE_COPY mode (#6411)

def ogr_pg_85():

    if gdaltest.pg_ds is None or gdaltest.ogr_pg_second_run :
        return 'skip'

    gdaltest.pg_ds.CreateLayer('ogr_pg_85_1')
    lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_85_2')
    lyr.CreateField(ogr.FieldDefn('foo'))
    gdaltest.pg_ds.ReleaseResultSet(gdaltest.pg_ds.ExecuteSQL('SELECT 1')) # make sure the layers are well created

    old_val = gdal.GetConfigOption('PG_USE_COPY')
    gdal.SetConfigOption('PG_USE_COPY', 'YES')
    ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string, update = 1 )
    ds.GetLayerCount()
    ds.StartTransaction()
    lyr = ds.GetLayerByName('ogr_pg_85_1')
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    lyr = ds.GetLayerByName('ogr_pg_85_2')
    feat_defn = lyr.GetLayerDefn()
    if feat_defn.GetFieldCount() != 1:
        gdaltest.post_reason('fail')
        return 'fail'
    f = ogr.Feature(feat_defn)
    if lyr.CreateFeature(f) != 0:
        gdaltest.post_reason('fail')
        return 'fail'
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

    gdal.VectorTranslate('PG:' + gdaltest.pg_connection_string, '/vsimem/ogr_pg_85', accessMode = 'append')

    gdal.Unlink('/vsimem/ogr_pg_85/ogr_pg_85_1.csv')
    gdal.Unlink('/vsimem/ogr_pg_85/ogr_pg_85_2.csv')
    gdal.Unlink('/vsimem/ogr_pg_85')

    gdal.SetConfigOption('PG_USE_COPY',old_val)

    lyr = gdaltest.pg_ds.GetLayerByName('ogr_pg_85_2')
    if lyr.GetFeatureCount() != 2:
        gdaltest.post_reason('fail')
        print(lyr.GetFeatureCount())
        return 'fail'

    return 'success'

###############################################################################
# Test OFTBinary

def ogr_pg_86():

    if gdaltest.pg_ds is None or gdaltest.ogr_pg_second_run :
        return 'skip'

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
        gdaltest.post_reason('fail')
        gdal.SetConfigOption('PG_USE_COPY', old_val)
        return 'fail'

    gdal.SetConfigOption('PG_USE_COPY', 'NO')

    lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_86', options = ['OVERWRITE=YES'])
    lyr.CreateField(ogr.FieldDefn('test', ogr.OFTBinary))
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetFieldBinaryFromHexString('test', '3020')
    lyr.CreateFeature(f)
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    if f.GetField(0) != '3020':
        gdaltest.post_reason('fail')
        gdal.SetConfigOption('PG_USE_COPY', old_val)
        return 'fail'


    gdal.SetConfigOption('PG_USE_COPY', old_val)

    return 'success'

###############################################################################
#

def ogr_pg_table_cleanup():

    if gdaltest.pg_ds is None:
        return 'skip'

    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:tpoly' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:tpolycopy' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:test_for_tables_equal_param' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:datetest' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:testgeom' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:datatypetest' )
    #gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:datatypetest_withouttimestamptz' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:datatypetest2' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:testsrtext' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:testsrtext2' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:testsrtext3' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:testsrtext4' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:testsrtext5' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:testoverflows' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:table36_inherited2' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:table36_inherited' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:table36_base' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:table37_inherited' )
    gdaltest.pg_ds.ExecuteSQL( 'DROP TABLE table37_base CASCADE')
    gdaltest.pg_ds.ExecuteSQL( 'DROP VIEW testview')
    gdaltest.pg_ds.ExecuteSQL( "DELETE FROM geometry_columns WHERE f_table_name='testview'")
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:select' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:bigtable' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:test_geog' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:no_pk_table' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:no_geometry_table' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:ogr_pg_55' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:ogr_pg_56' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:ogr_pg_57' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:ogr_pg_58' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:ogr_pg_60' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:ogr_pg_61' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:ogr_pg_63' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:ogr_pg_65' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:ogr_pg_65_copied' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:ogr_pg_67' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:ogr_pg_68' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:ogr_pg_70' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:ogr_pg_72' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:ogr_pg_73' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:ogr_pg_74' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:ogr_pg_75' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:ogr_pg_76_lyr1' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:ogr_pg_76_lyr2' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:test_curve' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:test_curve_3d' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:ogr_pg_77_1' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:ogr_pg_77_2' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:ogr_pg_78' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:ogr_pg_78_2' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:ogr_pg_81_1' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:ogr_pg_81_2' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:ogr_pg_82' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:ogr_pg_83' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:ogr_pg_84' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:ogr_pg_85_1' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:ogr_pg_85_2' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:ogr_pg_86' )

    # Drop second 'tpoly' from schema 'AutoTest-schema' (do NOT quote names here)
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:AutoTest-schema.tpoly' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:AutoTest-schema.test41' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:AutoTest-schema.table36_base' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:AutoTest-schema.table36_inherited' )
    # Drop 'AutoTest-schema' (here, double quotes are required)
    gdaltest.pg_ds.ExecuteSQL( 'DROP SCHEMA \"AutoTest-schema\" CASCADE')
    gdal.PopErrorHandler()

    return 'success'

def ogr_pg_cleanup():

    if gdaltest.pg_ds is None:
        return 'skip'

    gdaltest.pg_ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string, update = 1 )
    ogr_pg_table_cleanup()

    gdaltest.pg_ds.Destroy()
    gdaltest.pg_ds = None

    return 'success'

# NOTE: The ogr_pg_19 intentionally executed after ogr_pg_2

gdaltest_list_internal = [
    ogr_pg_table_cleanup,
    ogr_pg_2,
    ogr_pg_19,
    ogr_pg_19_2,
    ogr_pg_3,
    ogr_pg_4,
    ogr_pg_5,
    ogr_pg_6,
    ogr_pg_7,
    ogr_pg_8,
    ogr_pg_9,
    ogr_pg_10,
    ogr_pg_11,
    ogr_pg_12,
    ogr_pg_13,
    ogr_pg_14,
    ogr_pg_15,
    ogr_pg_16,
    ogr_pg_17,
    ogr_pg_18,
    ogr_pg_20,
    ogr_pg_21,
    ogr_pg_21_subgeoms,
    ogr_pg_21_3d_geometries,
    ogr_pg_22,
    ogr_pg_23,
    ogr_pg_24,
    ogr_pg_25,
    #ogr_pg_26,
    #ogr_pg_27,
    ogr_pg_28,
    ogr_pg_29,
    ogr_pg_30,
    ogr_pg_29,
    ogr_pg_31,
    ogr_pg_32,
    ogr_pg_33,
    ogr_pg_34,
    ogr_pg_35,
    ogr_pg_36,
    ogr_pg_36_bis,
    ogr_pg_37,
    ogr_pg_38,
    ogr_pg_39,
    ogr_pg_40,
    ogr_pg_41,
    ogr_pg_42,
    ogr_pg_43,
    ogr_pg_44,
    ogr_pg_45,
    ogr_pg_46,
    ogr_pg_47,
    ogr_pg_48,
    ogr_pg_49,
    ogr_pg_50,
    ogr_pg_51,
    ogr_pg_52,
    ogr_pg_53,
    ogr_pg_53_bis,
    ogr_pg_54,
    ogr_pg_55,
    ogr_pg_56,
    ogr_pg_57,
    ogr_pg_58,
    ogr_pg_59,
    ogr_pg_60,
    ogr_pg_61,
    ogr_pg_62,
    ogr_pg_63,
    ogr_pg_64,
    ogr_pg_65,
    ogr_pg_66,
    ogr_pg_67,
    ogr_pg_68,
    ogr_pg_69,
    ogr_pg_70,
    ogr_pg_71,
    ogr_pg_72,
    ogr_pg_73,
    ogr_pg_74,
    ogr_pg_75,
    ogr_pg_76,
    ogr_pg_77,
    ogr_pg_78,
    ogr_pg_79,
    ogr_pg_80,
    ogr_pg_81,
    ogr_pg_82,
    ogr_pg_83,
    ogr_pg_84,
    ogr_pg_85,
    ogr_pg_86,
    ogr_pg_cleanup
]

disabled_gdaltest_list_internal = [
    ogr_pg_table_cleanup,
    ogr_pg_86,
    ogr_pg_cleanup ]

###############################################################################
# Run gdaltest_list_internal with PostGIS enabled and then with PostGIS disabled

def ogr_pg_with_and_without_postgis():

    gdaltest.ogr_pg_second_run = False
    gdaltest.run_tests( [ ogr_pg_1 ] )
    if gdaltest.pg_ds is None:
        return 'skip'
    #gdaltest.run_tests( [ ogr_pg_71 ] )
    #gdaltest.run_tests( [ ogr_pg_cleanup ] )
    if True:
        gdaltest.run_tests( gdaltest_list_internal )

        if gdaltest.pg_has_postgis:
            gdal.SetConfigOption("PG_USE_POSTGIS", "NO")
            gdaltest.ogr_pg_second_run = True
            gdaltest.run_tests( [ ogr_pg_1 ] )
            gdaltest.run_tests( gdaltest_list_internal )
            gdal.SetConfigOption("PG_USE_POSTGIS", "YES")
            gdaltest.ogr_pg_second_run = False

    return 'success'

gdaltest_list = [
    ogr_pg_with_and_without_postgis
]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_pg' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

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
import string
import math

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
        gdaltest.post_reason( 'GetNextFeature() didn not return None' )
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

    if feat is None:
        return 'success'

    feat.Destroy()
    gdaltest.post_reason( 'DeleteFeature() seems to have had no effect.' )

    return 'fail'

###############################################################################
# Create table from data/poly.shp in COPY mode.

def ogr_pg_11():

    if gdaltest.pg_ds is None:
        return 'skip'

    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:tpolycopy' )
    gdal.PopErrorHandler()

    gdal.SetConfigOption( 'PG_USE_COPY', 'YES' )

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
        
    gdal.SetConfigOption( 'PG_USE_COPY', 'NO' )
    
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
        gdaltest.post_reason( 'got layer for non-existant table!' )
        return 'fail'

    if count != gdaltest.pg_ds.GetLayerCount():
        gdaltest.post_reason( 'layer count changed unexpectedly.' )
        return 'fail'

    return 'success'

###############################################################################
# Test getting a layer by name that wasn't previously a layer.

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
          'POINT (10 20 5)' ),
        ( 'LINESTRING (10 10 1 2,20 20 3 4,30 30 5 6,40 40 7 8)',
          'LINESTRING (10 10 1,20 20 3,30 30 5,40 40 7)' ),
        ( 'POLYGON ((0 0 1 2,4 0 3 4,4 4 5 6,0 4 7 8,0 0 1 2))',
          'POLYGON ((0 0 1,4 0 3,4 4 5,0 4 7,0 0 1))' ),
        ( 'MULTIPOINT (10 20 5 5,30 30 7 7)',
          'MULTIPOINT (10 20 5,30 30 7)' ),
        ( 'MULTILINESTRING ((10 10 1 2,20 20 3 4),(30 30 5 6,40 40 7 8))',
          'MULTILINESTRING ((10 10 1,20 20 3),(30 30 5,40 40 7))' ),
        ( 'MULTIPOLYGON(((0 0 0 1,4 0 0 1,4 4 0 1,0 4 0 1,0 0 0 1),(1 1 0 5,2 1 0 5,2 2 0 5,1 2 0 5,1 1 0 5)),((-1 -1 0 10,-1 -2 0 10,-2 -2 0 10,-2 -1 0 10,-1 -1 0 10)))',
          'MULTIPOLYGON (((0 0 0,4 0 0,4 4 0,0 4 0,0 0 0),(1 1 0,2 1 0,2 2 0,1 2 0,1 1 0)),((-1 -1 0,-1 -2 0,-2 -2 0,-2 -1 0,-1 -1 0)))' ),
        ( 'GEOMETRYCOLLECTION(POINT(2 3 11 101),LINESTRING(2 3 12 102,3 4 13 103))',
          'GEOMETRYCOLLECTION (POINT (2 3 11),LINESTRING (2 3 12,3 4 13))' )
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
 
    for i in range(len(geometries)):
        feat = layer.GetFeature(i)
        geom = feat.GetGeometryRef()
        if geom is None:
            gdaltest.post_reason( 'did not get geometry, expected %s' % geometries[i][1] )
            return 'fail'
        wkt = geom.ExportToWkt()
        feat.Destroy()
        feat = None

        if wkt != geometries[i][1]:
            gdaltest.post_reason( 'WKT do not match' )
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

    fail = False

    feat = layer.GetNextFeature()
    while feat is not None:
        geom = feat.GetGeometryRef()
        if geom is not None:
            gdaltest.post_reason( 'expected feature with None geometry' )
            feat.Destroy()
            feat = None
            return 'fail'

        feat.Destroy()
        feat = layer.GetNextFeature()

    feat = None
    gdaltest.pg_ds.ReleaseResultSet(layer)
    layer = None

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

def ogr_pg_23():

    if gdaltest.pg_ds is None:
        return 'skip'

    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:datatypetest' )
    gdal.PopErrorHandler()

    ######################################################
    # Create Table
    lyr = gdaltest.pg_ds.CreateLayer( 'datatypetest' )

    ######################################################
    # Setup Schema
    # ogrtest.quick_create_layer_def( lyr, None )

    ######################################################
    # add some custom date fields.
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE datatypetest ADD COLUMN my_numeric5 numeric(5)' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE datatypetest ADD COLUMN my_numeric5_3 numeric(5,3)' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE datatypetest ADD COLUMN my_bool bool' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE datatypetest ADD COLUMN my_int2 int2' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE datatypetest ADD COLUMN my_int4 int4' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE datatypetest ADD COLUMN my_int8 int8' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE datatypetest ADD COLUMN my_float4 float4' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE datatypetest ADD COLUMN my_float8 float8' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE datatypetest ADD COLUMN my_real real' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE datatypetest ADD COLUMN my_char char' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE datatypetest ADD COLUMN my_varchar character varying' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE datatypetest ADD COLUMN my_varchar10 character varying(10)' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE datatypetest ADD COLUMN my_text text' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE datatypetest ADD COLUMN my_bytea bytea' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE datatypetest ADD COLUMN my_time time' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE datatypetest ADD COLUMN my_date date' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE datatypetest ADD COLUMN my_timestamp timestamp without time zone' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE datatypetest ADD COLUMN my_timestamptz timestamp with time zone' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE datatypetest ADD COLUMN my_chararray char(1)[]' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE datatypetest ADD COLUMN my_textarray text[]' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE datatypetest ADD COLUMN my_varchararray character varying[]' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE datatypetest ADD COLUMN my_int4array int4[]' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE datatypetest ADD COLUMN my_float4array float4[]' )
    gdaltest.pg_ds.ExecuteSQL( 'ALTER TABLE datatypetest ADD COLUMN my_float8array float8[]' )

    ######################################################
    # Create a populated records.

    if gdaltest.pg_has_postgis:
        geom_str = "GeomFromEWKT('POINT(10 20)')"
    else:
        geom_str = "'\\\\001\\\\001\\\\000\\\\000\\\\000\\\\000\\\\000\\\\000\\\\000\\\\000\\\\000$@\\\\000\\\\000\\\\000\\\\000\\\\000\\\\0004@'"
        if gdaltest.pg_quote_with_E:
            geom_str = "E" + geom_str
    gdaltest.pg_ds.ExecuteSQL( "INSERT INTO datatypetest ( my_numeric5, my_numeric5_3, my_bool, my_int2, my_int4, my_int8, my_float4, my_float8, my_real, my_char, my_varchar, my_varchar10, my_text, my_bytea, my_time, my_date, my_timestamp, my_timestamptz, my_chararray, my_textarray, my_varchararray, my_int4array, my_float4array, my_float8array, wkb_geometry) VALUES ( 12345, 0.123, 'T', 12345, 12345678, 1234567901234, 0.123, 0.12345678, 0.876, 'a', 'ab', 'varchar10 ', 'abc', 'xyz', '12:34:56', '2000-01-01', '2000-01-01 00:00:00', '2000-01-01 00:00:00+00', '{a,b}', '{aa,bb}', '{cc,dd}', '{100,200}', '{100.1,200.1}', '{100.12,200.12}', " + geom_str + " )" )


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

    if feat.my_numeric5 != 12345 or \
    feat.my_numeric5_3 != 0.123 or \
    feat.my_bool != 1 or \
    feat.my_int2 != 12345 or \
    feat.my_int4 != 12345678 or \
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
    feat.GetFieldAsString('my_timestamptz') != '2000/01/01 00:00:00+00' or \
    feat.GetFieldAsString('my_chararray') != '(2:a,b)' or \
    feat.GetFieldAsString('my_textarray') != '(2:aa,bb)' or \
    feat.GetFieldAsString('my_varchararray') != '(2:cc,dd)' or \
    feat.GetFieldAsString('my_int4array') != '(2:100,200)' :
#    feat.my_float4array != '(2:100.1,200.1)'
#    feat.my_float4array != '(2:100.12,200.12)'
#    feat.my_int8 != 1234567901234
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

def ogr_pg_26():

    if gdaltest.pg_ds is None:
        return 'skip'

    ds = ogr.Open( 'PGB:' + gdaltest.pg_connection_string, update = 1 )

    ds.ExecuteSQL( 'set timezone to "UTC"' )

    lyr = ds.GetLayerByName( 'datatypetest' )

    feat = lyr.GetNextFeature()
    if test_val_test_23(lyr.GetLayerDefn(), feat) != 'success':
        return 'fail'

    feat = None

    ds.Destroy()

    return 'success'

###############################################################################
# Test with PGB: connection and SELECT query

def ogr_pg_27():

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

    ds.Destroy()

    return 'success'

###############################################################################
# Duplicate all data types

def ogr_pg_28():

    if gdaltest.pg_ds is None:
        return 'skip'

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

    gdal.SetConfigOption( 'PG_USE_COPY', 'NO' )

    return 'success'


###############################################################################
# Test the tables= connection string option

def ogr_pg_31():

    if gdaltest.pg_ds is None:
        return 'skip'

    ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string + ' tables=tpoly,tpolycopy', update = 1 )

    if ds is None or ds.GetLayerCount() != 2:
        return 'fail'

    ds.Destroy()

    return 'success'

###############################################################################
# Test approximate srtext (#2123, #3508)

def ogr_pg_32():

    if gdaltest.pg_ds is None or not gdaltest.pg_has_postgis:
        return 'skip'

    gdaltest.pg_ds.ExecuteSQL("DELETE FROM spatial_ref_sys");

    ######################################################
    # Create Layer with EPSG:4326
    srs = osr.SpatialReference()
    srs.ImportFromEPSG( 4326 )

    gdaltest.pg_lyr = gdaltest.pg_ds.CreateLayer( 'testsrtext', srs = srs);

    sql_lyr = gdaltest.pg_ds.ExecuteSQL("SELECT COUNT(*) FROM spatial_ref_sys");
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
    gdaltest.pg_lyr = gdaltest.pg_ds.CreateLayer( 'testsrtext2', srs = srs);

    # Must still be 1
    sql_lyr = gdaltest.pg_ds.ExecuteSQL("SELECT COUNT(*) FROM spatial_ref_sys");
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
    gdaltest.pg_lyr = gdaltest.pg_ds.CreateLayer( 'testsrtext3', srs = srs);

    # Must still be 1
    sql_lyr = gdaltest.pg_ds.ExecuteSQL("SELECT COUNT(*) FROM spatial_ref_sys");
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

    gdaltest.pg_lyr = gdaltest.pg_ds.CreateLayer( 'testsrtext4', geom_type = ogr.wkbPoint, srs = srs);
    feat = ogr.Feature(gdaltest.pg_lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(0 0)'))
    gdaltest.pg_lyr.CreateFeature(feat)
    feat = None
    sr = gdaltest.pg_lyr.GetSpatialRef()
    if sr.ExportToWkt().find('26632') == -1:
        gdaltest.post_reason('did not get expected SRS')
        return 'fail'

    sql_lyr = gdaltest.pg_ds.ExecuteSQL("SELECT COUNT(*) FROM spatial_ref_sys");
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

    gdaltest.pg_lyr = gdaltest.pg_ds.CreateLayer( 'testsrtext5', srs = srs);

    sql_lyr = gdaltest.pg_ds.ExecuteSQL("SELECT COUNT(*) FROM spatial_ref_sys");
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

    try:
        gdaltest.pg_lyr = gdaltest.pg_ds.CreateLayer( 'testoverflows' )
        ogrtest.quick_create_layer_def( gdaltest.pg_lyr, [ ('0123456789' * 1000, ogr.OFTReal)] )
    except:
        pass

    try:
        gdaltest.pg_lyr = gdaltest.pg_ds.CreateLayer( 'testoverflows', options = [ 'OVERWRITE=YES', 'GEOMETRY_NAME=' + ('0123456789' * 1000) ] )
    except:
        pass

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
# Test support for multiple geometry columns (#1476)

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
        return 'fail'
    if gdaltest.pg_has_postgis and lyr.GetGeomType() != ogr.wkbPoint:
        return 'fail'

    try:
        if lyr.GetGeometryColumn() != 'wkb_geometry':
            gdaltest.post_reason( 'wrong geometry column name' )
            return 'fail'
    except:
        pass

    # Explicit query to 'table37_inherited(wkb_geometry)' should also work
    lyr = ds.GetLayerByName( 'table37_inherited(wkb_geometry)' )
    if lyr is None:
        return 'fail'


    # Check for the layer with the new 'pointBase' geometry column inherited from table37_base
    found = ogr_pg_check_layer_in_list(ds, 'table37_inherited(pointBase)')
    if found is False:
        gdaltest.post_reason( 'layer table37_inherited(pointBase) not listed' )
        return 'fail'

    lyr = ds.GetLayerByName( 'table37_inherited(pointBase)' )
    if lyr is None:
        return 'fail'
    if gdaltest.pg_has_postgis and lyr.GetGeomType() != ogr.wkbPoint:
        return 'fail'

    try:
        if lyr.GetGeometryColumn() != 'pointBase':
            gdaltest.post_reason( 'wrong geometry column name' )
            return 'fail'
    except:
        pass


    # Check for the layer with the new 'point25D' geometry column
    found = ogr_pg_check_layer_in_list(ds, 'table37_inherited(point25D)')
    if found is False:
        gdaltest.post_reason( 'layer table37_inherited(point25D) not listed' )
        return 'fail'

    lyr = ds.GetLayerByName( 'table37_inherited(point25D)' )
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

    ds.Destroy()

    # Check for the layer with the new 'point25D' geometry column when tables= is specified
    ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string + ' tables=table37_inherited(point25D)', update = 1 )
    found = ogr_pg_check_layer_in_list(ds, 'table37_inherited(point25D)')
    if found is False:
        gdaltest.post_reason( 'layer table37_inherited(point25D) not listed' )
        return 'fail'

    lyr = ds.GetLayerByName( 'table37_inherited(point25D)' )
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
    if gdaltest.pg_has_postgis and lyr.GetGeomType() != ogr.wkbPoint:
        return 'fail'

    try:
        if lyr.GetGeometryColumn() != 'wkb_geometry':
            gdaltest.post_reason( 'wrong geometry column name' )
            return 'fail'
    except:
        pass

    feat = lyr.GetNextFeature()
    if feat is None:
        gdaltest.post_reason( 'no feature')
        return 'fail'

    if feat.GetGeometryRef() is None or feat.GetGeometryRef().ExportToWkt() != 'POINT (0 1)':
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
    found = ogr_pg_check_layer_in_list(ds, 'testview(point25D)')
    if found is False:
        gdaltest.post_reason( 'layer testview(point25D) not listed' )
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
        print(layer.GetFeatureCount())
        return 'fail'

    found = ogr_pg_check_layer_in_list(ds, 'AutoTest-schema.tpoly')
    if found is True:
        gdaltest.post_reason( 'layer AutoTest-schema.tpoly is listed, but should not' )
        return 'fail'

    layer = ds.GetLayerByName('AutoTest-schema.tpoly')
    if layer.GetFeatureCount() != 3:
        print(layer.GetFeatureCount())
        return 'fail'

    found = ogr_pg_check_layer_in_list(ds, 'public.tpoly')
    if found is False:
        gdaltest.post_reason( 'layer tpoly not listed' )
        return 'fail'

    layer = ds.GetLayerByName('public.tpoly')
    if layer.GetFeatureCount() != 19:
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
        print(layer.GetFeatureCount())
        return 'fail'

    layer = ds.GetLayerByName('AutoTest-schema.test41')
    if layer.GetFeatureCount() != 0:
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
        print(layer.GetFeatureCount())
        return 'fail'

    found = ogr_pg_check_layer_in_list(ds, 'AutoTest-schema.tpoly')
    if found is True:
        gdaltest.post_reason( 'layer AutoTest-schema.tpoly is listed, but should not' )
        return 'fail'

    layer = ds.GetLayerByName('AutoTest-schema.tpoly')
    if layer.GetFeatureCount() != 3:
        print(layer.GetFeatureCount())
        return 'fail'

    found = ogr_pg_check_layer_in_list(ds, 'public.tpoly')
    if found is True:
        gdaltest.post_reason( 'layer public.tpoly is listed, but should not' )
        return 'fail'

    layer = ds.GetLayerByName('public.tpoly')
    if layer.GetFeatureCount() != 19:
        print(layer.GetFeatureCount())
        return 'fail'

    found = ogr_pg_check_layer_in_list(ds, 'test41')
    if found is False:
        gdaltest.post_reason( 'layer test41 not listed' )
        return 'fail'

    layer = ds.GetLayerByName('test41')
    if layer.GetFeatureCount() != 0:
        print(layer.GetFeatureCount())
        return 'fail'

    layer = ds.GetLayerByName('AutoTest-schema.test41')
    if layer.GetFeatureCount() != 0:
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
        print(layer.GetFeatureCount())
        return 'fail'

    found = ogr_pg_check_layer_in_list(ds, 'AutoTest-schema.tpoly')
    if found is False:
        gdaltest.post_reason( 'layer AutoTest-schema.tpoly not listed' )
        return 'fail'

    layer = ds.GetLayerByName('AutoTest-schema.tpoly')
    if layer.GetFeatureCount() != 3:
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

    gdaltest.pg_ds.ExecuteSQL('ALTER TABLE "select" RENAME COLUMN "ogc_fid" to "AND"');

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
        tab_feat[i] = lyr.GetNextFeature();
    
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
    gdaltest.pg_ds.Destroy()
    gdaltest.pg_ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string, update = 1 )
    lyr = gdaltest.pg_ds.GetLayer('no_pk_table')
    gdal.SetConfigOption('PGSQL_OGR_FID', None)

    feat = lyr.GetNextFeature()

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

    gdal.SetConfigOption( 'PG_USE_COPY', 'NO' )
    dst_feat.Destroy()

    for option in [ 'NO', 'YES' ]:
        for value in [ 'NaN', 'Inf', '-Inf' ]:
            gdaltest.pg_lyr.SetAttributeFilter( 'PRFEDEA = \''+value+'\' AND SHORTNAME = \''+option+'\'' )
            feat = gdaltest.pg_lyr.GetNextFeature()
            got_val = feat.GetField( 'AREA' )
            if value == 'NaN':
                if not gdaltest.isnan(got_val):
                    print(feat.GetFieldAsString( 'AREA' )+' returned for AREA instead of '+value)
                    return 'fail'
            elif got_val != float(value):
                print(feat.GetFieldAsString( 'AREA' )+' returned for AREA instead of '+value)
                return 'fail'

            if bHasSetFieldDoubleList:
                got_val = feat.GetFieldAsDoubleList( feature_def.GetFieldIndex('REALLIST') )
                if value == 'NaN':
                    if not gdaltest.isnan(got_val[0]) or not gdaltest.isnan(got_val[1]):
                        print(feat.GetFieldAsString( 'REALLIST' )+' returned for REALLIST instead of '+value)
                        return 'fail'
                elif got_val[0] != float(value) or got_val[1] != float(value):
                    print(feat.GetFieldAsString( 'REALLIST' )+' returned for REALLIST instead of '+value)
                    return 'fail'

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
# Test creating a layer with explicitely wkbNone geometry type

def ogr_pg_53():

    if gdaltest.pg_ds is None:
        return 'skip'

    lyr = gdaltest.pg_ds.CreateLayer( 'no_geometry_table', geom_type = ogr.wkbNone )
    field_defn = ogr.FieldDefn('foo')
    lyr.CreateField(field_defn)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 'bar')
    lyr.CreateFeature(feat)

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
    ret = gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -f PostgreSQL "' + 'PG:' + gdaltest.pg_connection_string + '" tmp/no_geometry_table.csv -overwrite')

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
# Test insertion of features with first field being a 0-charachter string in a
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

    gdal.SetConfigOption( 'PG_USE_COPY', None )

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

    ret = gdaltest.runexternal(test_cli_utilities.get_ogr2ogr_path() + ' -append -f PostgreSQL "' + 'PG:' + gdaltest.pg_connection_string + '" data/poly.shp -nln public.tpoly')

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

    sql_lyr = gdaltest.pg_ds.ExecuteSQL("CREATE TABLE ogr_pg_60(id integer,name varchar(50),primary key (id)) without oids")
    gdaltest.pg_ds.ReleaseResultSet(sql_lyr)

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

    gdal.SetConfigOption( 'PG_USE_COPY', None )

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
    gdaltest.pg_ds.CreateLayer( 'testsrtext2', srs = srs);

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

    lyr = gdaltest.pg_ds.CreateLayer('ogr_pg_63', options = ['COLUMN_TYPES=foo=int8,bar=numeric(10,5),baz=hstore'])
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('bar', ogr.OFTString))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('foo', '123')
    lyr.CreateFeature(feat)
    feat = None
    ds = None

    ds = ogr.Open('PG:' + gdaltest.pg_connection_string)
    lyr = ds.GetLayerByName('ogr_pg_63')
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('foo')).GetType() != ogr.OFTInteger:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldDefn(lyr.GetLayerDefn().GetFieldIndex('bar')).GetType() != ogr.OFTReal:
        gdaltest.post_reason('fail')
        return 'fail'

    feat = lyr.GetNextFeature()
    if feat.GetField('foo') != 123:
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
# 

def ogr_pg_table_cleanup():

    if gdaltest.pg_ds is None:
        return 'skip'

    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:tpoly' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:tpolycopy' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:datetest' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:testgeom' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:datatypetest' )
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
    
    # Drop second 'tpoly' from schema 'AutoTest-schema' (do NOT quote names here)
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:AutoTest-schema.tpoly' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:AutoTest-schema.test41' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:AutoTest-schema.table36_base' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:AutoTest-schema.table36_inherited' )
    # Drop 'AutoTest-schema' (here, double qoutes are required)
    gdaltest.pg_ds.ExecuteSQL( 'DROP SCHEMA \"AutoTest-schema\" CASCADE')
    gdal.PopErrorHandler()

    return 'success'

def ogr_pg_cleanup():

    if gdaltest.pg_ds is None:
        return 'skip'

    gdaltest.pg_ds = ogr.Open( 'PG:' + gdaltest.pg_connection_string, update = 1 )
    ogr_pg_table_cleanup();

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
    ogr_pg_22,
    ogr_pg_23,
    ogr_pg_24,
    ogr_pg_25,
    ogr_pg_26,
    ogr_pg_27,
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
    ogr_pg_cleanup ]

###############################################################################
# Run gdaltest_list_internal with PostGIS enabled and then with PostGIS disabled

def ogr_pg_with_and_without_postgis():

    gdaltest.run_tests( [ ogr_pg_1 ] )
    if gdaltest.pg_ds is None:
        return 'skip'

    gdaltest.run_tests( gdaltest_list_internal )

    if gdaltest.pg_has_postgis:
        gdal.SetConfigOption("PG_USE_POSTGIS", "NO")
        gdaltest.run_tests( [ ogr_pg_1 ] )
        gdaltest.run_tests( gdaltest_list_internal )
        gdal.SetConfigOption("PG_USE_POSTGIS", "YES")

    return 'success'

gdaltest_list = [
    ogr_pg_with_and_without_postgis
]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_pg' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()


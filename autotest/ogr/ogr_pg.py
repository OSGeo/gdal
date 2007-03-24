#!/usr/bin/env python
###############################################################################
# $Id: ogr_pg.py,v 1.9 2006/04/05 03:03:18 fwarmerdam Exp $
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

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
import ogr
import osr
import gdal

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

    try:
        dods_dr = ogr.GetDriverByName( 'PostgreSQL' )
    except:
        return 'skip'
    
    try:
        gdaltest.pg_ds = ogr.Open( 'PG:dbname=autotest', update = 1 )
    except:
        gdaltest.pg_ds = None

    if gdaltest.pg_ds is not None:
        return 'success'
    else:
        return 'skip'

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
    # Setup Schema
    ogrtest.quick_create_layer_def( gdaltest.pg_lyr,
                                    [ ('AREA', ogr.OFTReal),
                                      ('EAS_ID', ogr.OFTInteger),
                                      ('PRFEDEA', ogr.OFTString),
                                      ('SHORTNAME', ogr.OFTString, 8) ] )

    ######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature( feature_def = gdaltest.pg_lyr.GetLayerDefn() )

    shp_ds = ogr.Open( 'data/poly.shp' )
    gdaltest.shp_ds = shp_ds
    shp_lyr = shp_ds.GetLayer(0)
    
    feat = shp_lyr.GetNextFeature()
    gdaltest.poly_feat = []
    
    while feat is not None:

        gdaltest.poly_feat.append( feat )

        dst_feat.SetFrom( feat )
        gdaltest.pg_lyr.CreateFeature( dst_feat )

        feat = shp_lyr.GetNextFeature()

    dst_feat.Destroy()
        
    return 'success'

###############################################################################
# Verify that stuff we just wrote is still OK.

def ogr_pg_3():
    if gdaltest.pg_ds is None:
        return 'skip'

    expect = [168, 169, 166, 158, 165]

    gdaltest.pg_lyr.SetAttributeFilter( 'eas_id < 170' )
    tr = ogrtest.check_features_against_list( gdaltest.pg_lyr,
                                              'eas_id', expect )
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
    gdaltest.shp_ds.Destroy()

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
        gdaltest.pg_lyr.CreateFeature( dst_feat )
        
        ######################################################################
        # Read back the feature and get the geometry.

        gdaltest.pg_lyr.SetAttributeFilter( "PRFEDEA = '%s'" % item )
        feat_read = gdaltest.pg_lyr.GetNextFeature()
        geom_read = feat_read.GetGeometryRef()

        if ogrtest.check_feature_geometry( feat_read, geom ) != 0:
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
    
    tr = ogrtest.check_features_against_list( gdaltest.pg_lyr, 'eas_id',
                                              [ 158 ] )

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
        print feat.GetGeometryRef()
        gdaltest.post_reason( 'Geometry update failed' )
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
    gdaltest.shp_ds = shp_ds
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
    gdaltest.shp_ds.Destroy()
    gdaltest.shp_ds = None

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

    ds = ogr.Open( 'PG:dbname=autotest', update = 1 )

    ds.ExecuteSQL( 'set timezone to "UTC"' )

    lyr = ds.GetLayerByName( 'datetest' )

    feat = lyr.GetNextFeature()
    if feat.ogrdatetime != '2005/10/12 15:41:33+00' \
       or feat.ogrdate != '2005/10/12' \
       or feat.ogrtime != '10:41:33' \
       or feat.tsz != '2005/10/12 15:41:33+00' \
       or feat.ts != '2005/10/12 10:41:33' \
       or feat.dt != '2005/10/12' \
       or feat.tm != '10:41:33':
        gdaltest.post_reason( 'UTC value wrong' )
        feat.DumpReadable()
        return 'fail'
    
    ds.ExecuteSQL( 'set timezone to "Canada/Newfoundland"' )

    lyr.ResetReading()
    
    feat = lyr.GetNextFeature()

    if feat.ogrdatetime != '2005/10/12 13:11:33-0230' \
       or feat.tsz != '2005/10/12 13:11:33-0230' \
       or feat.ts != '2005/10/12 10:41:33' \
       or feat.dt != '2005/10/12' \
       or feat.tm != '10:41:33':
        gdaltest.post_reason( 'Newfoundland value wrong' )
        feat.DumpReadable()
        return 'fail'
    
    ds.ExecuteSQL( 'set timezone to "+5"' )

    lyr.ResetReading()
    
    feat = lyr.GetNextFeature()
    if feat.ogrdatetime != '2005/10/12 20:41:33+05' \
       or feat.tsz != '2005/10/12 20:41:33+05':
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

    if gdaltest.pg_ds is None:
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
# Test reading 4-dim geometry in EWKT format

def ogr_pg_20():

    if gdaltest.pg_ds is None:
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
    gdaltest.pg_ds.ExecuteSQL( "SELECT AddGeometryColumn('public','testgeom','wkb_geometry',-1,'GEOMETRY',4)" )
    for i in range(len(geometries)):
        gdaltest.pg_ds.ExecuteSQL( "INSERT INTO testgeom (ogc_fid,wkb_geometry) \
                                    VALUES (%d,GeomFromEWKT('%s'))" % ( i, geometries[i][0])  )

    # We need to re-read layers
    gdaltest.pg_ds.Destroy()
    gdaltest.pg_ds = None
    try:
        gdaltest.pg_ds = ogr.Open( 'PG:dbname=autotest', update = 1 )
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

    if gdaltest.pg_ds is None:
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
    layer = None

    return 'success'

###############################################################################
# 

def ogr_pg_cleanup():

    if gdaltest.pg_ds is None:
        return 'skip'

    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:tpoly' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:tpolycopy' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:datetest' )
    gdaltest.pg_ds.ExecuteSQL( 'DELLAYER:testgeom' )

    gdaltest.pg_ds.Destroy()
    gdaltest.pg_ds = None

    return 'success'

# NOTE: The ogr_pg_19 intentionally executed after ogr_pg_2

gdaltest_list = [ 
    ogr_pg_1,
    ogr_pg_2,
    ogr_pg_19,
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
    ogr_pg_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_pg' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()


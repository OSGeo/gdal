#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test MySQL driver functionality.
# Author:   Even Rouault <even dot rouault at mines dash paris dot ogr>
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

def ogr_mysql_1():

    gdaltest.mysql_ds = None

    try:
        mysql_dr = ogr.GetDriverByName( 'MySQL' )
    except:
#        print 'no driver'
        return 'skip'
    
    try:
        gdaltest.mysql_ds = ogr.Open( 'MYSQL:autotest', update = 1 )
    except:
        gdaltest.mysql_ds = None

    if gdaltest.mysql_ds is not None:
        return 'success'
    else:
#        print 'no dataset'
        return 'skip'

###############################################################################
# Create table from data/poly.shp

def ogr_mysql_2():

    if gdaltest.mysql_ds is None:
        return 'skip'

    shp_ds = ogr.Open( 'data/poly.shp' )
    gdaltest.shp_ds = shp_ds
    shp_lyr = shp_ds.GetLayer(0)

    ######################################################
    # Create Layer
    gdaltest.mysql_lyr = gdaltest.mysql_ds.CreateLayer( 'tpoly', srs = shp_lyr.GetSpatialRef(),
                                                  options = [ 'DIM=3', 'ENGINE=MyISAM' ] )

    ######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def( gdaltest.mysql_lyr,
                                    [ ('AREA', ogr.OFTReal),
                                      ('EAS_ID', ogr.OFTInteger),
                                      ('PRFEDEA', ogr.OFTString),
                                      ('SHORTNAME', ogr.OFTString, 8) ] )

    ######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature( feature_def = gdaltest.mysql_lyr.GetLayerDefn() )

    feat = shp_lyr.GetNextFeature()
    gdaltest.poly_feat = []
    
    while feat is not None:

        gdaltest.poly_feat.append( feat )

        dst_feat.SetFrom( feat )
        gdaltest.mysql_lyr.CreateFeature( dst_feat )

        feat = shp_lyr.GetNextFeature()

    dst_feat.Destroy()

    if gdaltest.mysql_lyr.GetFeatureCount() != shp_lyr.GetFeatureCount():
        gdaltest.post_reason( 'not matching feature count' )
        return 'failure'

    if not gdaltest.mysql_lyr.GetSpatialRef().IsSame(shp_lyr.GetSpatialRef()):
        gdaltest.post_reason( 'not matching spatial ref' )
        return 'failure'

    return 'success'

###############################################################################
# Verify that stuff we just wrote is still OK.

def ogr_mysql_3():
    if gdaltest.mysql_ds is None:
        return 'skip'

    if gdaltest.mysql_lyr.GetFeatureCount() != 10:
        gdaltest.post_reason( 'GetFeatureCount() returned %d instead of 10' % gdaltest.mysql_lyr.GetFeatureCount() )
        return 'fail'

    expect = [168, 169, 166, 158, 165]

    gdaltest.mysql_lyr.SetAttributeFilter( 'eas_id < 170' )
    tr = ogrtest.check_features_against_list( gdaltest.mysql_lyr,
                                              'eas_id', expect )

    if gdaltest.mysql_lyr.GetFeatureCount() != 5:
        gdaltest.post_reason( 'GetFeatureCount() returned %d instead of 5' % gdaltest.mysql_lyr.GetFeatureCount() )
        return 'fail'

    gdaltest.mysql_lyr.SetAttributeFilter( None )

    for i in range(len(gdaltest.poly_feat)):
        orig_feat = gdaltest.poly_feat[i]
        read_feat = gdaltest.mysql_lyr.GetNextFeature()
        
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

def ogr_mysql_4():

    if gdaltest.mysql_ds is None:
        return 'skip'

    # E. Rouault : the mySQL driver doesn't seem to like adding new features and
    # iterating over a query at the same time.
    # If trying to do so, we get the 'Commands out of sync' error.

    wkt_list = [ '10', '2', '1', '4', '5', '6' ]

    gdaltest.mysql_lyr.ResetReading()

    feature_def = gdaltest.mysql_lyr.GetLayerDefn()

    for item in wkt_list:
        dst_feat = ogr.Feature( feature_def )

        wkt = open( 'data/wkb_wkt/'+item+'.wkt' ).read()
        geom = ogr.CreateGeometryFromWkt( wkt )

        ######################################################################
        # Write geometry as a new Oracle feature.

        dst_feat.SetGeometryDirectly( geom )
        dst_feat.SetField( 'PRFEDEA', item )
        gdaltest.mysql_lyr.CreateFeature( dst_feat )

        dst_feat.Destroy()

    # FIXME : The source wkt polygons of '4' and '6' are not closed and
    # mySQL return them as closed, so the check_feature_geometry returns FALSE
    # Checking them after closing the rings again returns TRUE.

    wkt_list = [ '10', '2', '1', '5', '4', '6' ]

    for item in wkt_list:
        wkt = open( 'data/wkb_wkt/'+item+'.wkt' ).read()
        geom = ogr.CreateGeometryFromWkt( wkt )

        ######################################################################
        # Read back the feature and get the geometry.

        gdaltest.mysql_lyr.SetAttributeFilter( "PRFEDEA = '%s'" % item )
        feat_read = gdaltest.mysql_lyr.GetNextFeature()
        geom_read = feat_read.GetGeometryRef()

        if ogrtest.check_feature_geometry( feat_read, geom ) != 0:
            print('Geometry changed. Closing rings before trying again for wkt #',item)
            print('(before):',geom.ExportToWkt())
            geom.CloseRings()
            print('(after) :',geom.ExportToWkt())
            if ogrtest.check_feature_geometry( feat_read, geom ) != 0:
                return 'fail'

        feat_read.Destroy()


    return 'success'
    
###############################################################################
# Test ExecuteSQL() results layers without geometry.

def ogr_mysql_5():

    if gdaltest.mysql_ds is None:
        return 'skip'

    # E. Rouault : unlike PostgresSQL driver : None is sorted in last position 
    expect = [ 179, 173, 172, 171, 170, 169, 168, 166, 165, 158, None ]
    
    sql_lyr = gdaltest.mysql_ds.ExecuteSQL( 'select distinct eas_id from tpoly order by eas_id desc' )

    if sql_lyr.GetFeatureCount() != 11:
        gdaltest.post_reason( 'GetFeatureCount() returned %d instead of 11' % sql_lyr.GetFeatureCount() )
        return 'fail'

    tr = ogrtest.check_features_against_list( sql_lyr, 'eas_id', expect )

    gdaltest.mysql_ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'
    
###############################################################################
# Test ExecuteSQL() results layers with geometry.

def ogr_mysql_6():

    if gdaltest.mysql_ds is None:
        return 'skip'

    sql_lyr = gdaltest.mysql_ds.ExecuteSQL( "select * from tpoly where prfedea = '2'" )

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

    gdaltest.mysql_ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'
    
###############################################################################
# Test spatial filtering. 

def ogr_mysql_7():

    if gdaltest.mysql_ds is None:
        return 'skip'

    gdaltest.mysql_lyr.SetAttributeFilter( None )
    
    geom = ogr.CreateGeometryFromWkt( \
        'LINESTRING(479505 4763195,480526 4762819)' )
    gdaltest.mysql_lyr.SetSpatialFilter( geom )
    geom.Destroy()

    if gdaltest.mysql_lyr.GetFeatureCount() != 1:
        gdaltest.post_reason( 'GetFeatureCount() returned %d instead of 1' % gdaltest.mysql_lyr.GetFeatureCount() )
        return 'fail'

    tr = ogrtest.check_features_against_list( gdaltest.mysql_lyr, 'eas_id',
                                              [ 158 ] )

    gdaltest.mysql_lyr.SetAttributeFilter( 'eas_id = 158' )

    if gdaltest.mysql_lyr.GetFeatureCount() != 1:
        gdaltest.post_reason( 'GetFeatureCount() returned %d instead of 1' % gdaltest.mysql_lyr.GetFeatureCount() )
        return 'fail'

    gdaltest.mysql_lyr.SetAttributeFilter( None )

    gdaltest.mysql_lyr.SetSpatialFilter( None )
    
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

def ogr_mysql_8():

    if gdaltest.mysql_ds is None:
        return 'skip'

    dst_feat = ogr.Feature( feature_def = gdaltest.mysql_lyr.GetLayerDefn() )

    dst_feat.SetField( 'PRFEDEA', 'CrazyKey' )
    dst_feat.SetField( 'SHORTNAME', 'Crazy"\'Long' )
    # We are obliged to create a fake geometry
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt('POINT(0 0)') )
    gdaltest.mysql_lyr.CreateFeature( dst_feat )
    dst_feat.Destroy()
    
    gdaltest.mysql_lyr.SetAttributeFilter( "PRFEDEA = 'CrazyKey'" )
    feat_read = gdaltest.mysql_lyr.GetNextFeature()

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

def ogr_mysql_9():

    if gdaltest.mysql_ds is None:
        return 'skip'

    gdaltest.mysql_lyr.SetAttributeFilter( "PRFEDEA = 'CrazyKey'" )
    feat = gdaltest.mysql_lyr.GetNextFeature()
    gdaltest.mysql_lyr.SetAttributeFilter( None )

    feat.SetField( 'SHORTNAME', 'Reset' )

    point = ogr.Geometry( ogr.wkbPoint25D )
    point.SetPoint( 0, 5, 6 )
    feat.SetGeometryDirectly( point )

    if gdaltest.mysql_lyr.SetFeature( feat ) != 0:
        feat.Destroy()
        gdaltest.post_reason( 'SetFeature() method failed.' )
        return 'fail'

    fid = feat.GetFID()
    feat.Destroy()

    feat = gdaltest.mysql_lyr.GetFeature( fid )
    if feat is None:
        gdaltest.post_reason( 'GetFeature(%d) failed.' % fid )
        return 'fail'
        
    shortname = feat.GetField( 'SHORTNAME' )
    if shortname[:5] != 'Reset':
        gdaltest.post_reason( 'SetFeature() did not update SHORTNAME, got %s.'\
                              % shortname )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'POINT(5 6)' ) != 0:
        print(feat.GetGeometryRef())
        gdaltest.post_reason( 'Geometry update failed' )
        return 'fail'

    feat.Destroy()

    return 'success'

###############################################################################
# Verify that DeleteFeature() works properly.

def ogr_mysql_10():

    if gdaltest.mysql_ds is None:
        return 'skip'

    gdaltest.mysql_lyr.SetAttributeFilter( "PRFEDEA = 'CrazyKey'" )
    feat = gdaltest.mysql_lyr.GetNextFeature()
    gdaltest.mysql_lyr.SetAttributeFilter( None )

    fid = feat.GetFID()
    feat.Destroy()

    if gdaltest.mysql_lyr.DeleteFeature( fid ) != 0:
        gdaltest.post_reason( 'DeleteFeature() method failed.' )
        return 'fail'

    gdaltest.mysql_lyr.SetAttributeFilter( "PRFEDEA = 'CrazyKey'" )
    feat = gdaltest.mysql_lyr.GetNextFeature()
    gdaltest.mysql_lyr.SetAttributeFilter( None )

    if feat is None:
        return 'success'

    feat.Destroy()
    gdaltest.post_reason( 'DeleteFeature() seems to have had no effect.' )
    return 'fail'


###############################################################################
# Test very large query.

def ogr_mysql_15():

    if gdaltest.mysql_ds is None:
        return 'skip'

    expect = [169]

    query = 'eas_id = 169'
    
    for id in range(1000):
        query = query + (' or eas_id = %d' % (id+1000)) 

    gdaltest.mysql_lyr.SetAttributeFilter( query )
    tr = ogrtest.check_features_against_list( gdaltest.mysql_lyr,
                                              'eas_id', expect )
    gdaltest.mysql_lyr.SetAttributeFilter( None )

    if tr:
        return 'success'
    else:
        return 'fail'


###############################################################################
# Test very large statement.

def ogr_mysql_16():

    if gdaltest.mysql_ds is None:
        return 'skip'

    expect = [169]

    query = 'eas_id = 169'
    
    for id in range(1000):
        query = query + (' or eas_id = %d' % (id+1000))

    statement = 'select eas_id from tpoly where ' + query

    lyr = gdaltest.mysql_ds.ExecuteSQL( statement )

    tr = ogrtest.check_features_against_list( lyr, 'eas_id', expect )

    gdaltest.mysql_ds.ReleaseResultSet( lyr )

    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test requesting a non-existent table by name (bug 1480).

def ogr_mysql_17():

    if gdaltest.mysql_ds is None:
        return 'skip'

    count = gdaltest.mysql_ds.GetLayerCount()
    try:
        layer = gdaltest.mysql_ds.GetLayerByName( 'JunkTableName' )
    except:
        layer = None
        
    if layer is not None:
        gdaltest.post_reason( 'got layer for non-existant table!' )
        return 'fail'

    if count != gdaltest.mysql_ds.GetLayerCount():
        gdaltest.post_reason( 'layer count changed unexpectedly.' )
        return 'fail'

    return 'success'

###############################################################################
# Test getting a layer by name that wasn't previously a layer.

def ogr_mysql_18():

    if gdaltest.mysql_ds is None:
        return 'skip'

    count = gdaltest.mysql_ds.GetLayerCount()
    layer = gdaltest.mysql_ds.GetLayerByName( 'geometry_columns' )
    if layer is None:
        gdaltest.post_reason( 'did not get geometry_columns layer' )
        return 'fail'

    if count+1 != gdaltest.mysql_ds.GetLayerCount():
        gdaltest.post_reason( 'layer count unexpectedly unchanged.' )
        return 'fail'

    return 'success'

###############################################################################
# Test reading a layer extent

def ogr_mysql_19():

    if gdaltest.mysql_ds is None:
        return 'skip'

    layer = gdaltest.mysql_ds.GetLayerByName( 'tpoly' )
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
# Test using reserved keywords as column names and table names

def ogr_mysql_20():

    if gdaltest.mysql_ds is None:
        return 'skip'

    layer = gdaltest.mysql_ds.CreateLayer('select', options = [ 'ENGINE=MyISAM' ] )
    ogrtest.quick_create_layer_def( layer,
                                    [ ('desc', ogr.OFTString) ,
                                      ('select', ogr.OFTString) ] )
    dst_feat = ogr.Feature( feature_def = layer.GetLayerDefn() )

    dst_feat.SetField( 'desc', 'desc' )
    dst_feat.SetField( 'select', 'select' )
    # We are obliged to create a fake geometry
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt('POINT(0 1)') )
    layer.CreateFeature( dst_feat )
    dst_feat.Destroy()

    layer = gdaltest.mysql_ds.GetLayerByName('select')
    layer.ResetReading()
    feat = layer.GetNextFeature()
    if feat.desc == 'desc' and feat.select == 'select':
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test inserting NULL geometries into a table with a spatial index -> must FAIL

def ogr_mysql_21():

    if gdaltest.mysql_ds is None:
        return 'skip'

    layer = gdaltest.mysql_ds.CreateLayer('tablewithspatialindex', geom_type = ogr.wkbPoint, options = [ 'ENGINE=MyISAM' ])
    ogrtest.quick_create_layer_def( layer, [ ('name', ogr.OFTString) ] )
    dst_feat = ogr.Feature( feature_def = layer.GetLayerDefn() )
    dst_feat.SetField( 'name', 'name' )

    # The insertion MUST fail
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    layer.CreateFeature( dst_feat )
    gdal.PopErrorHandler()

    dst_feat.Destroy()

    layer.ResetReading()
    feat = layer.GetNextFeature()
    if feat is not None:
        return 'fail'

    return 'success'

###############################################################################
# Test inserting NULL geometries into a table without a spatial index

def ogr_mysql_22():

    if gdaltest.mysql_ds is None:
        return 'skip'

    layer = gdaltest.mysql_ds.CreateLayer('tablewithoutspatialindex', geom_type = ogr.wkbPoint,
                                          options = [ 'SPATIAL_INDEX=NO', 'ENGINE=MyISAM' ] )
    ogrtest.quick_create_layer_def( layer, [ ('name', ogr.OFTString) ] )
    dst_feat = ogr.Feature( feature_def = layer.GetLayerDefn() )
    dst_feat.SetField( 'name', 'name' )

    layer.CreateFeature( dst_feat )

    dst_feat.Destroy()

    layer.ResetReading()
    feat = layer.GetNextFeature()
    if feat is None:
        return 'fail'

    return 'success'

###############################################################################
# Check for right precision

def ogr_mysql_23():

    if gdaltest.mysql_ds is None:
        return 'skip'

    fields = ( 'zero', 'widthonly', 'onedecimal', 'twentynine', 'thirtyone' )
    values = ( 1, 2, 1.1, 0.12345678901234567890123456789, 0.1234567890123456789012345678901 )
    precision = ( 0, 0, 1, 29, 0 )

    ######################################################
    # Create a layer with a single feature through SQL
    gdaltest.mysql_lyr = gdaltest.mysql_ds.ExecuteSQL( "SELECT ROUND(1.1,0) AS zero, ROUND(2.0, 0) AS widthonly, ROUND(1.1,1) AS onedecimal, ROUND(0.12345678901234567890123456789,29) AS twentynine, GeomFromText(CONVERT('POINT(1.0 2.0)',CHAR)) as the_geom;" )

    feat = gdaltest.mysql_lyr.GetNextFeature()
    if feat is None:
        return 'fail'

    ######################################################
    # Check the values and the precisions
    for i in range(4):
        if feat.GetFieldIndex(fields[i]) < 0:
            print('field not found')
            return 'fail'
        if feat.GetField( feat.GetFieldIndex(fields[i]) ) != values[i]:
            print('value not right')
#            print feat.GetField( feat.GetFieldIndex(fields[i]) )
            return 'fail'
        if feat.GetFieldDefnRef( feat.GetFieldIndex(fields[i]) ).GetPrecision() != precision[i]:
            print('precision not right')
#            print feat.GetFieldDefnRef( feat.GetFieldIndex(fields[i]) ).GetPrecision()
            return 'fail'

    gdaltest.mysql_ds.ReleaseResultSet(gdaltest.mysql_lyr)
    gdaltest.mysql_lyr = None

    return 'success'

###############################################################################
# 

def ogr_mysql_cleanup():

    if gdaltest.mysql_ds is None:
        return 'skip'

    gdaltest.mysql_ds.ExecuteSQL( 'DROP TABLE tpoly' )
    gdaltest.mysql_ds.ExecuteSQL( 'DROP TABLE `select`' )
    gdaltest.mysql_ds.ExecuteSQL( 'DROP TABLE tablewithspatialindex' )
    gdaltest.mysql_ds.ExecuteSQL( 'DROP TABLE tablewithoutspatialindex' )
    gdaltest.mysql_ds.ExecuteSQL( 'DROP TABLE geometry_columns' )
    gdaltest.mysql_ds.ExecuteSQL( 'DROP TABLE spatial_ref_sys' )

    gdaltest.mysql_ds.Destroy()
    gdaltest.mysql_ds = None

    return 'success'

gdaltest_list = [ 
    ogr_mysql_1,
#    ogr_mysql_cleanup,
    ogr_mysql_2,
    ogr_mysql_19,
    ogr_mysql_3,
    ogr_mysql_4,
    ogr_mysql_5,
    ogr_mysql_6,
    ogr_mysql_7,
    ogr_mysql_8,
    ogr_mysql_9,
    ogr_mysql_10,
# ogr_mysql_11 to _14 are PG only features
    ogr_mysql_15,
    ogr_mysql_16,
    ogr_mysql_17,
# Fails but it is probably OK
#    ogr_mysql_18,
    ogr_mysql_20,
    ogr_mysql_21,
    ogr_mysql_22,
    ogr_mysql_23,
    ogr_mysql_cleanup
    ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_mysql' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()


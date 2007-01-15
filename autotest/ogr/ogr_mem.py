#!/usr/bin/env python
###############################################################################
# $Id: ogr_mem.py,v 1.3 2003/10/09 15:26:46 warmerda Exp $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR Memory driver functionality.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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
# 
#  $Log: ogr_mem.py,v $
#  Revision 1.3  2003/10/09 15:26:46  warmerda
#  added DeleteFeature check
#
#  Revision 1.2  2003/07/18 04:49:00  warmerda
#  fixed name
#
#  Revision 1.1  2003/04/08 20:55:51  warmerda
#  New
#
#

import os
import sys
import string

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
import ogr
import gdal

###############################################################################
# Open Memory datasource.

def ogr_mem_1():

    mem_drv = ogr.GetDriverByName('Memory')
    gdaltest.mem_ds = mem_drv.CreateDataSource( 'wrk_in_memory' )

    if gdaltest.mem_ds is not None:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Create table from data/poly.shp

def ogr_mem_2():

    if gdaltest.mem_ds is None:
        return 'skip'

    #######################################################
    # Create memory Layer
    gdaltest.mem_lyr = gdaltest.mem_ds.CreateLayer( 'tpoly' )

    #######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def( gdaltest.mem_lyr,
                                    [ ('AREA', ogr.OFTReal),
                                      ('EAS_ID', ogr.OFTInteger),
                                      ('PRFEDEA', ogr.OFTString) ] )
    
    #######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature( feature_def = gdaltest.mem_lyr.GetLayerDefn() )

    shp_ds = ogr.Open( 'data/poly.shp' )
    gdaltest.shp_ds = shp_ds
    shp_lyr = shp_ds.GetLayer(0)
    
    feat = shp_lyr.GetNextFeature()
    gdaltest.poly_feat = []
    
    while feat is not None:

        gdaltest.poly_feat.append( feat )

        dst_feat.SetFrom( feat )
        gdaltest.mem_lyr.CreateFeature( dst_feat )

        feat = shp_lyr.GetNextFeature()

    dst_feat.Destroy()
        
    return 'success'

###############################################################################
# Verify that stuff we just wrote is still OK.

def ogr_mem_3():
    if gdaltest.mem_ds is None:
        return 'skip'

    expect = [168, 169, 166, 158, 165]
    
    gdaltest.mem_lyr.SetAttributeFilter( 'eas_id < 170' )
    tr = ogrtest.check_features_against_list( gdaltest.mem_lyr,
                                              'eas_id', expect )
    gdaltest.mem_lyr.SetAttributeFilter( None )

    for i in range(len(gdaltest.poly_feat)):
        orig_feat = gdaltest.poly_feat[i]
        read_feat = gdaltest.mem_lyr.GetNextFeature()

        if ogrtest.check_feature_geometry(read_feat,orig_feat.GetGeometryRef(),
                                          max_error = 0.000000001 ) != 0:
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

def ogr_mem_4():

    if gdaltest.mem_ds is None:
        return 'skip'

    dst_feat = ogr.Feature( feature_def = gdaltest.mem_lyr.GetLayerDefn() )
    wkt_list = [ '10', '2', '1', '3d_1', '4', '5', '6' ]

    for item in wkt_list:

        wkt = open( 'data/wkb_wkt/'+item+'.wkt' ).read()
        geom = ogr.CreateGeometryFromWkt( wkt )
        
        ######################################################################
        # Write geometry as a new memory feature.
    
        dst_feat.SetGeometryDirectly( geom )
        dst_feat.SetField( 'PRFEDEA', item )
        gdaltest.mem_lyr.CreateFeature( dst_feat )
        
        ######################################################################
        # Read back the feature and get the geometry.

        gdaltest.mem_lyr.SetAttributeFilter( "PRFEDEA = '%s'" % item )
        feat_read = gdaltest.mem_lyr.GetNextFeature()
        geom_read = feat_read.GetGeometryRef()

        if ogrtest.check_feature_geometry( feat_read, geom ) != 0:
            return 'fail'

        feat_read.Destroy()

    dst_feat.Destroy()
    
    return 'success'
    
###############################################################################
# Test ExecuteSQL() results layers without geometry.

def ogr_mem_5():

    if gdaltest.mem_ds is None:
        return 'skip'

    expect = [ 179, 173, 172, 171, 170, 169, 168, 166, 165, 158, 0 ]
    
    sql_lyr = gdaltest.mem_ds.ExecuteSQL( 'select distinct eas_id from tpoly order by eas_id desc' )

    tr = ogrtest.check_features_against_list( sql_lyr, 'eas_id', expect )

    gdaltest.mem_ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'
    
###############################################################################
# Test ExecuteSQL() results layers with geometry.

def ogr_mem_6():

    if gdaltest.mem_ds is None:
        return 'skip'

    sql_lyr = gdaltest.mem_ds.ExecuteSQL( \
        'select * from tpoly where prfedea = "2"' )

    tr = ogrtest.check_features_against_list( sql_lyr, 'prfedea', [ '2' ] )
    if tr:
        sql_lyr.ResetReading()
        feat_read = sql_lyr.GetNextFeature()
        if ogrtest.check_feature_geometry( feat_read, 'MULTILINESTRING ((5.00121349 2.99853132,5.00121349 1.99853133),(5.00121349 1.99853133,5.00121349 0.99853133),(3.00121351 1.99853127,5.00121349 1.99853133),(5.00121349 1.99853133,6.00121348 1.99853135))' ) != 0:
            tr = 0
        feat_read.Destroy()
        
    gdaltest.mem_ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'
    
###############################################################################
# Test spatial filtering. 

def ogr_mem_7():

    if gdaltest.mem_ds is None:
        return 'skip'

    gdaltest.mem_lyr.SetAttributeFilter( None )
    
    geom = ogr.CreateGeometryFromWkt( \
        'LINESTRING(479505 4763195,480526 4762819)' )
    gdaltest.mem_lyr.SetSpatialFilter( geom )
    geom.Destroy()
    
    tr = ogrtest.check_features_against_list( gdaltest.mem_lyr, 'eas_id',
                                              [ 158 ] )

    gdaltest.mem_lyr.SetSpatialFilter( None )
    
    if tr:
        return 'success'
    else:
        return 'fail'
    
###############################################################################
# Test adding a new field.

def ogr_mem_8():

    ####################################################################
    # Add new string field.
    field_defn = ogr.FieldDefn( 'new_string', ogr.OFTString )
    gdaltest.mem_lyr.CreateField( field_defn )
    field_defn.Destroy()
    
    ####################################################################
    # Apply a value to this field in one feature.
    
    gdaltest.mem_lyr.SetAttributeFilter( "PRFEDEA = '2'" )
    feat_read = gdaltest.mem_lyr.GetNextFeature()
    feat_read.SetField( 'new_string', 'test1' )
    gdaltest.mem_lyr.SetFeature( feat_read )
    feat_read.Destroy()
    
    ####################################################################
    # Now featch two features and verify the new column works OK.

    gdaltest.mem_lyr.SetAttributeFilter( "PRFEDEA IN ( '2', '1' )" )

    tr = ogrtest.check_features_against_list( gdaltest.mem_lyr, 'new_string',
                                              [ 'test1', None ] )

    gdaltest.mem_lyr.SetAttributeFilter( None )
    
    if tr:
        return 'success'
    else:
        return 'fail'
    
###############################################################################
# Test deleting a feature.

def ogr_mem_9():

    if not gdaltest.mem_lyr.TestCapability( 'DeleteFeature' ):
        gdaltest.post_reason( 'DeleteFeature capability test failed.' )
        return 'fail'

    old_count = gdaltest.mem_lyr.GetFeatureCount()
    
    ####################################################################
    # Delete target feature.

    target_fid = 2
    if gdaltest.mem_lyr.DeleteFeature( target_fid ) != 0:
        gdaltest.post_reason( 'DeleteFeature returned error code.' )
        return 'fail'
    
    ####################################################################
    # Verify that count has dropped by one, and that the feature in question
    # can't be fetched.
    new_count = gdaltest.mem_lyr.GetFeatureCount()
    if new_count != old_count - 1:
        gdaltest.post_reason( 'got feature count of %d, not expected %d.' \
                              % (new_count, old_count -1) )

    if gdaltest.mem_lyr.GetFeature( target_fid ) is not None:
        gdaltest.post_reason( 'Got deleted feature!' )
        return 'fail'

    return 'success'
    
###############################################################################
# 

def ogr_mem_cleanup():

    if gdaltest.mem_ds is None:
        return 'skip'

    gdaltest.mem_ds.Destroy()
    gdaltest.mem_ds = None

    return 'success'

gdaltest_list = [ 
    ogr_mem_1,
    ogr_mem_2,
    ogr_mem_3,
    ogr_mem_4,
    ogr_mem_5,
    ogr_mem_6,
    ogr_mem_7,
    ogr_mem_8,
    ogr_mem_9,
    ogr_mem_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_mem' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()


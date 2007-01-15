#!/usr/bin/env python
###############################################################################
# $Id: ogr_sqlite.py,v 1.6 2006/04/28 03:34:18 fwarmerdam Exp $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test SQLite driver functionality.
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
# 
#  $Log: ogr_sqlite.py,v $
#  Revision 1.6  2006/04/28 03:34:18  fwarmerdam
#  skip if GetDriverByName() returns NULL
#
#  Revision 1.5  2004/10/27 19:41:22  fwarmerdam
#  Use reasonable geometry precision test for intermediate WKT rep.
#
#  Revision 1.4  2004/07/13 15:46:10  warmerda
#  added tests for transactions, GetFeature() and SetFeature()
#
#  Revision 1.3  2004/07/12 21:49:55  warmerda
#  fixed up .db removal
#
#  Revision 1.2  2004/07/12 20:48:58  warmerda
#  now creates temporary database
#
#  Revision 1.1  2004/07/12 20:08:43  warmerda
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
import osr
import gdal

###############################################################################
# Create a fresh database.

def ogr_sqlite_1():

    gdaltest.sl_ds = None

    try:
        sqlite_dr = ogr.GetDriverByName( 'SQLite' )
        if sqlite_dr is None:
            return 'skip'
    except:
        return 'skip'

    try:
        os.remove( 'tmp/sqlite_test.db' )
    except:
        pass

    gdaltest.sl_ds = sqlite_dr.CreateDataSource( 'tmp/sqlite_test.db' )

    if gdaltest.sl_ds is not None:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Create table from data/poly.shp

def ogr_sqlite_2():

    if gdaltest.sl_ds is None:
        return 'skip'

    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    gdaltest.sl_ds.ExecuteSQL( 'DELLAYER:tpoly' )
    gdal.PopErrorHandler()

    ######################################################
    # Create Layer
    gdaltest.sl_lyr = gdaltest.sl_ds.CreateLayer( 'tpoly' )

    ######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def( gdaltest.sl_lyr,
                                    [ ('AREA', ogr.OFTReal),
                                      ('EAS_ID', ogr.OFTInteger),
                                      ('PRFEDEA', ogr.OFTString) ] )

    ######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature( feature_def = gdaltest.sl_lyr.GetLayerDefn() )

    shp_ds = ogr.Open( 'data/poly.shp' )
    gdaltest.shp_ds = shp_ds
    shp_lyr = shp_ds.GetLayer(0)
    
    feat = shp_lyr.GetNextFeature()
    gdaltest.poly_feat = []
    
    while feat is not None:

        gdaltest.poly_feat.append( feat )

        dst_feat.SetFrom( feat )
        gdaltest.sl_lyr.CreateFeature( dst_feat )

        feat = shp_lyr.GetNextFeature()

    dst_feat.Destroy()
        
    return 'success'

###############################################################################
# Verify that stuff we just wrote is still OK.

def ogr_sqlite_3():
    if gdaltest.sl_ds is None:
        return 'skip'

    expect = [168, 169, 166, 158, 165]

    gdaltest.sl_lyr.SetAttributeFilter( 'eas_id < 170' )
    tr = ogrtest.check_features_against_list( gdaltest.sl_lyr,
                                              'eas_id', expect )
    gdaltest.sl_lyr.SetAttributeFilter( None )

    for i in range(len(gdaltest.poly_feat)):
        orig_feat = gdaltest.poly_feat[i]
        read_feat = gdaltest.sl_lyr.GetNextFeature()

        if read_feat is None:
            gdaltest.post_reason( 'Did not get as many features as expected.')
            return 'fail'

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

def ogr_sqlite_4():

    if gdaltest.sl_ds is None:
        return 'skip'

    dst_feat = ogr.Feature( feature_def = gdaltest.sl_lyr.GetLayerDefn() )
    wkt_list = [ '10', '2', '1', '3d_1', '4', '5', '6' ]

    for item in wkt_list:

        wkt = open( 'data/wkb_wkt/'+item+'.wkt' ).read()
        geom = ogr.CreateGeometryFromWkt( wkt )
        
        ######################################################################
        # Write geometry as a new feature.
    
        dst_feat.SetGeometryDirectly( geom )
        dst_feat.SetField( 'PRFEDEA', item )
        gdaltest.sl_lyr.CreateFeature( dst_feat )
        
        ######################################################################
        # Read back the feature and get the geometry.

        gdaltest.sl_lyr.SetAttributeFilter( "PRFEDEA = '%s'" % item )
        feat_read = gdaltest.sl_lyr.GetNextFeature()

        if feat_read is None:
            gdaltest.post_reason( 'Did not get as many features as expected.')
            return 'fail'

        geom_read = feat_read.GetGeometryRef()

        if ogrtest.check_feature_geometry( feat_read, geom ) != 0:
            return 'fail'

        feat_read.Destroy()

    dst_feat.Destroy()
    
    return 'success'
    
###############################################################################
# Test ExecuteSQL() results layers without geometry.

def ogr_sqlite_5():

    if gdaltest.sl_ds is None:
        return 'skip'

    expect = [ 179, 173, 172, 171, 170, 169, 168, 166, 165, 158, None ]
    
    sql_lyr = gdaltest.sl_ds.ExecuteSQL( 'select distinct eas_id from tpoly order by eas_id desc' )

    tr = ogrtest.check_features_against_list( sql_lyr, 'eas_id', expect )

    gdaltest.sl_ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'
    
###############################################################################
# Test ExecuteSQL() results layers with geometry.

def ogr_sqlite_6():

    if gdaltest.sl_ds is None:
        return 'skip'

    sql_lyr = gdaltest.sl_ds.ExecuteSQL( "select * from tpoly where prfedea = '2'" )

    tr = ogrtest.check_features_against_list( sql_lyr, 'prfedea', [ '2' ] )
    if tr:
        sql_lyr.ResetReading()
        feat_read = sql_lyr.GetNextFeature()
        if ogrtest.check_feature_geometry( feat_read, 'MULTILINESTRING ((5.00121349 2.99853132,5.00121349 1.99853133),(5.00121349 1.99853133,5.00121349 0.99853133),(3.00121351 1.99853127,5.00121349 1.99853133),(5.00121349 1.99853133,6.00121348 1.99853135))' ) != 0:
            tr = 0
        feat_read.Destroy()
        
    gdaltest.sl_ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'
    
###############################################################################
# Test spatial filtering. 

def ogr_sqlite_7():

    if gdaltest.sl_ds is None:
        return 'skip'

    gdaltest.sl_lyr.SetAttributeFilter( None )
    
    geom = ogr.CreateGeometryFromWkt( \
        'LINESTRING(479505 4763195,480526 4762819)' )
    gdaltest.sl_lyr.SetSpatialFilter( geom )
    geom.Destroy()
    
    tr = ogrtest.check_features_against_list( gdaltest.sl_lyr, 'eas_id',
                                              [ 158 ] )

    gdaltest.sl_lyr.SetSpatialFilter( None )
    
    if tr:
        return 'success'
    else:
        return 'fail'
    
###############################################################################
# Test transactions with rollback.  

def ogr_sqlite_8():

    if gdaltest.sl_ds is None:
        return 'skip'

    ######################################################################
    # Prepare working feature.
    
    dst_feat = ogr.Feature( feature_def = gdaltest.sl_lyr.GetLayerDefn() )
    dst_feat.SetGeometryDirectly( ogr.CreateGeometryFromWkt( 'POINT(10 20)' ))

    dst_feat.SetField( 'PRFEDEA', 'rollbacktest' )

    ######################################################################
    # Create it, but rollback the transaction.
    
    gdaltest.sl_lyr.StartTransaction()
    gdaltest.sl_lyr.CreateFeature( dst_feat )
    gdaltest.sl_lyr.RollbackTransaction()

    ######################################################################
    # Verify that it is not in the layer.

    gdaltest.sl_lyr.SetAttributeFilter( "PRFEDEA = 'rollbacktest'" )
    feat_read = gdaltest.sl_lyr.GetNextFeature()
    gdaltest.sl_lyr.SetAttributeFilter( None )

    if feat_read is not None:
        gdaltest.post_reason( 'Unexpectedly got rollbacktest feature.' )
        return 'fail'

    ######################################################################
    # Create it, and commit the transaction.
    
    gdaltest.sl_lyr.StartTransaction()
    gdaltest.sl_lyr.CreateFeature( dst_feat )
    gdaltest.sl_lyr.CommitTransaction()

    ######################################################################
    # Verify that it is not in the layer.

    gdaltest.sl_lyr.SetAttributeFilter( "PRFEDEA = 'rollbacktest'" )
    feat_read = gdaltest.sl_lyr.GetNextFeature()
    gdaltest.sl_lyr.SetAttributeFilter( None )

    if feat_read is None:
        gdaltest.post_reason( 'Failed to get committed feature.' )
        return 'fail'

    feat_read.Destroy()
    dst_feat.Destroy()

    return 'success'
    
###############################################################################
# Test SetFeature()

def ogr_sqlite_9():

    if gdaltest.sl_ds is None:
        return 'skip'

    ######################################################################
    # Read feature with EAS_ID 158. 

    gdaltest.sl_lyr.SetAttributeFilter( "eas_id = 158" )
    feat_read = gdaltest.sl_lyr.GetNextFeature()
    gdaltest.sl_lyr.SetAttributeFilter( None )

    if feat_read is None:
        gdaltest.post_reason( 'did not find eas_id 158!' )
        return 'fail'

    ######################################################################
    # Modify the PRFEDEA value, and reset it. 

    feat_read.SetField( 'PRFEDEA', 'SetWorked' )
    err = gdaltest.sl_lyr.SetFeature( feat_read )
    if err != 0:
        gdaltest.post_reason( 'SetFeature() reported error %d' % err )
        return 'fail'

    ######################################################################
    # Read feature with EAS_ID 158 and check that PRFEDEA was altered.

    gdaltest.sl_lyr.SetAttributeFilter( "eas_id = 158" )
    feat_read_2 = gdaltest.sl_lyr.GetNextFeature()
    gdaltest.sl_lyr.SetAttributeFilter( None )

    if feat_read_2 is None:
        gdaltest.post_reason( 'did not find eas_id 158!' )
        return 'fail'

    if feat_read_2.GetField('PRFEDEA') != 'SetWorked':
        feat_read_2.DumpReadable()
        gdaltest.post_reason( 'PRFEDEA apparently not reset as expected.' )
        return 'fail'

    feat_read.Destroy()
    feat_read_2.Destroy()

    return 'success'
    
###############################################################################
# Test GetFeature()

def ogr_sqlite_10():

    if gdaltest.sl_ds is None:
        return 'skip'

    ######################################################################
    # Read feature with EAS_ID 158. 

    gdaltest.sl_lyr.SetAttributeFilter( "eas_id = 158" )
    feat_read = gdaltest.sl_lyr.GetNextFeature()
    gdaltest.sl_lyr.SetAttributeFilter( None )

    if feat_read is None:
        gdaltest.post_reason( 'did not find eas_id 158!' )
        return 'fail'

    ######################################################################
    # Now read the feature by FID.

    feat_read_2 = gdaltest.sl_lyr.GetFeature( feat_read.GetFID() )

    if feat_read_2 is None:
        gdaltest.post_reason( 'did not find FID %d' % feat_read.GetFID() )
        return 'fail'

    if feat_read_2.GetField('PRFEDEA') != feat_read.GetField('PRFEDEA'):
        feat_read.DumpReadable()
        feat_read_2.DumpReadable()
        gdaltest.post_reason( 'GetFeature() result seems to not match expected.' )
        return 'fail'

    feat_read.Destroy()
    feat_read_2.Destroy()

    return 'success'
    
###############################################################################
# 

def ogr_sqlite_cleanup():

    if gdaltest.sl_ds is None:
        return 'skip'

    gdaltest.sl_ds.ExecuteSQL( 'DELLAYER:tpoly' )

    gdaltest.sl_ds.Destroy()
    gdaltest.sl_ds = None

    try:
        os.remove( 'tmp/sqlite_test.db' )
    except:
        pass

    return 'success'

gdaltest_list = [ 
    ogr_sqlite_1,
    ogr_sqlite_2,
    ogr_sqlite_3,
    ogr_sqlite_4,
    ogr_sqlite_5,
    ogr_sqlite_6,
    ogr_sqlite_7,
    ogr_sqlite_8,
    ogr_sqlite_9,
    ogr_sqlite_10,
    ogr_sqlite_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_sqlite' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()


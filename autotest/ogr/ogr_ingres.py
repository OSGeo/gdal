#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR Ingres driver functionality.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
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
import string

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import ogr
from osgeo import gdal

###############################################################################
# Open INGRES test datasource.

def ogr_ingres_1():

    gdaltest.ingres_ds = None

    try:
        drv = ogr.GetDriverByName('Ingres')
        if drv is None:
            return 'skip'
    except:
        return 'skip'

    try:
        gdaltest.ingres_ds = ogr.Open( '@driver=ingres,dbname=test', update=1 )
        if gdaltest.ingres_ds is None:
            return 'skip'
    except:
        return 'skip'

    return 'success'

###############################################################################
# Create table from data/poly.shp

def ogr_ingres_2():

    if gdaltest.ingres_ds is None:
        return 'skip'

    #######################################################
    # Create Layer
    gdaltest.ingres_lyr = gdaltest.ingres_ds.CreateLayer( \
        'tpoly', geom_type=ogr.wkbPolygon,
        options = [ 'OVERWRITE=YES' ] )

    #######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def( gdaltest.ingres_lyr,
                                    [ ('AREA', ogr.OFTReal),
                                      ('EAS_ID', ogr.OFTInteger),
                                      ('PRFEDEA', ogr.OFTString) ] )
    
    #######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature( feature_def = gdaltest.ingres_lyr.GetLayerDefn() )

    shp_ds = ogr.Open( 'data/poly.shp' )
    gdaltest.shp_ds = shp_ds
    shp_lyr = shp_ds.GetLayer(0)
    
    feat = shp_lyr.GetNextFeature()
    gdaltest.poly_feat = []
    
    while feat is not None:

        gdaltest.poly_feat.append( feat )

        dst_feat.SetFrom( feat )
        gdaltest.ingres_lyr.CreateFeature( dst_feat )

        feat = shp_lyr.GetNextFeature()

    dst_feat.Destroy()
        
    return 'success'

###############################################################################
# Verify that stuff we just wrote is still OK.

def ogr_ingres_3():
    if gdaltest.ingres_ds is None:
        return 'skip'

    expect = [168, 169, 166, 158, 165]
    
    gdaltest.ingres_lyr.SetAttributeFilter( 'eas_id < 170' )
    tr = ogrtest.check_features_against_list( gdaltest.ingres_lyr,
                                              'eas_id', expect )
    gdaltest.ingres_lyr.SetAttributeFilter( None )

    for i in range(len(gdaltest.poly_feat)):
        orig_feat = gdaltest.poly_feat[i]
        read_feat = gdaltest.ingres_lyr.GetNextFeature()

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

    # This is to force cleanup of the transaction.  We need a way of
    # automating this in the driver.
    read_feat = gdaltest.ingres_lyr.GetNextFeature()
        
    if tr:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Test ExecuteSQL() results layers without geometry.

def ogr_ingres_4():

    if gdaltest.ingres_ds is None:
        return 'skip'

    expect = [ 179, 173, 172, 171, 170, 169, 168, 166, 165, 158 ]
    
    sql_lyr = gdaltest.ingres_ds.ExecuteSQL( 'select distinct eas_id from tpoly order by eas_id desc' )

    tr = ogrtest.check_features_against_list( sql_lyr, 'eas_id', expect )

    gdaltest.ingres_ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'
    
###############################################################################
# Test ExecuteSQL() results layers with geometry.
#
# Unfortunately, for now an executesql result that includes new geometries
# fails to ever get any result records as executed by ogringresstatement.cpp,
# so we disable this test.

def ogr_ingres_5():

    if gdaltest.ingres_ds is None:
        return 'skip'

    return 'skip'

    sql_lyr = gdaltest.ingres_ds.ExecuteSQL( \
        "select * from tpoly where prfedea = '35043413'" )

    tr = ogrtest.check_features_against_list( sql_lyr, 'prfedea',
                                              [ '35043413' ] )
    if tr:
        sql_lyr.ResetReading()
        feat_read = sql_lyr.GetNextFeature()
        if ogrtest.check_feature_geometry( feat_read, 'POLYGON ((479750.6875 4764702.0,479658.59375 4764670.0,479640.09375 4764721.0,479735.90625 4764752.0,479750.6875 4764702.0))' ) != 0:
            tr = 0
        feat_read.Destroy()
        
    gdaltest.ingres_ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'
    
###############################################################################
# Test spatial filtering. 

def ogr_ingres_6():

    if gdaltest.ingres_ds is None:
        return 'skip'

    gdaltest.ingres_lyr.SetAttributeFilter( None )
    
    geom = ogr.CreateGeometryFromWkt( \
        'LINESTRING(479505 4763195,480526 4762819)' )
    gdaltest.ingres_lyr.SetSpatialFilter( geom )
    geom.Destroy()
    
    tr = ogrtest.check_features_against_list( gdaltest.ingres_lyr, 'eas_id',
                                              [ 158 ] )

    gdaltest.ingres_lyr.SetSpatialFilter( None )
    
    if tr:
        return 'success'
    else:
        return 'fail'
    
###############################################################################
# Test adding a new field.

def ogr_ingres_7():

    if gdaltest.ingres_ds is None:
        return 'skip'
    
    ####################################################################
    # Add new string field.
    field_defn = ogr.FieldDefn( 'new_string', ogr.OFTString )
    result = gdaltest.ingres_lyr.CreateField( field_defn )
    field_defn.Destroy()

    if result is not 0:
        gdaltest.post_reason( 'CreateField failed!' )
        return 'fail'
    
    ####################################################################
    # Apply a value to this field in one feature.
    
    gdaltest.ingres_lyr.SetAttributeFilter( "prfedea = '35043423'" )
    feat_read = gdaltest.ingres_lyr.GetNextFeature()
    if feat_read is None:
        gdaltest.post_reason( 'failed to read target feature!' )
        return 'fail'

    gdaltest.ingres_fid = feat_read.GetFID()
    
    feat_read.SetField( 'new_string', 'test1' )
    gdaltest.ingres_lyr.SetFeature( feat_read )
    feat_read.Destroy()
    
    ####################################################################
    # Now fetch two features and verify the new column works OK.

    gdaltest.ingres_lyr.SetAttributeFilter( \
        "PRFEDEA IN ( '35043423', '35043414' )" )

    tr = ogrtest.check_features_against_list( gdaltest.ingres_lyr, 'new_string',
                                              [ None, 'test1' ] )

    gdaltest.ingres_lyr.SetAttributeFilter( None )
    
    if tr:
        return 'success'
    else:
        return 'fail'
    
###############################################################################
# Test deleting a feature.

def ogr_ingres_8():

    if gdaltest.ingres_ds is None:
        return 'skip'

    if not gdaltest.ingres_lyr.TestCapability( 'DeleteFeature' ):
        gdaltest.post_reason( 'DeleteFeature capability test failed.' )
        return 'fail'

    old_count = gdaltest.ingres_lyr.GetFeatureCount()
    
    ####################################################################
    # Delete target feature.

    target_fid = gdaltest.ingres_fid
    if gdaltest.ingres_lyr.DeleteFeature( target_fid ) != 0:
        gdaltest.post_reason( 'DeleteFeature returned error code.' )
        return 'fail'
    
    ####################################################################
    # Verify that count has dropped by one, and that the feature in question
    # can't be fetched.
    new_count = gdaltest.ingres_lyr.GetFeatureCount()
    if new_count != old_count - 1:
        gdaltest.post_reason( 'got feature count of %d, not expected %d.' \
                              % (new_count, old_count -1) )

    if gdaltest.ingres_lyr.GetFeature( target_fid ) is not None:
        gdaltest.post_reason( 'Got deleted feature!' )
        return 'fail'

    return 'success'
    
###############################################################################
# 

def ogr_ingres_cleanup():

    if gdaltest.ingres_ds is None:
        return 'skip'

    gdaltest.ingres_ds.Destroy()
    gdaltest.ingres_ds = None

    return 'success'

gdaltest_list = [ 
    ogr_ingres_1,
    ogr_ingres_2,
    ogr_ingres_3,
    ogr_ingres_4,
    ogr_ingres_5,
    ogr_ingres_6,
    ogr_ingres_7,
    ogr_ingres_8,
    ogr_ingres_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_ingres' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()


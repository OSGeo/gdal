#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test OGR Memory driver functionality.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2011, Even Rouault <even dot rouault at mines-paris dot org>
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
# Open Memory datasource.

def ogr_mem_1():

    mem_drv = ogr.GetDriverByName('Memory')
    gdaltest.mem_ds = mem_drv.CreateDataSource( 'wrk_in_memory' )

    if gdaltest.mem_ds is None:
        return 'fail'

    return 'success'

###############################################################################
# Create table from data/poly.shp

def ogr_mem_2():

    if gdaltest.mem_ds is None:
        return 'skip'

    if gdaltest.mem_ds.TestCapability( ogr.ODsCCreateLayer ) == 0:
        gdaltest.post_reason ('ODsCCreateLayer TestCapability failed.' )
        return 'fail'

    #######################################################
    # Create memory Layer
    gdaltest.mem_lyr = gdaltest.mem_ds.CreateLayer( 'tpoly' )

    #######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def( gdaltest.mem_lyr,
                                    [ ('AREA', ogr.OFTReal),
                                      ('EAS_ID', ogr.OFTInteger),
                                      ('PRFEDEA', ogr.OFTString),
                                      ('WHEN', ogr.OFTDateTime) ] )
    
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

    expect = [ 179, 173, 172, 171, 170, 169, 168, 166, 165, 158, None ]
    
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

    if gdaltest.mem_lyr.TestCapability( ogr.OLCFastSpatialFilter ):
        gdaltest.post_reason( 'OLCFastSpatialFilter capability test should have failed.' )
        return 'fail'
    
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

    # Test expected failed case of SetFeature()
    new_feat = ogr.Feature(gdaltest.mem_lyr.GetLayerDefn())
    new_feat.SetFID(-2)
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = gdaltest.mem_lyr.SetFeature( new_feat )
    gdal.PopErrorHandler()
    if ret == 0:
        return 'fail'
    new_feat = None
    
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

    if not gdaltest.mem_lyr.TestCapability( ogr.OLCDeleteFeature ):
        gdaltest.post_reason( 'OLCDeleteFeature capability test failed.' )
        return 'fail'

    if not gdaltest.mem_lyr.TestCapability( ogr.OLCFastFeatureCount ):
        gdaltest.post_reason( 'OLCFastFeatureCount capability test failed.' )
        return 'fail'

    old_count = gdaltest.mem_lyr.GetFeatureCount()
    
    ####################################################################
    # Delete target feature.

    target_fid = 2
    if gdaltest.mem_lyr.DeleteFeature( target_fid ) != 0:
        gdaltest.post_reason( 'DeleteFeature returned error code.' )
        return 'fail'

    if gdaltest.mem_lyr.DeleteFeature( target_fid ) == 0:
        gdaltest.post_reason( 'DeleteFeature should have returned error code.' )
        return 'fail'

    ####################################################################
    # Verify that count has dropped by one, and that the feature in question
    # can't be fetched.
    new_count = gdaltest.mem_lyr.GetFeatureCount()
    if new_count != old_count - 1:
        gdaltest.post_reason( 'got feature count of %d, not expected %d.' \
                              % (new_count, old_count -1) )

    if not gdaltest.mem_lyr.TestCapability( ogr.OLCRandomRead ):
        gdaltest.post_reason( 'OLCRandomRead capability test failed.' )
        return 'fail'

    if gdaltest.mem_lyr.GetFeature( target_fid ) is not None:
        gdaltest.post_reason( 'Got deleted feature!' )
        return 'fail'

    if gdaltest.mem_lyr.GetFeature( -1 ) is not None:
        gdaltest.post_reason( 'GetFeature() should have failed' )
        return 'fail'

    if gdaltest.mem_lyr.GetFeature( 1000 ) is not None:
        gdaltest.post_reason( 'GetFeature() should have failed' )
        return 'fail'

    return 'success'
    
###############################################################################
# Test GetDriver() / name bug (#1674)
#
# Mostly we are verifying that this doesn't still cause a crash. 

def ogr_mem_10():

    d = ogr.GetDriverByName( 'Memory' )
    ds =  d.CreateDataSource('xxxxxx')

    try:
        d2 = ds.GetDriver()
    except:
        d2 = None

    if d2 is None or d2.GetName() != 'Memory':
        gdaltest.post_reason( 'Did not get expected driver name.' )
        return 'fail'

    return 'success'
    
###############################################################################
# Verify that we can delete layers properly

def ogr_mem_11():
    
    if gdaltest.mem_ds.TestCapability( 'DeleteLayer' ) == 0:
        gdaltest.post_reason ('Deletelayer TestCapability failed.' )
        return 'fail'

    gdaltest.mem_ds.CreateLayer( 'extra' )
    gdaltest.mem_ds.CreateLayer( 'extra2' )
    layer_count = gdaltest.mem_ds.GetLayerCount()

    gdaltest.mem_lyr = None
    # Delete extra layer
    if gdaltest.mem_ds.DeleteLayer(layer_count - 2)  != 0:
        gdaltest.post_reason( 'DeleteLayer() failed' )
        return 'fail'

    if gdaltest.mem_ds.DeleteLayer(-1)  == 0:
        gdaltest.post_reason( 'DeleteLayer() should have failed' )
        return 'fail'

    if gdaltest.mem_ds.DeleteLayer(gdaltest.mem_ds.GetLayerCount())  == 0:
        gdaltest.post_reason( 'DeleteLayer() should have failed' )
        return 'fail'

    if gdaltest.mem_ds.GetLayer(-1) is not None:
        gdaltest.post_reason( 'GetLayer() should have failed' )
        return 'fail'

    if gdaltest.mem_ds.GetLayer(gdaltest.mem_ds.GetLayerCount()) is not None:
        gdaltest.post_reason( 'GetLayer() should have failed' )
        return 'fail'

    lyr = gdaltest.mem_ds.GetLayer(gdaltest.mem_ds.GetLayerCount() - 1)

    if lyr.GetName() != 'extra2':
        gdaltest.post_reason( 'delete layer seems iffy' )
        return 'failure'

    return 'success'
    
###############################################################################
# Test some date handling
def ogr_mem_12():

    if gdaltest.mem_ds is None:
        return 'skip'

    #######################################################
    # Create memory Layer
    lyr = gdaltest.mem_ds.GetLayerByName('tpoly')
    if lyr is None:
        return 'fail'
    
    # Set the date of the first feature
    f = lyr.GetFeature(1)
    try:
        # Old-gen bindings don't accept this form of SetField
        f.SetField("WHEN", 2008, 3, 19, 16, 15, 00, 0)
    except:
        return 'skip'
    lyr.SetFeature(f)
    f = lyr.GetFeature(1)
    idx = f.GetFieldIndex('WHEN')
    print(f.GetFieldAsDateTime(idx))
    return 'success'

###############################################################################
# Test Get/Set on StringList, IntegerList, RealList

def ogr_mem_13():

    if gdaltest.mem_ds is None:
        return 'skip'

    lyr = gdaltest.mem_ds.CreateLayer( 'listlayer' )
    field_defn = ogr.FieldDefn( 'stringlist', ogr.OFTStringList )
    lyr.CreateField( field_defn )
    field_defn = ogr.FieldDefn( 'intlist', ogr.OFTIntegerList )
    lyr.CreateField( field_defn )
    field_defn = ogr.FieldDefn( 'reallist', ogr.OFTRealList )
    lyr.CreateField( field_defn )
    feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )

    try:
        feat.SetFieldStringList
    except:
        # OG python bindings
        return 'skip'

    feat.SetFieldStringList(0, ['a', 'b'])
    if feat.GetFieldAsStringList(0) != ['a', 'b']:
        print(feat.GetFieldAsStringList(0))
        return 'fail'

    feat.SetFieldIntegerList(1, [2, 3])
    if feat.GetFieldAsIntegerList(1) != [2, 3]:
        print(feat.GetFieldAsIntegerList(1))
        return 'fail'

    feat.SetFieldDoubleList(2, [4., 5.])
    if feat.GetFieldAsDoubleList(2) != [4., 5.]:
        print(feat.GetFieldAsDoubleList(2))
        return 'fail'

    return 'success'

###############################################################################
# Test SetNextByIndex

def ogr_mem_14():

    if gdaltest.mem_ds is None:
        return 'skip'

    lyr = gdaltest.mem_ds.CreateLayer('SetNextByIndex')
    field_defn = ogr.FieldDefn( 'foo', ogr.OFTString )
    lyr.CreateField(field_defn)
    feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    feat.SetField(0, 'first feature')
    lyr.CreateFeature(feat)
    feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    feat.SetField(0, 'second feature')
    lyr.CreateFeature(feat)
    feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    feat.SetField(0, 'third feature')
    lyr.CreateFeature(feat)

    if not lyr.TestCapability( ogr.OLCFastSetNextByIndex ):
        gdaltest.post_reason( 'OLCFastSetNextByIndex capability test failed.' )
        return 'fail'

    if lyr.SetNextByIndex(1) != 0:
        gdaltest.post_reason('SetNextByIndex() failed')
        return 'fail'
    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString(0) != 'second feature':
        gdaltest.post_reason('did not get expected feature')
        return 'fail'

    if lyr.SetNextByIndex(-1) == 0:
        gdaltest.post_reason('SetNextByIndex() should have failed')
        return 'fail'

    if lyr.SetNextByIndex(100) == 0:
        gdaltest.post_reason('SetNextByIndex() should have failed')
        return 'fail'

    lyr.SetAttributeFilter("foo != 'second feature'")

    if lyr.TestCapability( ogr.OLCFastSetNextByIndex ):
        gdaltest.post_reason( 'OLCFastSetNextByIndex capability test should have failed.' )
        return 'fail'

    if lyr.SetNextByIndex(1) != 0:
        gdaltest.post_reason('SetNextByIndex() failed')
        return 'fail'
    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString(0) != 'third feature':
        gdaltest.post_reason('did not get expected feature')
        return 'fail'

    return 'success'

###############################################################################
# Test non-linear geometries

def ogr_mem_15():

    lyr = gdaltest.mem_ds.CreateLayer('wkbCircularString', geom_type = ogr.wkbCircularString)
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt('CIRCULARSTRING(0 0,1 0,0 0)'))
    lyr.CreateFeature(f)
    f = None

    if lyr.GetGeomType() != ogr.wkbCircularString:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetGeomType() != ogr.wkbCircularString:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetGeomFieldDefn(0).GetType() != ogr.wkbCircularString:
        gdaltest.post_reason('fail')
        return 'fail'
    f = lyr.GetNextFeature()
    g = f.GetGeometryRef()
    if g.GetGeometryType() != ogr.wkbCircularString:
        gdaltest.post_reason('fail')
        return 'fail'

    # Test SetNonLinearGeometriesEnabledFlag(False)
    old_val = ogr.GetNonLinearGeometriesEnabledFlag()
    ogr.SetNonLinearGeometriesEnabledFlag(False)
    if lyr.GetGeomType() != ogr.wkbLineString:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetGeomType() != ogr.wkbLineString:
        gdaltest.post_reason('fail')
        return 'fail'
    if lyr.GetLayerDefn().GetGeomFieldDefn(0).GetType() != ogr.wkbLineString:
        gdaltest.post_reason('fail')
        return 'fail'
    lyr.ResetReading()
    f = lyr.GetNextFeature()
    g = f.GetGeometryRef()
    if g.GetGeometryType() != ogr.wkbLineString:
        gdaltest.post_reason('fail')
        return 'fail'

    lyr.ResetReading()
    f = lyr.GetNextFeature()
    g = f.GetGeomFieldRef(0)
    if g.GetGeometryType() != ogr.wkbLineString:
        gdaltest.post_reason('fail')
        return 'fail'
    ogr.SetNonLinearGeometriesEnabledFlag(True)

    return 'success'

def ogr_mem_cleanup():

    if gdaltest.mem_ds is None:
        return 'skip'

    ogr.SetNonLinearGeometriesEnabledFlag(True)
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
    ogr_mem_10,
    ogr_mem_11,
    ogr_mem_12,
    ogr_mem_13,
    ogr_mem_14,
    ogr_mem_15,
    ogr_mem_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_mem' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()


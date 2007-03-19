#!/usr/bin/env python
###############################################################################
# $Id: ogr_shape.py,v 1.15 2006/06/21 20:41:42 fwarmerdam Exp $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Shapefile driver testing.
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
# Open Shapefile 

def ogr_shape_1():

    shape_drv = ogr.GetDriverByName('ESRI Shapefile')

    shape_drv = ogr.GetDriverByName('ESRI Shapefile')
    shape_drv.DeleteDataSource( 'tmp' )
    
    gdaltest.shape_ds = shape_drv.CreateDataSource( 'tmp' )

    if gdaltest.shape_ds is not None:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Create table from data/poly.shp

def ogr_shape_2():

    if gdaltest.shape_ds is None:
        return 'skip'

    #######################################################
    # Create memory Layer
    gdaltest.shape_lyr = gdaltest.shape_ds.CreateLayer( 'tpoly' )

    #######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def( gdaltest.shape_lyr,
                                    [ ('AREA', ogr.OFTReal),
                                      ('EAS_ID', ogr.OFTInteger),
                                      ('PRFEDEA', ogr.OFTString) ] )
    
    #######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature( feature_def = gdaltest.shape_lyr.GetLayerDefn() )

    shp_ds = ogr.Open( 'data/poly.shp' )
    gdaltest.shp_ds = shp_ds
    shp_lyr = shp_ds.GetLayer(0)
    
    feat = shp_lyr.GetNextFeature()
    gdaltest.poly_feat = []
    
    while feat is not None:

        gdaltest.poly_feat.append( feat )

        dst_feat.SetFrom( feat )
        gdaltest.shape_lyr.CreateFeature( dst_feat )

        feat = shp_lyr.GetNextFeature()

    dst_feat.Destroy()
        
    return 'success'

###############################################################################
# Verify that stuff we just wrote is still OK.

def ogr_shape_3():
    if gdaltest.shape_ds is None:
        return 'skip'

    expect = [168, 169, 166, 158, 165]
    
    gdaltest.shape_lyr.SetAttributeFilter( 'eas_id < 170' )
    tr = ogrtest.check_features_against_list( gdaltest.shape_lyr,
                                              'eas_id', expect )
    gdaltest.shape_lyr.SetAttributeFilter( None )

    for i in range(len(gdaltest.poly_feat)):
        orig_feat = gdaltest.poly_feat[i]
        read_feat = gdaltest.shape_lyr.GetNextFeature()

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
# Write a feature without a geometry, and verify that it works OK.

def ogr_shape_4():

    if gdaltest.shape_ds is None:
        return 'skip'

    ######################################################################
    # Create feature without geometry.
    
    dst_feat = ogr.Feature( feature_def = gdaltest.shape_lyr.GetLayerDefn() )
    dst_feat.SetField( 'PRFEDEA', 'nulled' )
    gdaltest.shape_lyr.CreateFeature( dst_feat )
        
    ######################################################################
    # Read back the feature and get the geometry.
    
    gdaltest.shape_lyr.SetAttributeFilter( "PRFEDEA = 'nulled'" )
    feat_read = gdaltest.shape_lyr.GetNextFeature()
    if feat_read is None:
        gdaltest.post_reason( 'Didnt get feature with null geometry back.' )
        return 'fail'

    if feat_read.GetGeometryRef() is not None:
        print feat_read.GetGeometryRef()
        print feat_read.GetGeometryRef().ExportToWkt()
        gdaltest.post_reason( 'Didnt get null geometry as expected.' )
        return 'fail'
        
    feat_read.Destroy()
    dst_feat.Destroy()
    
    return 'success'
    
###############################################################################
# Test ExecuteSQL() results layers without geometry.

def ogr_shape_5():

    if gdaltest.shape_ds is None:
        return 'skip'

    expect = [ 179, 173, 172, 171, 170, 169, 168, 166, 165, 158, 0 ]
    
    sql_lyr = gdaltest.shape_ds.ExecuteSQL( 'select distinct eas_id from tpoly order by eas_id desc' )

    tr = ogrtest.check_features_against_list( sql_lyr, 'eas_id', expect )

    gdaltest.shape_ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'
    
###############################################################################
# Test ExecuteSQL() results layers with geometry.

def ogr_shape_6():

    if gdaltest.shape_ds is None:
        return 'skip'

    sql_lyr = gdaltest.shape_ds.ExecuteSQL( \
        'select * from tpoly where prfedea = "35043413"' )

    tr = ogrtest.check_features_against_list( sql_lyr, 'prfedea', [ '35043413' ] )
    if tr:
        sql_lyr.ResetReading()
        feat_read = sql_lyr.GetNextFeature()
        if ogrtest.check_feature_geometry( feat_read, 'POLYGON ((479750.688 4764702.000,479658.594 4764670.000,479640.094 4764721.000,479735.906 4764752.000,479750.688 4764702.000))', max_error = 0.001 ) != 0:
            tr = 0
        feat_read.Destroy()
        
    gdaltest.shape_ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'
    
###############################################################################
# Test spatial filtering. 

def ogr_shape_7():

    if gdaltest.shape_ds is None:
        return 'skip'

    gdaltest.shape_lyr.SetAttributeFilter( None )
    
    geom = ogr.CreateGeometryFromWkt( \
        'LINESTRING(479505 4763195,480526 4762819)' )
    gdaltest.shape_lyr.SetSpatialFilter( geom )
    geom.Destroy()

    tr = ogrtest.check_features_against_list( gdaltest.shape_lyr, 'eas_id',
                                              [ 158, None ] )

    gdaltest.shape_lyr.SetSpatialFilter( None )
    
    if tr:
        return 'success'
    else:
        return 'fail'
    
###############################################################################
# Create spatial index, and verify we get the same results.

def ogr_shape_8():

    if gdaltest.shape_ds is None:
        return 'skip'

    gdaltest.shape_lyr.SetAttributeFilter( None )
    gdaltest.shape_ds.ExecuteSQL( 'CREATE SPATIAL INDEX ON tpoly' )

    if not os.access( 'tmp/tpoly.qix', os.F_OK ):
        gdaltest.post_reason( 'tpoly.qix not created' )
        return 'fail'
    
    geom = ogr.CreateGeometryFromWkt( \
        'LINESTRING(479505 4763195,480526 4762819)' )
    gdaltest.shape_lyr.SetSpatialFilter( geom )
    geom.Destroy()
    
    tr = ogrtest.check_features_against_list( gdaltest.shape_lyr, 'eas_id',
                                              [ 158, None ] )

    gdaltest.shape_lyr.SetSpatialFilter( None )

    if not tr:
        return 'fail'

    gdaltest.shape_ds.ExecuteSQL( 'DROP SPATIAL INDEX ON tpoly' )

    if os.access( 'tmp/tpoly.qix', os.F_OK ):
        gdaltest.post_reason( 'tpoly.qix not deleted' )
        return 'fail'

    return 'success'
    
###############################################################################
# Test that we don't return a polygon if we are "inside" but non-overlapping.
# For now we actually do return this shape, but eventually we won't. 

def ogr_shape_9():

    if gdaltest.shape_ds is None:
        return 'skip'

    gdaltest.shape_ds.Destroy()
    gdaltest.shape_ds = ogr.Open( 'data/testpoly.shp' )
    gdaltest.shape_lyr = gdaltest.shape_ds.GetLayer(0)

    gdaltest.shape_lyr.SetSpatialFilterRect( -10, -130, 10, -110 )
    
    tr = ogrtest.check_features_against_list( gdaltest.shape_lyr, 'FID',
                                              [ 13 ] )

    if tr:
        return 'success'
    else:
        return 'fail'
    
###############################################################################
# Do a fair size query that should pull in a few shapes. 

def ogr_shape_10():

    if gdaltest.shape_ds is None:
        return 'skip'

    gdaltest.shape_lyr.SetSpatialFilterRect( -400, 22, -120, 400 )
    
    tr = ogrtest.check_features_against_list( gdaltest.shape_lyr, 'FID',
                                              [ 0, 4, 8 ] )

    if tr:
        return 'success'
    else:
        return 'fail'
    
###############################################################################
# Do a mixed indexed attribute and spatial query.

def ogr_shape_11():

    if gdaltest.shape_ds is None:
        return 'skip'

    gdaltest.shape_lyr.SetAttributeFilter( 'FID = 5' )
    gdaltest.shape_lyr.SetSpatialFilterRect( -400, 22, -120, 400 )
    
    tr = ogrtest.check_features_against_list( gdaltest.shape_lyr, 'FID',
                                              [] )

    if not tr:
        return 'fail'

    gdaltest.shape_lyr.SetAttributeFilter( 'FID = 4' )
    gdaltest.shape_lyr.SetSpatialFilterRect( -400, 22, -120, 400 )
    
    tr = ogrtest.check_features_against_list( gdaltest.shape_lyr, 'FID',
                                              [ 4 ] )

    gdaltest.shape_lyr.SetAttributeFilter( None )
    gdaltest.shape_lyr.SetSpatialFilter( None )

    if tr:
        return 'success'
    else:
        return 'fail'
    
###############################################################################
# Check that multipolygon of asm.shp is properly returned.

def ogr_shape_12():

    if gdaltest.shape_ds is None:
        return 'skip'

    asm_ds = ogr.Open( 'data/asm.shp' )
    asm_lyr = asm_ds.GetLayer(0)

    feat = asm_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()

    if geom.GetCoordinateDimension() != 2:
        gdaltest.post_reason( 'dimension wrong.' )
        return 'fail'

    if geom.GetGeometryName() != 'MULTIPOLYGON':
        gdaltest.post_reason( 'Geometry of wrong type.' )
        return 'fail'

    if geom.GetGeometryCount() != 5:
        gdaltest.post_reason( 'Did not get the expected number of polygons.')
        return 'fail'

    counts = [15, 11, 17, 20, 9]
    for i in range(5):
        poly = geom.GetGeometryRef( i )
        if poly.GetGeometryName() != 'POLYGON':
            gdaltest.post_reason( 'Did not get right type for polygons' )
            return 'fail'

        if poly.GetGeometryCount() != 1:
            gdaltest.post_reason( 'polygon with more than one ring.' )
            return 'fail'

        pnt_count = poly.GetGeometryRef(0).GetPointCount()
        if pnt_count != counts[i]:
            gdaltest.post_reason( ('Polygon %d has %d points instead of %d.' %
                                   (i, pnt_count, counts[i]) ) )
            return 'fail'

    asm_ds.Destroy()

    return 'success'
    
###############################################################################
# Perform a SetFeature() on a couple features, resetting the size.

def ogr_shape_13():

    if gdaltest.shape_ds is None:
        return 'skip'

    gdaltest.shape_ds.Destroy()
    gdaltest.shape_ds = ogr.Open( 'tmp/tpoly.shp', update=1 )
    gdaltest.shape_lyr = gdaltest.shape_ds.GetLayer(0)

    ######################################################################
    # Update FID 9 (EAS_ID=170), making the polygon larger. 

    feat = gdaltest.shape_lyr.GetFeature( 9 )
    feat.SetField( 'AREA', '6000.00' )

    geom = ogr.CreateGeometryFromWkt( \
        'POLYGON ((0 0, 0 60, 100 60, 100 0, 200 30, 0 0))')
    feat.SetGeometry( geom )

    if gdaltest.shape_lyr.SetFeature( feat ) != 0:
        gdaltest.post_reason( 'SetFeature() failed.' )
        return 'fail'

    ######################################################################
    # Update FID 8 (EAS_ID=165), making the polygon smaller.

    feat = gdaltest.shape_lyr.GetFeature( 8 )
    feat.SetField( 'AREA', '7000.00' )

    geom = ogr.CreateGeometryFromWkt( \
        'POLYGON ((0 0, 0 60, 100 60, 100 0, 0 0))')
    feat.SetGeometry( geom )

    if gdaltest.shape_lyr.SetFeature( feat ) != 0:
        gdaltest.post_reason( 'SetFeature() failed.' )
        return 'fail'

    return 'success'
    
###############################################################################
# Verify last changes.

def ogr_shape_14():

    if gdaltest.shape_ds is None:
        return 'skip'

    gdaltest.shape_ds.Destroy()
    gdaltest.shape_ds = ogr.Open( 'tmp/tpoly.shp', update=1 )
    gdaltest.shape_lyr = gdaltest.shape_ds.GetLayer(0)

    ######################################################################
    # Check FID 9.

    feat = gdaltest.shape_lyr.GetFeature( 9 )

    if feat.GetField( 'AREA' ) != 6000.0:
        gdaltest.post_reason( 'AREA update failed, FID 9.' )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'POLYGON ((0 0, 0 60, 100 60, 100 0, 200 30, 0 0))') != 0:
        gdaltest.post_reason( 'Geometry update failed, FID 9.' )
        return 'fail'

    ######################################################################
    # Update FID 8 (EAS_ID=165), making the polygon smaller.

    feat = gdaltest.shape_lyr.GetFeature( 8 )

    if feat.GetField( 'AREA' ) != 7000.0:
        gdaltest.post_reason( 'AREA update failed, FID 8.' )
        return 'fail'

    if ogrtest.check_feature_geometry( feat, 'POLYGON ((0 0, 0 60, 100 60, 100 0, 0 0))') != 0:
        gdaltest.post_reason( 'Geometry update failed, FID 8.' )
        return 'fail'

    return 'success'
    
###############################################################################
# Delete a feature, and verify reduced count.

def ogr_shape_15():

    if gdaltest.shape_ds is None:
        return 'skip'

    ######################################################################
    # Delete FID 9.

    if gdaltest.shape_lyr.DeleteFeature( 9 ) != 0:
        gdaltest.post_reason( 'DeleteFeature failed.' )
        return 'fail'

    ######################################################################
    # Count features, verifying that none are FID 9.

    count = 0
    feat = gdaltest.shape_lyr.GetNextFeature()
    while feat is not None:
        if feat.GetFID() == 9:
            gdaltest.post_reason( 'Still an FID 9 in dataset.' )
            return 'fail'
        
        count = count+1
        feat = gdaltest.shape_lyr.GetNextFeature()

    if count is not 10:
        gdaltest.post_reason( 'Did not get expected FID count.' )
        return 'fail'
    
    return 'success'
    
###############################################################################
# Repack and verify a few things.

def ogr_shape_16():

    if gdaltest.shape_ds is None:
        return 'skip'

    gdaltest.shape_ds.ExecuteSQL( 'REPACK tpoly' )

    ######################################################################
    # Count features.

    got_9 = 0
    count = 0
    gdaltest.shape_lyr.ResetReading()
    feat = gdaltest.shape_lyr.GetNextFeature()
    while feat is not None:
        if feat.GetFID() == 9:
            got_9 = 1
        
        count = count+1
        feat = gdaltest.shape_lyr.GetNextFeature()

    if count is not 10:
        gdaltest.post_reason( 'Did not get expected FID count.' )
        return 'fail'

    if got_9 == 0:
        gdaltest.post_reason( 'Did not get FID 9 as expected.' )
        return 'fail'

    feat = gdaltest.shape_lyr.GetFeature( 9 )
    
    return 'success'
    
###############################################################################
# Simple test with point shapefile with no associated .dbf

def ogr_shape_17():

    if gdaltest.shape_ds is None:
        return 'skip'

    shp_ds = ogr.Open( 'data/can_caps.shp' )
    shp_lyr = shp_ds.GetLayer(0)

    if shp_lyr.GetLayerDefn().GetFieldCount() != 0:
        gdaltest.post_reason( 'Unexpectedly got attribute fields.' )
        return 'fail'

    count = 0
    while 1:
        feat = shp_lyr.GetNextFeature()
        if feat is None:
            break

        count = count + 1
        feat.Destroy()

    if count != 13:
        gdaltest.post_reason( 'Got wrong number of features.' )
        return 'fail'

    shp_lyr = None
    shp_ds.Destroy()
    shp_ds = None

    return 'success'

###############################################################################
# Test reading data/poly.PRJ file with mixed-case file name

def ogr_shape_18():

    shp_ds = ogr.Open( 'data/poly.shp' )
    shp_lyr = shp_ds.GetLayer(0)

    srs_lyr = shp_lyr.GetSpatialRef()

    if srs_lyr is None:
        gdaltest.post_reason( 'Missing projection definition.' )
        return 'fail'

    # data/poly.shp has arbitraily assigned EPSG:27700
    srs = osr.SpatialReference()
    srs.ImportFromEPSG( 27700 )

    if not srs_lyr.IsSame(srs):
        print
        print 'expected = ', srs.ExportToPrettyWkt()
        print 'existing = ', srs_lyr.ExportToPrettyWkt()
        gdaltest.post_reason( 'Projections differ' )
        return 'fail'

    return 'success'

###############################################################################
# 

def ogr_shape_cleanup():

    if gdaltest.shape_ds is None:
        return 'skip'

    gdaltest.shape_ds.Destroy()
    gdaltest.shape_ds = None

    shape_drv = ogr.GetDriverByName('ESRI Shapefile')
    shape_drv.DeleteDataSource( 'tmp' )
    
    return 'success'

gdaltest_list = [ 
    ogr_shape_1,
    ogr_shape_2,
    ogr_shape_3,
    ogr_shape_4,
    ogr_shape_5,
    ogr_shape_6,
    ogr_shape_7,
    ogr_shape_8,
    ogr_shape_9,
    ogr_shape_10,
    ogr_shape_11,
    ogr_shape_12,
    ogr_shape_13,
    ogr_shape_14,
    ogr_shape_15,
    ogr_shape_16,
    ogr_shape_17,
    ogr_shape_18,
    ogr_shape_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_shape' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()


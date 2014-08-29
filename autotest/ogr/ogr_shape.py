#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Shapefile driver testing.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2008-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
import shutil
import sys
import string

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import ogr, osr, gdal

###############################################################################
# Open Shapefile 

def ogr_shape_1():

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
    shp_ds.Destroy()
        
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
        print(feat_read.GetGeometryRef())
        print(feat_read.GetGeometryRef().ExportToWkt())
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

    expect = [ 179, 173, 172, 171, 170, 169, 168, 166, 165, 158, None ]
    
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

    # Test recreating while already existing
    gdaltest.shape_ds.ExecuteSQL( 'CREATE SPATIAL INDEX ON tpoly' )

    gdaltest.shape_ds.ExecuteSQL( 'DROP SPATIAL INDEX ON tpoly' )

    if os.access( 'tmp/tpoly.qix', os.F_OK ):
        gdaltest.post_reason( 'tpoly.qix not deleted' )
        return 'fail'

    return 'success'
    
###############################################################################
# Test that we don't return a polygon if we are "inside" but non-overlapping.

def ogr_shape_9():

    if gdaltest.shape_ds is None:
        return 'skip'

    gdaltest.shape_ds.Destroy()
    gdaltest.shape_ds = ogr.Open( 'data/testpoly.shp' )
    gdaltest.shape_lyr = gdaltest.shape_ds.GetLayer(0)

    gdaltest.shape_lyr.SetSpatialFilterRect( -10, -130, 10, -110 )

    if ogrtest.have_geos() and gdaltest.shape_lyr.GetFeatureCount() == 0:
        return 'success'
    elif not ogrtest.have_geos() and gdaltest.shape_lyr.GetFeatureCount() == 1:
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
# Test adding a field to the schema of a populated layer.

def ogr_shape_16_1():

    if gdaltest.shape_ds is None:
        return 'skip'

    ######################################################################
    # Add a new field.
    field_defn = ogr.FieldDefn( 'NEWFLD', ogr.OFTString )
    field_defn.SetWidth( 12 )

    result = gdaltest.shape_lyr.CreateField( field_defn )

    field_defn.Destroy()

    if result != 0:
        gdaltest.post_reason( 'failed to create new field.' )
        return 'fail'

    ######################################################################
    # Check at least one feature. 

    feat = gdaltest.shape_lyr.GetFeature(8)
    if feat.EAS_ID != 165:
        gdaltest.post_reason( 'Got wrong EAS_ID' )
        return 'fail'

    if feat.IsFieldSet( 'NEWFLD' ):
        gdaltest.post_reason( 'Expected NULL NEWFLD value!' )
        return 'fail'

    feat.Destroy()
    
    return 'success'

###############################################################################
# Simple test with point shapefile with no associated .dbf

def ogr_shape_17():

    if gdaltest.shape_ds is None:
        return 'skip'

    shutil.copy( 'data/can_caps.shp', 'tmp/can_caps.shp' )
    shutil.copy( 'data/can_caps.shx', 'tmp/can_caps.shx' )

   
    shp_ds = ogr.Open( 'tmp/can_caps.shp', update = 1 )
    shp_lyr = shp_ds.GetLayer(0)

    if shp_lyr.GetLayerDefn().GetFieldCount() != 0:
        gdaltest.post_reason( 'Unexpectedly got attribute fields.' )
        return 'fail'

    count = 0
    while 1:
        feat = shp_lyr.GetNextFeature()
        if feat is None:
            break

        # Re-write feature to test that we can use SetFeature() without
        # a DBF
        shp_lyr.SetFeature(feat)

        count = count + 1
        feat.Destroy()

    if count != 13:
        gdaltest.post_reason( 'Got wrong number of features.' )
        return 'fail'

    # Create new feature without a DBF
    feat = ogr.Feature(shp_lyr.GetLayerDefn())
    shp_lyr.CreateFeature(feat)
    if feat.GetFID() != 13:
        print(feat.GetFID())
        gdaltest.post_reason( 'Got wrong FID.' )
        return 'fail'

    shp_lyr = None
    shp_ds.Destroy()
    shp_ds = None

    os.remove( 'tmp/can_caps.shp' )
    os.remove( 'tmp/can_caps.shx' )

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
    srs.StripCTParms()

    if not srs_lyr.IsSame(srs):
        print('')
        print('expected = %s' % srs.ExportToPrettyWkt())
        print('existing = %s' % srs_lyr.ExportToPrettyWkt())
        gdaltest.post_reason( 'Projections differ' )
        return 'fail'

    shp_ds.Destroy()

    return 'success'

###############################################################################
# Test polygon formation logic - recognising what rings are inner/outer
# and deciding on polygon vs. multipolygon (#1217)

def ogr_shape_19():

    ds = ogr.Open('data/Stacks.shp')
    lyr = ds.GetLayer(0)
    lyr.ResetReading()
    feat = lyr.GetNextFeature()

    wkt = 'MULTIPOLYGON (((3115478.809630727861077 13939288.008583962917328,3134266.47213465673849 13971973.394036004319787,3176989.101938112173229 13957303.575368551537395,3198607.7820796193555 13921787.172278933227062,3169010.779504936654121 13891675.439224690198898,3120368.749186545144767 13897852.204979406669736,3115478.809630727861077 13939288.008583962917328),(3130405.993537959177047 13935427.529987264424562,3135038.567853996530175 13902742.144535223022103,3167209.22282647760585 13902227.414055664092302,3184452.693891727831215 13922559.267998272553086,3172871.258101634215564 13947781.061496697366238,3144561.081725850701332 13957818.305848112329841,3130405.993537959177047 13935427.529987264424562)),((3143016.890287171583623 13932596.512349685654044,3152282.038919246289879 13947266.331017138436437,3166179.761867358349264 13940060.104303302243352,3172099.162382294889539 13928221.303273428231478,3169268.144744716584682 13916897.23272311501205,3158201.439434182830155 13911235.197447959333658,3144818.446965630631894 13911749.927927518263459,3139928.507409813348204 13916382.502243556082249,3143016.890287171583623 13932596.512349685654044),(3149193.65604188805446 13926677.11183474957943,3150737.84748056717217 13918698.789401574060321,3158458.804673962760717 13919728.250360693782568,3164892.935668459162116 13923331.36371761187911,3163863.474709339439869 13928736.033752989023924,3157171.978475063573569 13935427.529987264424562,3149193.65604188805446 13926677.11183474957943)))'
        
    if ogrtest.check_feature_geometry(feat, wkt, 
                                      max_error = 0.00000001 ) != 0:
        return 'fail'

    feat.Destroy()
    lyr = None
    ds.Destroy()
    
    return 'success'

###############################################################################
# Test empty multipoint, multiline, multipolygon.
# From GDAL 1.6.0, the expected behaviour is to return a feature with a NULL geometry

def ogr_shape_20():

    if gdaltest.shape_ds is None:
        return 'skip'

    ds = ogr.Open('data/emptymultipoint.shp')
    lyr = ds.GetLayer(0)
    lyr.ResetReading()
    feat = lyr.GetNextFeature()

    if feat is None:
        return 'fail'
    if feat.GetGeometryRef() is not None:
        return 'fail'

    feat.Destroy()
    lyr = None
    ds.Destroy()


    ds = ogr.Open('data/emptymultiline.shp')
    lyr = ds.GetLayer(0)
    lyr.ResetReading()
    feat = lyr.GetNextFeature()

    if feat is None:
        return 'fail'
    if feat.GetGeometryRef() is not None:
        return 'fail'

    feat.Destroy()
    lyr = None
    ds.Destroy()


    ds = ogr.Open('data/emptymultipoly.shp')
    lyr = ds.GetLayer(0)
    lyr.ResetReading()
    feat = lyr.GetNextFeature()

    if feat is None:
        return 'fail'
    if feat.GetGeometryRef() is not None:
        return 'fail'

    feat.Destroy()
    lyr = None
    ds.Destroy()

    return 'success'

###############################################################################
# Test robutness towards broken/unfriendly shapefiles

def ogr_shape_21():

    if gdaltest.shape_ds is None:
        return 'skip'


    files = [ 'data/buggypoint.shp',
              'data/buggymultipoint.shp',
              'data/buggymultiline.shp',
              'data/buggymultipoly.shp',
              'data/buggymultipoly2.shp' ]
    for file in files:

        ds = ogr.Open(file)
        lyr = ds.GetLayer(0)
        lyr.ResetReading()
        gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
        feat = lyr.GetNextFeature()
        gdal.PopErrorHandler()

        if feat.GetGeometryRef() is not None:
            return 'fail'

        feat.Destroy()

        # Test fix for #3665
        lyr.ResetReading()
        (minx, maxx, miny, maxy) = lyr.GetExtent()
        lyr.SetSpatialFilterRect(minx+1e-9,miny+1e-9,maxx-1e-9,maxy-1e-9)
        gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
        feat = lyr.GetNextFeature()
        gdal.PopErrorHandler()

        if feat.GetGeometryRef() is not None:
            return 'fail'

        feat.Destroy()

        lyr = None
        ds.Destroy()

    return 'success'


###############################################################################
# Test writing and reading all handled data types

def ogr_shape_22():

    if gdaltest.shape_ds is None:
        return 'skip'

    #######################################################
    # Create memory Layer
    gdaltest.shape_ds.Destroy()
    gdaltest.shape_ds = ogr.GetDriverByName('ESRI Shapefile').Open('tmp', update = 1)
    gdaltest.shape_lyr = gdaltest.shape_ds.CreateLayer( 'datatypes' )

    #######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def( gdaltest.shape_lyr,
                                    [ ('REAL', ogr.OFTReal),
                                      ('INTEGER', ogr.OFTInteger),
                                      ('STRING', ogr.OFTString),
                                      ('DATE', ogr.OFTDate) ] )

    #######################################################
    # Create a feature
    dst_feat = ogr.Feature( feature_def = gdaltest.shape_lyr.GetLayerDefn() )
    dst_feat.SetField( 'REAL', 1.2 )
    dst_feat.SetField( 'INTEGER', 3 )
    dst_feat.SetField( 'STRING', 'aString' )
    dst_feat.SetField( 'DATE', '2005/10/12' )
    gdaltest.shape_lyr.CreateFeature( dst_feat )
    dst_feat.Destroy()

    gdaltest.shape_ds.Destroy()

    #######################################################
    # Read back the feature
    gdaltest.shape_ds = ogr.GetDriverByName('ESRI Shapefile').Open('tmp', update = 1)
    gdaltest.shape_lyr = gdaltest.shape_ds.GetLayerByName( 'datatypes' )
    feat_read = gdaltest.shape_lyr.GetNextFeature()
    if feat_read.GetField('REAL') != 1.2 or \
       feat_read.GetField('INTEGER') != 3 or \
       feat_read.GetField('STRING') != 'aString' or \
       feat_read.GetFieldAsString('DATE') != '2005/10/12':
        return 'fail'

    return 'success'


###############################################################################
# Function used internaly by ogr_shape_23

def ogr_shape_23_write_valid_and_invalid(layer_name, wkt, invalid_wkt, wkbType, isEmpty):

    #######################################################
    # Create a layer
    if wkbType == ogr.wkbUnknown:
        gdaltest.shape_lyr = gdaltest.shape_ds.CreateLayer( layer_name )
    else:
        gdaltest.shape_lyr = gdaltest.shape_ds.CreateLayer( layer_name, geom_type = wkbType )

    #######################################################
    # Write a geometry
    dst_feat = ogr.Feature( feature_def = gdaltest.shape_lyr.GetLayerDefn() )
    dst_feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt(wkt))
    gdaltest.shape_lyr.CreateFeature( dst_feat )
    dst_feat.Destroy()

    #######################################################
    # Write an invalid geometry for this layer type
    dst_feat = ogr.Feature( feature_def = gdaltest.shape_lyr.GetLayerDefn() )
    dst_feat.SetGeometryDirectly(ogr.CreateGeometryFromWkt(invalid_wkt))
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    gdaltest.shape_lyr.CreateFeature( dst_feat )
    gdal.PopErrorHandler()
    dst_feat.Destroy()


    #######################################################
    # Check feature

    gdaltest.shape_ds.Destroy()
    gdaltest.shape_ds = ogr.GetDriverByName('ESRI Shapefile').Open( 'tmp', update = 1 )

    read_lyr = gdaltest.shape_ds.GetLayerByName( layer_name )
    if read_lyr.GetFeatureCount() != 1:
        return 'fail'
    feat_read = read_lyr.GetNextFeature()

    if isEmpty and feat_read.GetGeometryRef() == None:
        return 'success'

    if ogrtest.check_feature_geometry(feat_read,ogr.CreateGeometryFromWkt(wkt),
                                max_error = 0.000000001 ) != 0:
        print(feat_read.GetGeometryRef().ExportToWkt())
        return 'fail'

    return 'success'


def ogr_shape_23_write_geom(layer_name, geom, expected_geom, wkbType):

    #######################################################
    # Create a layer
    if wkbType == ogr.wkbUnknown:
        gdaltest.shape_lyr = gdaltest.shape_ds.CreateLayer( layer_name )
    else:
        gdaltest.shape_lyr = gdaltest.shape_ds.CreateLayer( layer_name, geom_type = wkbType )

    #######################################################
    # Write a geometry
    dst_feat = ogr.Feature( feature_def = gdaltest.shape_lyr.GetLayerDefn() )
    dst_feat.SetGeometry(geom)
    gdaltest.shape_lyr.CreateFeature( dst_feat )
    dst_feat.Destroy()

    #######################################################
    # Check feature

    gdaltest.shape_lyr = None
    gdaltest.shape_ds.Destroy()
    gdaltest.shape_ds = ogr.GetDriverByName('ESRI Shapefile').Open( 'tmp', update = 1 )

    read_lyr = gdaltest.shape_ds.GetLayerByName( layer_name )
    if read_lyr.GetFeatureCount() != 1:
        return 'fail'
    feat_read = read_lyr.GetNextFeature()

    if expected_geom is None:
        if feat_read.GetGeometryRef() is not None:
            print(feat_read.GetGeometryRef().ExportToWkt())
            return 'fail'
        else:
            return 'success'

    if ogrtest.check_feature_geometry(feat_read,expected_geom,
                                max_error = 0.000000001 ) != 0:
        print(feat_read.GetGeometryRef().ExportToWkt())
        return 'fail'

    return 'success'


###############################################################################
# Test writing and reading all handled geometry types

def ogr_shape_23():

    if gdaltest.shape_ds is None:
        return 'skip'

    test_geom_array = [
        ('points', 'POINT(0 1)', 'LINESTRING(0 1)', ogr.wkbPoint),
        ('points25D', 'POINT(0 1 2)', 'LINESTRING(0 1)', ogr.wkbPoint25D),
        ('multipoints', 'MULTIPOINT(0 1,2 3)', 'POINT (0 1)', ogr.wkbMultiPoint),
        ('multipoints25D', 'MULTIPOINT(0 1 2,3 4 5)', 'POINT (0 1)', ogr.wkbMultiPoint25D),
        ('linestrings', 'LINESTRING(0 1,2 3,4 5,0 1)', 'POINT (0 1)', ogr.wkbLineString),
        ('linestrings25D', 'LINESTRING(0 1 2,3 4 5,6 7 8,0 1 2)', 'POINT (0 1)', ogr.wkbLineString25D),
        ('multilinestrings', 'MULTILINESTRING((0 1,2 3,4 5,0 1), (0 1,2 3,4 5,0 1))', 'POINT (0 1)', ogr.wkbMultiLineString),
        ('multilinestrings25D', 'MULTILINESTRING((0 1 2,3 4 5,6 7 8,0 1 2),(0 1 2,3 4 5,6 7 8,0 1 2))', 'POINT (0 1)', ogr.wkbMultiLineString25D),
        ('polygons', 'POLYGON((0 0,0 10,10 10,0 0),(0.25 0.5,1 1,0.5 1,0.25 0.5))', 'POINT (0 1)', ogr.wkbPolygon),
        ('polygons25D', 'POLYGON((0 0 2,0 10 5,10 10 8,0 1 2))', 'POINT (0 1)', ogr.wkbPolygon25D),
        ('multipolygons', 'MULTIPOLYGON(((0 0,0 10,10 10,0 0),(0.25 0.5,1 1,0.5 1,0.25 0.5)),((100 0,100 10,110 10,100 0),(100.25 0.5,100.5 1,100 1,100.25 0.5)))', 'POINT (0 1)', ogr.wkbMultiPolygon),
        ('multipolygons25D', 'MULTIPOLYGON(((0 0 0,0 10,10 10,0 0),(0.25 0.5,1 1,0.5 1,0.25 0.5)),((100 0,100 10,110 10,100 0),(100.25 0.5,100.5 1,100 1,100.25 0.5)))', 'POINT (0 1)', ogr.wkbMultiPolygon25D),
    ]

    test_empty_geom_array = [
        ('emptypoints', 'POINT EMPTY', 'LINESTRING(0 1)', ogr.wkbPoint),
        ('emptymultipoints', 'MULTIPOINT EMPTY', 'POINT(0 1)', ogr.wkbMultiPoint),
        ('emptylinestrings', 'LINESTRING EMPTY', 'POINT(0 1)', ogr.wkbLineString),
        ('emptymultilinestrings', 'MULTILINESTRING EMPTY', 'POINT(0 1)', ogr.wkbMultiLineString),
        ('emptypolygons', 'POLYGON EMPTY', 'POINT(0 1)', ogr.wkbPolygon),
        ('emptymultipolygons', 'MULTIPOLYGON EMPTY', 'POINT(0 1)', ogr.wkbMultiPolygon),
    ]

    #######################################################
    # Write a feature in a new layer (geometry type unset at layer creation)

    for item in test_geom_array:
        if ogr_shape_23_write_valid_and_invalid(item[0], item[1], item[2], ogr.wkbUnknown, 0) != 'success':
            gdaltest.post_reason( 'Test for layer %s failed' % item[0] )
            return 'fail'
    for item in test_empty_geom_array:
        if ogr_shape_23_write_valid_and_invalid(item[0], item[1], item[2], ogr.wkbUnknown, 1) != 'success':
            gdaltest.post_reason( 'Test for layer %s failed' % item[0] )
            return 'fail'

    #######################################################
    # Same test but use the wkb type when creating the layer

    gdaltest.shape_ds.Destroy()
    shape_drv = ogr.GetDriverByName('ESRI Shapefile')
    shape_drv.DeleteDataSource( 'tmp' )
    gdaltest.shape_ds = shape_drv.CreateDataSource( 'tmp' )

    for item in test_geom_array:
        if ogr_shape_23_write_valid_and_invalid(item[0], item[1], item[2], item[3], 0) != 'success':
            gdaltest.post_reason( '(2) Test for layer %s failed' % item[0] )
            return 'fail'
    for item in test_empty_geom_array:
        if ogr_shape_23_write_valid_and_invalid(item[0], item[1], item[2], item[3], 1) != 'success':
            gdaltest.post_reason( '(2) Test for layer %s failed' % item[0] )
            return 'fail'

    #######################################################
    # Test writing of a geometrycollection
    layer_name = 'geometrycollections'
    gdaltest.shape_lyr = gdaltest.shape_ds.CreateLayer( layer_name, geom_type = ogr.wkbMultiPolygon )

    # This geometry collection is not compatible with a multipolygon layer
    geom = ogr.CreateGeometryFromWkt('GEOMETRYCOLLECTION(POINT (0 0))')
    dst_feat = ogr.Feature( feature_def = gdaltest.shape_lyr.GetLayerDefn() )
    dst_feat.SetGeometry(geom)
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    gdaltest.shape_lyr.CreateFeature( dst_feat )
    gdal.PopErrorHandler()
    dst_feat.Destroy()

    # This geometry will be dealt as a multipolygon
    wkt = 'GEOMETRYCOLLECTION(POLYGON((0 0,0 10,10 10,0 0),(0.25 0.5,1 1,0.5 1,0.25 0.5)),POLYGON((100 0,100 10,110 10,100 0),(100.25 0.5,100.5 1,100 1,100.25 0.5)))'
    geom = ogr.CreateGeometryFromWkt(wkt)
    dst_feat = ogr.Feature( feature_def = gdaltest.shape_lyr.GetLayerDefn() )
    dst_feat.SetGeometry(geom)
    gdaltest.shape_lyr.CreateFeature( dst_feat )
    dst_feat.Destroy()

    gdaltest.shape_ds.Destroy()
    gdaltest.shape_ds = ogr.GetDriverByName('ESRI Shapefile').Open( 'tmp', update = 1 )

    read_lyr = gdaltest.shape_ds.GetLayerByName( layer_name )
    feat_read = read_lyr.GetNextFeature()

    if ogrtest.check_feature_geometry(feat_read,ogr.CreateGeometryFromWkt('MULTIPOLYGON(((0 0 0,0 10,10 10,0 0),(0.25 0.5,1 1,0.5 1,0.25 0.5)),((100 0,100 10,110 10,100 0),(100.25 0.5,100.5 1,100 1,100.25 0.5)))'),
                                max_error = 0.000000001 ) != 0:
        print(feat_read.GetGeometryRef().ExportToWkt())
        return 'fail'


    #######################################################
    # Test writing of a multipoint with an empty point inside
    layer_name = 'strangemultipoints'
    wkt = 'MULTIPOINT(0 1)'
    geom = ogr.CreateGeometryFromWkt(wkt)
    geom.AddGeometry(ogr.Geometry( type = ogr.wkbPoint ))

    if ogr_shape_23_write_geom(layer_name, geom, ogr.CreateGeometryFromWkt(geom.ExportToWkt()), ogr.wkbUnknown) != 'success':
        gdaltest.post_reason( 'Test for layer %s failed' % layer_name )
        return 'fail'

    #######################################################
    # Test writing of a multilinestring with an empty linestring inside
    layer_name = 'strangemultilinestrings'
    wkt = 'MULTILINESTRING((0 1,2 3,4 5,0 1), (0 1,2 3,4 5,0 1))'
    geom = ogr.CreateGeometryFromWkt(wkt)
    geom.AddGeometry(ogr.Geometry( type = ogr.wkbLineString ))

    if ogr_shape_23_write_geom(layer_name, geom, ogr.CreateGeometryFromWkt(geom.ExportToWkt()), ogr.wkbUnknown) != 'success':
        gdaltest.post_reason( 'Test for layer %s failed' % layer_name )
        return 'fail'

    #######################################################
    # Test writing of a polygon with an empty external ring
    layer_name = 'polygonwithemptyexternalring'
    geom = ogr.CreateGeometryFromWkt('POLYGON EMPTY')
    geom.AddGeometry(ogr.Geometry( type = ogr.wkbLinearRing ))
    ring = ogr.Geometry( type = ogr.wkbLinearRing )
    ring.AddPoint_2D( 0, 0)
    ring.AddPoint_2D( 10, 0)
    ring.AddPoint_2D( 10, 10)
    ring.AddPoint_2D( 0, 10)
    ring.AddPoint_2D( 0, 0)
    geom.AddGeometry(ring)

    if ogr_shape_23_write_geom(layer_name, geom, None, ogr.wkbUnknown) != 'success':
        gdaltest.post_reason( 'Test for layer %s failed' % layer_name )
        return 'fail'

    #######################################################
    # Test writing of a polygon with an empty external ring
    layer_name = 'polygonwithemptyinternalring'
    wkt = 'POLYGON((100 0,100 10,110 10,100 0))';
    geom = ogr.CreateGeometryFromWkt(wkt)
    geom.AddGeometry(ogr.Geometry( type = ogr.wkbLinearRing ))

    if ogr_shape_23_write_geom(layer_name, geom, ogr.CreateGeometryFromWkt(geom.ExportToWkt()), ogr.wkbUnknown) != 'success':
        gdaltest.post_reason( 'Test for layer %s failed' % layer_name )
        return 'fail'

    #######################################################
    # Test writing of a multipolygon with an empty polygon and a polygon with an empty external ring
    layer_name = 'strangemultipolygons'
    wkt = 'MULTIPOLYGON(((0 0,0 10,10 10,0 0)), ((100 0,100 10,110 10,100 0)))'
    geom = ogr.CreateGeometryFromWkt(wkt)
    geom.AddGeometry(ogr.Geometry( type = ogr.wkbPolygon ))
    poly = ogr.CreateGeometryFromWkt('POLYGON((100 0,100 10,110 10,100 0))');
    poly.AddGeometry(ogr.Geometry( type = ogr.wkbLinearRing ))
    geom.AddGeometry(poly)

    if ogr_shape_23_write_geom(layer_name, geom, ogr.CreateGeometryFromWkt(geom.ExportToWkt()), ogr.wkbUnknown) != 'success':
        gdaltest.post_reason( 'Test for layer %s failed' % layer_name )
        return 'fail'

    return 'success'


###############################################################################
# Test reading a polygon whose outer and the inner ring touches at one point (#2589)

def ogr_shape_24():

    if gdaltest.shape_ds is None:
        return 'skip'

    layer_name = 'touchingrings'
    wkt = 'MULTIPOLYGON(((0 0,0 10,10 10,0 0), (0 0,1 1,0 1,0 0)), ((100 100,100 200,200 200,200 100,100 100)))'
    geom = ogr.CreateGeometryFromWkt(wkt)

    if ogr_shape_23_write_geom(layer_name, geom, ogr.CreateGeometryFromWkt(geom.ExportToWkt()), ogr.wkbUnknown) != 'success':
        gdaltest.post_reason( 'Test for layer %s failed' % layer_name )
        return 'fail'

    return 'success'

###############################################################################
# Test reading a multipolygon with one part inside the bounding box of the other 
# part, but not inside it, and sharing the same first point... (#2589)

def ogr_shape_25():
    layer_name = 'touchingrings2'
    wkt = 'MULTIPOLYGON(((10 5, 5 5,5 0,0 0,0 10,10 10,10 5)),((10 5,10 0,5 0,5 4.9,10 5)), ((100 100,100 200,200 200,200 100,100 100)))'
    geom = ogr.CreateGeometryFromWkt(wkt)

    if ogr_shape_23_write_geom(layer_name, geom, ogr.CreateGeometryFromWkt(geom.ExportToWkt()), ogr.wkbUnknown) != 'success':
        gdaltest.post_reason( 'Test for layer %s failed' % layer_name )
        return 'fail'

    # Same test, but use OGR_ORGANIZE_POLYGONS=DEFAULT to avoid relying only on the winding order
    layer_name = 'touchingrings3'
    wkt = 'MULTIPOLYGON(((10 5, 5 5,5 0,0 0,0 10,10 10,10 5)),((10 5,10 0,5 0,5 4.9,10 5)), ((100 100,100 200,200 200,200 100,100 100)))'
    geom = ogr.CreateGeometryFromWkt(wkt)

    gdal.SetConfigOption('OGR_ORGANIZE_POLYGONS', 'DEFAULT')
    ret = ogr_shape_23_write_geom(layer_name, geom, ogr.CreateGeometryFromWkt(geom.ExportToWkt()), ogr.wkbUnknown)
    gdal.SetConfigOption('OGR_ORGANIZE_POLYGONS', '')
    if ret != 'success':
        gdaltest.post_reason( 'Test for layer %s failed' % layer_name )
        return 'fail'

    return 'success'


###############################################################################
# Test a polygon made of one outer ring and two inner rings (special case
# in organizePolygons()

def ogr_shape_26():
    layer_name = 'oneouterring'
    wkt = 'POLYGON ((100 100,100 200,200 200,200 100,100 100),(110 110,120 110,120 120,110 120,110 110),(130 110,140 110,140 120,130 120,130 110))'
    geom = ogr.CreateGeometryFromWkt(wkt)

    ret = ogr_shape_23_write_geom(layer_name, geom, ogr.CreateGeometryFromWkt(geom.ExportToWkt()), ogr.wkbUnknown)
    if ret != 'success':
        gdaltest.post_reason( 'Test for layer %s failed' % layer_name )
        return 'fail'

    return 'success'

###############################################################################
# Test alternate date formatting (#2746)

def ogr_shape_27():

    result = 'success'
    
    ds = ogr.Open( 'data/water_main_dist.dbf' )
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()

    if feat.installe_1 != '1989/04/25':
        print(feat.installe_1)
        gdaltest.post_reason( 'got wrong date result!' )
        result = 'fail'

    feat = None
    lyr = None
    ds.Destroy()
    ds = None
    
    return 'success'

###############################################################################
# Test reading a 3 GB .DBF (#3011)

def ogr_shape_28():

    # Determine if the filesystem supports sparse files (we don't want to create a real 3 GB
    # file !
    if (gdaltest.filesystem_supports_sparse_files('tmp') == False):
        return 'skip'

    for filename in ('tmp/hugedbf.dbf', 'tmp/hugedbf.shp', 'tmp/hugedbf.shx'):
        try:
            os.remove(filename)
        except:
            pass

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('tmp/hugedbf.shp')
    lyr = ds.CreateLayer('test')
    field_defn = ogr.FieldDefn()
    field_defn.SetName('test')
    field_defn.SetWidth(99)
    lyr.CreateField(field_defn)
    field_defn.Destroy()
    ds = None

    os.remove('tmp/hugedbf.shp')
    os.remove('tmp/hugedbf.shx')

    file = open("tmp/hugedbf.dbf", "rb+")

    # Set recourd count to 24,000,000
    file.seek(4, 0)
    file.write("\x00".encode('latin1'))
    file.write("\x36".encode('latin1'))
    file.write("\x6e".encode('latin1'))
    file.write("\x01".encode('latin1'))

    # Set value for record 23,900,000 at offset 2,390,000,066 = (23,900,000 * (99 + 1) + 65) + 1    
    file.seek(2390000066, 0)
    file.write("value_over_2GB".encode('latin1'))

    # Extend to 3 GB file
    file.seek(3000000000, 0)
    file.write("0".encode('latin1'))

    file.close()

    ds = ogr.Open('tmp/hugedbf.dbf', update = 1)
    if ds is None:
        gdaltest.post_reason('Cannot open tmp/hugedbf.dbf')
        return 'fail'

    # Check that the hand-written value can be read back
    lyr = ds.GetLayer(0)
    feat = lyr.GetFeature(23900000);
    if feat.GetFieldAsString(0) != 'value_over_2GB':
        print(feat.GetFieldAsString(0))
        return 'fail'

    # Update with a new value
    feat.SetField(0, 'updated_value')
    lyr.SetFeature(feat)
    feat = None

    # Test creating a feature over 2 GB file limit -> should work
    gdal.ErrorReset()
    feat = ogr.Feature(lyr.GetLayerDefn())
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.CreateFeature(feat)
    gdal.PopErrorHandler()
    if ret != 0:
        gdaltest.post_reason('failure')
        return 'fail'
    feat = None
    if gdal.GetLastErrorMsg().find('2GB file size limit reached') < 0:
        gdaltest.post_reason('did not find expected warning')
        return 'fail'

    ds.Destroy()
    ds = None

    # Re-open and check the new value
    gdal.SetConfigOption('SHAPE_2GB_LIMIT', 'TRUE')
    ds = ogr.Open('tmp/hugedbf.dbf', 1)
    gdal.SetConfigOption('SHAPE_2GB_LIMIT', None)
    lyr = ds.GetLayer(0)
    feat = lyr.GetFeature(23900000);
    if feat.GetFieldAsString(0) != 'updated_value':
        print(feat.GetFieldAsString(0))
        return 'fail'
    feat = None

    # Test creating a feature over 2 GB file limit -> should fail
    gdal.ErrorReset()
    feat = ogr.Feature(lyr.GetLayerDefn())
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.CreateFeature(feat)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('failure')
        return 'fail'
    feat = None
    if gdal.GetLastErrorMsg().find('2GB file size limit reached') < 0:
        gdaltest.post_reason('did not find expected warning')
        return 'fail'

    ds = None

    return 'success'
    
###############################################################################
# Test that REPACK doesn't change extension case (#3293)

def ogr_shape_29():

    os.mkdir('tmp/UPPERCASE')
    shutil.copy('data/poly.shp', 'tmp/UPPERCASE/UPPERCASE.SHP')
    shutil.copy('data/poly.shx', 'tmp/UPPERCASE/UPPERCASE.SHX')
    shutil.copy('data/poly.dbf', 'tmp/UPPERCASE/UPPERCASE.DBF')
    f = open('tmp/UPPERCASE/UPPERCASE.CPG', 'wb')
    f.write('UTF-8')
    f.close()

    ds = ogr.Open('tmp/UPPERCASE', update = 1)
    lyr = ds.GetLayer(0)
    lyr.DeleteFeature(0)
    ds.ExecuteSQL( 'REPACK UPPERCASE' )
    ds.Destroy()
    
    list = gdal.ReadDir('tmp/UPPERCASE')

    if len(list) != 6:
        print(list)
        return 'fail'
        
    for filename in list:
        if filename not in ['.', '..', 'UPPERCASE.SHP', 'UPPERCASE.SHX', 'UPPERCASE.DBF', 'UPPERCASE.CPG']:
            gdaltest.post_reason('fail')
            print(list)
            print(filename)
            return 'fail'
        if filename.find('packed') >= 0:
            gdaltest.post_reason('fail')
            print(list)
            print(filename)
            return 'fail'

    return 'success'
    
###############################################################################
# Test that REPACK doesn't change extension case (#3293)

def ogr_shape_30():

    os.mkdir('tmp/lowercase')
    shutil.copy('data/poly.shp', 'tmp/lowercase/lowercase.shp')
    shutil.copy('data/poly.shx', 'tmp/lowercase/lowercase.shx')
    shutil.copy('data/poly.dbf', 'tmp/lowercase/lowercase.dbf')

    ds = ogr.Open('tmp/lowercase', update = 1)
    lyr = ds.GetLayer(0)
    lyr.DeleteFeature(0)
    ds.ExecuteSQL( 'REPACK lowercase' )
    ds.Destroy()
    
    list = gdal.ReadDir('tmp/lowercase')

    if len(list) != 5:
        print(list)
        return 'fail'
        
    for filename in list:
        if filename not in ['.', '..', 'lowercase.shp', 'lowercase.shx', 'lowercase.dbf']:
            print(list)
            return 'fail'

    return 'success'
    
###############################################################################
# Test truncation of long and duplicate field names.
# FIXME: Empty field names are allowed now!

def ogr_shape_31():

    if gdaltest.shape_ds is None:
        return 'skip'

    fields = [ ('a', ogr.OFTReal),
               ('A', ogr.OFTInteger),
               ('A_1', ogr.OFTInteger),
               ('A_1', ogr.OFTInteger),
               ('a_1_2', ogr.OFTInteger),
               ('aaaaaAAAAAb', ogr.OFTInteger),
               ('aAaaaAAAAAc', ogr.OFTInteger),
               ('aaaaaAAAABa', ogr.OFTInteger),
               ('aaaaaAAAABb', ogr.OFTInteger),
               ('aaaaaAAA_1', ogr.OFTInteger),
               ('aaaaaAAAABc', ogr.OFTInteger),
               ('aaaaaAAAABd', ogr.OFTInteger),
               ('aaaaaAAAABe', ogr.OFTInteger),
               ('aaaaaAAAABf', ogr.OFTInteger),
               ('aaaaaAAAABg', ogr.OFTInteger),
               ('aaaaaAAAABh', ogr.OFTInteger),
               ('aaaaaAAAABi', ogr.OFTInteger),
               ('aaaaaAAA10', ogr.OFTString),
               ('', ogr.OFTInteger),
               ('', ogr.OFTInteger) ]

    expected_fields = [ 'a',
                        'A_1',
                        'A_1_1',
                        'A_1_2',
                        'a_1_2_1',
                        'aaaaaAAAAA',
                        'aAaaaAAA_1',
                        'aaaaaAAAAB',
                        'aaaaaAAA_2',
                        'aaaaaAAA_3',
                        'aaaaaAAA_4',
                        'aaaaaAAA_5',
                        'aaaaaAAA_6',
                        'aaaaaAAA_7',
                        'aaaaaAAA_8',
                        'aaaaaAAA_9',
                        'aaaaaAAA10',
                        'aaaaaAAA11',
                        '',
                        '_1' ]

    #######################################################
    # Create Layer
    gdaltest.shape_lyr = gdaltest.shape_ds.CreateLayer( 'Fields' )

    #######################################################
    # Setup Schema with weird field names
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    ogrtest.quick_create_layer_def( gdaltest.shape_lyr, fields )
    gdal.PopErrorHandler()

    layer_defn = gdaltest.shape_lyr.GetLayerDefn()
    error_occured = False
    for i in range( layer_defn.GetFieldCount() ):
        if layer_defn.GetFieldDefn( i ).GetNameRef() != expected_fields[i]:
            print('Expected ', expected_fields[i],',but got',layer_defn.GetFieldDefn( i ).GetNameRef())
            error_occured = True

    if error_occured:
        return 'fail'
    return 'success'

###############################################################################
# Test creating a nearly 4GB (2^32 Bytes) .shp (#3236)
# Check for proper error report.
# Assuming 2^32 is the max value for unsigned int.

def ogr_shape_32():
# This test takes a few minutes and disk space. Hence, skipped by default.
# To run this test, make sure that the directory BigFilePath points to has
# 4.5 GB space available or give a new directory that does and delete the
# directory afterwards.

    return 'skip'
    
    from decimal import Decimal

    BigFilePath = '/tmp'

    #######################################################
    # Create a layer
    shape_drv = ogr.GetDriverByName('ESRI Shapefile')
    gdaltest.shape_ds_big = shape_drv.CreateDataSource( BigFilePath )
    gdaltest.shape_lyr = gdaltest.shape_ds_big.CreateLayer( "bigLayer", geom_type = ogr.wkbPolygon )

    #######################################################
    # Write a geometry repeatedly.
    # File size is pre-calculated according to the geometry's size.
    wkt = 'POLYGON((0 0,0 10,10 10,0 0),(0.25 0.5,1 1.1,0.5 1,0.25 0.5))';
    geom = ogr.CreateGeometryFromWkt(wkt)
    geom.AddGeometry(ogr.Geometry( type = ogr.wkbPolygon ))

    ret = 0
    n = 0
    print('')
#    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    for n in range( 0, 22845571 ):
        dst_feat = ogr.Feature( feature_def = gdaltest.shape_lyr.GetLayerDefn() )
        dst_feat.SetGeometry(geom)
        ret = gdaltest.shape_lyr.CreateFeature( dst_feat )
        if ret != 0 and n < 22845570:
            print('File limit reached before 4GB!')
            return 'fail'
        dst_feat.Destroy()
        if (n % 22846) == 0:
            sys.stdout.write( '\r%.1f%%   ' % (n/Decimal('228460.0')))
            sys.stdout.flush()
#    gdal.PopErrorHandler()

    #######################################################
    # Check some features

    gdaltest.shape_ds_big.Destroy()
    gdaltest.shape_ds_big = ogr.GetDriverByName('ESRI Shapefile').Open( BigFilePath, update = 0 )

    read_lyr = gdaltest.shape_ds_big.GetLayerByName( 'bigLayer' )

    for i in [0, 1, read_lyr.GetFeatureCount()-1]:
      feat_read = read_lyr.GetFeature(i)
      if feat_read is None:
        print('Couldn\' retrieve geometry at FID',i)
        return 'fail'
      if ogrtest.check_feature_geometry(feat_read,ogr.CreateGeometryFromWkt('POLYGON((0 0,0 10,10 10,0 0),(0.25 0.5,1 1.1,0.5 1,0.25 0.5))'),
                                max_error = 0.000000001 ) != 0:
        print('Wrong geometry encountered at FID',i,':', (feat_read.GetGeometryRef().ExportToWkt()))
        return 'fail'

    return 'success'

###############################################################################
# Check that we can detect correct winding order even with polygons with big
# coordinate offset (#3356)
def ogr_shape_33():

    ds = ogr.Open('data/bigoffset.shp')
    lyr = ds.GetLayer(0)
    feat_read = lyr.GetNextFeature()

    if ogrtest.check_feature_geometry(feat_read,ogr.CreateGeometryFromWkt('MULTIPOLYGON( ((0 0,0 1,1 1,1 0,0 0)),((100000000000 100000000000,100000000000 100000000001,100000000001 100000000001,100000000001 100000000000,100000000000 100000000000)) )'),
                                max_error = 0.000000001 ) != 0:
        print('Wrong geometry : %s' % feat_read.GetGeometryRef().ExportToWkt())
        return 'fail'

    ds.Destroy()

    return 'success'


###############################################################################
# Check that we can write correct winding order even with polygons with big
# coordinate offset (#33XX)

def ogr_shape_34():
    
    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource( 'tmp/bigoffset.shp' )
    lyr = ds.CreateLayer( 'bigoffset' )
    feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    geom_wkt = 'MULTIPOLYGON( ((0 0,0 1,1 1,1 0,0 0)),((100000000000 100000000000,100000000000 100000000001,100000000001 100000000001,100000000001 100000000000,100000000000 100000000000)) )'
    geom = ogr.CreateGeometryFromWkt( geom_wkt )
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)
    ds.Destroy()

    ds = ogr.Open('tmp/bigoffset.shp')
    lyr = ds.GetLayer(0)
    feat_read = lyr.GetNextFeature()

    if ogrtest.check_feature_geometry(feat_read,ogr.CreateGeometryFromWkt('MULTIPOLYGON( ((0 0,0 1,1 1,1 0,0 0)),((100000000000 100000000000,100000000000 100000000001,100000000001 100000000001,100000000001 100000000000,100000000000 100000000000)) )'),
                                max_error = 0.000000001 ) != 0:
        print('Wrong geometry : %s' % feat_read.GetGeometryRef().ExportToWkt())
        return 'fail'

    ds.Destroy()

    return 'success'

###############################################################################
# Check that we can read & write a VSI*L dataset

def ogr_shape_35():
    
    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource( '/vsimem/test35.shp' )
    srs = osr.SpatialReference()
    srs.ImportFromEPSG( 4326 )
    lyr = ds.CreateLayer( 'test35', srs = srs )
    feat = ogr.Feature( feature_def = lyr.GetLayerDefn() )
    geom_wkt = 'POINT(0 1)'
    geom = ogr.CreateGeometryFromWkt( geom_wkt )
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)
    ds.Destroy()

    ds = ogr.Open('/vsimem/test35.shp')
    lyr = ds.GetLayer(0)
    srs_read = lyr.GetSpatialRef()
    if srs_read.ExportToWkt().find('GCS_WGS_1984') == -1:
        gdaltest.post_reason('did not get expected SRS')
        print(srs_read)
        return 'fail'
    feat_read = lyr.GetNextFeature()

    if ogrtest.check_feature_geometry(feat_read,ogr.CreateGeometryFromWkt('POINT(0 1)'),
                                max_error = 0.000000001 ) != 0:
        print('Wrong geometry : %s' % feat_read.GetGeometryRef().ExportToWkt())
        return 'fail'

    ds.Destroy()

    return 'success'

###############################################################################
# Check that we can read from the root of a .ZIP file

def ogr_shape_36():

    ds = ogr.Open('/vsizip/data/poly.zip')
    if ds is None:
        return 'fail'

    lyr = ds.GetLayer(0)

    srs = lyr.GetSpatialRef()
    wkt = srs.ExportToWkt()
    if wkt.find('OSGB') == -1:
        gdaltest.post_reason('did not get expected SRS')
        return 'fail'

    feat_read = lyr.GetFeature(9)
    if ogrtest.check_feature_geometry(feat_read,ogr.CreateGeometryFromWkt('POLYGON ((479750.6875 4764702.0,479658.59375 4764670.0,479640.09375 4764721.0,479735.90625 4764752.0,479750.6875 4764702.0))'),
                                max_error = 0.000000001 ) != 0:
        print('Wrong geometry : %s' % feat_read.GetGeometryRef().ExportToWkt())
        return 'fail'

    ds.Destroy()

    return 'success'

###############################################################################
# Check that we can read from the root of a .tar.gz file

def ogr_shape_37():

    ds = ogr.Open('/vsitar/data/poly.tar.gz')
    if ds is None:
        return 'fail'

    lyr = ds.GetLayer(0)

    srs = lyr.GetSpatialRef()
    wkt = srs.ExportToWkt()
    if wkt.find('OSGB') == -1:
        gdaltest.post_reason('did not get expected SRS')
        return 'fail'

    for i in range(10):
        feat_read = lyr.GetNextFeature()
        if i == 9:
            if ogrtest.check_feature_geometry(feat_read,ogr.CreateGeometryFromWkt('POLYGON ((479750.6875 4764702.0,479658.59375 4764670.0,479640.09375 4764721.0,479735.90625 4764752.0,479750.6875 4764702.0))'),
                                        max_error = 0.000000001 ) != 0:
                print('Wrong geometry : %s' % feat_read.GetGeometryRef().ExportToWkt())
                return 'fail'

    lyr.ResetReading()
    feat_read = lyr.GetFeature(9)
    if ogrtest.check_feature_geometry(feat_read,ogr.CreateGeometryFromWkt('POLYGON ((479750.6875 4764702.0,479658.59375 4764670.0,479640.09375 4764721.0,479735.90625 4764752.0,479750.6875 4764702.0))'),
                                max_error = 0.000000001 ) != 0:
        print('Wrong geometry : %s' % feat_read.GetGeometryRef().ExportToWkt())
        return 'fail'

    ds.Destroy()

    return 'success'

###############################################################################
# Check that we can read from a .tar file

def ogr_shape_37_bis():

    ds = ogr.Open('/vsitar/data/poly.tar')
    if ds is None:
        return 'fail'

    lyr = ds.GetLayer(0)

    srs = lyr.GetSpatialRef()
    wkt = srs.ExportToWkt()
    if wkt.find('OSGB') == -1:
        gdaltest.post_reason('did not get expected SRS')
        return 'fail'

    for i in range(10):
        feat_read = lyr.GetNextFeature()
        if i == 9:
            if ogrtest.check_feature_geometry(feat_read,ogr.CreateGeometryFromWkt('POLYGON ((479750.6875 4764702.0,479658.59375 4764670.0,479640.09375 4764721.0,479735.90625 4764752.0,479750.6875 4764702.0))'),
                                        max_error = 0.000000001 ) != 0:
                print('Wrong geometry : %s' % feat_read.GetGeometryRef().ExportToWkt())
                return 'fail'

    lyr.ResetReading()
    feat_read = lyr.GetFeature(9)
    if ogrtest.check_feature_geometry(feat_read,ogr.CreateGeometryFromWkt('POLYGON ((479750.6875 4764702.0,479658.59375 4764670.0,479640.09375 4764721.0,479735.90625 4764752.0,479750.6875 4764702.0))'),
                                max_error = 0.000000001 ) != 0:
        print('Wrong geometry : %s' % feat_read.GetGeometryRef().ExportToWkt())
        return 'fail'

    ds.Destroy()

    return 'success'

###############################################################################
# Check that we cannot create duplicated layers

def ogr_shape_38():

    ds = ogr.Open( '/vsimem/', update = 1 )
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr = ds.CreateLayer( 'test35' )
    gdal.PopErrorHandler()
    ds.Destroy()

    if lyr is not None:
        gdaltest.post_reason('should not have created a new layer')
        return 'fail'

    return 'success'

###############################################################################
# Check that we can detect correct winding order even with polygons with big
# coordinate offset (#3356)
def ogr_shape_39():

    ds = ogr.Open('data/multipatch.shp')
    lyr = ds.GetLayer(0)
    feat_read = lyr.GetNextFeature()

    if ogrtest.check_feature_geometry(feat_read,ogr.CreateGeometryFromWkt('MULTIPOLYGON (((5 4 10,0 0 5,10 0 5,5 4 10)),((5 4 10,10 0 5,10 8 5,5 4 10)),((5 4 10,10 8 5,0 8 5,5 4 10)),((5 4 10,0 8 5,0 0 5,5 4 10)),((10 0 5,10 0 0,10 8 5,10 0 5)),((10 0 0,10 8 5,10 8 0,10 0 0)),((10 8 5,10 8 0,0 8 5,10 8 5)),((10 8 0,0 8 5,0 8 0,10 8 0)),((0 8 5,0 8 0,0 0 5,0 8 5)),((0 8 0,0 0 5,0 0 0,0 8 0)),((0 0 0,0 0 5,10 0 5,10 0 0,6 0 0,6 0 3,4 0 3,4 0 0,0 0 0),(1 0 2,3 0 2,3 0 4,1 0 4,1 0 2),(7 0 2,9 0 2,9 0 4,7 0 4,7 0 2)))'),
                                max_error = 0.000000001 ) != 0:
        print('Wrong geometry : %s' % feat_read.GetGeometryRef().ExportToWkt())
        return 'fail'

    ds.Destroy()

    return 'success'


###############################################################################
# Make some changes to a shapefile and check the index files. qix, sbn & sbx

def ogr_shape_40():

    if gdaltest.shape_ds is None:
        return 'skip'

    datafiles = ( 'gjpoint.dbf', 'gjpoint.shp', 'gjpoint.shx' )
    indexfiles = ( 'gjpoint.sbn', 'gjpoint.sbx', 'gjpoint.qix' )
    for f in datafiles:
        shutil.copy( os.path.join('data', f), os.path.join('tmp', f) )
    for i in range(2):
        shutil.copy( os.path.join('data', indexfiles[i]), os.path.join('tmp', indexfiles[i]) )

    gdaltest.shape_ds = ogr.Open( 'tmp/gjpoint.shp', update=1 )
    gdaltest.shape_lyr = gdaltest.shape_ds.GetLayer(0)
    gdaltest.shape_lyr.SetAttributeFilter( None )
    gdaltest.shape_ds.ExecuteSQL( 'CREATE SPATIAL INDEX ON gjpoint' )

    # Check if updating a feature removes the indices
    feat = gdaltest.shape_lyr.GetFeature( 0 )
    geom = ogr.CreateGeometryFromWkt( 'POINT (99 1)')
    feat.SetGeometry( geom )
    for f in indexfiles:
        if not os.path.exists( os.path.join('tmp', f) ):
            print('SetFeature(): ' + f)
            return 'fail'
    gdaltest.shape_lyr.SetFeature( feat )
    for f in indexfiles:
        if os.path.exists( os.path.join('tmp', f) ):
            print('SetFeature(): ' + f)
            return 'fail'

    # Check if adding a feature removes the indices
    for i in range(2):
        shutil.copy( os.path.join('data', indexfiles[i]), os.path.join('tmp', indexfiles[i]) )

    gdaltest.shape_ds = ogr.Open( 'tmp/gjpoint.shp', update=1 )
    gdaltest.shape_lyr = gdaltest.shape_ds.GetLayer(0)
    gdaltest.shape_lyr.SetAttributeFilter( None )
    gdaltest.shape_ds.ExecuteSQL( 'CREATE SPATIAL INDEX ON gjpoint' )
    feat = ogr.Feature(gdaltest.shape_lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt( 'POINT (98 2)')
    feat.SetGeometry( geom )
    feat.SetField( 'NAME', 'Point 2' )
    feat.SetField( 'FID', '2' )
    feat.SetFID( 1 )

    for f in indexfiles:
        if not os.path.exists( os.path.join('tmp', f) ):
            print('CreateFeature(): ' + f)
            return 'fail'
    gdaltest.shape_lyr.CreateFeature( feat )
    for f in indexfiles:
        if os.path.exists( os.path.join('tmp', f) ):
            print('CreateFeature(): ' + f)
            return 'fail'

    # Check if deleting a feature removes the indices
    for i in range(2):
        shutil.copy( os.path.join('data', indexfiles[i]), os.path.join('tmp', indexfiles[i]) )
    gdaltest.shape_ds = ogr.Open( 'tmp/gjpoint.shp', update=1 )
    gdaltest.shape_lyr = gdaltest.shape_ds.GetLayer(0)
    gdaltest.shape_lyr.SetAttributeFilter( None )
    gdaltest.shape_ds.ExecuteSQL( 'CREATE SPATIAL INDEX ON gjpoint' )

    for f in indexfiles:
        if not os.path.exists( os.path.join('tmp', f) ):
            print('DeleteFeature(): ' + f)
            return 'fail'
    if gdaltest.shape_lyr.DeleteFeature( 0 ) != 0:
        gdaltest.post_reason( 'DeleteFeature failed.' )
        return 'fail'
    for f in indexfiles:
        if os.path.exists( os.path.join('tmp', f) ):
            print('DeleteFeature(): ' + f)
            return 'fail'

    return 'success'

###############################################################################
# Run test_ogrsf

def ogr_shape_41():

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    shutil.copy('data/poly.shp', 'tmp/poly.shp')
    shutil.copy('data/poly.shx', 'tmp/poly.shx')
    shutil.copy('data/poly.dbf', 'tmp/poly.dbf')

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -fsf tmp/poly.shp')

    os.remove('tmp/poly.shp')
    os.remove('tmp/poly.shx')
    os.remove('tmp/poly.dbf')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Run test_ogrsf with -sql

def ogr_shape_42():

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    shutil.copy('data/poly.shp', 'tmp/poly.shp')
    shutil.copy('data/poly.shx', 'tmp/poly.shx')
    shutil.copy('data/poly.dbf', 'tmp/poly.dbf')

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' tmp/poly.shp -sql "SELECT * FROM poly"')

    os.remove('tmp/poly.shp')
    os.remove('tmp/poly.shx')
    os.remove('tmp/poly.dbf')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

###############################################################################
# Test /vsizip//vsicurl/

def ogr_shape_43():

    try:
        drv = gdal.GetDriverByName( 'HTTP' )
    except:
        drv = None

    if drv is None:
        return 'skip'

    conn = gdaltest.gdalurlopen('http://svn.osgeo.org/gdal/trunk/autotest/ogr/data/poly.zip')
    if conn is None:
        print('cannot open URL')
        return 'skip'
    conn.close()

    ds = ogr.Open('/vsizip//vsicurl/http://svn.osgeo.org/gdal/trunk/autotest/ogr/data/poly.zip')
    if ds is None:
        return 'fail'

    lyr = ds.GetLayer(0)

    srs = lyr.GetSpatialRef()
    wkt = srs.ExportToWkt()
    if wkt.find('OSGB') == -1:
        gdaltest.post_reason('did not get expected SRS')
        return 'fail'

    return 'success'

###############################################################################
# Test /vsicurl/ on a directory

def ogr_shape_44():

    try:
        drv = gdal.GetDriverByName( 'HTTP' )
    except:
        drv = None

    if drv is None:
        return 'skip'

    conn = gdaltest.gdalurlopen('http://svn.osgeo.org/gdal/trunk/autotest/ogr/data/poly.zip')
    if conn is None:
        print('cannot open URL')
        return 'skip'
    conn.close()

    ds = ogr.Open('/vsicurl/http://svn.osgeo.org/gdal/trunk/autotest/ogr/data/testshp')
    if ds is None:
        return 'fail'

    lyr = ds.GetLayer(0)

    srs = lyr.GetSpatialRef()
    wkt = srs.ExportToWkt()
    if wkt.find('OSGB') == -1:
        gdaltest.post_reason('did not get expected SRS')
        return 'fail'

    return 'success'
    
###############################################################################
# Test ignored fields works ok on a shapefile.

def ogr_shape_45():

    shp_ds = ogr.Open( 'data/poly.shp' )
    shp_layer = shp_ds.GetLayer(0)
    shp_layer.SetIgnoredFields( ['AREA'] )

    feat = shp_layer.GetNextFeature()

    if feat.IsFieldSet( 'AREA' ):
        gdaltest.post_reason( 'got area despite request to ignore it.' )
        return 'fail'

    if feat.GetFieldAsInteger('EAS_ID') != 168:
        gdaltest.post_reason( 'missing or wrong eas_id' )
        return 'fail'

    wkt = 'POLYGON ((479819.84375 4765180.5,479690.1875 4765259.5,479647.0 4765369.5,479730.375 4765400.5,480039.03125 4765539.5,480035.34375 4765558.5,480159.78125 4765610.5,480202.28125 4765482.0,480365.0 4765015.5,480389.6875 4764950.0,480133.96875 4764856.5,480080.28125 4764979.5,480082.96875 4765049.5,480088.8125 4765139.5,480059.90625 4765239.5,480019.71875 4765319.5,479980.21875 4765409.5,479909.875 4765370.0,479859.875 4765270.0,479819.84375 4765180.5))'
    if ogrtest.check_feature_geometry(feat, wkt, 
                                      max_error = 0.00000001 ) != 0:
        return 'fail'

    fd = shp_layer.GetLayerDefn()
    fld = fd.GetFieldDefn(0) # area
    if not fld.IsIgnored():
        gdaltest.post_reason( 'AREA unexpectedly not marked as ignored.' )
        return 'fail'

    fld = fd.GetFieldDefn(1) # eas_id
    if fld.IsIgnored():
        gdaltest.post_reason( 'EASI unexpectedly marked as ignored.' )
        return 'fail'

    if fd.IsGeometryIgnored():
        gdaltest.post_reason( 'geometry unexpectedly ignored.' )
        return 'fail'

    if fd.IsStyleIgnored():
        gdaltest.post_reason( 'style unexpectedly ignored.' )
        return 'fail'

    fd.SetGeometryIgnored( 1 )

    if not fd.IsGeometryIgnored():
        gdaltest.post_reason( 'geometry unexpectedly not ignored.' )
        return 'fail'

    feat = shp_layer.GetNextFeature()

    if feat.GetGeometryRef() != None:
        gdaltest.post_reason( 'Unexpectedly got a geometry on feature 2.' )
        return 'fail'

    if feat.IsFieldSet( 'AREA' ):
        gdaltest.post_reason( 'got area despite request to ignore it.' )
        return 'fail'

    if feat.GetFieldAsInteger('EAS_ID') != 179:
        gdaltest.post_reason( 'missing or wrong eas_id' )
        return 'fail'
    
    feat = None
    shp_layer = None
    shp_ds = None
    
    return 'success'

###############################################################################
# This is a very weird use case : the user creates/open a datasource
# made of a single shapefile 'foo.shp' and wants to add a new layer
# to it, 'bar'. So we create a new shapefile 'bar.shp' in the same
# directory as 'foo.shp'

def ogr_shape_46():

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource( '/vsimem/ogr_shape_46.shp' )
    lyr = ds.CreateLayer( 'you_can_put_here_what_you_want_i_dont_care' )
    lyr = ds.CreateLayer( 'this_one_i_care_46' )
    ds = None

    ds = ogr.Open('/vsimem/ogr_shape_46.shp')
    if ds.GetLayerCount() != 1:
        return 'fail'
    ds = None

    ds = ogr.Open('/vsimem/this_one_i_care_46.shp')
    if ds.GetLayerCount() != 1:
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test that we can open a symlink whose pointed filename isn't a real
# file, but a filename that OGR recognizes

def ogr_shape_47():

    if not gdaltest.support_symlink():
        return 'skip'

    try:
        os.unlink('tmp/poly.zip')
    except:
        pass
    os.symlink('/vsizip/data/poly.zip', 'tmp/poly.zip')

    ds = ogr.Open('tmp/poly.zip')
    if ds is None:
        gdaltest.post_reason( 'tmp/polyzip symlink does not open.' )
        return 'fail'
    ds = None

    os.remove('tmp/poly.zip')

    return 'success'

###############################################################################
# Test RECOMPUTE EXTENT ON (#4027)

def ogr_shape_48():

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('/vsimem/ogr_shape_48.shp')
    lyr = ds.CreateLayer('ogr_shape_48')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1 2)'))
    lyr.CreateFeature(feat)

    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT(3 4)'))
    lyr.SetFeature(feat)
    extent = lyr.GetExtent()
    if extent != (1,3,2,4):
        gdaltest.post_reason('did not get expected extent (1)')
        print(lyr.GetExtent())
        return 'fail'
    ds.ExecuteSQL('RECOMPUTE EXTENT ON ogr_shape_48')
    extent = lyr.GetExtent()
    if extent != (3,3,4,4):
        gdaltest.post_reason('did not get expected extent (2)')
        print(lyr.GetExtent())
        return 'fail'
    ds = None
    
    ds = ogr.Open('/vsimem/ogr_shape_48.shp')
    lyr = ds.GetLayer(0)
    extent = lyr.GetExtent()
    if extent != (3,3,4,4):
        gdaltest.post_reason('did not get expected extent (3)')
        print(lyr.GetExtent())
        return 'fail'
    ds = None
    
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('/vsimem/ogr_shape_48.shp')
    
    # Test with Polygon
    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('/vsimem/ogr_shape_48.shp')
    lyr = ds.CreateLayer('ogr_shape_48')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((0 0,0 -1,-1 -1,-1 0,0 0))'))
    lyr.CreateFeature(feat)
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((0 0,0 1,1 1,1 0,0 0))'))
    lyr.SetFeature(feat)
    ds.ExecuteSQL('RECOMPUTE EXTENT ON ogr_shape_48')
    extent = lyr.GetExtent()
    if extent != (0,1,0,1):
        gdaltest.post_reason('did not get expected extent (4)')
        print(lyr.GetExtent())
        return 'fail'
    ds = None
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('/vsimem/ogr_shape_48.shp')
    
    # Test with PolygonZ
    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('/vsimem/ogr_shape_48.shp')
    lyr = ds.CreateLayer('ogr_shape_48')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((0 0 -2,0 -1 -2,-1 -1 -2,-1 0 -2,0 0 -2))'))
    lyr.CreateFeature(feat)
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POLYGON((0 0 2,0 1 2,1 1 2,1 0 2,0 0 2))'))
    lyr.SetFeature(feat)
    ds.ExecuteSQL('RECOMPUTE EXTENT ON ogr_shape_48')
    # FIXME: when we have a GetExtent3D
    extent = lyr.GetExtent()
    if extent != (0,1,0,1):
        gdaltest.post_reason('did not get expected extent (4)')
        print(lyr.GetExtent())
        return 'fail'
    ds = None
    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('/vsimem/ogr_shape_48.shp')


    return 'success'
    
###############################################################################
# Test that we can read at an LDID/87 file and recode to UTF-8.

def ogr_shape_49():

    ds = ogr.Open( 'data/facility_surface_dd.dbf')
    lyr = ds.GetLayer(0)

    feat = lyr.GetFeature( 91 )

    name = feat.GetField('NAME')

    # Setup the utf-8 string.
    if sys.version_info >= (3,0,0):
        gdaltest.exp_name = 'OSEBERG S\u00D8R'
    else:
        exec("gdaltest.exp_name =  u'OSEBERG S\u00D8R'")
        gdaltest.exp_name = gdaltest.exp_name.encode('utf-8')

    if name != gdaltest.exp_name:
        gdaltest.post_reason( 'Did not get expected name, encoding problems?' )
        return 'fail'

    return 'success'

###############################################################################
# Test that we can read encoded file names

def ogr_shape_50():

    try:
        drv = gdal.GetDriverByName( 'HTTP' )
    except:
        drv = None

    if drv is None:
        return 'skip'

    ds = ogr.Open( '/vsizip/vsicurl/http://jira.codehaus.org/secure/attachment/37994/test1.zip')
    if ds is None:
        return 'skip'
    lyr = ds.GetLayer(0)

    reconv_possible = lyr.TestCapability(ogr.OLCStringsAsUTF8) == 1

    if gdal.GetLastErrorMsg().find('Recode from CP936 to UTF-8 not supported, treated as ISO8859-1 to UTF-8.') != -1:
        if reconv_possible:
            gdaltest.post_reason( 'Recode failed, but TestCapability(OLCStringsAsUTF8) returns TRUE' )
            return 'fail'

        gdaltest.post_reason( 'skipping test: iconv support needed' )
        return 'skip'

    # Setup the utf-8 string.
    if sys.version_info >= (3,0,0):
        gdaltest.fieldname = '\u540d\u79f0'
    else:
        exec("gdaltest.fieldname =  u'\u540d\u79f0'")
        gdaltest.fieldname = gdaltest.fieldname.encode('utf-8')

    if lyr.GetLayerDefn().GetFieldIndex(gdaltest.fieldname) != 1:
        return 'fail'

    if not reconv_possible:
        gdaltest.post_reason( 'TestCapability(OLCStringsAsUTF8) should return TRUE' )
        return 'fail'

    return 'success'

###############################################################################
# Test that we can add a field when there's no dbf file initialy

def ogr_shape_51():

    if int(gdal.VersionInfo('VERSION_NUM')) < 1900:
        gdaltest.post_reason('would crash')
        return 'skip'

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('/vsimem/ogr_shape_51.shp')
    lyr = ds.CreateLayer('ogr_shape_51')
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)
    ds = None

    gdal.Unlink('/vsimem/ogr_shape_51.dbf')

    ds = ogr.Open('/vsimem/ogr_shape_51.shp', update = 1)
    lyr = ds.GetLayer(0)
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    feat = lyr.GetNextFeature()
    feat.SetField(0, 'bar')
    lyr.SetFeature(feat)
    ds = None

    ds = ogr.Open('/vsimem/ogr_shape_51.shp')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    value = feat.GetFieldAsString(0)
    field_count = lyr.GetLayerDefn().GetFieldCount()
    ds = None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource( '/vsimem/ogr_shape_51.shp' )

    if field_count != 1:
        gdaltest.post_reason('did not get expected field count')
        print(field_count)
        return 'fail'

    if value != 'bar':
        gdaltest.post_reason('did not get expected value')
        print(value)
        return 'fail'

    return 'success'

###############################################################################
# Test fix for #3356

def ogr_shape_52():

    expected_geom = ogr.CreateGeometryFromWkt('MULTIPOLYGON (((175.524709766699999 -40.17203475,175.524757883299998 -40.172050566700001,175.52480505 -40.1720663,175.524858766699992 -40.172091433299997,175.524913916700001 -40.172112966699999,175.524966049999989 -40.172136933300003,175.525030633299991 -40.17216185,175.5250873 -40.17218215,175.52515168330001 -40.1722011,175.525217666700001 -40.172221216700002,175.525269416700013 -40.172234466699997,175.5253165 -40.1722478,175.52535415 -40.1722577667,175.52538385 -40.17226365,175.525436816699994 -40.1722814333,175.525507016700004 -40.17229905,175.525594783299994 -40.172322033299999,175.525669933300009 -40.172339533299997,175.52574 -40.17235335,175.525807566699996 -40.1723672,175.52585005 -40.17237395,175.52588115 -40.172378683300003,175.525969816700012 -40.172388633300002,175.526057266700008 -40.1724020833,175.52723455 -40.17253515,175.527275583299996 -40.1725388,175.527324533300003 -40.17254675,175.527394866700007 -40.172552766700001,175.527473066699997 -40.172561616700001,175.527576666700014 -40.172572916699998,175.527678333300003 -40.172584266699999,175.527787883299993 -40.17259845,175.52789345 -40.172609716700002,175.527953933300012 -40.17261295,175.528028083300001 -40.1726174,175.52809835 -40.1726219333,175.528151650000012 -40.172625833300003,175.528190349999988 -40.17262725,175.528230900000011 -40.172631183299998,175.5282776 -40.1726338,175.528322800000012 -40.172637633299999,175.5283648 -40.17263915,175.5284115 -40.172641766700004,175.528452133299993 -40.17264435,175.528492133300006 -40.172646033299998,175.52856465 -40.17264805,175.528621733300014 -40.1726492,175.52868035 -40.172650333299998,175.528751333299994 -40.172652383299997,175.528814566699992 -40.1726534,175.528883933299994 -40.172653116699998,175.528939383300013 -40.17265195,175.529002566700001 -40.1726518,175.529070350000012 -40.172650366699997,175.529136633299998 -40.17265015,175.529193616700013 -40.17264895,175.529250616700011 -40.172647733300003,175.529313800000011 -40.172647583299998,175.529376783299995 -40.172647016699997,175.52895773329999 -40.172694633299997,175.528450866700013 -40.172752216699998,175.52835635 -40.172753466700001,175.52741181670001 -40.1727757333,175.52685245 -40.172532333299998,175.52627245 -40.172501266700003,175.5262405167 -40.172502816700003,175.5258356 -40.172522816700003,175.5256125 -40.172533833300001,175.525424433300003 -40.172543116699998,175.524834133300004 -40.1725533,175.524739033299994 -40.172414983300001,175.5247128 -40.17207405,175.524709766699999 -40.17203475)),((175.531267916699989 -40.17286525,175.5312654 -40.172863283300003,175.531252849999987 -40.172853516700002,175.531054566699993 -40.172822366699997,175.530193283300008 -40.172687333299997,175.529890266699994 -40.1726398,175.529916116700008 -40.172639383300002,175.529972483300014 -40.172639216699999,175.53002885 -40.1726398,175.530085183300002 -40.17264115,175.530141500000013 -40.17264325,175.530197733300014 -40.172646133299999,175.530253916699991 -40.172649766699998,175.530309983299986 -40.172654166699999,175.53036595 -40.172659333299997,175.5304218 -40.17266525,175.53047748329999 -40.172671916699997,175.530533016699991 -40.17267935,175.5305883833 -40.1726875333,175.530643533300008 -40.172696466700003,175.530722333299991 -40.172710633299999,175.530800633300004 -40.1727263167,175.5308541 -40.17273795,175.5309073 -40.1727503,175.530960216700009 -40.172763366700003,175.531012816700013 -40.172777133300002,175.5310651 -40.1727916,175.53111705 -40.172806766699999,175.531168650000012 -40.172822633300001,175.531219883299997 -40.172839183299999,175.531270733300005 -40.1728564,175.531267916699989 -40.17286525)))')

    ds = ogr.Open('data/test3356.shp')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()

    if ogrtest.check_feature_geometry(feat,expected_geom,
                                      max_error = 0.000000001 ) != 0:
        gdaltest.post_reason('failed reading geom')
        return 'fail'

    ds = None

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('/vsimem/ogr_shape_52.shp')
    lyr = ds.CreateLayer('ogr_shape_52')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(expected_geom)
    lyr.CreateFeature(feat)
    ds = None

    ds = ogr.Open('/vsimem/ogr_shape_52.shp')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()

    if ogrtest.check_feature_geometry(feat,expected_geom,
                                      max_error = 0.000000001 ) != 0:
        gdaltest.post_reason('failed writing and reading back geom')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test various expected error cases

def ogr_shape_53():

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('/vsimem/ogr_shape_53.shp')
    lyr = ds.CreateLayer('ogr_shape_53')

    # Test ReorderFields() when there are no fields
    ret = lyr.ReorderFields([])
    if ret != 0:
        gdaltest.post_reason('failed')
        return 'fail'

    # Test REPACK when there are no features
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = ds.ExecuteSQL("REPACK ogr_shape_53")
    gdal.PopErrorHandler()
    # Should work without any error
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('failed')
        return 'fail'

    # Create a field
    fd = ogr.FieldDefn("foo", ogr.OFTString)
    lyr.CreateField(fd)

    # GetFeature() on a invalid FID
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    feat = lyr.GetFeature(-1)
    gdal.PopErrorHandler()
    if feat is not None or gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('failed')
        return 'fail'

    # SetFeature() on a invalid FID
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    feat = ogr.Feature(lyr.GetLayerDefn())
    ret = lyr.SetFeature(feat)
    feat = None
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('failed')
        return 'fail'

    # SetFeature() on a invalid FID
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetFID(1000)
    ret = lyr.SetFeature(feat)
    feat = None
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('failed')
        return 'fail'

    # DeleteFeature() on a invalid FID
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.DeleteFeature(-1)
    gdal.PopErrorHandler()
    if ret == 0 or gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('failed')
        return 'fail'

    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)
    feat = None

    ret = lyr.DeleteFeature(0)
    if ret != 0:
        gdaltest.post_reason('failed')
        return 'fail'

    # Try deleting an already deleted feature
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.DeleteFeature(0)
    gdal.PopErrorHandler()
    if ret == 0 or gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('failed')
        return 'fail'

    # Test DeleteField() on a invalid index
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.DeleteField(-1)
    gdal.PopErrorHandler()
    if ret == 0 or gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('failed')
        return 'fail'

    # Test ReorderFields() with invalid permutation
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.ReorderFields([1])
    gdal.PopErrorHandler()
    if ret == 0 or gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('failed')
        return 'fail'

    # Test AlterFieldDefn() on a invalid index
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    fd = ogr.FieldDefn("foo2", ogr.OFTString)
    ret = lyr.AlterFieldDefn(-1, fd, 0)
    gdal.PopErrorHandler()
    if ret == 0 or gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('failed')
        return 'fail'

    # Test AlterFieldDefn() when attempting to convert from OFTString to something else
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    fd = ogr.FieldDefn("foo", ogr.OFTInteger)
    ret = lyr.AlterFieldDefn(0, fd, ogr.ALTER_TYPE_FLAG)
    gdal.PopErrorHandler()
    if ret == 0 or gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('failed')
        return 'fail'

    # Test DROP SPATIAL INDEX ON layer without index
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = ds.ExecuteSQL("DROP SPATIAL INDEX ON ogr_shape_53")
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('failed')
        return 'fail'

    # Re-create a feature
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)
    feat = None

    lyr = None
    ds = None

    # Test that some operations are not possible in read-only mode
    ds = ogr.Open('/vsimem/ogr_shape_53.shp')
    lyr = ds.GetLayer(0)

    if lyr.TestCapability(ogr.OLCSequentialWrite) != 0:
        gdaltest.post_reason('failed')
        return 'fail'
    if lyr.TestCapability(ogr.OLCDeleteFeature) != 0:
        gdaltest.post_reason('failed')
        return 'fail'
    if lyr.TestCapability(ogr.OLCCreateField) != 0:
        gdaltest.post_reason('failed')
        return 'fail'
    if lyr.TestCapability(ogr.OLCDeleteField) != 0:
        gdaltest.post_reason('failed')
        return 'fail'
    if lyr.TestCapability(ogr.OLCReorderFields) != 0:
        gdaltest.post_reason('failed')
        return 'fail'
    if lyr.TestCapability(ogr.OLCAlterFieldDefn) != 0:
        gdaltest.post_reason('failed')
        return 'fail'

    # Test CreateField()
    fd = ogr.FieldDefn("bar", ogr.OFTString)

    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.CreateField(fd)
    gdal.PopErrorHandler()
    if ret == 0 or gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('failed')
        return 'fail'

    # Test ReorderFields()
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.ReorderFields([0])
    gdal.PopErrorHandler()
    if ret == 0 or gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('failed')
        return 'fail'

    # Test DeleteField()
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.DeleteField(0)
    gdal.PopErrorHandler()
    if ret == 0 or gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('failed')
        return 'fail'

    # Test AlterFieldDefn()
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    fd = ogr.FieldDefn("foo2", ogr.OFTString)
    ret = lyr.AlterFieldDefn(0, fd, 0)
    gdal.PopErrorHandler()
    if ret == 0 or gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('failed')
        return 'fail'

    # Test CreateFeature()
    feat = ogr.Feature(lyr.GetLayerDefn())

    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.CreateFeature(feat)
    gdal.PopErrorHandler()
    if ret == 0 or gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('failed')
        return 'fail'

    # Test DeleteFeature()
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.DeleteFeature(0)
    gdal.PopErrorHandler()
    if ret == 0 or gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('failed')
        return 'fail'

    # Test SetFeature()
    feat = lyr.GetNextFeature()

    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.SetFeature(feat)
    gdal.PopErrorHandler()
    if ret == 0 or gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('failed')
        return 'fail'

    # Test REPACK
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = ds.ExecuteSQL("REPACK ogr_shape_53")
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('failed')
        return 'fail'

    # Test RECOMPUTE EXTENT ON
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = ds.ExecuteSQL("RECOMPUTE EXTENT ON ogr_shape_53")
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('failed')
        return 'fail'

    feat = None
    lyr = None
    ds = None

    # Attempt to delete shape in shapefile with no .dbf file
    gdal.Unlink('/vsimem/ogr_shape_53.dbf' )
    ds = ogr.Open('/vsimem/ogr_shape_53.shp', update = 1)
    lyr = ds.GetLayer(0)

    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.DeleteFeature(0)
    gdal.PopErrorHandler()
    if ret == 0 or gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('failed')
        return 'fail'

    # Test REPACK
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = ds.ExecuteSQL("REPACK ogr_shape_53")
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('failed')
        return 'fail'

    lyr = None
    ds = None

    # Tests on a DBF only
    ds = ogr.Open('data/idlink.dbf')
    lyr = ds.GetLayer(0)

    # Test GetExtent()
    # FIXME : GetExtent() should fail. Currently we'll get garbage here
    lyr.GetExtent()

    # Test RECOMPUTE EXTENT ON
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = ds.ExecuteSQL("RECOMPUTE EXTENT ON ogr_shape_53")
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('failed')
        return 'fail'

    lyr = None
    ds = None

    return 'success'

###############################################################################
# Test accessing a shape datasource with hundreds of layers (#4306)

def ogr_shape_54_create_layer(ds, layer_index):
    lyr = ds.CreateLayer('layer%03d' % layer_index)
    lyr.CreateField(ogr.FieldDefn('strfield', ogr.OFTString))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 'val%d' % layer_index)
    if (layer_index % 2) == 0:
        feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT (%d %d)' % (layer_index, layer_index+1)))
    lyr.CreateFeature(feat)
    feat = None
    return

def ogr_shape_54_test_layer(ds, layer_index):
    lyr = ds.GetLayerByName('layer%03d' % layer_index)
    if lyr is None:
        gdaltest.post_reason('failed for layer %d' % layer_index)
        return 'fail'
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    if feat is None:
        gdaltest.post_reason('failed for layer %d' % layer_index)
        return 'fail'
    if feat.GetField(0) != 'val%d' % layer_index:
        gdaltest.post_reason('failed for layer %d' % layer_index)
        return 'fail'
    if (layer_index % 2) == 0:
        if feat.GetGeometryRef() is None or \
           feat.GetGeometryRef().ExportToWkt() != 'POINT (%d %d)' % (layer_index, layer_index+1):
            gdaltest.post_reason('failed for layer %d' % layer_index)
            return 'fail'

    return 'success'

def ogr_shape_54():

    shape_drv = ogr.GetDriverByName('ESRI Shapefile')
    ds_name = '/vsimem/ogr_shape_54'
    #ds_name = 'tmp/ogr_shape_54'
    N = 500
    LRUListSize = 100

    # Test creating N layers
    ds = shape_drv.CreateDataSource( ds_name )
    for i in range(N):
        ogr_shape_54_create_layer(ds, i)

    ds = None

    # Test access to the N layers in sequence
    ds = ogr.Open( ds_name )
    for i in range(N):
        ret = ogr_shape_54_test_layer(ds, i)
        if ret != 'success':
            return ret

    # Now some 'random' access
    ret = ogr_shape_54_test_layer(ds, N - 1 - LRUListSize)
    if ret != 'success':
        return ret
    ret = ogr_shape_54_test_layer(ds, N - LRUListSize / 2)
    if ret != 'success':
        return ret
    ret = ogr_shape_54_test_layer(ds, N - LRUListSize / 4)
    if ret != 'success':
        return ret
    ret = ogr_shape_54_test_layer(ds, 0)
    if ret != 'success':
        return ret
    ret = ogr_shape_54_test_layer(ds, 0)
    if ret != 'success':
        return ret
    ret = ogr_shape_54_test_layer(ds, 2)
    if ret != 'success':
        return ret
    ret = ogr_shape_54_test_layer(ds, 1)
    if ret != 'success':
        return ret
    ds = None

    # Test adding a new layer
    ds = ogr.Open( ds_name, update = 1 )
    ogr_shape_54_create_layer(ds, N)
    ds = None

    # Test accessing the new layer
    ds = ogr.Open( ds_name )
    ret = ogr_shape_54_test_layer(ds, N)
    if ret != 'success':
        return ret
    ds = None

    # Test deleting layers
    ds = ogr.Open( ds_name, update = 1 )
    for i in range(N):
        ret = ogr_shape_54_test_layer(ds, i)
        if ret != 'success':
            return ret
    for i in range(N - LRUListSize + 1,N):
        ds.ExecuteSQL('DROP TABLE layer%03d' % i)
    ret = ogr_shape_54_test_layer(ds, N - LRUListSize)
    if ret != 'success':
        return ret
    ogr_shape_54_create_layer(ds, N + 2)
    for i in range(0,N - LRUListSize + 1):
        ds.ExecuteSQL('DROP TABLE layer%03d' % i)
    ret = ogr_shape_54_test_layer(ds, N)
    if ret != 'success':
        return ret
    ret = ogr_shape_54_test_layer(ds, N + 2)
    if ret != 'success':
        return ret
    ds = None

    # Destroy and recreate datasource
    shape_drv.DeleteDataSource( ds_name )
    ds = shape_drv.CreateDataSource( ds_name )
    for i in range(N):
        ogr_shape_54_create_layer(ds, i)
    ds = None

    # Reopen in read-only so as to be able to delete files */
    # if testing on a real filesystem.
    ds = ogr.Open( ds_name )

    # Test corner case where we cannot reopen a closed layer
    ideletedlayer = 0
    gdal.Unlink( ds_name + '/' + 'layer%03d.shp' % ideletedlayer)
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr = ds.GetLayerByName('layer%03d' % ideletedlayer)
    gdal.PopErrorHandler()
    if lyr is not None:
        gdal.ErrorReset()
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        lyr.ResetReading()
        feat = lyr.GetNextFeature()
        gdal.PopErrorHandler()
        if gdal.GetLastErrorMsg() == '':
            gdaltest.post_reason('failed')
            return 'fail'
    gdal.ErrorReset()

    ideletedlayer = 1
    gdal.Unlink( ds_name + '/' + 'layer%03d.dbf' % ideletedlayer)
    lyr = ds.GetLayerByName('layer%03d' % ideletedlayer)
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr.ResetReading()
    feat = lyr.GetNextFeature()
    gdal.PopErrorHandler()
    #if gdal.GetLastErrorMsg() == '':
    #    gdaltest.post_reason('failed')
    #    return 'fail'
    gdal.ErrorReset()


    ds = None


    return 'success'

###############################################################################
# Test that we cannot add more fields that the maximum allowed

def ogr_shape_55():
    shape_drv = ogr.GetDriverByName('ESRI Shapefile')
    ds_name = '/vsimem/ogr_shape_55'
    ds = shape_drv.CreateDataSource( ds_name )
    lyr = ds.CreateLayer('ogr_shape_55')

    max_field_count = int((65535 - 33) / 32) # 2046

    for i in range(max_field_count):
        if i == 255:
            gdal.ErrorReset()
            gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
        ret = lyr.CreateField(ogr.FieldDefn('foo%d' % i, ogr.OFTInteger))
        if i == 255:
            gdal.PopErrorHandler()
            if gdal.GetLastErrorMsg() == '':
                gdaltest.post_reason('expecting a warning for 256th field added')
                return 'fail'
        if ret != 0:
            gdaltest.post_reason('failed creating field foo%d' % i)
            return 'fail'

    i = max_field_count
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    ret = lyr.CreateField(ogr.FieldDefn('foo%d' % i, ogr.OFTInteger))
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('should have failed creating field foo%d' % i)
        return 'fail'

    feat = ogr.Feature(lyr.GetLayerDefn())
    for i in range(max_field_count):
        feat.SetField(i, i)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    for i in range(max_field_count):
        feat.SetField(i, i)
    lyr.CreateFeature(feat)

    ds = None

    return 'success'

###############################################################################
# Test that we cannot add more fields that the maximum allowed record length

def ogr_shape_56():
    shape_drv = ogr.GetDriverByName('ESRI Shapefile')
    ds_name = '/vsimem/ogr_shape_56'
    ds = shape_drv.CreateDataSource( ds_name )
    lyr = ds.CreateLayer('ogr_shape_56')

    max_field_count = int(65535 / 80) # 819

    for i in range(max_field_count):
        if i == 255:
            gdal.ErrorReset()
            gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
        ret = lyr.CreateField(ogr.FieldDefn('foo%d' % i, ogr.OFTString))
        if i == 255:
            gdal.PopErrorHandler()
            if gdal.GetLastErrorMsg() == '':
                gdaltest.post_reason('expecting a warning for 256th field added')
                return 'fail'
        if ret != 0:
            gdaltest.post_reason('failed creating field foo%d' % i)
            return 'fail'

    i = max_field_count
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    ret = lyr.CreateField(ogr.FieldDefn('foo%d' % i, ogr.OFTString))
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('should have failed creating field foo%d' % i)
        return 'fail'

    feat = ogr.Feature(lyr.GetLayerDefn())
    for i in range(max_field_count):
        feat.SetField(i, 'foo%d' % i)
    lyr.CreateFeature(feat)

    feat = ogr.Feature(lyr.GetLayerDefn())
    for i in range(max_field_count):
        feat.SetField(i, 'foo%d' % i)
    lyr.CreateFeature(feat)

    ds = None

    return 'success'

###############################################################################
# Test that we emit a warning if the truncation of a field value occurs

def ogr_shape_57():
    shape_drv = ogr.GetDriverByName('ESRI Shapefile')
    ds_name = '/vsimem/ogr_shape_57'
    ds = shape_drv.CreateDataSource( ds_name )
    lyr = ds.CreateLayer('ogr_shape_57')

    field_defn = ogr.FieldDefn('foo', ogr.OFTString)
    field_defn.SetWidth(1024)

    gdal.ErrorReset()
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    lyr.CreateField(field_defn)
    gdal.PopErrorHandler()
    #print(gdal.GetLastErrorMsg())
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('expecting a warning')
        return 'fail'

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, '0123456789'.join(['' for i in range(27)]))

    gdal.ErrorReset()
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    lyr.CreateFeature(feat)
    gdal.PopErrorHandler()
    #print(gdal.GetLastErrorMsg())
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('expecting a warning')
        return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test creating and reading back all geometry types

def ogr_shape_58():
    shape_drv = ogr.GetDriverByName('ESRI Shapefile')
    ds_name = '/vsimem/ogr_shape_58'
    ds = shape_drv.CreateDataSource( ds_name )

    wkt_list = [ 'POINT (0 1)',
                 'POINT (0 1 2)',
                 'MULTIPOINT (0 1,2 3)',
                 'MULTIPOINT (0 1 2,3 4 5)',
                 'LINESTRING (0 1,2 3)',
                 'LINESTRING (0 1 2,3 4 5)',
                 'MULTILINESTRING ((0 1,2 3),(0 1,2 3))',
                 'MULTILINESTRING ((0 1 2,3 4 5),(0 1 2,3 4 5))',
                 'POLYGON ((0 0,0 1,1 1,1 0,0 0))',
                 'POLYGON ((0 0 2,0 1 2,1 1 2,1 0 2,0 0 2))',
                 'MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0)),((0 0,0 1,1 1,1 0,0 0)))',
                 'MULTIPOLYGON (((0 0 2,0 1 2,1 1 2,1 0 2,0 0 2)),((0 0 2,0 1 2,1 1 2,1 0 2,0 0 2)))']

    for wkt in wkt_list:
        geom = ogr.CreateGeometryFromWkt(wkt)
        layer_name = geom.GetGeometryName()
        if geom.GetGeometryType() & ogr.wkb25Bit:
            layer_name = layer_name + "3D"
        lyr = ds.CreateLayer(layer_name)
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetGeometry(geom)
        lyr.CreateFeature(feat)

    ds = None

    ds = ogr.Open('/vsimem/ogr_shape_58')

    for wkt in wkt_list:
        geom = ogr.CreateGeometryFromWkt(wkt)
        layer_name = geom.GetGeometryName()
        if geom.GetGeometryType() & ogr.wkb25Bit:
            layer_name = layer_name + "3D"
        lyr = ds.GetLayerByName(layer_name)
        lyr.ResetReading()
        feat = lyr.GetNextFeature()
        geom_read = feat.GetGeometryRef()
        if geom_read.ExportToWkt() != wkt:
            gdaltest.post_reason('did not get expectet geom for field %s' % layer_name)
            print(geom_read.ExportToWkt())
            return 'fail'

    ds = None

    return 'success'

###############################################################################
# Test reading a shape with XYM geometries

def ogr_shape_59():

    if gdaltest.shape_ds is None:
        return 'skip'

    shp_ds = ogr.Open( 'data/testpointm.shp' )
    if shp_ds is None:
        return 'skip'
    shp_lyr = shp_ds.GetLayer(0)

    feat = shp_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()

    if geom.GetGeometryName() != 'POINT':
        gdaltest.post_reason( 'Geometry of wrong type.' )
        return 'fail'

    if geom.GetCoordinateDimension() != 3:
        gdaltest.post_reason( 'dimension wrong.' )
        return 'fail'

    if geom.GetPoint(0) != (1.0,2.0,3.0):
        print(geom.GetPoint(0))
        gdaltest.post_reason( 'Did not get right point result.' )
        return 'fail'

    geom = None
    feat = None

    shp_ds.Destroy()

    return 'success'

###############################################################################
# Test reading a shape with XYZM geometries

def ogr_shape_60():

    if gdaltest.shape_ds is None:
        return 'skip'

    shp_ds = ogr.Open( 'data/testpointzm.shp' )
    if shp_ds is None:
        return 'skip'
    shp_lyr = shp_ds.GetLayer(0)

    feat = shp_lyr.GetNextFeature()
    geom = feat.GetGeometryRef()

    if geom.GetGeometryName() != 'POINT':
        gdaltest.post_reason( 'Geometry of wrong type.' )
        return 'fail'

    if geom.GetCoordinateDimension() != 3:
        gdaltest.post_reason( 'dimension wrong.' )
        return 'fail'

    if geom.GetPoint(0) != (1.0,2.0,3.0):
        print(geom.GetPoint(0))
        gdaltest.post_reason( 'Did not get right point result.' )
        return 'fail'

    geom = None
    feat = None

    shp_ds.Destroy()

    return 'success'

###############################################################################
# Test field auto-growing

def ogr_shape_61():
    shape_drv = ogr.GetDriverByName('ESRI Shapefile')
    ds_name = '/vsimem/ogr_shape_61'
    ds = shape_drv.CreateDataSource( ds_name )
    lyr = ds.CreateLayer('ogr_shape_61')

    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))

    field_defn = ogr.FieldDefn('intfield', ogr.OFTInteger)
    field_defn.SetWidth(1)
    lyr.CreateField(field_defn)

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, ''.join(['0123456789' for i in range(8)]))
    feat.SetField(1, 2)
    lyr.CreateFeature(feat)
    feat = None

    field_defn = lyr.GetLayerDefn().GetFieldDefn(0)
    if field_defn.GetWidth() != 80:
        gdaltest.post_reason('did not get initial field size')
        print(field_defn.GetWidth())
        return 'fail'

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, ''.join(['0123456789' for i in range(9)]))
    feat.SetField(1, 34)
    lyr.CreateFeature(feat)
    feat = None

    field_defn = lyr.GetLayerDefn().GetFieldDefn(0)
    if field_defn.GetWidth() != 90:
        gdaltest.post_reason('did not extend field')
        print(field_defn.GetWidth())
        return 'fail'

    field_defn = lyr.GetLayerDefn().GetFieldDefn(1)
    if field_defn.GetWidth() != 2:
        gdaltest.post_reason('did not extend field')
        print(field_defn.GetWidth())
        return 'fail'

    ds = None

    ds = ogr.Open(ds_name)
    lyr = ds.GetLayer(0)
    field_defn = lyr.GetLayerDefn().GetFieldDefn(0)
    if field_defn.GetWidth() != 90:
        gdaltest.post_reason('did not get expected field size')
        print(field_defn.GetWidth())
        return 'fail'

    feat = lyr.GetFeature(1)
    val = feat.GetFieldAsString(0)
    if val != ''.join(['0123456789' for i in range(9)]):
        gdaltest.post_reason('did not get expected field value')
        print(val)
        return 'fail'
    val = feat.GetFieldAsInteger(1)
    if val != 34:
        gdaltest.post_reason('did not get expected field value')
        print(val)
        return 'fail'
        
    return 'success'

###############################################################################
# Test field resizing

def ogr_shape_62():
    shape_drv = ogr.GetDriverByName('ESRI Shapefile')
    ds_name = '/vsimem/ogr_shape_62'
    ds = shape_drv.CreateDataSource( ds_name )
    lyr = ds.CreateLayer('ogr_shape_62', options = ['RESIZE=YES'] )

    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn('bar', ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn('baz', ogr.OFTInteger))

    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0, 'hugehugehugehuge')
    lyr.CreateFeature(feat)
    feat = None

    lyr.DeleteFeature(0)

    values = [ 'ab', 'deef', 'ghi' ]
    for value in values:
        feat = ogr.Feature(lyr.GetLayerDefn())
        feat.SetField(0, value)
        feat.SetField(2, 12)
        lyr.CreateFeature(feat)
        feat = None

    ds = None

    # Reopen file
    ds = ogr.Open(ds_name)
    lyr = ds.GetLayer(0)

    # Check
    field_defn = lyr.GetLayerDefn().GetFieldDefn(0)
    if field_defn.GetWidth() != 4:
        gdaltest.post_reason('did not get expected field size')
        print(field_defn.GetWidth())
        return 'fail'

    # Reopen file
    ds = ogr.Open(ds_name, update = 1)
    lyr = ds.GetLayer(0)

    # Should do nothing
    ds.ExecuteSQL('RESIZE ogr_shape_62')

    # Check
    lyr.ResetReading()
    for expected_value in values:
        feat = lyr.GetNextFeature()
        got_val = feat.GetFieldAsString(0)
        if got_val != expected_value:
            gdaltest.post_reason('did not get expected value')
            print(got_val)
            return 'fail'
        got_val = feat.GetFieldAsInteger(2)
        if got_val != 12:
            gdaltest.post_reason('did not get expected value')
            print(got_val)
            return 'fail'

    ds = None

    return 'success'

###############################################################################
# More testing of recoding

def ogr_shape_63():

    import struct

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('/vsimem/ogr_shape_63.dbf')
    lyr = ds.CreateLayer('ogr_shape_63')
    gdaltest.fieldname = '\xc3\xa9'
    if lyr.CreateField(ogr.FieldDefn(gdaltest.fieldname, ogr.OFTString)) != 0:
        gdaltest.post_reason('failed')
        return 'fail'

    gdaltest.fieldname = '\xc3\xa9\xc3\xa9'
    if lyr.AlterFieldDefn(0, ogr.FieldDefn(gdaltest.fieldname, ogr.OFTString), ogr.ALTER_NAME_FLAG) != 0:
        gdaltest.post_reason('failed')
        return 'fail'

    chinese_str = struct.pack('B' * 6, 229, 144, 141, 231, 167, 176)
    if sys.version_info >= (3,0,0):
        chinese_str = chinese_str.decode('UTF-8')

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.AlterFieldDefn(0, ogr.FieldDefn(chinese_str, ogr.OFTString), ogr.ALTER_NAME_FLAG)
    gdal.PopErrorHandler()

    if ret == 0:
        gdaltest.post_reason('failed')
        return 'fail'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.CreateField(ogr.FieldDefn(chinese_str, ogr.OFTString))
    gdal.PopErrorHandler()

    if ret == 0:
        gdaltest.post_reason('failed')
        return 'fail'

    ds = None

    ds = ogr.Open('/vsimem/ogr_shape_63.dbf')
    lyr = ds.GetLayer(0)
    if lyr.TestCapability(ogr.OLCStringsAsUTF8) != 1:
        gdaltest.post_reason('failed')
        return 'fail'
    if lyr.GetLayerDefn().GetFieldDefn(0).GetName() != gdaltest.fieldname:
        gdaltest.post_reason('failed')
        print(gdaltest.fieldname)
        return 'fail'
    ds = None

    # Set an invalid encoding
    gdal.FileFromMemBuffer('/vsimem/ogr_shape_63.cpg', 'FOO')

    ds = ogr.Open('/vsimem/ogr_shape_63.dbf')
    lyr = ds.GetLayer(0)
    # TestCapability(OLCStringsAsUTF8) should return FALSE
    if lyr.TestCapability(ogr.OLCStringsAsUTF8) != 0:
        gdaltest.post_reason('failed')
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/ogr_shape_63.dbf')
    gdal.Unlink('/vsimem/ogr_shape_63.cpg')

    return 'success'

###############################################################################
# Test creating layers whose name include dot character

def ogr_shape_64():

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('/vsimem/ogr_shape_64')

    lyr = ds.CreateLayer('a.b')
    if lyr.GetName() != 'a.b':
        gdaltest.post_reason('failed')
        return 'fail'
    lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField('foo', 'bar')
    lyr.CreateFeature(feat)
    feat = None

    lyr = ds.CreateLayer('a.c')
    if lyr.GetName() != 'a.c':
        gdaltest.post_reason('failed')
        return 'fail'

    # Test that we cannot create a duplicate layer
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    lyr = ds.CreateLayer('a.b')
    gdal.PopErrorHandler()
    if lyr is not None:
        gdaltest.post_reason('failed')
        return 'fail'

    ds = None

    ds = ogr.Open('/vsimem/ogr_shape_64/a.b.shp')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    if feat.GetFieldAsString('foo') != 'bar':
        gdaltest.post_reason('failed')
        return 'fail'
    ds = None

    gdal.Unlink('/vsimem/ogr_shape_64/a.b.shp')
    gdal.Unlink('/vsimem/ogr_shape_64/a.b.shx')
    gdal.Unlink('/vsimem/ogr_shape_64/a.b.dbf')
    gdal.Unlink('/vsimem/ogr_shape_64/a.c.shp')
    gdal.Unlink('/vsimem/ogr_shape_64/a.c.shx')
    gdal.Unlink('/vsimem/ogr_shape_64/a.c.dbf')
    gdal.Unlink('/vsimem/ogr_shape_64')

    return 'success'

###############################################################################
# Test reading a DBF with a 'nan' as a numeric value (#4799)

def ogr_shape_65():

    ds = ogr.Open('data/nan.dbf')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    val = feat.GetFieldAsDouble(0)
    feat = None
    ds = None
    
    if not gdaltest.isnan(val):
        print(val)
        return 'fail'

    return 'success'

###############################################################################
# Test failures when creating files and datasources

def ogr_shape_66():

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('/i_dont_exist/bar.dbf')
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr = ds.CreateLayer('bar', geom_type = ogr.wkbNone)
    gdal.PopErrorHandler()
    if lyr is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('/i_dont_exist/bar.shp')
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr = ds.CreateLayer('bar', geom_type = ogr.wkbPoint)
    gdal.PopErrorHandler()
    if lyr is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None
    
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('/i_dont_exist/bar')
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'

    f = open('tmp/foo','wb')
    f.close()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('tmp/foo')
    gdal.PopErrorHandler()
    if ds is not None:
        gdaltest.post_reason('fail')
        return 'fail'
    os.unlink('tmp/foo')

    return 'success'

###############################################################################
# Test opening an empty .sbn spatial index

def ogr_shape_67():

    shutil.copy('data/emptyshapefilewithsbn.shp', 'tmp/emptyshapefilewithsbn.shp')
    shutil.copy('data/emptyshapefilewithsbn.shx', 'tmp/emptyshapefilewithsbn.shx')
    shutil.copy('data/emptyshapefilewithsbn.sbn', 'tmp/emptyshapefilewithsbn.sbn')
    shutil.copy('data/emptyshapefilewithsbn.sbx', 'tmp/emptyshapefilewithsbn.sbx')

    ds = ogr.Open('tmp/emptyshapefilewithsbn.shp', update = 1)
    ds.ExecuteSQL('DROP SPATIAL INDEX ON emptyshapefilewithsbn')
    ds = None
    
    try:
        os.stat('tmp/emptyshapefilewithsbn.sbn')
        return 'fail'
    except:
        pass
    
    os.unlink('tmp/emptyshapefilewithsbn.shp')
    os.unlink('tmp/emptyshapefilewithsbn.shx')

    return 'success'
    
###############################################################################
# Test opening a shape datasource with files with mixed case and then REPACK

def ogr_shape_68():

    if sys.platform == 'darwin':
        print("Fails on MacOSX. Not sure why.")
        return 'skip'

    for i in range(2):
        if i == 1 and sys.platform != 'win32':
            break

        try:
            shutil.rmtree('tmp/mixedcase')
        except:
            pass
        os.mkdir('tmp/mixedcase')
        shutil.copy('data/poly.shp', 'tmp/mixedcase/mixedcase.shp')
        shutil.copy('data/poly.shx', 'tmp/mixedcase/mixedcase.shx')
        shutil.copy('data/poly.dbf', 'tmp/mixedcase/MIXEDCASE.DBF') # funny !

        ds = ogr.Open('tmp/mixedcase', update = 1)
        if sys.platform == 'win32':
            expected_layer_count = 1
        else:
            expected_layer_count = 2
        if ds.GetLayerCount() != expected_layer_count:
            gdaltest.post_reason('expected %d layers, got %d' % (expected_layer_count, ds.GetLayerCount()))
            return 'fail'
        if i == 1:
            lyr = ds.GetLayerByName('mixedcase')
        else:
            lyr = ds.GetLayerByName('MIXEDCASE')
        lyr.DeleteFeature(0)
        if i == 1:
            ds.ExecuteSQL( 'REPACK mixedcase' )
        else:
            ds.ExecuteSQL( 'REPACK MIXEDCASE' )

        if sys.platform == 'win32':
            if lyr.GetGeomType() != ogr.wkbPolygon:
                gdaltest.post_reason('fail')
                return 'fail'
        else:
            if lyr.GetGeomType() != ogr.wkbNone:
                gdaltest.post_reason('fail')
                return 'fail'
            lyr = ds.GetLayerByName('mixedcase')
            if lyr.GetGeomType() != ogr.wkbPolygon:
                gdaltest.post_reason('fail')
                return 'fail'
            gdal.PushErrorHandler('CPLQuietErrorHandler')
            ret = lyr.DeleteFeature(0)
            gdal.PopErrorHandler()
            if ret == 0:
                gdaltest.post_reason('expected failure on DeleteFeature()')
                return 'fail'
            gdal.ErrorReset()
            gdal.PushErrorHandler('CPLQuietErrorHandler')
            ds.ExecuteSQL( 'REPACK mixedcase' )
            gdal.PopErrorHandler()
            if gdal.GetLastErrorMsg() == '':
                gdaltest.post_reason('expected failure on REPACK mixedcase')
                return 'fail'

        ds = None
        
        ori_shp_size = os.stat('data/poly.shp').st_size
        ori_shx_size = os.stat('data/poly.shx').st_size
        ori_dbf_size = os.stat('data/poly.dbf').st_size

        new_shp_size = os.stat('tmp/mixedcase/mixedcase.shp').st_size
        new_shx_size = os.stat('tmp/mixedcase/mixedcase.shx').st_size
        new_dbf_size = os.stat('tmp/mixedcase/MIXEDCASE.DBF').st_size

        if new_dbf_size == ori_dbf_size:
            gdaltest.post_reason('fail')
            return 'fail'

        if sys.platform == 'win32':
            if new_shp_size == ori_shp_size:
                gdaltest.post_reason('fail')
                return 'fail'
            if new_shx_size == ori_shx_size:
                gdaltest.post_reason('fail')
                return 'fail'
        else:
            if new_shp_size != ori_shp_size:
                gdaltest.post_reason('fail')
                return 'fail'
            if new_shx_size != ori_shx_size:
                gdaltest.post_reason('fail')
                return 'fail'

    return 'success'

###############################################################################
# Test fix for #5135 (creating a field of type Integer with a big width)

def ogr_shape_69():

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('/vsimem/ogr_shape_69.shp')
    lyr = ds.CreateLayer('ogr_shape_69')
    field_defn = ogr.FieldDefn('intfield', ogr.OFTInteger)
    field_defn.SetWidth(64)
    lyr.CreateField(field_defn)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetField(0,123456)
    lyr.CreateFeature(feat)
    feat = None
    ds = None

    ds = ogr.Open('/vsimem/ogr_shape_69.shp')
    lyr = ds.GetLayer(0)
    if lyr.GetLayerDefn().GetFieldDefn(0).GetType() != ogr.OFTReal:
        return 'fail'
    feat = lyr.GetNextFeature()
    if feat.GetField(0) != 123456:
        return 'fail'
    ds = None

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('/vsimem/ogr_shape_69.shp')

    return 'success'

###############################################################################
# Test fix for https://github.com/OSGeo/gdal/pull/17
# (shapefile opened twice on Windows)

def ogr_shape_70():

    if sys.platform != 'win32':
        return 'skip'

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('tmp/ogr_shape_70.shp')
    lyr = ds.CreateLayer('ogr_shape_70')
    field_defn = ogr.FieldDefn('intfield', ogr.OFTInteger)
    lyr.CreateField(field_defn)
    feat = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(feat)
    fid = feat.GetFID()
    feat = None
    lyr.DeleteFeature(fid)

    # Locks the file. No way to do this on Unix easily
    f = open('tmp/ogr_shape_70.dbf', 'r+')

    gdal.ErrorReset()
    gdal.PushErrorHandler()
    ds.ExecuteSQL('REPACK ogr_shape_70')
    gdal.PopErrorHandler()
    errmsg = gdal.GetLastErrorMsg()
    ds = None

    f.close()

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource('tmp/ogr_shape_70.shp')

    if errmsg == '':
        return 'fail'

    return 'success'

###############################################################################
# Test heterogenous file permissions on .shp and .dbf

def ogr_shape_71():

    if sys.platform.find('linux') != 0:
        return 'skip'

    import stat
    shutil.copy('data/poly.shp', 'tmp/ogr_shape_71.shp')
    shutil.copy('data/poly.shx', 'tmp/ogr_shape_71.shx')
    shutil.copy('data/poly.dbf', 'tmp/ogr_shape_71.dbf')
    old_mode = os.stat('tmp/ogr_shape_71.dbf').st_mode
    os.chmod('tmp/ogr_shape_71.dbf', stat.S_IREAD)
    ds = ogr.Open('tmp/ogr_shape_71.shp', update = 1)
    ok = ds is None
    ds = None
    os.chmod('tmp/ogr_shape_71.dbf', old_mode)

    ogr.GetDriverByName('ESRI Shapefile').DeleteDataSource( 'tmp/ogr_shape_71.shp' )

    if not ok:
        return 'fail'

    return 'success'

###############################################################################
# Test shapefile size limit

def ogr_shape_72():

    # Determine if the filesystem supports sparse files (we don't want to create a real 3 GB
    # file !
    if (gdaltest.filesystem_supports_sparse_files('tmp') == False):
        return 'skip'

    import struct
    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('tmp/ogr_shape_72.shp')
    lyr = ds.CreateLayer('2gb', geom_type = ogr.wkbPoint)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT (1 2)'))
    lyr.CreateFeature(feat)
    ds = None

    f = open('tmp/ogr_shape_72.shp', 'rb+')
    f.seek(24)
    f.write(struct.pack('B' * 4, 0x7f,0xff,0xff,0xfe))
    f.close()

    # Test creating a feature over 4 GB file limit -> should fail
    ds = ogr.Open('tmp/ogr_shape_72.shp', update = 1)
    lyr = ds.GetLayer(0)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT (3 4)'))
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.CreateFeature(feat)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    f = open('tmp/ogr_shape_72.shp', 'rb+')
    f.seek(24)
    f.write(struct.pack('B' * 4, 0x3f,0xff,0xff,0xfe))
    f.close()

    # Test creating a feature over 2 GB file limit -> should fail
    gdal.SetConfigOption('SHAPE_2GB_LIMIT', 'TRUE')
    ds = ogr.Open('tmp/ogr_shape_72.shp', update = 1)
    gdal.SetConfigOption('SHAPE_2GB_LIMIT', None)
    lyr = ds.GetLayer(0)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT (5 6)'))
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.CreateFeature(feat)
    gdal.PopErrorHandler()
    if ret == 0:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    # Test creating a feature over 2 GB file limit -> should succeed with warning
    ds = ogr.Open('tmp/ogr_shape_72.shp', update = 1)
    lyr = ds.GetLayer(0)
    feat = ogr.Feature(lyr.GetLayerDefn())
    feat.SetGeometry(ogr.CreateGeometryFromWkt('POINT (7 8)'))
    gdal.ErrorReset()
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ret = lyr.CreateFeature(feat)
    gdal.PopErrorHandler()
    if ret != 0:
        gdaltest.post_reason('fail')
        return 'fail'
    if gdal.GetLastErrorMsg().find('2GB file size limit reached') < 0:
        gdaltest.post_reason('did not find expected warning')
        return 'fail'
    ds = None

    ds = ogr.Open('tmp/ogr_shape_72.shp')
    lyr = ds.GetLayer(0)
    feat = lyr.GetFeature(1)
    if feat.GetGeometryRef().ExportToWkt() != 'POINT (7 8)':
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test that isClockwise() works correctly on a degenerated ring that passes
# twice by the same point (#5342)

def ogr_shape_73():

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('/vsimem/ogr_shape_73.shp')
    lyr = ds.CreateLayer('ogr_shape_73', geom_type = ogr.wkbPolygon)
    feat = ogr.Feature(lyr.GetLayerDefn())
    # (5 1) is the first(and last) point, and the pivot point selected by the
    # algorithm (lowest rightmost vertex), but is is also reused later in the
    # coordinate list
    # But the second ring is counter-clock-wise
    geom = ogr.CreateGeometryFromWkt('POLYGON ((0 0,0 10,10 10,10 0,0 0),(5 1,4 3,4 2,5 1,6 2,6 3,5 1))')
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)
    feat = None
    ds = None
    
    ds = ogr.Open('/vsimem/ogr_shape_73.shp')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    got_geom = feat.GetGeometryRef()
    if geom.ExportToWkt() != got_geom.ExportToWkt():
        feat.DumpReadable()
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test organizePolygons() in OGR_ORGANIZE_POLYGONS=DEFAULT mode when
# two outer rings are touching, by the first vertex of one.

def ogr_shape_74():

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('/vsimem/ogr_shape_74.shp')
    lyr = ds.CreateLayer('ogr_shape_74', geom_type = ogr.wkbPolygon)
    feat = ogr.Feature(lyr.GetLayerDefn())
    geom = ogr.CreateGeometryFromWkt('MULTIPOLYGON (((0 10,10 10,10 0,0 0,0 1,9 1,9 9,0 9,0 10)),((9 5,5 4,0 5,5 6, 9 5)))')
    feat.SetGeometry(geom)
    lyr.CreateFeature(feat)
    feat = None
    ds = None

    ds = ogr.Open('/vsimem/ogr_shape_74.shp')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    got_geom = feat.GetGeometryRef()
    if geom.ExportToWkt() != got_geom.ExportToWkt():
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'

    lyr.ResetReading()
    gdal.SetConfigOption('OGR_ORGANIZE_POLYGONS', 'DEFAULT')
    feat = lyr.GetNextFeature()
    gdal.SetConfigOption('OGR_ORGANIZE_POLYGONS', None)
    got_geom = feat.GetGeometryRef()
    if geom.ExportToWkt() != got_geom.ExportToWkt():
        gdaltest.post_reason('fail')
        feat.DumpReadable()
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test GetFileList()

def ogr_shape_75():

    ds = gdal.OpenEx('data/poly.shp')
    if ds.GetFileList() != ['data/poly.shp', 'data/poly.shx', 'data/poly.dbf', 'data/poly.PRJ'] and \
       ds.GetFileList() != ['data/poly.shp', 'data/poly.shx', 'data/poly.dbf', 'data/poly.prj']:
        gdaltest.post_reason('fail')
        print(ds.GetFileList())
        return 'failure'
    ds = None

    ds = gdal.OpenEx('data/idlink.dbf')
    if ds.GetFileList() != ['data/idlink.dbf']:
        gdaltest.post_reason('fail')
        print(ds.GetFileList())
        return 'failure'
    ds = None

    ds = gdal.OpenEx('data/testpoly.shp')
    if ds.GetFileList() != ['data/testpoly.shp', 'data/testpoly.shx', 'data/testpoly.dbf', 'data/testpoly.qix']:
        gdaltest.post_reason('fail')
        print(ds.GetFileList())
        return 'failure'
    ds = None

    ds = gdal.OpenEx('data/emptyshapefilewithsbn.shx')
    if ds.GetFileList() != ['data/emptyshapefilewithsbn.shp', 'data/emptyshapefilewithsbn.shx', 'data/emptyshapefilewithsbn.sbn', 'data/emptyshapefilewithsbn.sbx']:
        gdaltest.post_reason('fail')
        print(ds.GetFileList())
        return 'failure'
    ds = None

    return 'success'

###############################################################################
# Test opening shapefile whose .prj has a UTF-8 BOM marker

def ogr_shape_76():

    ds = ogr.Open('data/prjwithutf8bom.shp')
    lyr = ds.GetLayer(0)
    sr = lyr.GetSpatialRef()
    if sr.ExportToWkt().find('GEOGCS["GCS_North_American_1983"') != 0:
        return 'failure'

    return 'success'

###############################################################################
# Test opening shapefile whose .shx doesn't follow the official shapefile spec (#5608)

def ogr_shape_77():

    ds = ogr.Open('data/nonconformant_shx_ticket5608.shp')
    lyr = ds.GetLayer(0)
    feat = lyr.GetNextFeature()
    geom = feat.GetGeometryRef()
    if geom.ExportToWkt() != 'LINESTRING (0 1,2 3)':
        return 'failure'

    return 'success'

###############################################################################
# Test writing integer values through double fields, and cases of truncation or
# loss of precision (#5625)

def ogr_shape_78():

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('/vsimem/ogr_shape_78.dbf')
    lyr = ds.CreateLayer('ogr_shape_78')

    fd = ogr.FieldDefn('dblfield', ogr.OFTReal)
    fd.SetWidth(20)
    lyr.CreateField(fd)
    
    fd = ogr.FieldDefn('dblfield2', ogr.OFTReal)
    fd.SetWidth(20)
    fd.SetPrecision(1)
    lyr.CreateField(fd)

    # Integer values up to 2^53 can be exactly tansported into a double.
    gdal.ErrorReset()
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('dblfield', (2**53) * 1.0)
    lyr.CreateFeature(f)
    if gdal.GetLastErrorMsg() != '':
        gdaltest.post_reason('got unexpected error/warning')
        return 'fail'

    # Field width too small
    gdal.ErrorReset()
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('dblfield2', 1e21)
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('did not get expected error/warning')
        return 'fail'

    # Likely precision loss
    gdal.ErrorReset()
    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetField('dblfield', (2**53) * 1.0 + 2)  # 2^53+1 == 2^53 !
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    lyr.CreateFeature(f)
    gdal.PopErrorHandler()
    if gdal.GetLastErrorMsg() == '':
        gdaltest.post_reason('did not get expected error/warning')
        return 'fail'

    gdal.ErrorReset()
    ds = None
    
    ds = ogr.Open('/vsimem/ogr_shape_78.dbf')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f.GetField('dblfield') != 9007199254740992.:
        gdaltest.post_reason('did not get expected value')
        f.DumpReadable()
        return 'fail'
    ds = None

    return 'success'

###############################################################################
# Test adding a field after creating features with 0 field

def ogr_shape_79():

    ds = ogr.GetDriverByName('ESRI Shapefile').CreateDataSource('/vsimem/ogr_shape_79.dbf')
    lyr = ds.CreateLayer('ogr_shape_79')

    # This will create a (for now) invisible 'FID' field
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))

    # This will delete the implicit field
    fd = ogr.FieldDefn('field1', ogr.OFTReal)
    lyr.CreateField(fd)
    fd = ogr.FieldDefn('field2', ogr.OFTReal)
    lyr.CreateField(fd)

    # If the implicit field isn't deleted, this will cause crash
    lyr.ReorderField(0,1)

    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))

    ds = None

    ds = ogr.Open('/vsimem/ogr_shape_79.dbf')
    lyr = ds.GetLayer(0)
    if lyr.GetLayerDefn().GetFieldCount() != 2:
        gdaltest.post_reason('fail')
        return 'fail'
    ds = None

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
    shape_drv.DeleteDataSource( 'tmp/UPPERCASE' )
    shape_drv.DeleteDataSource( 'tmp/lowercase' )
    shape_drv.DeleteDataSource( 'tmp/mixedcase' )
    shape_drv.DeleteDataSource( '/vsimem/test35.shp' )
    shape_drv.DeleteDataSource( '/vsimem/ogr_shape_46.shp' )
    shape_drv.DeleteDataSource( '/vsimem/this_one_i_care_46.shp' )
    shape_drv.DeleteDataSource( '/vsimem/ogr_shape_52.shp' )
    shape_drv.DeleteDataSource( '/vsimem/ogr_shape_53.shp' )
    shape_drv.DeleteDataSource( '/vsimem/ogr_shape_54' )
    shape_drv.DeleteDataSource( '/vsimem/ogr_shape_55' )
    shape_drv.DeleteDataSource( '/vsimem/ogr_shape_56' )
    shape_drv.DeleteDataSource( '/vsimem/ogr_shape_57' )
    shape_drv.DeleteDataSource( '/vsimem/ogr_shape_58' )
    shape_drv.DeleteDataSource( '/vsimem/ogr_shape_61' )
    shape_drv.DeleteDataSource( '/vsimem/ogr_shape_62' )
    shape_drv.DeleteDataSource( '/vsimem/ogr_shape_73.shp' )
    shape_drv.DeleteDataSource( '/vsimem/ogr_shape_74.shp' )
    shape_drv.DeleteDataSource( '/vsimem/ogr_shape_78.dbf' )
    shape_drv.DeleteDataSource( '/vsimem/ogr_shape_79.shp' )

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
    ogr_shape_16_1,
    ogr_shape_17,
    ogr_shape_18,
    ogr_shape_19,
    ogr_shape_20,
    ogr_shape_21,
    ogr_shape_22,
    ogr_shape_23,
    ogr_shape_24,
    ogr_shape_25,
    ogr_shape_26,
    ogr_shape_27,
    ogr_shape_28,
    ogr_shape_29,
    ogr_shape_30,
    ogr_shape_31,
    ogr_shape_32,
    ogr_shape_33,
    ogr_shape_34,
    ogr_shape_35,
    ogr_shape_36,
    ogr_shape_37,
    ogr_shape_37_bis,
    ogr_shape_38,
    ogr_shape_39,
    ogr_shape_40,
    ogr_shape_41,
    ogr_shape_42,
    ogr_shape_43,
    ogr_shape_44,
    ogr_shape_45,
    ogr_shape_46,
    ogr_shape_47,
    ogr_shape_48,
    ogr_shape_49,
    ogr_shape_50,
    ogr_shape_51,
    ogr_shape_52,
    ogr_shape_53,
    ogr_shape_54,
    ogr_shape_55,
    ogr_shape_56,
    ogr_shape_57,
    ogr_shape_58,
    ogr_shape_59,
    ogr_shape_60,
    ogr_shape_61,
    ogr_shape_62,
    ogr_shape_63,
    ogr_shape_64,
    ogr_shape_65,
    ogr_shape_66,
    ogr_shape_67,
    ogr_shape_68,
    ogr_shape_69,
    ogr_shape_70,
    ogr_shape_71,
    ogr_shape_72,
    ogr_shape_73,
    ogr_shape_74,
    ogr_shape_75,
    ogr_shape_76,
    ogr_shape_77,
    ogr_shape_78,
    ogr_shape_79,
    ogr_shape_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_shape' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()


#!/usr/bin/env python
###############################################################################
# $Id$
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
import shutil
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

    if not srs_lyr.IsSame(srs):
        print('')
        print('expected = ', srs.ExportToPrettyWkt())
        print('existing = ', srs_lyr.ExportToPrettyWkt())
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

    #######################################################
    # Test writing of a geometry collection
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
    ds.Destroy()
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
    feat.Destroy()

    ds.Destroy()
    ds = None

    # Re-open and check the new value
    ds = ogr.Open('tmp/hugedbf.dbf')
    lyr = ds.GetLayer(0)
    feat = lyr.GetFeature(23900000);
    if feat.GetFieldAsString(0) != 'updated_value':
        print(feat.GetFieldAsString(0))
        return 'fail'
    feat.Destroy()

    ds.Destroy()
    ds = None

    return 'success'
    
###############################################################################
# Test that REPACK doesn't change extension case (#3293)

def ogr_shape_29():

    os.mkdir('tmp/UPPERCASE')
    shutil.copy('data/poly.shp', 'tmp/UPPERCASE/UPPERCASE.SHP')
    shutil.copy('data/poly.shx', 'tmp/UPPERCASE/UPPERCASE.SHX')
    shutil.copy('data/poly.dbf', 'tmp/UPPERCASE/UPPERCASE.DBF')

    ds = ogr.Open('tmp/UPPERCASE', update = 1)
    lyr = ds.GetLayer(0)
    lyr.DeleteFeature(0)
    ds.ExecuteSQL( 'REPACK UPPERCASE' )
    ds.Destroy()
    
    list = gdal.ReadDir('tmp/UPPERCASE')

    if len(list) != 5:
        print(list)
        return 'fail'
        
    for filename in list:
        if filename not in ['.', '..', 'UPPERCASE.SHP', 'UPPERCASE.SHX', 'UPPERCASE.DBF']:
            print(list)
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

    BigFilePath = 'tmp'

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
            print('\b\b\b\b\b\b\b\b\b\b\b\b\b\b',(n/Decimal('228460.0')),'%  ', sys.stdout.flush())
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
    ogr_shape_cleanup ]
 
if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_shape' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()


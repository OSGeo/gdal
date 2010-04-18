#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  MapInfo driver testing.
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
import gdal

###############################################################################
# Create TAB file. 

def ogr_mitab_1():

    gdaltest.mapinfo_drv = ogr.GetDriverByName('MapInfo File')
    gdaltest.mapinfo_ds = gdaltest.mapinfo_drv.CreateDataSource( 'tmp' )

    if gdaltest.mapinfo_ds is not None:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Create table from data/poly.shp

def ogr_mitab_2():

    if gdaltest.mapinfo_ds is None:
        return 'skip'

    #######################################################
    # Create memory Layer
    gdaltest.mapinfo_lyr = gdaltest.mapinfo_ds.CreateLayer( 'tpoly' )

    #######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def( gdaltest.mapinfo_lyr,
                                    [ ('AREA', ogr.OFTReal),
                                      ('EAS_ID', ogr.OFTInteger),
                                      ('PRFEDEA', ogr.OFTString) ] )
    
    #######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature( feature_def = gdaltest.mapinfo_lyr.GetLayerDefn() )

    shp_ds = ogr.Open( 'data/poly.shp' )
    gdaltest.shp_ds = shp_ds
    shp_lyr = shp_ds.GetLayer(0)
    
    feat = shp_lyr.GetNextFeature()
    gdaltest.poly_feat = []
    
    while feat is not None:

        gdaltest.poly_feat.append( feat )

        dst_feat.SetFrom( feat )
        gdaltest.mapinfo_lyr.CreateFeature( dst_feat )

        feat = shp_lyr.GetNextFeature()

    dst_feat.Destroy()
        
    #######################################################
    # Close file.

    gdaltest.mapinfo_ds.Destroy()
    gdaltest.mapinfo_ds = None
    
    return 'success'

###############################################################################
# Verify that stuff we just wrote is still OK.
#
# Note that we allow a fairly significant error since projected
# coordinates are not stored with much precision in Mapinfo format.

def ogr_mitab_3():
    if gdaltest.mapinfo_drv is None:
        return 'skip'

    gdaltest.mapinfo_ds = ogr.Open( 'tmp' )
    gdaltest.mapinfo_lyr = gdaltest.mapinfo_ds.GetLayer(0)

    expect = [168, 169, 166, 158, 165]
    
    gdaltest.mapinfo_lyr.SetAttributeFilter( 'eas_id < 170' )
    tr = ogrtest.check_features_against_list( gdaltest.mapinfo_lyr,
                                              'eas_id', expect )
    gdaltest.mapinfo_lyr.SetAttributeFilter( None )

    for i in range(len(gdaltest.poly_feat)):
        orig_feat = gdaltest.poly_feat[i]
        read_feat = gdaltest.mapinfo_lyr.GetNextFeature()

        if ogrtest.check_feature_geometry(read_feat,orig_feat.GetGeometryRef(),
                                          max_error = 0.02 ) != 0:
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
# Test ExecuteSQL() results layers with geometry.

def ogr_mitab_4():

    if gdaltest.mapinfo_ds is None:
        return 'skip'

    sql_lyr = gdaltest.mapinfo_ds.ExecuteSQL( \
        'select * from tpoly where prfedea = "35043413"' )

    tr = ogrtest.check_features_against_list( sql_lyr, 'prfedea', [ '35043413' ] )
    if tr:
        sql_lyr.ResetReading()
        feat_read = sql_lyr.GetNextFeature()
        if ogrtest.check_feature_geometry( feat_read, 'POLYGON ((479750.688 4764702.000,479658.594 4764670.000,479640.094 4764721.000,479735.906 4764752.000,479750.688 4764702.000))', max_error = 0.02 ) != 0:
            tr = 0
        feat_read.Destroy()
        
    gdaltest.mapinfo_ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'
    
###############################################################################
# Test spatial filtering. 

def ogr_mitab_5():

    if gdaltest.mapinfo_ds is None:
        return 'skip'

    gdaltest.mapinfo_lyr.SetAttributeFilter( None )
    
    gdaltest.mapinfo_lyr.SetSpatialFilterRect( 479505, 4763195,
                                               480526, 4762819 )
    
    tr = ogrtest.check_features_against_list( gdaltest.mapinfo_lyr, 'eas_id',
                                              [ 158 ] )

    gdaltest.mapinfo_lyr.SetSpatialFilter( None )

    if tr:
        return 'success'
    else:
        return 'fail'
    
###############################################################################
# Create MIF file. 

def ogr_mitab_6():

    gdaltest.mapinfo_ds.Destroy()
    gdaltest.mapinfo_ds = None
    gdaltest.mapinfo_drv.DeleteDataSource( 'tmp' )

    gdaltest.mapinfo_ds = gdaltest.mapinfo_drv.CreateDataSource( 'tmp/wrk.mif' )

    if gdaltest.mapinfo_ds is not None:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Create table from data/poly.shp

def ogr_mitab_7():

    if gdaltest.mapinfo_ds is None:
        return 'skip'

    #######################################################
    # Create memory Layer
    gdaltest.mapinfo_lyr = gdaltest.mapinfo_ds.CreateLayer( 'tpoly' )

    #######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def( gdaltest.mapinfo_lyr,
                                    [ ('AREA', ogr.OFTReal),
                                      ('EAS_ID', ogr.OFTInteger),
                                      ('PRFEDEA', ogr.OFTString) ] )
    
    #######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature( feature_def = gdaltest.mapinfo_lyr.GetLayerDefn() )

    shp_ds = ogr.Open( 'data/poly.shp' )
    gdaltest.shp_ds = shp_ds
    shp_lyr = shp_ds.GetLayer(0)
    
    feat = shp_lyr.GetNextFeature()
    gdaltest.poly_feat = []
    
    while feat is not None:

        gdaltest.poly_feat.append( feat )

        dst_feat.SetFrom( feat )
        gdaltest.mapinfo_lyr.CreateFeature( dst_feat )

        feat = shp_lyr.GetNextFeature()

    dst_feat.Destroy()
        
    #######################################################
    # Close file.

    gdaltest.mapinfo_ds.Destroy()
    gdaltest.mapinfo_ds = None
    
    return 'success'

###############################################################################
# Verify that stuff we just wrote is still OK.

def ogr_mitab_8():
    if gdaltest.mapinfo_drv is None:
        return 'skip'

    gdaltest.mapinfo_ds = ogr.Open( 'tmp' )
    gdaltest.mapinfo_lyr = gdaltest.mapinfo_ds.GetLayer(0)

    expect = [168, 169, 166, 158, 165]
    
    gdaltest.mapinfo_lyr.SetAttributeFilter( 'eas_id < 170' )
    tr = ogrtest.check_features_against_list( gdaltest.mapinfo_lyr,
                                              'eas_id', expect )
    gdaltest.mapinfo_lyr.SetAttributeFilter( None )

    for i in range(len(gdaltest.poly_feat)):
        orig_feat = gdaltest.poly_feat[i]
        read_feat = gdaltest.mapinfo_lyr.GetNextFeature()

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
# Read mif file with 2 character .mid delimeter and verify operation.

def ogr_mitab_9():
    if gdaltest.mapinfo_drv is None:
        return 'skip'

    ds = ogr.Open( 'data/small.mif' )
    lyr = ds.GetLayer(0)

    feat = lyr.GetNextFeature()

    if feat.NAME != " S. 11th St.":
        gdaltest.post_reason( 'name attribute wrong.' )
        return 'fail'
    
    if feat.FLOODZONE != 10:
        gdaltest.post_reason( 'FLOODZONE attribute wrong.' )
        return 'fail'
    
    if ogrtest.check_feature_geometry(feat,
                                      'POLYGON ((407131.721 155322.441,407134.468 155329.616,407142.741 155327.242,407141.503 155322.467,407140.875 155320.049,407131.721 155322.441))',
                                      max_error = 0.000000001 ) != 0:
        return 'fail'

    feat = lyr.GetNextFeature()

    if feat.OWNER != 'Guarino "Chucky" Sandra':
        gdaltest.post_reason( 'owner attribute wrong.' )
        return 'fail'
    
    lyr = None
    ds.Destroy()

    return 'success'
    
###############################################################################
# Verify support for NTF datum with non-greenwich datum per
#    http://trac.osgeo.org/gdal/ticket/1416
#
# This test also excercises srs reference counting as described in issue:
#    http://trac.osgeo.org/gdal/ticket/1680 

def ogr_mitab_10():
    
    if gdaltest.mapinfo_drv is None:
        return 'skip'

    ds = ogr.Open( 'data/small_ntf.mif' )
    srs = ds.GetLayer(0).GetSpatialRef()
    ds = None

    pm_value = srs.GetAttrValue( 'PROJCS|GEOGCS|PRIMEM',1)
    if pm_value[:6] != '2.3372':
        gdaltest.post_reason( 'got unexpected prime meridian, not paris: ' + pm_value )
        return 'fail'

    return 'success'

###############################################################################
# Verify that a newly created mif layer returns a non null layer definition

def ogr_mitab_11():
    
    if gdaltest.mapinfo_drv is None:
        return 'skip'

    ds = gdaltest.mapinfo_drv.CreateDataSource( 'tmp', options = ['FORMAT=MIF'] )
    lyr = ds.CreateLayer( 'testlyrdef' )
    defn = lyr.GetLayerDefn()

    if defn is None:
        return 'fail'

    ogrtest.quick_create_layer_def( lyr, [ ('AREA', ogr.OFTReal) ] )

    ds.Destroy()    
    return 'success'

###############################################################################
# 

def ogr_mitab_cleanup():

    if gdaltest.mapinfo_ds is None:
        return 'skip'

    gdaltest.mapinfo_ds.Destroy()
    gdaltest.mapinfo_ds = None
    gdaltest.mapinfo_drv.DeleteDataSource( 'tmp' )

    return 'success'

gdaltest_list = [ 
    ogr_mitab_1,
    ogr_mitab_2,
    ogr_mitab_3,
    ogr_mitab_4,
    ogr_mitab_5,
    ogr_mitab_6,
    ogr_mitab_7,
    ogr_mitab_8,
    ogr_mitab_9,
    ogr_mitab_10,
    ogr_mitab_11,
    ogr_mitab_cleanup ]

if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_mitab' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()


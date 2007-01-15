#!/usr/bin/env python
###############################################################################
# $Id: ogr_oci.py,v 1.6 2004/07/09 17:51:09 warmerda Exp $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test Oracle OCI driver functionality.
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
#  $Log: ogr_oci.py,v $
#  Revision 1.6  2004/07/09 17:51:09  warmerda
#  Verify we have the OCI driver configured in.
#
#  Revision 1.5  2003/09/13 04:57:56  warmerda
#  Added extra SRS mapping testings
#
#  Revision 1.4  2003/04/07 14:15:04  warmerda
#  Added spatial test.
#
#  Revision 1.3  2003/04/07 14:05:45  warmerda
#  Added a few more tests.
#
#  Revision 1.2  2003/04/03 19:26:37  warmerda
#  added some tests
#
#  Revision 1.1  2003/04/02 07:42:46  warmerda
#  New
#
#  Revision 1.1  2003/03/05 05:05:10  warmerda
#  New
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
# Open ORACLE.

def ogr_oci_1():

    gdaltest.oci_ds = None
    
    try:
        dods_dr = ogr.GetDriverByName( 'OCI' )
    except:
        return 'skip'
    
    if not os.environ.has_key('OCI_DSNAME'):
        return 'skip'

    gdaltest.oci_ds = ogr.Open( os.environ['OCI_DSNAME'] )

    if gdaltest.oci_ds is not None:
        return 'success'
    else:
        return 'fail'

###############################################################################
# Create Oracle table from data/poly.shp

def ogr_oci_2():

    if gdaltest.oci_ds is None:
        return 'skip'

    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    gdaltest.oci_ds.ExecuteSQL( 'DELLAYER:tpoly' )
    gdal.PopErrorHandler()

    ######################################################
    # Create Oracle Layer
    gdaltest.oci_lyr = gdaltest.oci_ds.CreateLayer( 'tpoly' )

    ######################################################
    # Setup Schema
    ogrtest.quick_create_layer_def( gdaltest.oci_lyr,
                                    [ ('AREA', ogr.OFTReal),
                                      ('EAS_ID', ogr.OFTInteger),
                                      ('PRFEDEA', ogr.OFTString) ] )
    
    ######################################################
    # Copy in poly.shp

    dst_feat = ogr.Feature( feature_def = gdaltest.oci_lyr.GetLayerDefn() )

    shp_ds = ogr.Open( 'data/poly.shp' )
    gdaltest.shp_ds = shp_ds
    shp_lyr = shp_ds.GetLayer(0)
    
    feat = shp_lyr.GetNextFeature()
    gdaltest.poly_feat = []
    
    while feat is not None:

        gdaltest.poly_feat.append( feat )

        dst_feat.SetFrom( feat )
        gdaltest.oci_lyr.CreateFeature( dst_feat )

        feat = shp_lyr.GetNextFeature()

    dst_feat.Destroy()
        
    return 'success'

###############################################################################
# Verify that stuff we just wrote is still OK.

def ogr_oci_3():
    if gdaltest.oci_ds is None:
        return 'skip'

    expect = [168, 169, 166, 158, 165]
    
    gdaltest.oci_lyr.SetAttributeFilter( 'eas_id < 170' )
    tr = ogrtest.check_features_against_list( gdaltest.oci_lyr,
                                              'eas_id', expect )
    gdaltest.oci_lyr.SetAttributeFilter( None )

    for i in range(len(gdaltest.poly_feat)):
        orig_feat = gdaltest.poly_feat[i]
        read_feat = gdaltest.oci_lyr.GetNextFeature()

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

def ogr_oci_4():

    if gdaltest.oci_ds is None:
        return 'skip'

    dst_feat = ogr.Feature( feature_def = gdaltest.oci_lyr.GetLayerDefn() )
    wkt_list = [ '10', '2', '1', '3d_1', '4', '5', '6' ]

    for item in wkt_list:

        wkt = open( 'data/wkb_wkt/'+item+'.wkt' ).read()
        geom = ogr.CreateGeometryFromWkt( wkt )
        
        ######################################################################
        # Write geometry as a new Oracle feature.
    
        dst_feat.SetGeometryDirectly( geom )
        dst_feat.SetField( 'PRFEDEA', item )
        gdaltest.oci_lyr.CreateFeature( dst_feat )
        
        ######################################################################
        # Read back the feature and get the geometry.

        gdaltest.oci_lyr.SetAttributeFilter( "PRFEDEA = '%s'" % item )
        feat_read = gdaltest.oci_lyr.GetNextFeature()
        geom_read = feat_read.GetGeometryRef()

        if ogrtest.check_feature_geometry( feat_read, geom ) != 0:
            return 'fail'

        feat_read.Destroy()

    dst_feat.Destroy()
    
    return 'success'
    
###############################################################################
# Test ExecuteSQL() results layers without geometry.

def ogr_oci_5():

    if gdaltest.oci_ds is None:
        return 'skip'

    expect = [ None, 179, 173, 172, 171, 170, 169, 168, 166, 165, 158 ]
    
    sql_lyr = gdaltest.oci_ds.ExecuteSQL( 'select distinct eas_id from tpoly order by eas_id desc' )

    tr = ogrtest.check_features_against_list( sql_lyr, 'eas_id', expect )

    gdaltest.oci_ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'
    
###############################################################################
# Test ExecuteSQL() results layers with geometry.

def ogr_oci_6():

    if gdaltest.oci_ds is None:
        return 'skip'

    sql_lyr = gdaltest.oci_ds.ExecuteSQL( "select * from tpoly where prfedea = '2'" )

    tr = ogrtest.check_features_against_list( sql_lyr, 'prfedea', [ '2' ] )
    if tr:
        sql_lyr.ResetReading()
        feat_read = sql_lyr.GetNextFeature()
        if ogrtest.check_feature_geometry( feat_read, 'MULTILINESTRING ((5.00121349 2.99853132,5.00121349 1.99853133),(5.00121349 1.99853133,5.00121349 0.99853133),(3.00121351 1.99853127,5.00121349 1.99853133),(5.00121349 1.99853133,6.00121348 1.99853135))' ) != 0:
            tr = 0
        feat_read.Destroy()
        
    gdaltest.oci_ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'
    
###############################################################################
# Test spatial filtering. 

def ogr_oci_7():

    if gdaltest.oci_ds is None:
        return 'skip'

    gdaltest.oci_lyr.SetAttributeFilter( None )
    
    geom = ogr.CreateGeometryFromWkt( \
        'LINESTRING(479505 4763195,480526 4762819)' )
    gdaltest.oci_lyr.SetSpatialFilter( geom )
    geom.Destroy()
    
    tr = ogrtest.check_features_against_list( gdaltest.oci_lyr, 'eas_id',
                                              [ 158 ] )

    gdaltest.oci_lyr.SetSpatialFilter( None )
    
    if tr:
        return 'success'
    else:
        return 'fail'
    
###############################################################################
# Test that we can create a layer with a coordinate system that is mapped
# to an oracle coordinate system using the ORACLE authority code.

def ogr_oci_8():

    if gdaltest.oci_ds is None:
        return 'skip'

    #######################################################
    # Preclean.

    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    gdaltest.oci_ds.ExecuteSQL( 'DELLAYER:testsrs' )
    gdal.PopErrorHandler()

    #######################################################
    # Prepare an SRS with an ORACLE authority code.
    srs = osr.SpatialReference()
    srs.SetGeogCS( "gcs_dummy", "datum_dummy", "ellipse_dummy", 
	           osr.SRS_WGS84_SEMIMAJOR, osr.SRS_WGS84_INVFLATTENING )
    srs.SetAuthority( 'GEOGCS', 'Oracle', 8241 )
		
    #######################################################
    # Create Oracle Layer
    oci_lyr2 = gdaltest.oci_ds.CreateLayer( 'testsrs', srs = srs,
		options = [ 'INDEX=FALSE' ] )

    #######################################################
    # Now check that the srs for the layer is really the built-in
    # oracle SRS.
    srs2 = oci_lyr2.GetSpatialRef()

    if srs2.GetAuthorityCode( 'GEOGCS' ) != '8241':
	gdaltest.post_reason( 'Did not get expected authority code' )
	return 'fail'

    if srs2.GetAuthorityName( 'GEOGCS' ) != 'Oracle':
	gdaltest.post_reason( 'Did not get expected authority name' )
	return 'fail'

    if srs2.GetAttrValue( 'GEOGCS|DATUM' ) != 'Kertau 1948':
	gdaltest.post_reason( 'Did not get expected datum name' )
        return 'fail'

    return 'success'
    
###############################################################################
# This time we create a layer with a EPSG marked GEOGCS, and verify that
# the coordinate system gets properly remapped to the Oracle WGS84. 

def ogr_oci_9():

    if gdaltest.oci_ds is None:
        return 'skip'

    #######################################################
    # Preclean.

    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    gdaltest.oci_ds.ExecuteSQL( 'DELLAYER:testsrs2' )
    gdal.PopErrorHandler()

    #######################################################
    # Prepare an SRS with an EPSG authority code.
    srs = osr.SpatialReference()
    srs.SetWellKnownGeogCS( 'WGS84' )
		
    #######################################################
    # Create Oracle Layer
    oci_lyr2 = gdaltest.oci_ds.CreateLayer( 'testsrs2', srs = srs,
		options = [ 'INDEX=FALSE' ] )

    #######################################################
    # Now check that the srs for the layer is really the built-in
    # oracle SRS we expect.
    srs2 = oci_lyr2.GetSpatialRef()

    if srs2.GetAuthorityCode( 'GEOGCS' ) != '4326':
	gdaltest.post_reason( 'Did not get expected authority code' )
	return 'fail'

    if srs2.GetAuthorityName( 'GEOGCS' ) != 'EPSG':
	gdaltest.post_reason( 'Did not get expected authority name' )
	return 'fail'

    if srs2.GetAttrValue( 'GEOGCS|DATUM' ) != 'WGS 84':
	gdaltest.post_reason( 'Did not get expected datum name' )
        return 'fail'

    return 'success'
    
###############################################################################
# 

def ogr_oci_cleanup():

    if gdaltest.oci_ds is None:
        return 'skip'

    gdaltest.oci_ds.ExecuteSQL( 'DELLAYER:tpoly' )
    gdaltest.oci_ds.ExecuteSQL( 'DELLAYER:testsrs' )
    gdaltest.oci_ds.ExecuteSQL( 'DELLAYER:testsrs2' )

    gdaltest.oci_ds.Destroy()
    gdaltest.oci_ds = None

    return 'success'

gdaltest_list = [ 
    ogr_oci_1,
    ogr_oci_2,
    ogr_oci_3,
    ogr_oci_4,
    ogr_oci_5,
    ogr_oci_6,
    ogr_oci_7,
    ogr_oci_8,
    ogr_oci_9,
    ogr_oci_cleanup ]

if __name__ == '__main__':

    if not os.environ.has_key('OCI_DSNAME'):
        print 'Enter ORACLE DataSource (eg. OCI:scott/tiger):'
        oci_dsname = string.strip(sys.stdin.readline())
        os.environ['OCI_DSNAME'] = oci_dsname

    gdaltest.setup_run( 'ogr_oci' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()


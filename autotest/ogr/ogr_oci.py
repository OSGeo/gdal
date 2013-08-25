#!/usr/bin/env python
###############################################################################
# $Id$
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

import os
import sys
import string

sys.path.append( '../pymod' )

import gdaltest
import ogrtest
from osgeo import ogr
from osgeo import osr
from osgeo import gdal

###############################################################################
# Open ORACLE.

def ogr_oci_1():

    gdaltest.oci_ds = None
    
    try:
        dods_dr = ogr.GetDriverByName( 'OCI' )
    except:
        return 'skip'
    
    if 'OCI_DSNAME' not in os.environ:
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
# Helper method to reverse ring winding.  This is needed because the 
# winding direction in shapefiles, and in Oracle is opposite for polygons.

def reverse_rings( poly ):

    for i_ring in range(poly.GetGeometryCount()):
        ring = poly.GetGeometryRef(i_ring)
        v_count = ring.GetPointCount()
        for i_vert in range(v_count/2):
            i_other = v_count - i_vert - 1
            p1 = (ring.GetX(i_vert),ring.GetY(i_vert),ring.GetZ(i_vert))
            ring.SetPoint(i_vert,ring.GetX(i_other),ring.GetY(i_other),ring.GetZ(i_other))
            ring.SetPoint(i_other,p1[0],p1[1],p1[2])
	
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

        reverse_rings(orig_feat.GetGeometryRef())

        if ogrtest.check_feature_geometry(read_feat,orig_feat.GetGeometryRef(),
                                          max_error = 0.000000001 ) != 0:
            print('expected:', orig_feat.GetGeometryRef().ExportToWkt())
            print('got:', read_feat.GetGeometryRef().ExportToWkt())
            return 'fail'

        for fld in range(3):
            if orig_feat.GetField(fld) != read_feat.GetField(fld):
                gdaltest.post_reason( 'Attribute %d does not match' % fld )
                print('expected:')
                print(orig_feat.DumpReadable())
                print('got:')
                print(read_feat.DumpReadable())
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
# Test handling of specialized Oracle Rectangle Geometries.

def ogr_oci_10():

    if gdaltest.oci_ds is None:
        return 'skip'

    # Create a test table.
    gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
    gdaltest.oci_ds.ExecuteSQL( 'drop table geom_test' )
    gdal.PopErrorHandler()

    gdaltest.oci_ds.ExecuteSQL( 'CREATE TABLE geom_test(ora_fid number primary key, shape sdo_geometry)' )

    # insert a rectangle geometry.
    gdaltest.oci_ds.ExecuteSQL( """
INSERT INTO geom_test VALUES(
1,
SDO_GEOMETRY(
2003, -- two-dimensional polygon
NULL,
NULL,
SDO_ELEM_INFO_ARRAY(1,1003,3), -- one rectangle (1003 = exterior)
SDO_ORDINATE_ARRAY(1,1, 5,7) -- only 2 points needed to
-- define rectangle (lower left and upper right) with
-- Cartesian-coordinate data
)
)
""" )

    sql_lyr = gdaltest.oci_ds.ExecuteSQL( 'select * from geom_test where ora_fid = 1')

    feat_read = sql_lyr.GetNextFeature()

    expected_wkt = 'POLYGON ((1 1 0,5 1 0,5 7 0,1 7 0,1 1 0))'
    
    tr = 1
    if ogrtest.check_feature_geometry( feat_read, expected_wkt ) != 0:
        tr = 0
        print(feat_read.GetGeometryRef().ExportToWkt())

    feat_read.Destroy()
    gdaltest.oci_ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'
    
###############################################################################
# Test handling of specialized Oracle circle Geometries.

def ogr_oci_11():

    if gdaltest.oci_ds is None:
        return 'skip'

    # insert a rectangle geometry.
    gdaltest.oci_ds.ExecuteSQL( """
INSERT INTO geom_test VALUES(
4,
SDO_GEOMETRY(
2003, -- two-dimensional polygon
NULL,
NULL,
SDO_ELEM_INFO_ARRAY(1,1003,4), -- one circle
SDO_ORDINATE_ARRAY(8,7, 10,9, 8,11)
)
)
""" )

    sql_lyr = gdaltest.oci_ds.ExecuteSQL( 'select * from geom_test where ora_fid = 4')

    feat_read = sql_lyr.GetNextFeature()

    expected_wkt = 'POLYGON ((10 9,9.989043790736547 9.209056926535308,9.956295201467611 9.415823381635519,9.902113032590307 9.618033988749895,9.827090915285202 9.8134732861516,9.732050807568877 10.0,9.618033988749895 10.175570504584947,9.486289650954788 10.338261212717716,9.338261212717717 10.486289650954788,9.175570504584947 10.618033988749895,9.0 10.732050807568877,8.8134732861516 10.827090915285202,8.618033988749895 10.902113032590307,8.415823381635519 10.956295201467611,8.209056926535308 10.989043790736547,8 11,7.790943073464693 10.989043790736547,7.584176618364482 10.956295201467611,7.381966011250105 10.902113032590307,7.1865267138484 10.827090915285202,7.0 10.732050807568877,6.824429495415054 10.618033988749895,6.661738787282284 10.486289650954788,6.513710349045212 10.338261212717716,6.381966011250105 10.175570504584947,6.267949192431122 10.0,6.172909084714799 9.8134732861516,6.097886967409693 9.618033988749895,6.043704798532389 9.415823381635519,6.010956209263453 9.209056926535308,6 9,6.010956209263453 8.790943073464694,6.043704798532389 8.584176618364483,6.097886967409693 8.381966011250105,6.172909084714798 8.1865267138484,6.267949192431123 8.0,6.381966011250105 7.824429495415054,6.513710349045212 7.661738787282284,6.661738787282284 7.513710349045212,6.824429495415053 7.381966011250105,7 7.267949192431123,7.1865267138484 7.172909084714798,7.381966011250105 7.097886967409693,7.584176618364481 7.043704798532389,7.790943073464693 7.010956209263453,8 7,8.209056926535306 7.010956209263453,8.415823381635518 7.043704798532389,8.618033988749895 7.097886967409693,8.8134732861516 7.172909084714799,9.0 7.267949192431123,9.175570504584947 7.381966011250105,9.338261212717715 7.513710349045211,9.486289650954788 7.661738787282284,9.618033988749895 7.824429495415053,9.732050807568877 8,9.827090915285202 8.1865267138484,9.902113032590307 8.381966011250105,9.956295201467611 8.584176618364481,9.989043790736547 8.790943073464693,10 9))'
    
    tr = 1
    if ogrtest.check_feature_geometry( feat_read, expected_wkt ) != 0:
        tr = 0
        print(feat_read.GetGeometryRef().ExportToWkt())

    feat_read.Destroy()
    gdaltest.oci_ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'
    
###############################################################################
# Test handling of specialized Oracle circular arc linestring Geometries.

def ogr_oci_12():

    if gdaltest.oci_ds is None:
        return 'skip'

    # insert a rectangle geometry.
    gdaltest.oci_ds.ExecuteSQL( """
INSERT INTO geom_test VALUES(
12,
SDO_GEOMETRY(
2002,
NULL,
NULL,
SDO_ELEM_INFO_ARRAY(1,2,2), -- compound line string
SDO_ORDINATE_ARRAY(0,0, 1,1, 0,2, -1,3, 0,4, 2,2, 0,0 )
)
)
""" )

    sql_lyr = gdaltest.oci_ds.ExecuteSQL( 'select * from geom_test where ora_fid = 12')

    feat_read = sql_lyr.GetNextFeature()

    expected_wkt = 'LINESTRING (0.0 0.0,0.104528463267653 0.005478104631727,0.207911690817759 0.021852399266194,0.309016994374947 0.048943483704846,0.4067366430758 0.086454542357399,0.5 0.133974596215561,0.587785252292473 0.190983005625053,0.669130606358858 0.256855174522606,0.743144825477394 0.330869393641142,0.809016994374947 0.412214747707527,0.866025403784439 0.5,0.913545457642601 0.5932633569242,0.951056516295154 0.690983005625053,0.978147600733806 0.792088309182241,0.994521895368273 0.895471536732347,1 1,0.994521895368273 1.104528463267654,0.978147600733806 1.207911690817759,0.951056516295154 1.309016994374948,0.913545457642601 1.4067366430758,0.866025403784439 1.5,0.809016994374947 1.587785252292473,0.743144825477394 1.669130606358858,0.669130606358858 1.743144825477394,0.587785252292473 1.809016994374948,0.5 1.866025403784439,0.4067366430758 1.913545457642601,0.309016994374947 1.951056516295154,0.207911690817759 1.978147600733806,0.104528463267653 1.994521895368273,0 2,-0.104528463267653 2.005478104631727,-0.207911690817759 2.021852399266194,-0.309016994374947 2.048943483704846,-0.4067366430758 2.086454542357399,-0.5 2.133974596215561,-0.587785252292473 2.190983005625053,-0.669130606358858 2.256855174522606,-0.743144825477394 2.330869393641142,-0.809016994374947 2.412214747707527,-0.866025403784439 2.5,-0.913545457642601 2.593263356924199,-0.951056516295154 2.690983005625053,-0.978147600733806 2.792088309182241,-0.994521895368273 2.895471536732346,-1 3,-0.994521895368273 3.104528463267653,-0.978147600733806 3.207911690817759,-0.951056516295154 3.309016994374948,-0.913545457642601 3.4067366430758,-0.866025403784439 3.5,-0.809016994374948 3.587785252292473,-0.743144825477394 3.669130606358858,-0.669130606358858 3.743144825477394,-0.587785252292473 3.809016994374948,-0.5 3.866025403784438,-0.4067366430758 3.913545457642601,-0.309016994374948 3.951056516295154,-0.20791169081776 3.978147600733806,-0.104528463267653 3.994521895368274,0 4,0.209056926535307 3.989043790736547,0.415823381635519 3.956295201467611,0.618033988749895 3.902113032590307,0.8134732861516 3.827090915285202,1.0 3.732050807568877,1.175570504584946 3.618033988749895,1.338261212717717 3.486289650954788,1.486289650954789 3.338261212717717,1.618033988749895 3.175570504584946,1.732050807568877 3.0,1.827090915285202 2.8134732861516,1.902113032590307 2.618033988749895,1.956295201467611 2.415823381635519,1.989043790736547 2.209056926535307,2 2,1.989043790736547 1.790943073464693,1.956295201467611 1.584176618364481,1.902113032590307 1.381966011250105,1.827090915285202 1.1865267138484,1.732050807568877 1.0,1.618033988749895 0.824429495415054,1.486289650954789 0.661738787282284,1.338261212717717 0.513710349045212,1.175570504584946 0.381966011250105,1.0 0.267949192431123,0.8134732861516 0.172909084714798,0.618033988749895 0.097886967409693,0.415823381635519 0.043704798532389,0.209056926535307 0.010956209263453,0.0 0.0)'
    tr = 1
    if ogrtest.check_feature_geometry( feat_read, expected_wkt ) != 0:
        tr = 0
        print(feat_read.GetGeometryRef().ExportToWkt())

    feat_read.Destroy()
    gdaltest.oci_ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'
###############################################################################
# Test handling of specialized Oracle circular arc polygon Geometries.

def ogr_oci_13():

    if gdaltest.oci_ds is None:
        return 'skip'

    # insert a rectangle geometry.
    gdaltest.oci_ds.ExecuteSQL( """
INSERT INTO geom_test VALUES(
13,
SDO_GEOMETRY(
2003,
NULL,
NULL,
SDO_ELEM_INFO_ARRAY(1,1003,2), -- compound line string
SDO_ORDINATE_ARRAY(0,0, 1,1, 0,2, -1,3, 0,4, 2,2, 0,0 )
)
)
""" )

    sql_lyr = gdaltest.oci_ds.ExecuteSQL( 'select * from geom_test where ora_fid = 13')

    feat_read = sql_lyr.GetNextFeature()

    expected_wkt = 'POLYGON ((0.0 0.0,0.104528463267653 0.005478104631727,0.207911690817759 0.021852399266194,0.309016994374947 0.048943483704846,0.4067366430758 0.086454542357399,0.5 0.133974596215561,0.587785252292473 0.190983005625053,0.669130606358858 0.256855174522606,0.743144825477394 0.330869393641142,0.809016994374947 0.412214747707527,0.866025403784439 0.5,0.913545457642601 0.5932633569242,0.951056516295154 0.690983005625053,0.978147600733806 0.792088309182241,0.994521895368273 0.895471536732347,1 1,0.994521895368273 1.104528463267654,0.978147600733806 1.207911690817759,0.951056516295154 1.309016994374948,0.913545457642601 1.4067366430758,0.866025403784439 1.5,0.809016994374947 1.587785252292473,0.743144825477394 1.669130606358858,0.669130606358858 1.743144825477394,0.587785252292473 1.809016994374948,0.5 1.866025403784439,0.4067366430758 1.913545457642601,0.309016994374947 1.951056516295154,0.207911690817759 1.978147600733806,0.104528463267653 1.994521895368273,0 2,-0.104528463267653 2.005478104631727,-0.207911690817759 2.021852399266194,-0.309016994374947 2.048943483704846,-0.4067366430758 2.086454542357399,-0.5 2.133974596215561,-0.587785252292473 2.190983005625053,-0.669130606358858 2.256855174522606,-0.743144825477394 2.330869393641142,-0.809016994374947 2.412214747707527,-0.866025403784439 2.5,-0.913545457642601 2.593263356924199,-0.951056516295154 2.690983005625053,-0.978147600733806 2.792088309182241,-0.994521895368273 2.895471536732346,-1 3,-0.994521895368273 3.104528463267653,-0.978147600733806 3.207911690817759,-0.951056516295154 3.309016994374948,-0.913545457642601 3.4067366430758,-0.866025403784439 3.5,-0.809016994374948 3.587785252292473,-0.743144825477394 3.669130606358858,-0.669130606358858 3.743144825477394,-0.587785252292473 3.809016994374948,-0.5 3.866025403784438,-0.4067366430758 3.913545457642601,-0.309016994374948 3.951056516295154,-0.20791169081776 3.978147600733806,-0.104528463267653 3.994521895368274,0 4,0.209056926535307 3.989043790736547,0.415823381635519 3.956295201467611,0.618033988749895 3.902113032590307,0.8134732861516 3.827090915285202,1.0 3.732050807568877,1.175570504584946 3.618033988749895,1.338261212717717 3.486289650954788,1.486289650954789 3.338261212717717,1.618033988749895 3.175570504584946,1.732050807568877 3.0,1.827090915285202 2.8134732861516,1.902113032590307 2.618033988749895,1.956295201467611 2.415823381635519,1.989043790736547 2.209056926535307,2 2,1.989043790736547 1.790943073464693,1.956295201467611 1.584176618364481,1.902113032590307 1.381966011250105,1.827090915285202 1.1865267138484,1.732050807568877 1.0,1.618033988749895 0.824429495415054,1.486289650954789 0.661738787282284,1.338261212717717 0.513710349045212,1.175570504584946 0.381966011250105,1.0 0.267949192431123,0.8134732861516 0.172909084714798,0.618033988749895 0.097886967409693,0.415823381635519 0.043704798532389,0.209056926535307 0.010956209263453,0.0 0.0))'

    tr = 1
    if ogrtest.check_feature_geometry( feat_read, expected_wkt ) != 0:
        tr = 0
        print(feat_read.GetGeometryRef().ExportToWkt())

    feat_read.Destroy()
    gdaltest.oci_ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'
        
###############################################################################
# Test handling of compount linestring.

def ogr_oci_14():

    if gdaltest.oci_ds is None:
        return 'skip'

    # insert a rectangle geometry.
    gdaltest.oci_ds.ExecuteSQL( """
INSERT INTO geom_test VALUES(
11,
SDO_GEOMETRY(
2002,
NULL,
NULL,
SDO_ELEM_INFO_ARRAY(1,4,2, 1,2,1, 3,2,2), -- compound line string
SDO_ORDINATE_ARRAY(10,10, 10,14, 6,10, 14,10)
)
)
""" )

    sql_lyr = gdaltest.oci_ds.ExecuteSQL( 'select * from geom_test where ora_fid = 11')

    feat_read = sql_lyr.GetNextFeature()

    expected_wkt = 'LINESTRING (10 10 0,10 14 0,9.581886146929387 13.978087581473094 0,9.168353236728963 13.912590402935223 0,8.76393202250021 13.804226065180615 0,8.373053427696799 13.654181830570405 0,8.0 13.464101615137755 0,7.648858990830108 13.23606797749979 0,7.323477574564567 12.972579301909576 0,7.027420698090424 12.676522425435433 0,6.76393202250021 12.351141009169893 0,6.535898384862245 12.0 0,6.345818169429597 11.626946572303202 0,6.195773934819385 11.23606797749979 0,6.087409597064777 10.831646763271037 0,6.021912418526907 10.418113853070615 0,6 10 0,6.021912418526906 9.581886146929389 0,6.087409597064777 9.168353236728963 0,6.195773934819385 8.763932022500208 0,6.345818169429595 8.373053427696801 0,6.535898384862246 8.0 0,6.76393202250021 7.648858990830108 0,7.027420698090423 7.323477574564567 0,7.323477574564566 7.027420698090424 0,7.648858990830107 6.76393202250021 0,8 6.535898384862247 0,8.373053427696799 6.345818169429596 0,8.76393202250021 6.195773934819385 0,9.168353236728962 6.087409597064777 0,9.581886146929387 6.021912418526906 0,10 6 0,10.418113853070611 6.021912418526906 0,10.831646763271035 6.087409597064777 0,11.23606797749979 6.195773934819385 0,11.626946572303202 6.345818169429597 0,12.0 6.535898384862246 0,12.351141009169892 6.76393202250021 0,12.676522425435431 7.027420698090422 0,12.972579301909576 7.323477574564567 0,13.23606797749979 7.648858990830107 0,13.464101615137753 8 0,13.654181830570405 8.373053427696799 0,13.804226065180615 8.76393202250021 0,13.912590402935223 9.16835323672896 0,13.978087581473094 9.581886146929387 0,14.0 10 0)'

    tr = 1
    if ogrtest.check_feature_geometry( feat_read, expected_wkt ) != 0:
        tr = 0
        print(feat_read.GetGeometryRef().ExportToWkt())

    feat_read.Destroy()
    gdaltest.oci_ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'
        
###############################################################################
# Test handling of compount polygon.

def ogr_oci_15():

    if gdaltest.oci_ds is None:
        return 'skip'

    # insert a rectangle geometry.
    gdaltest.oci_ds.ExecuteSQL( """
INSERT INTO geom_test VALUES(
21,
SDO_GEOMETRY(
2003,
NULL,
NULL,
SDO_ELEM_INFO_ARRAY(1,1005,2, 1,1003,1, 3,1003,2),
SDO_ORDINATE_ARRAY(-10,10, 10,10, 0,0, -10,10)
)
)
""" )

    sql_lyr = gdaltest.oci_ds.ExecuteSQL( 'select * from geom_test where ora_fid = 21')

    feat_read = sql_lyr.GetNextFeature()

    expected_wkt = 'POLYGON ((-10 10 0,10 10 0,9.945218953682733 8.954715367323466 0,9.781476007338057 7.920883091822407 0,9.510565162951535 6.909830056250526 0,9.135454576426008 5.932633569241999 0,8.660254037844387 5.0 0,8.090169943749475 4.122147477075268 0,7.431448254773942 3.308693936411418 0,6.691306063588582 2.568551745226059 0,5.877852522924732 1.909830056250526 0,5.0 1.339745962155615 0,4.067366430758002 0.864545423573992 0,3.090169943749475 0.489434837048465 0,2.079116908177594 0.218523992661945 0,1.045284632676535 0.054781046317267 0,0.0 0.0 0,-1.045284632676533 0.054781046317267 0,-2.079116908177591 0.218523992661943 0,-3.090169943749474 0.489434837048464 0,-4.067366430758001 0.86454542357399 0,-5 1.339745962155613 0,-5.87785252292473 1.909830056250526 0,-6.691306063588582 2.568551745226058 0,-7.43144825477394 3.308693936411417 0,-8.090169943749473 4.122147477075267 0,-8.660254037844387 5.0 0,-9.135454576426007 5.932633569241996 0,-9.510565162951535 6.909830056250526 0,-9.781476007338057 7.920883091822407 0,-9.945218953682733 8.954715367323463 0,-10 10 0))'

    tr = 1
    if ogrtest.check_feature_geometry( feat_read, expected_wkt ) != 0:
        tr = 0
        print(feat_read.GetGeometryRef().ExportToWkt())

    feat_read.Destroy()
    gdaltest.oci_ds.ReleaseResultSet( sql_lyr )

    if tr:
        return 'success'
    else:
        return 'fail'
        
###############################################################################
# Test deleting an existing layer.

def ogr_oci_16():

    if gdaltest.oci_ds is None:
        return 'skip'

    target_index = -1
    lc = gdaltest.oci_ds.GetLayerCount()
    
    for i in range(lc):
        lyr = gdaltest.oci_ds.GetLayer( i )
        if lyr.GetName() == 'TESTSRS2':
            target_index = i
            break

    lyr = None

    if target_index == -1:
        gdaltest.post_reason( 'did not find testsrs2 layer' )
        return 'fail'
    
    result = gdaltest.oci_ds.DeleteLayer( target_index )
    if result != 0:
        gdaltest.post_reason( 'DeleteLayer() failed.' )
        return 'fail'
    
    lyr = gdaltest.oci_ds.GetLayerByName( 'testsrs2' )
    if lyr is not None:
        gdaltest.post_reason( 'apparently failed to remove testsrs2 layer' )
        return 'fail'
        
    return 'success'
        
###############################################################################
# Test that synctodisk actually sets the layer bounds metadata. 

def ogr_oci_17():

    if gdaltest.oci_ds is None:
        return 'skip'
    
    gdaltest.oci_ds.ExecuteSQL( 'DELLAYER:xpoly' )
    
    ######################################################
    # Create Oracle Layer
    gdaltest.oci_lyr = gdaltest.oci_ds.CreateLayer( 'xpoly' )

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

    ######################################################
    # Create a distinct connection to the same database to monitor the
    # metadata table.

    oci_ds2 = ogr.Open( os.environ['OCI_DSNAME'] )
    
    sql_lyr = oci_ds2.ExecuteSQL( "select column_name from user_sdo_geom_metadata where table_name = 'XPOLY'" )
    if sql_lyr.GetFeatureCount() > 0:
        gdaltest.post_reason( 'user_sdo_geom_metadata already populated!' )
        return 'fail'

    oci_ds2.ReleaseResultSet( sql_lyr )

    result = gdaltest.oci_ds.SyncToDisk()
    if result != 0:
        gdaltest.post_reason( 'SyncToDisk() failed.' )
        return 'fail'

    sql_lyr = oci_ds2.ExecuteSQL( "select column_name from user_sdo_geom_metadata where table_name = 'XPOLY'" )
    if sql_lyr.GetFeatureCount() == 0:
        gdaltest.post_reason( 'user_sdo_geom_metadata still not populated!' )
        return 'fail'

    oci_ds2.ReleaseResultSet( sql_lyr )

    oci_ds2 = None
    
    return 'success'
   

###############################################################################
# 

def ogr_oci_cleanup():

    if gdaltest.oci_ds is None:
        return 'skip'

    gdaltest.oci_ds.ExecuteSQL( 'DELLAYER:tpoly' )
    gdaltest.oci_ds.ExecuteSQL( 'DELLAYER:xpoly' )
    gdaltest.oci_ds.ExecuteSQL( 'DELLAYER:testsrs' )
    gdaltest.oci_ds.ExecuteSQL( 'DELLAYER:testsrs2' )
    gdaltest.oci_ds.ExecuteSQL( 'drop table geom_test' )

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
    ogr_oci_10,
    ogr_oci_11,
    ogr_oci_12,
    ogr_oci_13,
    ogr_oci_14,
    ogr_oci_15,
    ogr_oci_16,
    ogr_oci_17,
    ogr_oci_cleanup ]

if __name__ == '__main__':

    if 'OCI_DSNAME' not in os.environ:
        print('Enter ORACLE DataSource (eg. OCI:scott/tiger):')
        oci_dsname = sys.stdin.readline().strip()
        os.environ['OCI_DSNAME'] = oci_dsname

    gdaltest.setup_run( 'ogr_oci' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

